/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/url.h"
#include "php_http.h"

#define http_sock_name "Http Socket Buffer"

typedef struct {
    php_stream *stream;
    
    char *host;
    short port;
    double timeout;
    double read_timeout;

    int persistent;
    char *persistent_id;
} HttpSock;

/* If you declare any globals in php_http.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(http)
*/
ZEND_DECLARE_MODULE_GLOBALS(http);//declare module globals

/* True global resources - no need for thread safety here */
static int le_http_sock;

zend_class_entry *http_class_ce;//define http class
//php/Zend/zend_API.h:170:2: 错误：未知的类型名‘zend_http_globals

static zend_function_entry http_functions[] = {
    PHP_ME(http, __construct, NULL, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
    PHP_ME(http, __destruct, NULL, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
    PHP_ME(http, set, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(http, get, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(http, post, NULL, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

void exchange(char *a, char *b){
    *a = *a ^ *b;
    *b = *a ^ *b;
    *a = *a ^ *b;
}

void itoa(zend_long n, char *str)
{
    int i,j;
    zend_long sign;
    if((sign = n) < 0){//记录正负号
        n = -n;
    }

    i = 0;
    char *p = str;
    do{
        *p = n%10 + '0';
        p++;
        i++;
    } while( (n = n/10) > 0 );

    if(sign < 0){
        *p = '-';
        p++;
        i++;
    }
    *p = '\0';
    for(j = 0; j < i/2; j++){
        exchange(str+j, str+i-j-1);
    }
}

zend_string * zval2string(zval *z)
{
    zend_string *res = (zend_string *)emalloc(sizeof(zend_string));
    char *p = res->val;
    switch(Z_TYPE_P(z)){
        case IS_NULL:
            p = "null"; 
            res->len = strlen("null");
            break;
        case IS_FALSE:
            p = "false"; 
            res->len = strlen("false");
            break;
        case IS_TRUE:
            p = "true"; 
            res->len = strlen("true");
            break;
        case IS_LONG:
            itoa(z->value.lval, res->val); 
            res->len = strlen(res->val);
            break;
        case IS_DOUBLE:
            itoa(z->value.lval, res->val); 
            res->len = strlen(res->val);
            break;
        case IS_STRING:
            res = z->value.str;
            break;
        default :
            p = "array ? object ? resource or reference ?";
            res->len = 64;
            break;
    }
    return res;
}

char * http_build_query(zval *params)
{
    char *query = NULL, *bind = "&";
    int query_len = 0, bind_len = 1, size = 1024;

    if(Z_TYPE_P(params) != IS_ARRAY){
        zend_throw_exception(NULL, "Parameter 1 expected to be Array.  Incorrect value given", 0);
        return ;
    }

    zend_array *arr = Z_ARR_P(params);

    Bucket *p = arr->arData;
    Bucket *end = p + arr->nNumUsed;
    query = (char *)ecalloc(size, sizeof(char));
    for(; p != end; p++){
        query_len += p->key->len + zval2string(&p->val)->len + bind_len + 1;
        if(query_len > size){
            size *= 2;
            query = (char *)erealloc(query, size);
        }
        if(query){
            strcat(query, bind);
        }
        strcat(query, p->key->val);
        strcat(query, "=");
        strcat(query, zval2string(&p->val)->val);
    }
    //memcpy(query, query, query_len - bind_len);
    //printf("%s\n", query);
    return query;
}

void array_exists(char *context, zend_array *za, char *str, size_t len, char *def)
{
    if(za == NULL || za->nNumUsed == 0 || !zend_hash_str_exists(za, str, len)){
        strcat(context, def);
    }
}

char * http_build_header(INTERNAL_FUNCTION_PARAMETERS)
{
    Bucket *p, *end;
    zval *url, *rv;
    
    int size = 1024;
    char *context = (char *)emalloc(sizeof(char)*size);
    
    zval *header = zend_read_property(http_class_ce, getThis(), "header", strlen("header"), 1, rv);

    zend_array *arr_hd = (header->value).arr;

    if(arr_hd && arr_hd->nNumUsed > 0){
        p = arr_hd->arData;
        end = p + arr_hd->nNumUsed;
        for(; p != end; p++){
            strcat(context, Z_STR(p->val)->val);
            strcat(context, "\r\n");
        }
    }

    array_exists(context, arr_hd, "Accept",       strlen("Accept"),       "Accept: */*\r\n");
    array_exists(context, arr_hd, "User-Agent",   strlen("User-Agent"),   "User-Agent: A Http Client\r\n");
    array_exists(context, arr_hd, "Content-Type", strlen("Content-Type"), "Content-Type: application/x-www-form-urlencoded\r\n");
    array_exists(context, arr_hd, "Connection",   strlen("Connection"),   "Connection: Keep-Alive\r\n");
    //printf("%s\n", context);
    return context;
}

HttpSock * http_sock_create(char *host, short port, double timeout, int persistent)
{
    HttpSock *http_sock;

    http_sock = emalloc(sizeof(HttpSock));
    http_sock->stream = NULL;
    http_sock->host = host;
    http_sock->port = port;
    http_sock->timeout = timeout;
    http_sock->persistent = persistent;

    return http_sock;
}

void http_sock_disconnect(HttpSock *http_sock)
{
    if(!http_sock->persistent){
        php_stream_close(http_sock->stream);
    }

    http_sock->stream = NULL;
}

void http_free_socket(HttpSock *http_sock)
{
    efree(http_sock->host);
    efree(http_sock);
}

int strmcat(char *dest, int dest_len, char *src, int n, size_t *max_len)
{
    if(dest_len + n < *max_len){//判断是否需要扩容
        *max_len = *max_len*2;
        char *buf = (char *)ecalloc(*max_len, 1);
        memcpy(buf, dest, dest_len);
        efree(dest);
        dest = buf;
    }
    strncat(dest, src, n);
    dest_len += n;
    return dest_len;
}

int * next_prifix(char *p)
{
    size_t m = strlen(p);
    int *next = (int *)ecalloc(m, sizeof(int));

    int i, k = 0;
    next[0] = 0;
    for(i = 1; i < m; i++){
        while(k > 0 && p[i] != p[k]){
            k = next[k-1];
        }
        if(p[i] == p[k]){
            k = k + 1;
        }
        next[i] = k;
    }
    return next;
}

/**
 *
 * 字符串分隔，kmp算法改编
 */
zend_array * explode(char *str, char *d)
{
    zend_array *za = (zend_array *)emalloc(sizeof(zend_array));//分割结果数组
    zend_hash_init(za, 8, NULL, ZVAL_PTR_DTOR, 0);
    zend_ulong j = 0;
    zval *pData;
    
    //临时字符串
    size_t max_size = 1024;
    // size_t *buf_max_size;
    // buf_max_size = &max_size;
    zend_string *buf = zend_string_alloc(max_size, 0);
    //buf->len = 0;
    
    int str_len = strlen(str);

    int m = strlen(d);
    int *next = next_prifix(d);
    int i = 0, k = 0;
    char c[2] = "A";
    //printf("\n------------------------------------------------------------\n");
    for(i = 0; i < str_len; i++){
        while(k > 0 && str[i] != d[k]){
            //buf->len = strmcat(buf->val, buf->len, d, k - next[k-1], &max_size);
            strncat(buf->val, d, k - next[k-1]);
            k = next[k-1];
        }
        if(str[i] == d[k]){
            k = k + 1;
        }else{
            c[0] = str[i];//字符转变为1长度的字符串
            //buf->len = strmcat(buf->val, buf->len, c, 1, &max_size);
            strncat(buf->val, c, 1);
        }
        if(k == m || i == str_len-1){
            //printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++\n%s\n", buf->val);
            pData = (zval *)emalloc(sizeof(zval));
            Z_STR_P(pData) = buf;
            zend_hash_index_add(za, j, pData);
            j++;
            
            max_size = 1024;
            if(i != str_len-1){
                buf = zend_string_alloc(max_size, 0);
            }
            // printf("\n");
            k = 0;//look for the next match
        }
    }
    //efree(buf);
    efree(next);
    //printf("++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    return za;
}

char * substr(char *str, int s, int e)
{
    char *ret = (char *)emalloc(sizeof(char)*(e+1));
    int i,j = s;
    for(i = 0; i < e; i++,j++){
        *(ret+i) = *(str+j);
    }
    *(ret+i) = '\0';
    return ret;
}

char * http_parse(INTERNAL_FUNCTION_PARAMETERS, char *response)
{
    //printf("\n------------------------------------------------------------\n%s\n", response);
    zend_array *arr_rps = explode(response, "\r\n\r\n");
    Bucket *rps = arr_rps->arData;

    zend_string *http_header = Z_STR(rps->val);
    //printf("%s\n", http_header->val);
    zend_update_property_long(http_class_ce, getThis(), "code", strlen("code"), atoi(substr(http_header->val, 9, 3)));

    zend_array *header = explode(http_header->val, "\r\n");

    rps++;
    zend_string *http_data = Z_STR(rps->val);
    //printf("%s\n", http_data->val);

    zend_array *arr_data = explode(http_data->val, "\r\n");
    Bucket *p = arr_data->arData;
    Bucket *end = p + arr_data->nNumUsed;

    size_t max_len = 1024;
    char *ret = (char *)emalloc(sizeof(char)*max_len);
    int ret_len = 0;
    for(; p != end; p++){
        printf("%s\n", Z_STRVAL(p->val));
        //ret_len = strmcat(ret, ret_len, Z_STRVAL(p->val), Z_STRLEN(p->val), &max_len);
        strncat(ret, Z_STRVAL(p->val), Z_STRLEN(p->val));
    }
    efree(arr_rps);
    efree(arr_data);
    return ret;
}

int http_sock_connect(HttpSock *http_sock)
{
    char *host = NULL;
    size_t host_len;

    zend_string *errstr;
    int errcode = 0;
    char *persistent_id;

    host = (char *)emalloc(strlen(http_sock->host) + 16);
    host_len = sprintf(host, "tcp://%s:%d", http_sock->host, http_sock->port);
    
    if (http_sock->persistent) {
        persistent_id = (char *)emalloc(strlen(host) + 16);
        if (http_sock->persistent_id) {
            sprintf(persistent_id, "http:%s:%s", host, http_sock->persistent_id);
        } else {
            sprintf(persistent_id, "http:%s:%f", host, http_sock->timeout);
        }
    }

    http_sock->stream = php_stream_xport_create(host, host_len, 0, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT, 
        persistent_id, NULL, NULL, &errstr, &errcode);
    if(!http_sock->stream){
        http_free_socket(http_sock);
        zend_throw_exception(NULL, errstr, 0);
        efree(errstr);
        return 0;
    }

    efree(host);

    if(persistent_id){
        efree(persistent_id);    
    }

    return 1;
}

int http_request(INTERNAL_FUNCTION_PARAMETERS, char *context, char **response)
{
    double timeout = 1;
    int persistent = 1;

    zval *url, *rv;
    php_url *phpurl;

    url = zend_read_property(http_class_ce, getThis(), "url", strlen("url"), 1, rv);
    phpurl = php_url_parse(Z_STRVAL_P(url));

    if(!phpurl->port){ /* not unix socket, set to default value */
        phpurl->port = 80;
    }

    HttpSock *http_sock = http_sock_create(phpurl->host, phpurl->port, timeout, persistent);

    if(!http_sock_connect(http_sock)){
        zend_throw_exception(NULL, "http connect error", 0);
        return 0;
    }
    php_url_free(phpurl);
    //zend_list_insert(http_sock, le_http_sock TSRMLS_CC);

    if(php_stream_write_string(http_sock->stream, context)){
        efree(context);
        //printf("php stream write success!\n");
    }

    int buf_size = 1024;
    char *data = (char *)ecalloc(buf_size, sizeof(char));
    char *buf = (char *)ecalloc(buf_size, sizeof(char));
    int data_size = 0;
    int chunked = 0;

    while(!php_stream_eof(http_sock->stream)){
        if(php_stream_read(http_sock->stream, buf, buf_size) <= 0){
            /* Error or EOF */
            zend_throw_exception(NULL, "socket error on read socket", 0);
        }
        if(chunked == 0 && strstr(buf, "Transfer-Encoding: chunked")){
            chunked = 1;
        }else{
            chunked = -1;
        }
        if(buf){
            strcat(data, buf);
            data_size += buf_size;
            //erealloc(data, data_size);
            //strcat(data, buf);
        }
        if(chunked == 1){
            if(strstr(buf, "0\r\n\r\n")){
                break;
            }
        }else{
            if(strlen(buf) < buf_size){
                break;
            }
        }
    }

    *response = http_parse(INTERNAL_FUNCTION_PARAM_PASSTHRU, data);
    efree(data);
    efree(buf);
    //printf("1 %s %p\n", *response, *response);
    return 1;
}

/**
 *
 */
PHP_METHOD(http, __construct)
{
    char *url = NULL;
    size_t url_len;
    if(zend_parse_parameters(ZEND_NUM_ARGS(), "s", &url, &url_len) == FAILURE){
        RETURN_FALSE;
    }
    //printf("url:%s\n", url);
    if(url_len == 0){
        zend_throw_exception(NULL, "invalid url", 0);
    }

    zend_update_property_string(http_class_ce, getThis(), "url", strlen("url"), url);
}

/**
 *
 */
PHP_METHOD(http, __destruct)
{
    if(zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE){
        RETURN_FALSE;
    }

}

PHP_METHOD(http, set){
    zval *header;

    if(zend_parse_parameters(ZEND_NUM_ARGS(), "a", header) == FAILURE){
        RETURN_FALSE;
    }

    zend_update_property(http_class_ce, getThis(), "header", strlen("header"), header);
}

PHP_METHOD(http, get)
{
    char *query = "", *header, *result, *context;
    zval *params;
    zval *url, *rv;
    //获取参数
    if(zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &params) == FAILURE){
        RETURN_FALSE;
    }
    
    //获取url属性
    url = zend_read_property(http_class_ce, getThis(), "url", strlen("url"), 1, rv);

    //拼接query
    if(Z_TYPE_P(params) == IS_ARRAY){
        query = http_build_query(params);
    }
    //拼接header
    header = http_build_header(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    //生成context
    context = (char *)emalloc(1024);
    sprintf(context, "GET %s%s HTTP/1.1\r\n%s\r\n", Z_STRVAL_P(url), query, header);
    // printf("%s\n", context);
    char *response;
    if(http_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, context, &response) == FAILURE){
        RETURN_FALSE;
    }
    //printf("2 %s %p\n", response, response);
    RETURN_STRING(response);
}

PHP_METHOD(http, post)
{

    char *query, *data = NULL, *header = NULL, *result = NULL, *context = NULL;
    zval *params;
    zval *url, *rv;

    if(zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &params) == FAILURE){
        RETURN_FALSE;
    }

    url = zend_read_property(http_class_ce, getThis(), "url", strlen("url"), 1, rv);

    //data = http_build_query(params);
    if(Z_TYPE_P(params) == IS_ARRAY){
        data = http_build_query(params);
    }
    if(strlen(data) > 0){
        strcat(data, "\r\n\r\n");
    }

    header = http_build_header(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    sprintf(context, "POST %s HTTP/1.1\r\n%sContent-Length: %d\r\n\r\n%s", url, header, strlen(data), data);

    char *response;
    if(http_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, context, &response) == FAILURE){
        RETURN_FALSE;
    }

    RETURN_STRING(response);
}

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("http.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_http_globals, http_globals)
    STD_PHP_INI_ENTRY("http.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_http_globals, http_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_http_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_http_compiled)
{
	char *arg = NULL;
	size_t arg_len, len;
	zend_string *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "http", arg);

	RETURN_STR(strg);
}

static void http_sock_dtor(zend_resource *rsrc)
{
    HttpSock *http_sock = (HttpSock *) rsrc->ptr;
    http_sock_disconnect(http_sock);
    http_free_socket(http_sock);
}

/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_http_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_http_init_globals(zend_http_globals *http_globals)
{
	http_globals->global_value = 0;
	http_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(http)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
    zend_class_entry http_ce;
    INIT_CLASS_ENTRY(http_ce, "http", http_functions);

    http_class_ce = zend_register_internal_class(&http_ce TSRMLS_CC);

    //定义属性
    zend_declare_property_long(http_class_ce, "port", strlen("port"), 80, ZEND_ACC_PUBLIC);
    zend_declare_property_long(http_class_ce, "timeout", strlen("timeout"), 1, ZEND_ACC_PUBLIC);
    zend_declare_property_long(http_class_ce, "length", strlen("length"), 8196, ZEND_ACC_PUBLIC);
    zend_declare_property_string(http_class_ce, "url", strlen("url"), "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(http_class_ce, "user_agent", strlen("user_agent"), "your agent", ZEND_ACC_PUBLIC);
    zend_declare_property_null(http_class_ce, "header", strlen("header"), ZEND_ACC_PUBLIC);

    zend_declare_property_long(http_class_ce, "code", strlen("code"), 0, ZEND_ACC_PUBLIC);
    
    le_http_sock = zend_register_list_destructors_ex(
        http_sock_dtor,
        NULL,
        http_sock_name, module_number
    );

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(http)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there"s nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(http)
{
#if defined(COMPILE_DL_HTTP) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there"s nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(http)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(http)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "http support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */

/* {{{ http_functions[]
 *
 * Every user visible function must have an entry in http_functions[].
 */
const zend_function_entry http_test_functions[] = {
	PHP_FE(confirm_http_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in http_functions[] */
};
/* }}} */

/* {{{ http_module_entry
 */
zend_module_entry http_module_entry = {
	STANDARD_MODULE_HEADER,
	"http",
	http_test_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),		/* Replace with NULL if there"s nothing to do at request start */
	PHP_RSHUTDOWN(http),	/* Replace with NULL if there"s nothing to do at request end */
	PHP_MINFO(http),
	PHP_HTTP_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_HTTP
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(http)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
