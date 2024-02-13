list(APPEND WebKitTestRunner_SOURCES
    cairo/TestInvocationCairo.cpp

    libwpe/EventSenderProxyClientLibWPE.cpp
    libwpe/EventSenderProxyLibWPE.cpp
    libwpe/PlatformWebViewClientLibWPE.cpp
    libwpe/PlatformWebViewLibWPE.cpp

    playstation/TestControllerPlayStation.cpp
    playstation/UIScriptControllerPlayStation.cpp
    playstation/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WebKitTestRunner_DIR}/libwpe
    ${WebKitTestRunner_DIR}/playstation
)

list(APPEND WebKitTestRunner_PRIVATE_LIBRARIES
    Cairo::Cairo
    WebKit::WPEToolingBackends
)

list(APPEND TestRunnerInjectedBundle_SOURCES
    InjectedBundle/playstation/AccessibilityControllerPlayStation.cpp
    InjectedBundle/playstation/AccessibilityUIElementPlayStation.cpp
    InjectedBundle/playstation/ActivateFontsPlayStation.cpp
    InjectedBundle/playstation/InjectedBundlePlayStation.cpp
    InjectedBundle/playstation/TestRunnerPlayStation.cpp
)

list(APPEND TestRunnerInjectedBundle_INCLUDE_DIRECTORIES
    ${WebKitTestRunner_DIR}/InjectedBundle/playstation
)
