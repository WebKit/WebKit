set(BROTLI_DIR "${THIRDPARTY_DIR}/brotli")

set(BROTLI_INCLUDE_DIRECTORIES
  "${BROTLI_DIR}/dec"
)

set(BROTLI_SOURCES
    ${BROTLI_DIR}/dec/bit_reader.c
    ${BROTLI_DIR}/dec/decode.c
    ${BROTLI_DIR}/dec/dictionary.c
    ${BROTLI_DIR}/dec/huffman.c
    ${BROTLI_DIR}/dec/state.c
)

include_directories("${BROTLI_INCLUDE_DIRECTORIES}")
add_definitions(-DBROTLI_BUILD_PORTABLE)
add_library(brotli STATIC ${BROTLI_SOURCES})
WEBKIT_SET_EXTRA_COMPILER_FLAGS(brotli)

if (COMPILER_IS_CLANG)
    ADD_TARGET_PROPERTIES(brotli COMPILE_FLAGS "-Wno-cast-align")
endif ()
