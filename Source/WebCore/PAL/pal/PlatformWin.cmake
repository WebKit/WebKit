list(APPEND PAL_SOURCES
    crypto/win/CryptoDigestWin.cpp

    system/win/SoundWin.cpp

    text/KillRingNone.cpp
)

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})

file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/WebCore/pal)
file(GLOB _files_PAL "${PAL_DIR}/pal/*.h")
foreach (_file ${_files_PAL})
    file(COPY ${_file} DESTINATION ${FORWARDING_HEADERS_DIR}/WebCore/pal/)
endforeach ()
