# Use C99 as the language standard for C code.
CFLAGS="$CFLAGS -pthread -std=c99"
# Use the C++11 standard. Do not warn about C++11 incompatibilities.
CXXFLAGS="$CXXFLAGS -pthread -std=c++11 -Wno-c++11-compat"

# Clang requires suppression of unused arguments warnings.
if test "$c_compiler" = "clang"; then
    CFLAGS="$CFLAGS -Qunused-arguments"
fi

# Suppress unused arguments warnings for C++ files as well.
if test "$cxx_compiler" = "clang++"; then
    CXXFLAGS="$CXXFLAGS -Qunused-arguments"

    # Default to libc++ as the standard library on Darwin, if it isn't already enforced through CXXFLAGS.
    if test "$os_darwin" = "yes"; then
        AS_CASE([$CXXFLAGS], [*-stdlib=*], [], [CXXFLAGS="$CXXFLAGS -stdlib=libc++"])
    fi

    # If Clang will be using libstdc++ as the standard library, version >= 4.8.1 should be in use.
    AC_LANG_PUSH([C++])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#if defined(__GLIBCXX__) && __GLIBCXX__ >= 20130531
#include <type_traits>
bool libstdcxxTest = std::is_trivially_destructible<bool>::value;
#endif
])], [], [AC_MSG_ERROR([libstdc++ >= 4.8.1 is required as the standard library used with the Clang compiler.])])
    AC_LANG_POP([C++])
fi

if test "$host_cpu" = "sh4"; then
    CXXFLAGS="$CXXFLAGS -mieee -w"
    CFLAGS="$CFLAGS -mieee -w"
fi

# Add '-g' flag to gcc to build with debug symbols.
if test "$enable_debug_symbols" = "min"; then
    CXXFLAGS="$CXXFLAGS -g1"
    CFLAGS="$CFLAGS -g1"
elif test "$enable_debug_symbols" != "no"; then
    CXXFLAGS="$CXXFLAGS -g"
    CFLAGS="$CFLAGS -g"
fi

# Add the appropriate 'O' level for optimized builds.
if test "$enable_optimizations" = "yes"; then
    CXXFLAGS="$CXXFLAGS -O2"
    CFLAGS="$CFLAGS -O2"

    if test "$c_compiler" = "gcc"; then
        CFLAGS="$CFLAGS -D_FORTIFY_SOURCE=2"
    fi
    if test "$cxx_compiler" = "g++"; then
        CXXFLAGS="$CXXFLAGS -D_FORTIFY_SOURCE=2"
    fi
else
    CXXFLAGS="$CXXFLAGS -O0"
    CFLAGS="$CFLAGS -O0"
fi
