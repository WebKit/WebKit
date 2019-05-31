set(WebKitTestRunnerLib_SOURCES
    ${WebKitTestRunner_SOURCES}
    ${WEBKIT_TESTRUNNER_DIR}/cairo/TestInvocationCairo.cpp
    win/EventSenderProxyWin.cpp
    win/PlatformWebViewWin.cpp
    win/TestControllerWin.cpp
    win/main.cpp
)

set(WebKitTestRunner_SOURCES
    ${TOOLS_DIR}/win/DLLLauncher/DLLLauncherMain.cpp
)

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/ActivateFontsWin.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/InjectedBundleWin.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/TestRunnerWin.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/AccessibilityControllerWin.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/AccessibilityUIElementWin.cpp
)


list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    cairo
    win
    ${CAIRO_INCLUDE_DIRS}
    ${FORWARDING_HEADERS_DIR}
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win
)


set(WebKitTestRunnerLib_LIBRARIES
    ${WebKitTestRunner_LIBRARIES}
    Comsuppw
    Oleacc
)

list(APPEND WebKitTestRunner_LIBRARIES
    WebKit
)

set(WebKitTestRunnerInjectedBundle_LIBRARIES
    WebCoreTestSupport
    WebKit
)

list(REMOVE_ITEM
    WebKitTestRunnerLib_SOURCES
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)
list(REMOVE_ITEM
    WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)

WEBKIT_ADD_PRECOMPILED_HEADER(WebKitTestRunnerPrefix.h
    ${WEBKIT_TESTRUNNER_DIR}/win/WebKitTestRunnerPrefix.cpp
    WebKitTestRunnerLib_SOURCES
)
WEBKIT_ADD_PRECOMPILED_HEADER(TestRunnerInjectedBundlePrefix.h
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/win/TestRunnerInjectedBundlePrefix.cpp
    WebKitTestRunnerInjectedBundle_SOURCES
)

list(APPEND
    WebKitTestRunnerLib_SOURCES
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)
list(APPEND
    WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_BINDINGS_DIR}/JSWrapper.cpp
)


set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")
add_library(WebKitTestRunnerLib SHARED ${WebKitTestRunnerLib_SOURCES})
target_link_libraries(WebKitTestRunnerLib ${WebKitTestRunnerLib_LIBRARIES})

add_definitions(
    -DWIN_CAIRO
    -DUSE_CONSOLE_ENTRY_POINT
    -D_UNICODE
)
