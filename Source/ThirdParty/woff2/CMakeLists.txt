set(WOFF2_DIR "${THIRDPARTY_DIR}/woff2")

set(WOFF2_INCLUDE_DIRECTORIES
  "${WOFF2_DIR}/include"
  "${WOFF2_DIR}/src"
)

set(WOFF2_SOURCES
  ${WOFF2_DIR}/src/table_tags.cc
  ${WOFF2_DIR}/src/variable_length.cc
  ${WOFF2_DIR}/src/woff2_common.cc
  ${WOFF2_DIR}/src/woff2_dec.cc
  ${WOFF2_DIR}/src/woff2_out.cc
)

include_directories("${THIRDPARTY_DIR}/brotli/include")
include_directories("${WOFF2_INCLUDE_DIRECTORIES}")
add_library(woff2 STATIC ${WOFF2_SOURCES})
target_link_libraries(woff2 brotli)

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(woff2 -Wno-cast-align
                                      -Wno-implicit-fallthrough
                                      -Wno-sign-compare
                                      -Wno-unused-variable
                                      -Wno-unused-parameter
                                      -Wno-unused-but-set-variable)
endif ()
