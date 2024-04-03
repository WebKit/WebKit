set(AVIF_LIBYUV_BUILD_DIR "${AVIF_SOURCE_DIR}/ext/libyuv/build")

# If ${ANDROID_ABI} is set, look for the library under that subdirectory.
if(DEFINED ANDROID_ABI)
    set(AVIF_LIBYUV_BUILD_DIR "${AVIF_LIBYUV_BUILD_DIR}/${ANDROID_ABI}")
endif()
set(LIB_FILENAME "${AVIF_LIBYUV_BUILD_DIR}/${AVIF_LIBRARY_PREFIX}yuv${CMAKE_STATIC_LIBRARY_SUFFIX}")

if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif(AVIF_LIBYUV=LOCAL): ${LIB_FILENAME} is missing, bailing out")
endif()

message(STATUS "libavif: local libyuv found; libyuv-based fast paths enabled.")

set(LIBYUV_INCLUDE_DIR "${AVIF_SOURCE_DIR}/ext/libyuv/include")

add_library(yuv::yuv STATIC IMPORTED GLOBAL)
set_target_properties(yuv::yuv PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(yuv::yuv INTERFACE "${LIBYUV_INCLUDE_DIR}")

set(libyuv_FOUND ON)

set(LIBYUV_VERSION_H "${LIBYUV_INCLUDE_DIR}/libyuv/version.h")
if(EXISTS ${LIBYUV_VERSION_H})
    # message(STATUS "Reading: ${LIBYUV_VERSION_H}")
    file(READ ${LIBYUV_VERSION_H} LIBYUV_VERSION_H_CONTENTS)
    string(REGEX MATCH "#define LIBYUV_VERSION ([0-9]+)" _ ${LIBYUV_VERSION_H_CONTENTS})
    set(LIBYUV_VERSION ${CMAKE_MATCH_1})
    # message(STATUS "libyuv version detected: ${LIBYUV_VERSION}")
endif()
if(NOT LIBYUV_VERSION)
    message(STATUS "libyuv version detection failed.")
endif()
