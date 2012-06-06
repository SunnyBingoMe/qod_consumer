AC_INIT([consumer],[2], [dsv@bth.se])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall 1.11 foreign])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_CXX
AC_GNU_SOURCE
AC_PROG_LIBTOOL

PKG_CHECK_MODULES([libcap_stream], [libcap_stream-0.7 >= 0.7.3])

AC_ARG_WITH([python], AS_HELP_STRING([--with-python], [Build python bindings]))
AC_ARG_WITH([gtk], AS_HELP_STRING([--with-gtk+], [Support for gtk+]))

AS_IF([test "x$with_python" == "xyes"], [
	AM_PATH_PYTHON
	AC_ARG_VAR([PYTHON_INCLUDE], [Include flags for python, bypassing python-config])
	AC_ARG_VAR([PYTHON_CONFIG], [Path to python-config])
	AS_IF([test -z "$PYTHON_INCLUDE"], [
		AS_IF([test -z "$PYTHON_CONFIG"], [
	      		AC_PATH_PROGS([PYTHON_CONFIG],
				[python$PYTHON_VERSION-config python-config-$PYTHON_VERSION python-config],
				[no],
				[`dirname $PYTHON`])
			AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON.])])
		])
		AC_MSG_CHECKING([python cflags])
		PYTHON_INCLUDE=`$PYTHON_CONFIG --cflags`
		AC_MSG_RESULT([$PYTHON_INCLUDE])
	])
])
AM_CONDITIONAL([BUILD_PYTHON], [test "x$with_python" == "xyes"])

AS_IF([test "x$with_gtk" == "xyes"], [
	PKG_CHECK_MODULES([gtk], [gtk+-2.0])
	PKG_CHECK_MODULES([gtkglext], [gtkglext-1.0])
	PKG_CHECK_MODULES([glib], [glib-2.0])
])
AM_CONDITIONAL([BUILD_GTK], [test "x$with_gtk" == "xyes"])

AC_OUTPUT
