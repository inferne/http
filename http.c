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

char * http_build_query(zval *params)
{
    Bucket *p, *end;
    char *query = NULL, *bind = "&";
    int query_len = 0, bind_len = 1;

    if(Z_TYPE_P(params) != IS_ARRAY){
        zend_throw_exception(NULL, "Parameter 1 expected to be Array.  Incorrect value given", 0);
        return ;
    }

    zend_array *arr = Z_ARR_P(params);

    p = arr->arData;
    end = p + arr->nNumUsed;
    for(; p != end; p++){
        query_len += p->key->len + Z_STR(p->val)->len + bind_len + 1;
        query = (char *)erealloc(query, query_len);
        sprintf(query, "%s%s=%s%s", query, p->key->val, Z_STR(p->val)->val, bind);
    }
    memcpy(query, query, query_len - bind_len);

    return query;
}

char * http_build_header(INTERNAL_FUNCTION_PARAMETERS)
{
    Bucket *p, *end;
    char *context = "";
    zval *url, *rv;
    zval *header = zend_read_property(http_class_ce, getThis(), "header", strlen("header"), 1, rv);

    zend_array *arr_hd = (header->value).arr;

    if(arr_hd->nNumUsed > 0){
        p = arr_hd->arData;
        end = p + arr_hd->nNumUsed;
        for(; p != end; p++){
            sprintf(context, "%s%s\r\n", context, Z_STR(p->val)->val);
        }
    }

    if(!zend_hash_str_exists(arr_hd, "Accept", strlen("Accept"))) {
        sprintf(context, "%s%s", context, "Accept: */*\r\n");
    }

    //context .= "Host: ".$this->url["host"]."\r\n";
    
    if(!zend_hash_str_exists(arr_hd, "User-Agent", strlen("User-Agent"))) {
        sprintf(context, "%s%s", context, "User-Agent: Http Client\r\n");
    }

    if(!zend_hash_str_exists(arr_hd, "Content-Type", strlen("Content-Type"))) {
        sprintf(context, "%s%s", context, "Content-Type: application/x-www-form-urlencoded\r\n");
    }

    if(!zend_hash_str_exists(arr_hd, "Connection", strlen("Connection"))) {
        sprintf(context, "%s%s", context, "Connection: Keep-Alive\r\n");
    }

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
    //
}

void http_free_socket(HttpSock *http_sock)
{
    //
}

char * http_parse(char *response)
{
    char *http_header = NULL;
    char *http_data;
    char *data;

    http_header = strtok(response, "\r\n\r\n");
    http_data = strtok(response, "\r\n\r\n");

    while(strtok(http_data, "\r\n") > 0){
        strcat(data, strtok(http_data, "\r\n"));
    }
    return data;
}

char * http_request(INTERNAL_FUNCTION_PARAMETERS, char *context)
{
    char *name = NULL;
    size_t name_len;
    double timeout = 1;

    zend_string *errstr;
    char *persistent_id;
    int persistent = 1, errcode = 0;

    zval *url, *rv;
    php_url *phpurl;

    url = zend_read_property(http_class_ce, getThis(), "url", strlen("url"), 1, rv);
    phpurl = php_url_parse(Z_STRVAL_P(url));

    if(!phpurl->port){ /* not unix socket, set to default value */
        phpurl->port = 80;
    }

    HttpSock *http_sock = http_sock_create(phpurl->host, phpurl->port, timeout, persistent);

    name_len = sprintf(name, "tcp://%s:%d", phpurl->host, phpurl->port);
    
    if (http_sock->persistent) {
        if (http_sock->persistent_id) {
            sprintf(persistent_id, "http:%s:%s", name, http_sock->persistent_id);
        } else {
            sprintf(persistent_id, "http:%s:%f", name, http_sock->timeout);
        }
    }

    http_sock->stream = php_stream_xport_create(name, name_len, 0, STREAM_XPORT_CLIENT | STREAM_XPORT_CONNECT, 
        persistent_id, NULL, NULL, &errstr, &errcode);
    if(!http_sock->stream){
        http_free_socket(http_sock);
        return ;
    }

    efree(name);

    if(errstr){
        zend_throw_exception(NULL, errstr, 0);
        efree(errstr);
    }

    //zend_list_insert(http_sock, le_http_sock TSRMLS_CC);

    php_stream_write_string(http_sock->stream, context);

    char *response = NULL;
    char buf[1024] = "";
    int res_size = 0;

    while(!php_stream_eof(http_sock->stream)){
        php_stream_read(http_sock->stream, buf, 1024);
        if(buf){
            res_size += 1024;
            erealloc(response, res_size);
            strcat(response, buf);
        }
        if(strstr(buf, "0\r\n\r\n")){
            break;
        }
    }

    return http_parse(response);
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
    char *query, *header, *result, *context;
    zval *params;
    zval *url, *rv;
    //获取参数
    if(zend_parse_parameters(ZEND_NUM_ARGS(), "|a", &params) == FAILURE){
        RETURN_FALSE;
    }
    //获取url属性
    url = zend_read_property(http_class_ce, getThis(), "url", strlen("url"), 1, rv);

    //拼接query
    query = http_build_query(params);
    //拼接header
    header = http_build_header(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    //生成context
    sprintf(context, "GET %s?%s HTTP/1.1\r\n%s\r\n", url, query, header);

    result = http_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, context);

    RETURN_STRING(result);
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

    data = http_build_query(params);
    if(strlen(data) > 0){
        strcat(data, "\r\n\r\n");
    }

    header = http_build_header(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    sprintf(context, "POST %s HTTP/1.1\r\n%sContent-Length: %d\r\n\r\n%s", url, header, strlen(data), data);

    result = http_request(INTERNAL_FUNCTION_PARAM_PASSTHRU, context);

    RETURN_STRING(result);
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
