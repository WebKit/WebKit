AC_CANONICAL_HOST
AC_MSG_CHECKING([for native Win32])
case "$host" in
     *-*-mingw*)
       os_win32=yes
       ;;
     *)
       os_win32=no
       ;;
esac
AC_MSG_RESULT([$os_win32])

case "$host" in
     *-*-linux*)
       os_linux=yes
       ;;
     *-*-freebsd*)
       os_freebsd=yes
       ;;
     *-*-darwin*)
       os_darwin=yes
       ;;
esac

case "$host_os" in
     gnu* | linux* | k*bsd*-gnu)
       os_gnu=yes
       ;;
     *)
       os_gnu=no
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

# Check whether a C++ was found (AC_PROG_CXX sets $CXX to "g++" even when it doesn't exist).
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],[],[AC_MSG_ERROR([No C++ compiler found])])
AC_LANG_POP([C++])

# C/C++ Language Features
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

# C/C++ Headers
AC_HEADER_STDC
AC_HEADER_STDBOOL

# Check for -fvisibility=hidden compiler support (GCC >= 4).
saved_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -fvisibility=hidden -fvisibility-inlines-hidden"
AC_MSG_CHECKING([if ${CXX} supports -fvisibility=hidden -fvisibility-inlines-hidden])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([char foo;])], [AC_MSG_RESULT([yes])
    SYMBOL_VISIBILITY="-fvisibility=hidden" SYMBOL_VISIBILITY_INLINES="-fvisibility-inlines-hidden"], [AC_MSG_RESULT([no])])
CFLAGS="$saved_CFLAGS"
AC_SUBST(SYMBOL_VISIBILITY)
AC_SUBST(SYMBOL_VISIBILITY_INLINES)

# Disable C++0x compat warnings for GCC >= 4.6.0 until we build cleanly with that.
AC_LANG_PUSH(C++)
TMPCXXFLAGS=$CXXFLAGS
CXXFLAGS="-Wall -Werror"
AC_MSG_CHECKING([if we have to disable C++0x compat warnings for GCC >= 4.6.0])
AC_TRY_COMPILE([
namespace std {
    class nullptr_t { };
}
extern std::nullptr_t nullptr;
], [return 0;], disable_cxx0x_compat=no, disable_cxx0x_compat=yes)
AC_MSG_RESULT($disable_cxx0x_compat)
if test "$disable_cxx0x_compat" = yes; then
    CXXFLAGS="$TMPCXXFLAGS -Wno-c++0x-compat"
else
    CXXFLAGS="$TMPCXXFLAGS"
fi
AC_LANG_POP(C++)

