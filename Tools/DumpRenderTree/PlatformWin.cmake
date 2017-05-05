set(DumpRenderTreeLib_SOURCES
    ${DumpRenderTree_SOURCES}

    win/AccessibilityControllerWin.cpp
    win/AccessibilityUIElementWin.cpp
    win/DRTDataObject.cpp
    win/DRTDesktopNotificationPresenter.cpp
    win/DRTDropSource.cpp
    win/DumpRenderTree.cpp
    win/EditingDelegate.cpp
    win/EventSender.cpp
    win/FrameLoadDelegate.cpp
    win/GCControllerWin.cpp
    win/HistoryDelegate.cpp
    win/MD5.cpp
    win/PixelDumpSupportWin.cpp
    win/PolicyDelegate.cpp
    win/ResourceLoadDelegate.cpp
    win/TestRunnerWin.cpp
    win/TextInputController.cpp
    win/TextInputControllerWin.cpp
    win/UIDelegate.cpp
    win/WorkQueueItemWin.cpp
)

list(APPEND TestNetscapePlugIn_LIBRARIES
    WebKit
)

set(DumpRenderTree_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

list(APPEND TestNetscapePlugIn_SOURCES
    win/TestNetscapePlugin.def
    win/TestNetscapePlugin.rc

    TestNetscapePlugIn/Tests/win/CallJSThatDestroysPlugin.cpp
    TestNetscapePlugIn/Tests/win/DrawsGradient.cpp
    TestNetscapePlugIn/Tests/win/DumpWindowRect.cpp
    TestNetscapePlugIn/Tests/win/GetValueNetscapeWindow.cpp
    TestNetscapePlugIn/Tests/win/NPNInvalidateRectInvalidatesWindow.cpp
    TestNetscapePlugIn/Tests/win/WindowGeometryInitializedBeforeSetWindow.cpp
    TestNetscapePlugIn/Tests/win/WindowRegionIsSetToClipRect.cpp
    TestNetscapePlugIn/Tests/win/WindowlessPaintRectCoordinates.cpp

    TestNetscapePlugIn/win/WindowGeometryTest.cpp
    TestNetscapePlugIn/win/WindowedPluginTest.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

list(APPEND TestNetscapePlugIn_LIBRARIES
    Msimg32
    Shlwapi
    WebKit
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    win
    TestNetscapePlugIn
    TestNetscapePlugIn/ForwardingHeaders
    TestNetscapePlugIn/Tests
    TestNetscapePlugIn/win
    TestNetscapePlugIn/Tests/win
    ${WEBKIT_DIR}/win
    ${DERIVED_SOURCES_DIR}/WebKit/Interfaces
)

list(APPEND DumpRenderTree_LIBRARIES
    WTF
    WebKit
    shlwapi
)

set(DumpRenderTreeLib_LIBRARIES
    ${DumpRenderTree_LIBRARIES}
    Comsuppw
    Oleacc
    WebKitGUID
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
        cairo
        ${CAIRO_INCLUDE_DIRS}
    )
    list(APPEND DumpRenderTreeLib_SOURCES
        cairo/PixelDumpSupportCairo.cpp
    )
else ()
    list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
        cg
    )
    list(APPEND DumpRenderTreeLib_SOURCES
        cg/PixelDumpSupportCG.cpp
    )
    list(APPEND DumpRenderTreeLib_LIBRARIES
        CFNetwork
        CoreGraphics
        CoreText
    )
endif ()

ADD_PRECOMPILED_HEADER("DumpRenderTreePrefix.h" "win/DumpRenderTreePrefix.cpp" DumpRenderTreeLib_SOURCES)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

add_library(DumpRenderTreeLib SHARED ${DumpRenderTreeLib_SOURCES})
target_link_libraries(DumpRenderTreeLib ${DumpRenderTreeLib_LIBRARIES})

add_definitions(-D_UNICODE)
