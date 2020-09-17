WEBKIT_FRAMEWORK_DECLARE(TestRunnerShared)

set(DumpRenderTree_BINDINGS_DIR ${TOOLS_DIR}/DumpRenderTree/Bindings)
set(TestRunnerShared_DIR ${TOOLS_DIR}/TestRunnerShared)

file(MAKE_DIRECTORY ${TestRunnerShared_DERIVED_SOURCES_DIR})

set(TestRunnerShared_SOURCES
    ReftestFunctions.cpp

    Bindings/JSBasics.cpp
    Bindings/JSWrapper.cpp

    UIScriptContext/UIScriptContext.cpp
    UIScriptContext/UIScriptControllerShared.cpp
)

set(TestRunnerShared_PRIVATE_LIBRARIES
    WebKit::JavaScriptCore
    WebKit::WebCoreTestSupport
)

add_dependencies(TestRunnerShared WebCore_CopyPrivateHeaders)

set(TestRunnerShared_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TestRunnerShared_DERIVED_SOURCES_DIR}
    ${TestRunnerShared_DIR}
    ${TestRunnerShared_DIR}/Bindings
    ${TestRunnerShared_DIR}/UIScriptContext
    ${TestRunnerShared_DIR}/config
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

list(APPEND TestRunnerShared_PRIVATE_INCLUDE_DIRECTORIES
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
)

set(TestRunnerShared_IDL_FILES
    ${TestRunnerShared_DIR}/UIScriptContext/Bindings/UIScriptController.idl
)

GENERATE_BINDINGS(TestRunnerSharedBindings
    OUTPUT_SOURCE TestRunnerShared_SOURCES
    INPUT_FILES ${TestRunnerShared_IDL_FILES}
    BASE_DIR ${DumpRenderTree_BINDINGS_DIR}
    IDL_INCLUDES Bindings
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${TestRunnerShared_DERIVED_SOURCES_DIR}
    GENERATOR DumpRenderTree)

set(TestRunnerShared_LIBRARY_TYPE OBJECT)
set(TestRunnerShared_DEPENDENCIES TestRunnerSharedBindings)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_FRAMEWORK(TestRunnerShared)
