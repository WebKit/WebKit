set(LIB_FILENAME "${CMAKE_CURRENT_SOURCE_DIR}/ext/libwebp/build/libsharpyuv${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif(AVIF_LIBSHARPYUV=LOCAL): ${LIB_FILENAME} is missing, bailing out")
endif()

add_library(sharpyuv::sharpyuv STATIC IMPORTED GLOBAL)
set_target_properties(sharpyuv::sharpyuv PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(sharpyuv::sharpyuv INTERFACE "${AVIF_SOURCE_DIR}/ext/libwebp")

set(libsharpyuv_FOUND ON)
