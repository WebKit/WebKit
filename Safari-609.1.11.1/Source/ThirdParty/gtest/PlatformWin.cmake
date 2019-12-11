set(gtest_LIBRARY_TYPE STATIC)

# MSVC 2015 requires this definition for INTMAX_MAX to be defined.
add_definitions(-D__STDC_LIMIT_MACROS)
