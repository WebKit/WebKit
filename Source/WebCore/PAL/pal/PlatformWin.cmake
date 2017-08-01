list(APPEND PAL_SOURCES
    crypto/win/CryptoDigestWin.cpp

    system/win/SoundWin.cpp

    text/KillRingNone.cpp
)

list(APPEND PAL_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_BINARY_DIR}/../include/private"
)

list(APPEND PAL_FORWARDING_HEADERS_DIRECTORIES .)

if (${WTF_PLATFORM_WIN_CAIRO})
    include(PlatformWinCairo.cmake)
else ()
    include(PlatformAppleWin.cmake)
endif ()

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})

file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/WebCore/pal)
foreach (_directory ${PAL_FORWARDING_HEADERS_DIRECTORIES})
    file(MAKE_DIRECTORY ${FORWARDING_HEADERS_DIR}/WebCore/pal/${_directory})
    file(GLOB _files "${PAL_DIR}/pal/${_directory}/*.h")
    foreach (_file ${_files})
        file(COPY ${_file} DESTINATION ${FORWARDING_HEADERS_DIR}/WebCore/pal/${_directory})
    endforeach ()
endforeach ()
