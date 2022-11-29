list(APPEND DumpRenderTree_SOURCES
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

set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT)

list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
    ${DumpRenderTree_DIR}/win
)

list(APPEND DumpRenderTree_LIBRARIES
    Comsuppw
    Oleacc
    WebKitLegacy
    WebKitLegacyGUID
)

if (${WTF_PLATFORM_WIN_CAIRO})
    list(APPEND wrapper_DEFINITIONS WIN_CAIRO)
    list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
        ${DumpRenderTree_DIR}/cairo
    )
    list(APPEND DumpRenderTree_LIBRARIES
        $<TARGET_OBJECTS:WebCoreTestSupport>
        Cairo::Cairo
    )
    list(APPEND DumpRenderTree_SOURCES
        cairo/PixelDumpSupportCairo.cpp
    )
else ()
    list(APPEND DumpRenderTree_LIBRARIES
        CFNetwork
        CoreText
    )
    list(APPEND DumpRenderTree_PRIVATE_INCLUDE_DIRECTORIES
        ${DumpRenderTree_DIR}/cg
    )
    list(APPEND DumpRenderTree_SOURCES
        cg/PixelDumpSupportCG.cpp
    )
    list(APPEND DumpRenderTree_LIBRARIES
        CoreGraphics
    )
endif ()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")

WEBKIT_WRAP_EXECUTABLE(DumpRenderTree
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(DumpRenderTree PRIVATE ${wrapper_DEFINITIONS})

# Add precompiled headers to wrapper library
target_precompile_headers(DumpRenderTreeLib PRIVATE DumpRenderTreePrefix.h)
