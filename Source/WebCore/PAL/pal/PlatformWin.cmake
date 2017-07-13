list(APPEND PAL_SOURCES
    crypto/win/CryptoDigestWin.cpp

    system/win/SoundWin.cpp

    text/KillRingNone.cpp
)

list(APPEND PAL_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/../include/private"
)

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})

file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/WebCore/pal)
file(GLOB _files_PAL "${PAL_DIR}/pal/*.h")
foreach (_file ${_files_PAL})
    file(COPY ${_file} DESTINATION ${FORWARDING_HEADERS_DIR}/WebCore/pal/)
endforeach ()
