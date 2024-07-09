list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    win/EventSenderProxyWin.cpp
    win/PlatformWebViewWin.cpp
    win/TestControllerWin.cpp
    win/UIScriptControllerWin.cpp
    win/main.cpp
)

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
