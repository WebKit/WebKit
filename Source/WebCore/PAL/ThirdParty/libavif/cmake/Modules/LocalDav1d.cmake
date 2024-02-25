set(AVIF_DAV1D_BUILD_DIR "${AVIF_SOURCE_DIR}/ext/dav1d/build")
# If ${ANDROID_ABI} is set, look for the library under that subdirectory.
if(DEFINED ANDROID_ABI)
    set(AVIF_DAV1D_BUILD_DIR "${AVIF_DAV1D_BUILD_DIR}/${ANDROID_ABI}")
endif()
set(LIB_FILENAME "${AVIF_DAV1D_BUILD_DIR}/src/libdav1d${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    if("${CMAKE_STATIC_LIBRARY_SUFFIX}" STREQUAL ".a")
        message(FATAL_ERROR "libavif: ${LIB_FILENAME} is missing, bailing out")
    else()
        # On windows, meson will produce a libdav1d.a instead of the expected libdav1d.dll/.lib.
        # See https://github.com/mesonbuild/meson/issues/8153.
        set(LIB_FILENAME "${AVIF_SOURCE_DIR}/ext/dav1d/build/src/libdav1d.a")
        if(NOT EXISTS "${LIB_FILENAME}")
            message(FATAL_ERROR "libavif: ${LIB_FILENAME} (or libdav1d${CMAKE_STATIC_LIBRARY_SUFFIX}) is missing, bailing out")
        endif()
    endif()
endif()

add_library(dav1d::dav1d STATIC IMPORTED)
set_target_properties(dav1d::dav1d PROPERTIES IMPORTED_LOCATION ${LIB_FILENAME} AVIF_LOCAL ON)
target_include_directories(
    dav1d::dav1d INTERFACE "${AVIF_DAV1D_BUILD_DIR}" "${AVIF_DAV1D_BUILD_DIR}/include" "${AVIF_DAV1D_BUILD_DIR}/include/dav1d"
                           "${AVIF_SOURCE_DIR}/ext/dav1d/include"
)
