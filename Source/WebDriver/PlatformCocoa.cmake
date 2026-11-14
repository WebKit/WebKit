set(WebDriver_OUTPUT_NAME minidriver)

list(APPEND WebDriver_SOURCES
    cocoa/HTTPServerCocoa.cpp
    cocoa/LoopbackCocoa.cpp
    cocoa/SessionHostCocoa.cpp
    cocoa/WebDriverServiceCocoa.cpp

    unix/LoggingUnix.cpp
)

if (ENABLE_WEBDRIVER_BIDI)
    list(APPEND WebDriver_SOURCES cocoa/WebSocketServerCocoa.cpp)
endif ()

list(APPEND WebDriver_LIBRARIES
    "-framework CFNetwork"
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework Network"
)
