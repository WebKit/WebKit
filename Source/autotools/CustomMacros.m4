dnl AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS
dnl
dnl Usage:
dnl AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS([BUILD_TARGET], [OUTPUT_TARGET_VARIABLE], [BUILD_TARGETS])
AC_DEFUN([AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS], [
  AC_MSG_CHECKING([whether to enable the $1 target])
  AS_IF([echo "$$3" | grep -qE "$1=yes"; test $? -eq 0], [$2=yes],
    [echo "$$3" | grep -qE "$1=no"; test $? -eq 0], [$2=no],
    [echo "$$3" | grep -qE "$1="; test $? -eq 0], [$2=auto],
    [$2=no])
  AC_MSG_RESULT([$$2])
]) dnl AM_WEBKIT_DETERMINE_BUILD_TARGET_STATUS

dnl AM_APPEND_TO_DESCRIPTION
dnl Appends the given string to the description variable,
dnl using a separator if the description variable is not empty.
dnl
dnl Usage:
dnl AM_APPEND_TO_DESCRIPTION([DESCRIPTION], [STRING])
AC_DEFUN([AM_APPEND_TO_DESCRIPTION], [
  if test "$$1" != ""; then
    $1="${$1}, "
  fi

  $1="${$1}$2"
]) dnl AM_APPEND_TO_DESCRIPTION
