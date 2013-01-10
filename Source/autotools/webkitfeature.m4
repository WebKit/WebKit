dnl AM_WEBKIT_FEATURE_CONDITIONAL
dnl Checks whether the given feature is enabled in the
dnl build that is being configured and sets up equally-named
dnl Automake conditional reflecting the feature status.
dnl
dnl Usage:
dnl AM_WEBKIT_FEATURE_CONDITIONAL([FEATURE])

AC_DEFUN([AM_WEBKIT_FEATURE_CONDITIONAL], [
  AC_PROG_AWK

  dnl Grep the generated GNUmakefile.features.am
  dnl to determine if the specified feature is enabled.
  grep -qE "($1=1)" $srcdir/Source/WebCore/GNUmakefile.features.am
  if test $? -eq 0; then
    feature_enabled="yes";
  else
    feature_enabled="no";
  fi

  AM_CONDITIONAL([$1],[test "$feature_enabled" = "yes"])

]) dnl AM_WEBKIT_FEATURE_CONDITIONAL
