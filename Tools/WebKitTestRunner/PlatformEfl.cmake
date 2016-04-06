add_custom_target(forwarding-headersEflForWebKitTestRunner
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_TESTRUNNER_DIR} --output ${DERIVED_SOURCES_WEBKIT2_DIR}/include --platform efl --platform soup
)
set(ForwardingHeadersForWebKitTestRunner_NAME forwarding-headersEflForWebKitTestRunner)

list(APPEND WebKitTestRunner_SOURCES
    ${WEBKIT_TESTRUNNER_DIR}/cairo/TestInvocationCairo.cpp

    ${WEBKIT_TESTRUNNER_DIR}/efl/EventSenderProxyEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/PlatformWebViewEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/TestControllerEfl.cpp
    ${WEBKIT_TESTRUNNER_DIR}/efl/main.cpp
)

list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_WEBKIT2_DIR}/include
    ${WEBKIT2_DIR}/UIProcess/API/efl
)

list(APPEND WebKitTestRunner_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_FILE_INCLUDE_DIRS}
    ${ECORE_IMF_INCLUDE_DIRS}
)

list(APPEND WebKitTestRunner_LIBRARIES
    WebCore
    ${CAIRO_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EVAS_LIBRARIES}
)

if (ENABLE_ECORE_X)
    list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
        ${ECORE_X_INCLUDE_DIRS}
    )

    list(APPEND WebKitTestRunner_LIBRARIES
        ${ECORE_X_LIBRARIES}
        ${X11_Xext_LIB}
    )
endif ()

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebKitTestRunner_INCLUDE_DIRECTORIES
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND WebKitTestRunner_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

list(APPEND WebKitTestRunnerInjectedBundle_SOURCES
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityControllerAtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityNotificationHandlerAtk.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/atk/AccessibilityUIElementAtk.cpp

    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/ActivateFontsEfl.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/FontManagement.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/InjectedBundleEfl.cpp
    ${WEBKIT_TESTRUNNER_INJECTEDBUNDLE_DIR}/efl/TestRunnerEfl.cpp
)

# FIXME: EFL port needs to have own test font and font configure instead of gtk test font in future
# FIXME: DOWNLOADED_FONTS_DIR should not hardcode the directory structure.
add_definitions(-DFONTS_CONF_DIR="${TOOLS_DIR}/WebKitTestRunner/gtk/fonts"
                -DDOWNLOADED_FONTS_DIR="${CMAKE_SOURCE_DIR}/WebKitBuild/DependenciesEFL/Source/webkitgtk-test-fonts")
