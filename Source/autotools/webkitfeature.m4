dnl AC_CHECK_WEBKIT_FEATURE_ENABLED
dnl Checks whether the given feature is enabled in the
dnl build that is being configured.
dnl
dnl Usage:
dnl AC_CHECK_WEBKIT_FEATURE_ENABLED([FEATURE], [ACTION_IF_ENABLED], [ACTION_IF_DISABLED])

AC_DEFUN([AC_CHECK_WEBKIT_FEATURE_ENABLED], [
  AC_PROG_AWK

  dnl Grep the generated GNUmakefile.features.am
  dnl to determine if the specified feature is enabled.
  grep -qE "($1=1)" Source/WebCore/GNUmakefile.features.am
  if test $? -eq 0; then
    feature_enabled="true";
  else
    feature_enabled="false";
  fi

  dnl Execute ACTION_IF_ENABLED / ACTION_IF_DISABLED.
  if test "$feature_enabled" = "true" ; then
    m4_ifvaln([$2],[$2],[:])
    m4_ifvaln([$3],[else $3])
  fi
]) dnl AC_CHECK_WEBKIT_FEATURE_ENABLED
