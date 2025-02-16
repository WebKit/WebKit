set(WebDriver_OUTPUT_NAME WebKitWebDriver)

add_definitions(-DLIBEXECDIR="${CMAKE_INSTALL_FULL_LIBEXECDIR}")

list(APPEND WebDriver_SYSTEM_INCLUDE_DIRECTORIES
    "${GLIB_INCLUDE_DIRS}"
    "${LIBSOUP_INCLUDE_DIRS}"
)

list(APPEND WebDriver_SOURCES
    glib/SessionHostGlib.cpp
    glib/WebDriverServiceGLib.cpp

    gtk/WebDriverServiceGtk.cpp

    soup/HTTPServerSoup.cpp

    unix/LoggingUnix.cpp
)

if (ENABLE_WEBDRIVER_BIDI)
    list(APPEND WebDriver_SOURCES soup/WebSocketServerSoup.cpp)
endif ()

list(APPEND WebDriver_LIBRARIES
    ${LIBSOUP_LIBRARIES}
)
