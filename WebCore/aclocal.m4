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
