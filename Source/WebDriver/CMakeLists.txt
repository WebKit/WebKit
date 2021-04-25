set_property(DIRECTORY . PROPERTY FOLDER "WebDriver")

include(WebKitCommon)

set(WebDriver_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBDRIVER_DIR}"
    "${WebDriver_DERIVED_SOURCES_DIR}"
)

set(WebDriver_SOURCES
    CommandResult.cpp
    HTTPServer.cpp
    Session.cpp
    SessionHost.cpp
    WebDriverMain.cpp
    WebDriverService.cpp
)

set(WebDriver_FRAMEWORKS
    WTF
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebDriver_FRAMEWORKS bmalloc)
endif ()


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
    ${WebDriver_DERIVED_SOURCES_DIR}/WebDriverAtoms.cpp
    ${WebDriver_DERIVED_SOURCES_DIR}/WebDriverAtoms.h
    WebDriver
    WebDriver_SCRIPTS
    Session.cpp
)
list(APPEND WebDriver_SOURCES ${WebDriver_DERIVED_SOURCES_DIR}/WebDriverAtoms.cpp)

WEBKIT_EXECUTABLE_DECLARE(WebDriver)

add_dependencies(WebDriver JavaScriptCoreSharedScripts)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

WEBKIT_EXECUTABLE(WebDriver)

install(TARGETS WebDriver
    RUNTIME DESTINATION "${EXEC_INSTALL_DIR}"
)
