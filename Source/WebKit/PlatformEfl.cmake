list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/efl/ewk"
    "${WEBKIT_DIR}/efl/WebCoreSupport"
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/efl"
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/platform/network/soup"
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_INPUT_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFREET_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)

if (ENABLE_VIDEO)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gstreamer"
        ${GSTREAMER_APP_INCLUDE_DIRS}
        ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/html/track"
    )
endif ()

if (ENABLE_NOTIFICATIONS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/notifications"
    )
endif ()

if (ENABLE_VIBRATION)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/vibration"
    )
endif ()

if (ENABLE_BATTERY_STATUS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/battery"
    )
endif ()

if (ENABLE_NAVIGATOR_CONTENT_UTILS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/navigatorcontentutils"
    )
endif ()

if (WTF_USE_3D_GRAPHICS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/surfaces"
        "${WEBCORE_DIR}/platform/graphics/texmap"
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
        "${THIRDPARTY_DIR}/ANGLE/include/GLSLANG"
    )
endif ()

if (ENABLE_GEOLOCATION)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/geolocation"
    )
endif ()

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/accessibility"
        "${WEBCORE_DIR}/accessibility/atk"
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND WebKit_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

list(APPEND WebKit_SOURCES
    efl/WebCoreSupport/AcceleratedCompositingContextEfl.cpp
    efl/WebCoreSupport/BatteryClientEfl.cpp
    efl/WebCoreSupport/ChromeClientEfl.cpp
    efl/WebCoreSupport/ColorChooserEfl.cpp
    efl/WebCoreSupport/ContextMenuClientEfl.cpp
    efl/WebCoreSupport/DeviceMotionClientEfl.cpp
    efl/WebCoreSupport/DeviceOrientationClientEfl.cpp
    efl/WebCoreSupport/DragClientEfl.cpp
    efl/WebCoreSupport/DumpRenderTreeSupportEfl.cpp
    efl/WebCoreSupport/EditorClientEfl.cpp
    efl/WebCoreSupport/FrameLoaderClientEfl.cpp
    efl/WebCoreSupport/FrameNetworkingContextEfl.cpp
    efl/WebCoreSupport/IconDatabaseClientEfl.cpp
    efl/WebCoreSupport/InspectorClientEfl.cpp
    efl/WebCoreSupport/NavigatorContentUtilsClientEfl.cpp
    efl/WebCoreSupport/NetworkInfoClientEfl.cpp
    efl/WebCoreSupport/NotificationPresenterClientEfl.cpp
    efl/WebCoreSupport/PlatformStrategiesEfl.cpp
    efl/WebCoreSupport/ProgressTrackerClientEfl.cpp
    efl/WebCoreSupport/PopupMenuEfl.cpp
    efl/WebCoreSupport/SearchPopupMenuEfl.cpp
    efl/WebCoreSupport/StorageTrackerClientEfl.cpp
    efl/WebCoreSupport/VibrationClientEfl.cpp

    efl/ewk/ewk_auth.cpp
    efl/ewk/ewk_auth_soup.cpp
    efl/ewk/ewk_contextmenu.cpp
    efl/ewk/ewk_cookies.cpp
    efl/ewk/ewk_custom_handler.cpp
    efl/ewk/ewk_file_chooser.cpp
    efl/ewk/ewk_frame.cpp
    efl/ewk/ewk_history.cpp
    efl/ewk/ewk_js.cpp
    efl/ewk/ewk_main.cpp
    efl/ewk/ewk_network.cpp
    efl/ewk/ewk_paint_context.cpp
    efl/ewk/ewk_security_origin.cpp
    efl/ewk/ewk_security_policy.cpp
    efl/ewk/ewk_settings.cpp
    efl/ewk/ewk_touch_event.cpp
    efl/ewk/ewk_view.cpp
    efl/ewk/ewk_web_database.cpp
    efl/ewk/ewk_window_features.cpp
)

