set(gtest_LIBRARY_TYPE STATIC)

list(APPEND gtest_DEFINITIONS
    GTEST_HAS_POSIX_RE=0
    GTEST_HAS_TR1_TUPLE=0
    GTEST_INTERNAL_HAS_ANY=0
)

# localtime_r isn't available so map to localtime_s
add_definitions(-Dlocaltime_r=localtime_s)
