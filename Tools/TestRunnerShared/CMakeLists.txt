set(DumpRenderTree_BINDINGS_DIR ${TOOLS_DIR}/DumpRenderTree/Bindings)
set(TestRunnerShared_DIR ${TOOLS_DIR}/TestRunnerShared)

file(MAKE_DIRECTORY ${TestRunnerShared_DERIVED_SOURCES_DIR})

set(TestRunnerShared_SOURCES
    TestCommand.cpp
    TestFeatures.cpp
    WPTFunctions.cpp

    Bindings/JSBasics.cpp
    Bindings/JSWrapper.cpp

    UIScriptContext/UIScriptContext.cpp
    UIScriptContext/UIScriptControllerShared.cpp
)

set(TestRunnerShared_LIBRARIES WebKit::WebCoreTestSupport)
set(TestRunnerShared_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKitTestRunner_FRAMEWORKS bmalloc)
endif ()

set(TestRunnerShared_PRIVATE_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${TestRunnerShared_DERIVED_SOURCES_DIR}
    ${TestRunnerShared_DIR}
    ${TestRunnerShared_DIR}/Bindings
    ${TestRunnerShared_DIR}/UIScriptContext
    ${TestRunnerShared_DIR}/config
)

set(TestRunnerShared_IDL_FILES
    ${TestRunnerShared_DIR}/UIScriptContext/Bindings/UIScriptController.idl
)

GENERATE_BINDINGS(TestRunnerSharedBindings
    OUTPUT_SOURCE TestRunnerShared_SOURCES
    INPUT_FILES ${TestRunnerShared_IDL_FILES}
    BASE_DIR ${DumpRenderTree_BINDINGS_DIR}
    FEATURES ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}
    DESTINATION ${TestRunnerShared_DERIVED_SOURCES_DIR}
    GENERATOR DumpRenderTree)

set(TestRunnerShared_DEPENDENCIES TestRunnerSharedBindings)
set(TestRunnerShared_INTERFACE_LIBRARIES TestRunnerShared)
set(TestRunnerShared_INTERFACE_INCLUDE_DIRECTORIES
    ${TestRunnerShared_DIR}
    ${TestRunnerShared_DIR}/Bindings
    ${TestRunnerShared_DIR}/UIScriptContext
)

WEBKIT_LIBRARY_DECLARE(TestRunnerShared)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_LIBRARY(TestRunnerShared)
