set(WebDriver_OUTPUT_NAME minidriver)

list(APPEND WebDriver_SOURCES
    cocoa/HTTPServerCocoa.cpp
    cocoa/LoopbackCocoa.cpp
    cocoa/SessionHostCocoa.cpp
    cocoa/WebDriverServiceCocoa.cpp

    unix/LoggingUnix.cpp
)

# On Cocoa, ENABLE_WEBDRIVER_BIDI comes from PlatformEnableCocoa.h rather than from CMake, so
# CMakeLists.txt doesn't add the BiDi sources.
list(APPEND WebDriver_SOURCES
    WebSocketServer.cpp

    cocoa/WebSocketServerCocoa.cpp
)

list(APPEND WebDriver_LIBRARIES
    "-framework CFNetwork"
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework Network"
)
