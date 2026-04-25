if (FREETYPE_VERSION_STRING VERSION_GREATER_EQUAL 2.10.2)
    cmake_push_check_state()
    set(FREETYPE_WOFF2_TEST_SOURCE "
        #include <ft2build.h>
        #include FT_CONFIG_OPTIONS_H
        #if !defined(FT_CONFIG_OPTION_USE_BROTLI)
        #error
        #endif
        int main() { return 0; }
    ")
    set(CMAKE_REQUIRED_INCLUDES "${FREETYPE_INCLUDE_DIRS}")
    set(CMAKE_REQUIRED_LIBRARIES "${FREETYPE_LIBRARIES}")
    check_cxx_source_compiles("${FREETYPE_WOFF2_TEST_SOURCE}" FREETYPE_WOFF2_SUPPORT_IS_AVAILABLE)
    cmake_pop_check_state()
else ()
    set(FREETYPE_WOFF2_SUPPORT_IS_AVAILABLE OFF)
endif ()
