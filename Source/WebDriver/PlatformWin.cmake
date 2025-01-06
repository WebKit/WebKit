set(WebDriver_Process_OUTPUT_NAME WebViewDriver)

list(APPEND WebDriver_SOURCES
    socket/CapabilitiesSocket.cpp
    socket/HTTPParser.cpp
    socket/HTTPServerSocket.cpp
    socket/SessionHostSocket.cpp

    win/LoggingWin.cpp
    win/WebDriverServiceWin.cpp
)

list(APPEND WebDriver_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBDRIVER_DIR}/socket"
)

list(APPEND WebDriver_FRAMEWORKS
    JavaScriptCore
)
