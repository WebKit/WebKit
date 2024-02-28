set(LIB_FILENAME "${AVIF_SOURCE_DIR}/ext/aom/build.libavif/${CMAKE_STATIC_LIBRARY_PREFIX}aom${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif: ${LIB_FILENAME} is missing, bailing out")
endif()

add_library(aom STATIC IMPORTED GLOBAL)
set_target_properties(aom PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(aom INTERFACE "${AVIF_SOURCE_DIR}/ext/aom")
