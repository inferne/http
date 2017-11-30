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
#include "php_http.h"

/* If you declare any globals in php_http.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(http)
*/

/* True global resources - no need for thread safety here */
static int le_http;

zend_class_entry http_class_ce;//define http class

ZEND_DECLARE_MODULE_GLOBALS(http);//declare module globals

static zend_function_entry http_functions[] = {
    PHP_ME(http, __construct, NULL, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
    PHP_ME(http, __destruct, NULL, ZEND_ACC_DTOR | ZEND_ACC_PUBLIC)
    PHP_ME(http, get, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(http, post, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
}


/**
 *
 */
PHP_METHOD(http, __construct)
{
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE){
        RETURN_FALSE;
    }
}

/**
 *
 */
PHP_METHOD(http, __destruct)
{

}

PHP_METHOD(http, get)
{
    //
}

PHP_METHOD(http, post)
{
    //
}

http_create_sock();

http_build_header(){}

http_request(char *context)
{
    char *host;
    size_t host_len;
    zend_long port = -1;
    zend_long timeout = 1;

    int persistent = 1;
    char *persistent_id = NULL;
    int persistent_id_len = -1;

    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|ll", &host, &host_len, &port, &timeout) == FAILURE){
        RETURN_FALSE;
    }

    if(timeout < 0L || timeout > INT_MAX){
        zend_throw_exception(NULL, "Invalid timeout", 0);
        RETURN_FALSE;
    }

    if(port == -1 && host[0] != '/' && host_len){
        port = 80;
    }

    http_sock = http_create_sock();

    if(){

    }
}

http_parse(char *response){}

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
    zend_declare_property_string(http_class_ce, "header", strlen("header"), "", ZEND_ACC_PUBLIC);

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

/* Remove if there's nothing to do at request start */
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

/* Remove if there's nothing to do at request end */
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
const zend_function_entry http_functions[] = {
	PHP_FE(confirm_http_compiled,	NULL)		/* For testing, remove later. */
	PHP_FE_END	/* Must be the last line in http_functions[] */
};
/* }}} */

/* {{{ http_module_entry
 */
zend_module_entry http_module_entry = {
	STANDARD_MODULE_HEADER,
	"http",
	http_functions,
	PHP_MINIT(http),
	PHP_MSHUTDOWN(http),
	PHP_RINIT(http),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(http),	/* Replace with NULL if there's nothing to do at request end */
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
