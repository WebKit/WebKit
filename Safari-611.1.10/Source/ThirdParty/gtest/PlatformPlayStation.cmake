set(gtest_LIBRARY_TYPE STATIC)

list(APPEND gtest_DEFINITIONS
    -DGTEST_HAS_POSIX_RE=0
    -DGTEST_HAS_TR1_TUPLE=0
)

# localtime_r isn't available so map to localtime_s
add_definitions(-Dlocaltime_r=localtime_s)
