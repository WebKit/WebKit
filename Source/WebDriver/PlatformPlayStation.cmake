set(WebDriver_OUTPUT_NAME WebDriver)

list(APPEND WebDriver_SOURCES
    playstation/WebDriverServicePlayStation.cpp

    socket/CapabilitiesSocket.cpp
    socket/HTTPParser.cpp
    socket/HTTPServerSocket.cpp
    socket/SessionHostSocket.cpp
)

list(APPEND WebDriver_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBDRIVER_DIR}/socket"
)

list(APPEND WebDriver_PRIVATE_LIBRARIES
    WebKit::JavaScriptCore
)

if (ENABLE_STATIC_JSC)
    list(APPEND WebDriver_PRIVATE_LIBRARIES
        $<TARGET_OBJECTS:JavaScriptCore>
    )
endif ()
