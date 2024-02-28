add_subdirectory(${AVIF_SOURCE_DIR}/ext/libjpeg EXCLUDE_FROM_ALL)

set_property(TARGET jpeg PROPERTY AVIF_LOCAL ON)
set(JPEG_INCLUDE_DIR "${AVIF_SOURCE_DIR}/ext/libjpeg")
target_include_directories(jpeg INTERFACE ${JPEG_INCLUDE_DIR})

add_library(JPEG::JPEG ALIAS jpeg)
