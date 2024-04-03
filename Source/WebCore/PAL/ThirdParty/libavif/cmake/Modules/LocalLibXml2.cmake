set(LIB_FILENAME "${AVIF_SOURCE_DIR}/ext/libxml2/install.libavif/lib/${AVIF_LIBRARY_PREFIX}xml2${CMAKE_STATIC_LIBRARY_SUFFIX}")
if(NOT EXISTS "${LIB_FILENAME}")
    message(FATAL_ERROR "libavif: ${LIB_FILENAME} is missing, bailing out")
endif()

add_library(LibXml2 STATIC IMPORTED GLOBAL)
set_target_properties(LibXml2 PROPERTIES IMPORTED_LOCATION "${LIB_FILENAME}" AVIF_LOCAL ON)
target_include_directories(LibXml2 INTERFACE "${AVIF_SOURCE_DIR}/ext/libxml2/install.libavif/include/libxml2")
add_library(LibXml2::LibXml2 ALIAS LibXml2)
