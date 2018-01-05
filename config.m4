dnl $Id$
dnl config.m4 for extension http

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(http, for http support,
dnl Make sure that the comment is aligned:
dnl [  --with-http             Include http support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(http, whether to enable http support,
dnl Make sure that the comment is aligned:
[  --enable-http           Enable http support])

if test "$PHP_HTTP" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-http -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/http.h"  # you most likely want to change this
  dnl if test -r $PHP_HTTP/$SEARCH_FOR; then # path given as parameter
  dnl   HTTP_DIR=$PHP_HTTP
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for http files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       HTTP_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$HTTP_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the http distribution])
  dnl fi

  dnl # --with-http -> add include path
  dnl PHP_ADD_INCLUDE($HTTP_DIR/include)

  dnl # --with-http -> check for lib and symbol presence
  dnl LIBNAME=http # you may want to change this
  dnl LIBSYMBOL=http # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $HTTP_DIR/$PHP_LIBDIR, HTTP_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_HTTPLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong http lib version or lib not found])
  dnl ],[
  dnl   -L$HTTP_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(HTTP_SHARED_LIBADD)

  PHP_NEW_EXTENSION(http, http.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
