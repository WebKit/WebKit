list(APPEND DumpRenderTree_SOURCES
    cairo/PixelDumpSupportCairo.cpp

    win/AccessibilityControllerWin.cpp
    win/AccessibilityUIElementWin.cpp
    win/DRTDataObject.cpp
    win/DRTDesktopNotificationPresenter.cpp
    win/DRTDropSource.cpp
    win/DefaultPolicyDelegate.cpp
    win/DumpRenderTree.cpp
    win/EditingDelegate.cpp
    win/EventSender.cpp
    win/FrameLoadDelegate.cpp
    win/GCControllerWin.cpp
    win/HistoryDelegate.cpp
    win/PixelDumpSupportWin.cpp
    win/PolicyDelegate.cpp
    win/ResourceLoadDelegate.cpp
    win/TestRunnerWin.cpp
    win/TextInputController.cpp
    win/TextInputControllerWin.cpp
    win/UIDelegate.cpp
    win/UIScriptControllerWin.cpp
    win/WorkQueueItemWin.cpp
)

set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT WIN_CAIRO)

list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${DumpRenderTree_DIR}/cairo
    ${DumpRenderTree_DIR}/win
)

list(APPEND DumpRenderTree_LIBRARIES
    Cairo::Cairo
    Comsuppw
    Oleacc
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")

WEBKIT_WRAP_EXECUTABLE(DumpRenderTree
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(DumpRenderTree PRIVATE ${wrapper_DEFINITIONS})

# Add precompiled headers to wrapper library
target_precompile_headers(DumpRenderTreeLib PRIVATE DumpRenderTreePrefix.h)
