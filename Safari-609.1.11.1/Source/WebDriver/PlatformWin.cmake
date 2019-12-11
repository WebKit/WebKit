set(WebDriver_Process_OUTPUT_NAME WebKitWebDriver)

list(APPEND WebDriver_SOURCES
    socket/HTTPServerSocket.cpp
    socket/SessionHostSocket.cpp

    win/WebDriverServiceWin.cpp
)
