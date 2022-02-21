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
    ${WebDriver_DERIVED_SOURCES_DIR}/ElementAttribute.js
    ${WebDriver_DERIVED_SOURCES_DIR}/ElementDisplayed.js
    ${WebDriver_DERIVED_SOURCES_DIR}/ElementEnabled.js
    ${WebDriver_DERIVED_SOURCES_DIR}/ElementText.js
    ${WebDriver_DERIVED_SOURCES_DIR}/EnterFullscreen.js
    ${WebDriver_DERIVED_SOURCES_DIR}/FindNodes.js
    ${WebDriver_DERIVED_SOURCES_DIR}/FormElementClear.js
    ${WebDriver_DERIVED_SOURCES_DIR}/FormSubmit.js
)

macro(GENERATE_ATOMS _inputs)
    foreach (_file IN ITEMS ${_inputs})
        get_filename_component(_name ${_file} NAME)
        add_custom_command(
            OUTPUT ${_file}
            MAIN_DEPENDENCY ${WEBKIT_DIR}/Scripts/generate-automation-atom.py
            DEPENDS
                ${WEBKIT_DIR}/UIProcess/Automation/atoms/${_name}
                ${WEBKIT_DIR}/UIProcess/Automation/atoms/utils.js
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-automation-atom.py ${WEBKIT_DIR}/UIProcess/Automation/atoms/${_name} ${_file}
            VERBATIM
        )
    endforeach ()
endmacro()

GENERATE_ATOMS("${WebDriver_SCRIPTS}")

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
