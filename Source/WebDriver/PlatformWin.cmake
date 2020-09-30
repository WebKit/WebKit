set(wrapper_DEFINITIONS USE_CONSOLE_ENTRY_POINT WIN_CAIRO)

set(WebDriver_Process_OUTPUT_NAME WebViewDriver)

list(APPEND WebDriver_SOURCES
    socket/CapabilitiesSocket.cpp
    socket/HTTPParser.cpp
    socket/HTTPServerSocket.cpp
    socket/SessionHostSocket.cpp

    win/WebDriverServiceWin.cpp
)

list(APPEND WebDriver_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBDRIVER_DIR}/socket"
)

list(APPEND WebDriver_LIBRARIES
    WebKit::JavaScriptCore
)

WEBKIT_WRAP_EXECUTABLE(WebDriver
    SOURCES "${JAVASCRIPTCORE_DIR}/shell/DLLLauncherMain.cpp"
    LIBRARIES shlwapi
)
target_compile_definitions(WebDriver PRIVATE ${wrapper_DEFINITIONS})
