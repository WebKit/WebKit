AC_CANONICAL_HOST

os_win32=no
os_linux=no
os_freebsd=no
os_openbsd=no
os_netbsd=no
os_dragonfly=no
os_gnu=no

case "$host_os" in
    mingw*)
        os_win32=yes
        ;;
    freebsd*)
        os_freebsd=yes
        ;;
    openbsd*)
        os_openbsd=yes
        ;;
    netbsd*)
        os_netbsd=yes
        ;;
    dragonfly*)
        os_dragonfly=yes
        ;;
    linux*)
        os_linux=yes
        os_gnu=yes
        ;;
    darwin*)
        os_darwin=yes
        ;;
    gnu*|k*bsd*-gnu*)
        os_gnu=yes
        ;;
esac

AC_PATH_PROG(PERL, perl)
if test -z "$PERL"; then
    AC_MSG_ERROR([You need 'perl' to compile WebKit])
fi

AC_PATH_PROG(PYTHON, python)
if test -z "$PYTHON"; then
    AC_MSG_ERROR([You need 'python' to compile WebKit])
fi

AC_PATH_PROG(RUBY, ruby)
if test -z "$RUBY"; then
    AC_MSG_ERROR([You need 'ruby' to compile WebKit])
fi

AC_PATH_PROG(BISON, bison)
if test -z "$BISON"; then
    AC_MSG_ERROR([You need the 'bison' parser generator to compile WebKit])
fi

AC_PATH_PROG(MV, mv)
if test -z "$MV"; then
    AC_MSG_ERROR([You need 'mv' to compile WebKit])
fi

AC_PATH_PROG(GREP, grep)
if test -z "$GREP"; then
    AC_MSG_ERROR([You need 'grep' to compile WebKit])
fi

AC_PATH_PROG(GPERF, gperf)
if test -z "$GPERF"; then
    AC_MSG_ERROR([You need the 'gperf' hash function generator to compile WebKit])
fi

AC_PATH_PROG(FLEX, flex)
if test -z "$FLEX"; then
    AC_MSG_ERROR([You need the 'flex' lexer generator to compile WebKit])
else
    FLEX_VERSION=`$FLEX --version | sed 's,.*\ \([0-9]*\.[0-9]*\.[0-9]*\)$,\1,'`
    AX_COMPARE_VERSION([2.5.33],[gt],[$FLEX_VERSION],
        AC_MSG_WARN([You need at least version 2.5.33 of the 'flex' lexer generator to compile WebKit correctly]))
fi

# If CFLAGS and CXXFLAGS are unset, default to empty.
# This is to tell automake not to include '-g' if C{XX,}FLAGS is not set.
# For more info - http://www.gnu.org/software/automake/manual/autoconf.html#C_002b_002b-Compiler
if test -z "$CXXFLAGS"; then
    CXXFLAGS=""
fi
if test -z "$CFLAGS"; then
    CFLAGS=""
fi

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_SYS_LARGEFILE

# Check that an appropriate C compiler is available.
c_compiler="unknown"
AC_LANG_PUSH([C])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 7)
#error Not a supported GCC compiler
#endif
])], [c_compiler="gcc"], [])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__clang__) && __clang_major__ >= 3 && __clang_minor__ >= 3)
#error Not a supported Clang compiler
#endif
])], [c_compiler="clang"], [])
AC_LANG_POP([C])

if test "$c_compiler" = "unknown"; then
    AC_MSG_ERROR([Compiler GCC >= 4.7 or Clang >= 3.3 is required for C compilation])
fi

# Check that an appropriate C++ compiler is available.
cxx_compiler="unknown"
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__GNUG__) && defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 7)
#error Not a supported G++ compiler
#endif
])], [cxx_compiler="g++"], [])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if !(defined(__clang__) && __clang_major__ >= 3 && __clang_minor__ >= 3)
#error Not a supported Clang++ compiler
#endif
])], [cxx_compiler="clang++"], [])
AC_LANG_POP([C++])

if test "$cxx_compiler" = "unknown"; then
    AC_MSG_ERROR([Compiler GCC >= 4.7 or Clang >= 3.3 is required for C++ compilation])
fi

# C/C++ Headers
AC_HEADER_STDBOOL
