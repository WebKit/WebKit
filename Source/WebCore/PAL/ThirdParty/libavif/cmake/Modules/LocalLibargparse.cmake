set(LIBARGPARSE_FILENAME
    "${AVIF_SOURCE_DIR}/ext/libargparse/build/${CMAKE_STATIC_LIBRARY_PREFIX}argparse${CMAKE_STATIC_LIBRARY_SUFFIX}"
)
if(EXISTS "${LIBARGPARSE_FILENAME}")
    add_library(libargparse STATIC IMPORTED GLOBAL)
    set_target_properties(libargparse PROPERTIES IMPORTED_LOCATION "${LIBARGPARSE_FILENAME}" AVIF_LOCAL ON)
    target_include_directories(libargparse INTERFACE "${AVIF_SOURCE_DIR}/ext/libargparse/src")
else()
    message(WARNING "${LIBARGPARSE_FILENAME} is missing, not building avifgainmaputil, please run ext/libargparse.cmd")
endif()
