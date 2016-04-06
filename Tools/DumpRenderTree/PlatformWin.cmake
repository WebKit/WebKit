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

list(APPEND TestNetscapePlugin_LIBRARIES
    WebKit
)

set(DumpRenderTree_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

list(APPEND TestNetscapePlugin_SOURCES
    DumpRenderTree.vcxproj/TestNetscapePlugin/TestNetscapePlugin.def
    DumpRenderTree.vcxproj/TestNetscapePlugin/TestNetscapePlugin.rc

    TestNetscapePlugin/Tests/win/CallJSThatDestroysPlugin.cpp
    TestNetscapePlugin/Tests/win/DrawsGradient.cpp
    TestNetscapePlugin/Tests/win/DumpWindowRect.cpp
    TestNetscapePlugin/Tests/win/GetValueNetscapeWindow.cpp
    TestNetscapePlugin/Tests/win/NPNInvalidateRectInvalidatesWindow.cpp
    TestNetscapePlugin/Tests/win/WindowGeometryInitializedBeforeSetWindow.cpp
    TestNetscapePlugin/Tests/win/WindowRegionIsSetToClipRect.cpp
    TestNetscapePlugin/Tests/win/WindowlessPaintRectCoordinates.cpp

    TestNetscapePlugin/win/WindowGeometryTest.cpp
    TestNetscapePlugin/win/WindowedPluginTest.cpp
)

if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DWIN_CAIRO)
endif ()

list(APPEND TestNetscapePlugin_LIBRARIES
    Msimg32
    Shlwapi
    WebKit
)

set(ImageDiff_SOURCES
    win/ImageDiffWin.cpp
)

set(ImageDiff_LIBRARIES
   JavaScriptCore
   WTF
   WebKit
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    win
    DumpRenderTree.vcxproj/TestNetscapePlugin
    TestNetscapePlugin
    TestNetscapePlugin/ForwardingHeaders
    TestNetscapePlugin/Tests
    TestNetscapePlugin/win
    TestNetscapePlugin/Tests/win
)

list(APPEND DumpRenderTree_LIBRARIES
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
        "${WEBKIT_LIBRARIES_DIR}/include/cairo"
    )
    list(APPEND DumpRenderTreeLib_SOURCES
        cairo/PixelDumpSupportCairo.cpp
    )
    list(APPEND ImageDiff_SOURCES
        win/ImageDiffCairo.cpp
    )
    list(APPEND ImageDiff_LIBRARIES
        cairo
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
    )
    list(APPEND ImageDiff_SOURCES
        cg/ImageDiffCG.cpp
    )
    list(APPEND ImageDiff_LIBRARIES
       CoreFoundation
       CoreGraphics
    )
endif ()

ADD_PRECOMPILED_HEADER("DumpRenderTreePrefix.h" "win/DumpRenderTreePrefix.cpp" DumpRenderTreeLib_SOURCES)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCRTD")
add_definitions(-DUSE_CONSOLE_ENTRY_POINT)

add_library(DumpRenderTreeLib SHARED ${DumpRenderTreeLib_SOURCES})
set_target_properties(DumpRenderTreeLib PROPERTIES FOLDER "Tools")
target_link_libraries(DumpRenderTreeLib ${DumpRenderTreeLib_LIBRARIES})

add_executable(ImageDiff ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp)
target_link_libraries(ImageDiff shlwapi)
set_target_properties(ImageDiff PROPERTIES FOLDER "Tools")
set_target_properties(ImageDiff PROPERTIES OUTPUT_NAME "ImageDiff")

add_library(ImageDiffLib SHARED ${ImageDiff_SOURCES})
set_target_properties(ImageDiffLib PROPERTIES FOLDER "Tools")
target_link_libraries(ImageDiffLib ${ImageDiff_LIBRARIES})

add_dependencies(ImageDiff ImageDiffLib)

add_definitions(-D_UNICODE)
