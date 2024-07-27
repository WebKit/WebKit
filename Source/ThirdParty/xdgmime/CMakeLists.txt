set(XDGMIME_DIR "${THIRDPARTY_DIR}/xdgmime")

set(XDGMIME_INCLUDE_DIRECTORIES
    "${XDGMIME_DIR}/src"
    "${CMAKE_BINARY_DIR}"
)

set(XDGMIME_SOURCES
    ${XDGMIME_DIR}/src/xdgmimealias.c
    ${XDGMIME_DIR}/src/xdgmime.c
    ${XDGMIME_DIR}/src/xdgmimeicon.c
    ${XDGMIME_DIR}/src/xdgmimeint.c
    ${XDGMIME_DIR}/src/xdgmimemagic.c
    ${XDGMIME_DIR}/src/xdgmimeparent.c
    ${XDGMIME_DIR}/src/xdgmimecache.c
    ${XDGMIME_DIR}/src/xdgmimeglob.c
)

include_directories("${XDGMIME_INCLUDE_DIRECTORIES}")
add_definitions(-DXDG_PREFIX=_wk_xdg)
add_library(xdgmime STATIC ${XDGMIME_SOURCES})

if (CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
    add_definitions(-DLITTLE_ENDIAN=1)
else ()
    add_definitions(-DLITTLE_ENDIAN=0)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_C_FLAGS(xdgmime -Wno-cast-align
                                      -Wno-cast-function-type
                                      -Wno-sign-compare
                                      -Wno-unused-parameter)
endif ()