list(APPEND WebKit_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_INPUT_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFREET_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EO_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

if (ENABLE_ECORE_X)
    list(APPEND WebKit_LIBRARIES
        ${ECORE_X_LIBRARIES}
    )
endif ()

if (SHARED_CORE)
    set(LIBS_PRIVATE "-l${WTF_OUTPUT_NAME} -l${JavaScriptCore_OUTPUT_NAME} -l${WebCore_OUTPUT_NAME}")
else ()
    set(LIBS_PRIVATE "")
endif ()

configure_file(efl/ewebkit.pc.in ${CMAKE_BINARY_DIR}/WebKit/efl/ewebkit.pc @ONLY)
configure_file(efl/EWebKitConfig.cmake.in ${CMAKE_BINARY_DIR}/WebKit/efl/EWebKitConfig.cmake @ONLY)
configure_file(efl/EWebKitConfigVersion.cmake.in ${CMAKE_BINARY_DIR}/WebKit/efl/EWebKitConfigVersion.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/WebKit/efl/ewebkit.pc DESTINATION lib/pkgconfig)
install(FILES
        ${CMAKE_BINARY_DIR}/WebKit/efl/EWebKitConfig.cmake
        ${CMAKE_BINARY_DIR}/WebKit/efl/EWebKitConfigVersion.cmake
        DESTINATION lib/cmake/EWebKit)

unset(LIBS_PRIVATE)

set(EWebKit_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/EWebKit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_auth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_contextmenu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_cookies.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_file_chooser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_frame.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_history.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_js.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_main.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_network.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_security_origin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_security_policy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_web_database.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_window_features.h
)

install(FILES ${EWebKit_HEADERS}
        DESTINATION include/${WebKit_OUTPUT_NAME}-${PROJECT_VERSION_MAJOR})

include_directories(${THIRDPARTY_DIR}/gtest/include)

set(EWKUnitTests_LIBRARIES
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EVAS_LIBRARIES}
    JavaScriptCore
    WTF
    WebCore
    WebKit
    gtest
)

set(EWKUnitTests_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source"
    "${WEBKIT_DIR}/efl/ewk"
    "${JAVASCRIPTCORE_DIR}"
    "${WTF_DIR}"
    "${WTF_DIR}/wtf"
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
)

list(APPEND EWKUnitTests_INCLUDE_DIRECTORIES "${WTF_DIR}/wtf/gobject")
list(APPEND EWKUnitTests_LIBRARIES
    ${GLIB_GTHREAD_LIBRARIES}
    ${GLIB_LIBRARIES}
)

set(DEFAULT_TEST_PAGE_DIR ${CMAKE_SOURCE_DIR}/Source/WebKit/efl/tests/resources)

add_definitions(-DDEFAULT_TEST_PAGE_DIR=\"${DEFAULT_TEST_PAGE_DIR}\"
    -DDEFAULT_THEME_PATH=\"${THEME_BINARY_DIR}\"
    -DGTEST_LINKED_AS_SHARED_LIBRARY=1
)

add_library(ewkTestUtils
    ${WEBKIT_DIR}/efl/tests/UnitTestUtils/EWKTestBase.cpp
    ${WEBKIT_DIR}/efl/tests/UnitTestUtils/EWKTestEnvironment.cpp
    ${WEBKIT_DIR}/efl/tests/UnitTestUtils/EWKTestView.cpp
)
target_link_libraries(ewkTestUtils ${EWKUnitTests_LIBRARIES})

set(WEBKIT_EFL_TEST_DIR "${WEBKIT_DIR}/efl/tests/")
set(WEBKIT_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TestWebKitAPI/EWebKit)

set(EWKUnitTests_BINARIES
    test_ewk_contextmenu
    test_ewk_frame
    test_ewk_setting
    test_ewk_view
)

if (ENABLE_API_TESTS)
    foreach (testName ${EWKUnitTests_BINARIES})
        add_executable(${testName} ${WEBKIT_EFL_TEST_DIR}/${testName}.cpp ${WEBKIT_EFL_TEST_DIR}/test_runner.cpp)
        add_test(${testName} ${WEBKIT_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY}/${testName})
        set_tests_properties(${testName} PROPERTIES TIMEOUT 60)
        set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${WEBKIT_EFL_TEST_RUNTIME_OUTPUT_DIRECTORY})
        target_link_libraries(${testName} ${EWKUnitTests_LIBRARIES} ewkTestUtils)
    endforeach ()
endif ()

