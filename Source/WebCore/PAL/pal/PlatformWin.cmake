list(APPEND PAL_SOURCES
    system/ClockGeneric.cpp

    system/win/SoundWin.cpp

    text/KillRing.cpp

    win/LoggingWin.cpp
)

list(APPEND PAL_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/../include/private"
)

if (${WTF_PLATFORM_WIN_CAIRO})
    include(PlatformWinCairo.cmake)
else ()
    include(PlatformAppleWin.cmake)
endif ()

set(PAL_OUTPUT_NAME PAL${DEBUG_SUFFIX})
