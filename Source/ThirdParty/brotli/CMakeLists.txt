set(BROTLI_DIR "${THIRDPARTY_DIR}/brotli")

set(BROTLI_INCLUDE_DIRECTORIES
  "${BROTLI_DIR}/common"
  "${BROTLI_DIR}/dec"
  "${BROTLI_DIR}/include"
)

set(BROTLI_SOURCES
    ${BROTLI_DIR}/common/dictionary.c
    ${BROTLI_DIR}/dec/bit_reader.c
    ${BROTLI_DIR}/dec/decode.c
    ${BROTLI_DIR}/dec/huffman.c
    ${BROTLI_DIR}/dec/state.c
)

include_directories("${BROTLI_INCLUDE_DIRECTORIES}")
add_definitions(-DBROTLI_BUILD_PORTABLE)
add_library(brotli STATIC ${BROTLI_SOURCES})

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_C_FLAGS(brotli -Wno-cast-align
                                     -Wno-implicit-fallthrough)
endif ()
