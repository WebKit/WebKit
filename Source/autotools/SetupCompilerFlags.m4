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
else
    CXXFLAGS="$CXXFLAGS -O0"
    CFLAGS="$CFLAGS -O0"
fi


