set(LIB_FILENAME "${AVIF_SOURCE_DIR}/ext/rav1e/build.libavif/usr/lib/${AVIF_LIBRARY_PREFIX}rav1e${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif: compiled rav1e library is missing (in ext/rav1e/build.libavif/usr/lib), bailing out")
endif()

add_library(rav1e::rav1e STATIC IMPORTED)
set_target_properties(rav1e::rav1e PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" IMPORTED_SONAME rav1e AVIF_LOCAL ON)
target_include_directories(rav1e::rav1e INTERFACE "${AVIF_SOURCE_DIR}/ext/rav1e/build.libavif/usr/include/rav1e")
