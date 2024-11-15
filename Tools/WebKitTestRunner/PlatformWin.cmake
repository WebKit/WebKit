list(APPEND WebKitTestRunner_SOURCES
    win/EventSenderProxyWin.cpp
    win/PlatformWebViewWin.cpp
    win/TestControllerWin.cpp
    win/UIScriptControllerWin.cpp
    win/WebKitTestRunner.exe.manifest
    win/main.cpp
)

if (USE_CAIRO)
    list(APPEND WebKitTestRunner_SOURCES cairo/TestInvocationCairo.cpp)
elseif (USE_SKIA)
    list(APPEND WebKitTestRunner_SOURCES skia/TestInvocationSkia.cpp)
endif ()

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebKitTestRunner_DIR}/InjectedBundle/win
)

list(APPEND WebKitTestRunner_LIBRARIES
    Comsuppw
    Oleacc
)

target_precompile_headers(WebKitTestRunner PRIVATE WebKitTestRunnerPrefix.h)

list(APPEND TestRunnerInjectedBundle_SOURCES
    InjectedBundle/win/AccessibilityControllerWin.cpp
    InjectedBundle/win/AccessibilityUIElementWin.cpp
    InjectedBundle/win/ActivateFontsWin.cpp
    InjectedBundle/win/InjectedBundleWin.cpp
    InjectedBundle/win/TestRunnerWin.cpp
)

add_executable(WebKitTestRunnerWS win/WebKitTestRunnerWS.cpp)
