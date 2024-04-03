set(AVIF_LIBGAV1_BUILD_DIR "${AVIF_SOURCE_DIR}/ext/libgav1/build")
# If ${ANDROID_ABI} is set, look for the library under that subdirectory.
if(DEFINED ANDROID_ABI)
    set(AVIF_LIBGAV1_BUILD_DIR "${AVIF_LIBGAV1_BUILD_DIR}/${ANDROID_ABI}")
endif()
set(LIB_FILENAME "${AVIF_LIBGAV1_BUILD_DIR}/libgav1${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif: ${LIB_FILENAME} is missing, bailing out")
endif()

add_library(libgav1_static STATIC IMPORTED GLOBAL)
set_target_properties(libgav1_static PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(libgav1_static INTERFACE "${AVIF_SOURCE_DIR}/ext/libgav1/src")
add_library(libgav1::libgav1 ALIAS libgav1_static)
