set_property(DIRECTORY . PROPERTY FOLDER "WebDriver")

include(WebKitCommon)

set(WebDriver_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBDRIVER_DIR}"
    "${DERIVED_SOURCES_WEBDRIVER_DIR}"
)

set(WebDriver_SOURCES
    CommandResult.cpp
    HTTPServer.cpp
    Session.cpp
    SessionHost.cpp
    WebDriverMain.cpp
    WebDriverService.cpp
)

set(WebDriver_LIBRARIES
    WebKit::WTF
)

set(WebDriver_SCRIPTS
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementAttribute.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementDisplayed.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementEnabled.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/EnterFullscreen.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FindNodes.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormElementClear.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormSubmit.js
)

MAKE_JS_FILE_ARRAYS(
    ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.cpp
    ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.h
    WebDriver
    WebDriver_SCRIPTS
    Session.cpp
)
list(APPEND WebDriver_SOURCES ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.cpp)

WEBKIT_EXECUTABLE_DECLARE(WebDriver)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_EXECUTABLE(WebDriver)

install(TARGETS WebDriver
    RUNTIME DESTINATION "${EXEC_INSTALL_DIR}"
)
