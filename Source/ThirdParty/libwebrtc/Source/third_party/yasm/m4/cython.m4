dnl a macro to check for the installed Cython version; note PYTHON needs to
dnl be set before this function is called.
dnl  CYTHON_CHECK_VERSION([MIN-VERSION], [ACTION-IF-TRUE], [ACTION-IF-FALSE])
AC_DEFUN([CYTHON_CHECK_VERSION],
 [prog="import re, sys
from Cython.Compiler.Version import version
def get_int(arg):
    matched = re.match(r'\d+', arg)
    if matched is None:
        return 0
    else:
        return int(matched.group(0))
# split strings by '.' and convert to numeric.  Append some zeros
# because we need at least 4 digits for the hex conversion.
ver = map(get_int, version.rstrip('abcdefghijklmnopqrstuvwxyz').split('.')) + [[0, 0, 0]]
verhex = 0
for i in range(0, 4): verhex = (verhex << 8) + ver[[i]]
minver = map(get_int, '$1'.split('.')) + [[0, 0, 0]]
minverhex = 0
for i in range(0, 4): minverhex = (minverhex << 8) + minver[[i]]
sys.exit(verhex < minverhex)"
  AS_IF([AM_RUN_LOG([$PYTHON -c "$prog"])], [$2], [$3])])

