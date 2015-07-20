list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    win
)

list(APPEND DumpRenderTree_SOURCES
    win/AccessibilityControllerWin.cpp
    win/AccessibilityUIElementWin.cpp
    win/GCControllerWin.cpp
    win/PixelDumpSupportWin.cpp
    win/TestRunnerWin.cpp
    win/WorkQueueItemWin.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
        cairo
        "$ENV{WEBKIT_LIBRARIES}/include/cairo"
    )
    list(APPEND DumpRenderTree_SOURCES
        cairo/PixelDumpSupportCairo.cpp
    )
endif ()
