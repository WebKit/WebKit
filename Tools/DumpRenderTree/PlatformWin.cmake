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

set(DumpRenderTree_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    win
)

list(APPEND DumpRenderTree_LIBRARIES
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
        "$ENV{WEBKIT_LIBRARIES}/include/cairo"
    )
    list(APPEND DumpRenderTreeLib_SOURCES
        cairo/PixelDumpSupportCairo.cpp
    )
endif ()

ADD_PRECOMPILED_HEADER("DumpRenderTreePrefix.h" "win/DumpRenderTreePrefix.cpp" DumpRenderTreeLib_SOURCES)

add_library(DumpRenderTreeLib SHARED ${DumpRenderTreeLib_SOURCES})
set_target_properties(DumpRenderTreeLib PROPERTIES FOLDER "Tools")
set_target_properties(DumpRenderTreeLib PROPERTIES OUTPUT_NAME "DumpRenderTree")
target_link_libraries(DumpRenderTreeLib ${DumpRenderTreeLib_LIBRARIES})

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wWinMainCRTStartup")

add_definitions(-D_UNICODE)

