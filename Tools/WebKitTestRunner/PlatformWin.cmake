list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    win/EventSenderProxyWin.cpp
    win/PlatformWebViewWin.cpp
    win/TestControllerWin.cpp
    win/UIScriptControllerWin.cpp
    win/main.cpp
)

set(wrapper_DEFINITIONS
    USE_CONSOLE_ENTRY_POINT
    WIN_CAIRO
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    InjectedBundle/win/AccessibilityControllerWin.cpp
    InjectedBundle/win/AccessibilityUIElementWin.cpp
    InjectedBundle/win/ActivateFontsWin.cpp
    InjectedBundle/win/InjectedBundleWin.cpp
    InjectedBundle/win/TestRunnerWin.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebKitTestRunner_DIR}/InjectedBundle/win
)

list(APPEND WebKitTestRunner_LIBRARIES
    Comsuppw
    Oleacc
)

list(APPEND WebKitTestRunnerInjectedBundle_LIBRARIES
    $<TARGET_OBJECTS:WebCoreTestSupport>
)

WEBKIT_ADD_PRECOMPILED_HEADER("WebKitTestRunnerPrefix.h" "win/WebKitTestRunnerPrefix.cpp" WebKitTestRunner_SOURCES)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")

WEBKIT_WRAP_EXECUTABLE(WebKitTestRunner
    SOURCES ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
    LIBRARIES shlwapi
)
target_compile_definitions(WebKitTestRunner PRIVATE ${wrapper_DEFINITIONS})
