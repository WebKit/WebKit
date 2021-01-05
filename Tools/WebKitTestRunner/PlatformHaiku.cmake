add_custom_target(forwarding-headersHaikuForWebKitTestRunner
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT_TESTRUNNER_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include haiku
)
set(ForwardingHeadersForWebKitTestRunner_NAME forwarding-headersHaikuForWebKitTestRunner)

list(APPEND WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/haiku/EventSenderProxyHaiku.cpp
    ${WEBKIT_TESTRUNNER_DIR}/haiku/PlatformWebViewHaiku.cpp
    ${WEBKIT_TESTRUNNER_DIR}/haiku/TestControllerHaiku.cpp
    ${WEBKIT_TESTRUNNER_DIR}/haiku/TestInvocationHaiku.cpp
    ${WEBKIT_TESTRUNNER_DIR}/haiku/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${DERIVED_SOURCES_WEBCORE_DIR}/include
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include

    ${WEBKIT2_DIR}/UIProcess/API/cpp
    ${WEBKIT2_DIR}/UIProcess/API/haiku
    ${WEBKIT2_DIR}/UIProcess/API/C/haiku
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/haiku/ActivateFontsHaiku.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/haiku/FontManagement.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/haiku/InjectedBundleHaiku.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/haiku/TestRunnerHaiku.cpp
)
