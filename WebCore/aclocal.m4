dnl -------------------------------------------------------------------
dnl
dnl Local autoconf macros 
dnl
dnl $RCSfile$
dnl $Revision$
dnl $Author$
dnl $Date$
dnl
dnl -------------------------------------------------------------------
dnl
dnl Sets the top directory for the build
dnl
dnl Usage: AC_PATH_SET_TOPSRCDIR(PROGRAM_NAME)
dnl
AC_DEFUN(AC_PATH_SET_TOPSRCDIR,
[AC_MSG_CHECKING([top directory])
ac_cv_path_TOPSRCDIR=`pwd`
AC_MSG_RESULT(ok)
])dnl
dnl
dnl
dnl -------------------------------------------------------------------
dnl
dnl Sets the top directory for the build
dnl
dnl Usage: 
dnl
AC_DEFUN(AC_CHECK_ISSOCK, [
AC_MSG_CHECKING([whether sys/stat.h declares S_ISSOCK])
AC_TRY_COMPILE([#include <sys/stat.h>] ,[
#ifndef S_ISSOCK
#define S_ISSOCK no workie
#endif
void foo() {
    int mode = S_ISSOCK(0);
}
], [
    AC_DEFINE_UNQUOTED(HAVE_S_ISSOCK,1)
    AC_MSG_RESULT([yes])
] , [
    AC_MSG_RESULT([no])
])
])dnl
dnl
dnl -------------------------------------------------------------------
dnl
dnl#ifndef S_ISSOCK
dnl#define S_ISSOCK does not compile
dnl#endif
dnlint main() {
dnl    int i = S_ISSOCK(0);
dnl    return 0;
dnl}
