set(WebDriver_OUTPUT_NAME WPEWebDriver)

list(APPEND WebDriver_SOURCES
    glib/SessionHostGlib.cpp
    glib/WebDriverServiceGLib.cpp

    soup/HTTPServerSoup.cpp

    unix/LoggingUnix.cpp

    wpe/WebDriverServiceWPE.cpp
)

if (ENABLE_WEBDRIVER_BIDI)
    list(APPEND WebDriver_SOURCES soup/WebSocketServerSoup.cpp)
endif ()

list(APPEND WebDriver_LIBRARIES Soup3::Soup3)
