include(WebKitCommon)
set_property(DIRECTORY . PROPERTY FOLDER "WebDriver")

set(WebDriver_INCLUDE_DIRECTORIES
    "${WEBDRIVER_DIR}"
    "${DERIVED_SOURCES_WEBDRIVER_DIR}"
    "${FORWARDING_HEADERS_DIR}"
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
    WTF
)

set(WebDriver_SCRIPTS
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementAttribute.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/ElementDisplayed.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/EnterFullscreen.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FindNodes.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormElementClear.js
    ${WEBKIT_DIR}/UIProcess/Automation/atoms/FormSubmit.js
)

set(JavaScriptCore_SCRIPTS_DIR "${FORWARDING_HEADERS_DIR}/JavaScriptCore/Scripts")
MAKE_JS_FILE_ARRAYS(
    ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.cpp
    ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.h
    WebDriver
    WebDriver_SCRIPTS
    Session.cpp
)
list(APPEND WebDriver_SOURCES ${DERIVED_SOURCES_WEBDRIVER_DIR}/WebDriverAtoms.cpp)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

include_directories(${WebDriver_INCLUDE_DIRECTORIES})
include_directories(SYSTEM ${WebDriver_SYSTEM_INCLUDE_DIRECTORIES})
add_executable(WebDriver ${WebDriver_SOURCES})
target_link_libraries(WebDriver ${WebDriver_LIBRARIES})

if (WebDriver_Process_OUTPUT_NAME)
    set_target_properties(WebDriver PROPERTIES OUTPUT_NAME ${WebDriver_Process_OUTPUT_NAME})
endif ()

install(TARGETS WebDriver
    RUNTIME DESTINATION "${EXEC_INSTALL_DIR}"
)
