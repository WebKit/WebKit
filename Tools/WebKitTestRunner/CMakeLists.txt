SET(WEBKIT_TESTRUNNER_DIR "${TOOLS_DIR}/WebKitTestRunner")
SET(WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR "${TOOLS_DIR}/WebKitTestRunner/InjectedBundle")

FILE(MAKE_DIRECTORY ${DERIVED_SOURCES_DIR}/InjectedBundle)

SET(WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/CyclicRedundancyCheck.cpp
    ${WEBKIT_TESTRUNNER_DIR}/PixelDumpSupport.cpp
    ${WEBKIT_TESTRUNNER_DIR}/TestController.cpp
    ${WEBKIT_TESTRUNNER_DIR}/TestInvocation.cpp
)

SET(WebKitTestRunner_LIBRARIES
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebCoreTestSupport_LIBRARY_NAME}
    ${WebKit2_LIBRARY_NAME}
)

SET(WebKitTestRunner_INCLUDE_DIRECTORIES
    ${WEBKIT_TESTRUNNER_DIR}
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle
    ${WEBKIT_TESTRUNNER_DIR}/InjectedBundle/Bindings
    ${JAVASCRIPTCORE_DIR}
    ${JAVASCRIPTCORE_DIR}/ForwardingHeaders
    ${WEBCORE_DIR}/editing
    ${WEBCORE_DIR}/platform
    ${WEBCORE_DIR}/platform/graphics
    ${WEBCORE_DIR}/platform/graphics/harfbuzz
    ${WEBCORE_DIR}/platform/graphics/harfbuzz/ng
    ${WEBCORE_DIR}/platform/network
    ${WEBCORE_DIR}/platform/text
    ${WEBCORE_DIR}/testing/js
    ${WEBKIT2_DIR}/Shared
    ${WEBKIT2_DIR}/Shared/API/c
    ${WEBKIT2_DIR}/UIProcess
    ${WEBKIT2_DIR}/UIProcess/API/C/efl
    ${WEBKIT2_DIR}/UIProcess/API/C/soup
    ${WTF_DIR}
    ${DERIVED_SOURCES_WEBCORE_DIR}/include
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include
    ${DERIVED_SOURCES_DIR}/InjectedBundle
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${LIBSOUP_INCLUDE_DIRS}
)

SET(WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityController.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityTextMarker.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityTextMarkerRange.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/AccessibilityUIElement.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/EventSendingController.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/GCController.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundle.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundleMain.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/InjectedBundlePage.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/TestRunner.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/TextInputController.cpp

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/JSWrapper.cpp
)

FILE(GLOB WebKitTestRunnerInjectedBundle_IDL_FILES "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings/*.idl")

GENERATE_BINDINGS(WebKitTestRunnerInjectedBundle_SOURCES
    "${WebKitTestRunnerInjectedBundle_IDL_FILES}"
    "${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings"
    "--include=${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/Bindings"
    ""
    ${DERIVED_SOURCES_DIR}/InjectedBundle JS TestRunner
    )

INCLUDE_IF_EXISTS(${WEBKIT_TESTRUNNER_DIR}/Platform${PORT}.cmake)

INCLUDE_DIRECTORIES(${WebKitTestRunner_INCLUDE_DIRECTORIES})

ADD_LIBRARY(TestRunnerInjectedBundle SHARED ${WebKitTestRunnerInjectedBundle_SOURCES})
TARGET_LINK_LIBRARIES(TestRunnerInjectedBundle ${WebKitTestRunner_LIBRARIES})

ADD_EXECUTABLE(WebKitTestRunner ${WebKitTestRunner_SOURCES})
TARGET_LINK_LIBRARIES(WebKitTestRunner ${WebKitTestRunner_LIBRARIES})

IF (WebKitTestRunner_LINK_FLAGS)
    ADD_TARGET_PROPERTIES(WebKitTestRunner LINK_FLAGS ${WebKitTestRunner_LINK_FLAGS})
ENDIF ()

ADD_DEPENDENCIES(${WebKit2_LIBRARY_NAME} ${ForwardingHeadersForWebKitTestRunner_NAME})
ADD_DEPENDENCIES(${WebKit2_LIBRARY_NAME} ${ForwardingNetworkHeadersForWebKitTestRunner_NAME})
