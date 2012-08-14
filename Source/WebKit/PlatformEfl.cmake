LIST(APPEND WebKit_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EFREET_LDFLAGS}
    ${EVAS_LDFLAGS}
)

LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${CMAKE_SOURCE_DIR}/Source"
    "${WEBKIT_DIR}/efl/ewk"
    "${WEBKIT_DIR}/efl/WebCoreSupport"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/efl"
    "${WEBCORE_DIR}/platform/network/soup"
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EFREET_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${EUKIT_INCLUDE_DIRS}
    ${EDBUS_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

IF (ENABLE_SVG)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/animation"
    "${WEBCORE_DIR}/rendering/svg"
  )
ENDIF ()

IF (ENABLE_VIDEO)
LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_INTERFACES_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
)
ENDIF()

IF (ENABLE_VIDEO_TRACK)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/html/track"
  )
ENDIF ()

IF (WTF_USE_FREETYPE)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/freetype"
  )
ENDIF ()

IF (WTF_USE_PANGO)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/pango"
    ${Pango_INCLUDE_DIRS}
  )
  LIST(APPEND WebKit_LIBRARIES
    ${Pango_LIBRARIES}
  )
ENDIF ()

IF (ENABLE_NOTIFICATIONS)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/notifications"
  )
ENDIF ()

IF (ENABLE_VIBRATION)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/vibration"
  )
ENDIF ()

IF (ENABLE_BATTERY_STATUS)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/battery"
  )
ENDIF ()

IF (ENABLE_REGISTER_PROTOCOL_HANDLER)
  LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/protocolhandler"
  )
ENDIF ()

LIST(APPEND WebKit_SOURCES
    efl/WebCoreSupport/AssertMatchingEnums.cpp
    efl/WebCoreSupport/BatteryClientEfl.cpp
    efl/WebCoreSupport/ChromeClientEfl.cpp
    efl/WebCoreSupport/DeviceOrientationClientEfl.cpp
    efl/WebCoreSupport/DeviceMotionClientEfl.cpp
    efl/WebCoreSupport/DragClientEfl.cpp
    efl/WebCoreSupport/DumpRenderTreeSupportEfl.cpp
    efl/WebCoreSupport/EditorClientEfl.cpp
    efl/WebCoreSupport/FrameLoaderClientEfl.cpp
    efl/WebCoreSupport/FrameNetworkingContextEfl.cpp
    efl/WebCoreSupport/FullscreenVideoControllerEfl.cpp
    efl/WebCoreSupport/IconDatabaseClientEfl.cpp
    efl/WebCoreSupport/InspectorClientEfl.cpp
    efl/WebCoreSupport/NetworkInfoClientEfl.cpp
    efl/WebCoreSupport/NotificationPresenterClientEfl.cpp
    efl/WebCoreSupport/PageClientEfl.cpp
    efl/WebCoreSupport/PlatformStrategiesEfl.cpp 
    efl/WebCoreSupport/RegisterProtocolHandlerClientEfl.cpp 
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
    efl/ewk/ewk_intent.cpp
    efl/ewk/ewk_intent_request.cpp
    efl/ewk/ewk_js.cpp
    efl/ewk/ewk_main.cpp
    efl/ewk/ewk_network.cpp
    efl/ewk/ewk_paint_context.cpp
    efl/ewk/ewk_security_origin.cpp
    efl/ewk/ewk_security_policy.cpp
    efl/ewk/ewk_settings.cpp
    efl/ewk/ewk_tiled_backing_store.cpp
    efl/ewk/ewk_tiled_matrix.cpp
    efl/ewk/ewk_tiled_model.cpp
    efl/ewk/ewk_touch_event.cpp
    efl/ewk/ewk_util.cpp
    efl/ewk/ewk_view.cpp
    efl/ewk/ewk_view_single.cpp
    efl/ewk/ewk_view_tiled.cpp
    efl/ewk/ewk_window_features.cpp
    efl/ewk/ewk_web_database.cpp
)

LIST(APPEND WebKit_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EFREET_LIBRARIES}
    ${EUKIT_LIBRARIES}
    ${EDBUS_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${PNG_LIBRARY}
    ${JPEG_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${GLIB_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

SET(WebKit_THEME_DEFINITION "")
IF (ENABLE_PROGRESS_ELEMENT)
  LIST(APPEND WebKit_THEME_DEFINITION "-DENABLE_PROGRESS_ELEMENT")
ENDIF ()

FILE(MAKE_DIRECTORY ${THEME_BINARY_DIR})
SET(WebKit_THEME ${THEME_BINARY_DIR}/default.edj)
ADD_CUSTOM_COMMAND(
  OUTPUT ${WebKit_THEME}
  COMMAND ${EDJE_CC_EXECUTABLE} -v -id ${WEBKIT_DIR}/efl/DefaultTheme ${WebKit_THEME_DEFINITION} ${WEBKIT_DIR}/efl/DefaultTheme/default.edc ${WebKit_THEME}
  DEPENDS
    ${WEBKIT_DIR}/efl/DefaultTheme/default.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_knob_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_knob_press_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_knob_press_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_knob_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_fill_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_fill_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/slider/slider_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_focus_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_press.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/icon.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_normal.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_normal_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_hover_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/combo/combo_press_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/decoration/search_decoration.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/decoration/decoration_normal_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/field/field_hovered.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/field/search_field.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/field/field_normal.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/field/field_focused.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/cancel/cancel_normal_button.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/cancel/cancel_normal_button2.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/search/cancel/search_cancel.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_off_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/check.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_on_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_on_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_off_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_off.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/check/img_check_on.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_on.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_off_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_off_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_on_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/radio.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_off.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/radio/img_radio_on_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/button/img_button_normal.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/button/img_button_press.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/button/img_button_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/button/img_button_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/button/button.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/entry/entry.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/entry/img_normal.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/entry/img_focused.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/entry/img_hovered.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/scrollbar/scrollbar_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/scrollbar/scrollbar_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/scrollbar/scrollbar_knob_v.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/scrollbar/scrollbar_knob_h.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/scrollbar/scrollbar.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/file/file_normal.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/file/file_press.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/file/file_hover.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/file/file_focus.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/file/file.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/progressbar/progressbar.edc
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/progressbar/shelf_inset.png
    ${WEBKIT_DIR}/efl/DefaultTheme/widget/progressbar/bt_base.png
  VERBATIM
)

LIST(APPEND WebKit_SOURCES
     ${WebKit_THEME}
)

IF (SHARED_CORE)
    SET(LIBS_PRIVATE "-l${WTF_LIBRARY_NAME} -l${JavaScriptCore_LIBRARY_NAME} -l${WebCore_LIBRARY_NAME}")
ELSE ()
    SET(LIBS_PRIVATE "")
ENDIF ()

CONFIGURE_FILE(
    efl/ewebkit.pc.in
    ${CMAKE_BINARY_DIR}/WebKit/efl/ewebkit.pc
    @ONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/WebKit/efl/ewebkit.pc
    DESTINATION lib/pkgconfig)

UNSET(LIBS_PRIVATE)

SET(EWebKit_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/EWebKit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_auth.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_contextmenu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_cookies.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_file_chooser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_frame.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_history.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_intent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_intent_request.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_js.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_main.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_network.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_security_origin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_security_policy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_view.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_window_features.h
    ${CMAKE_CURRENT_SOURCE_DIR}/efl/ewk/ewk_web_database.h
)

INSTALL(FILES ${EWebKit_HEADERS}
        DESTINATION include/${WebKit_LIBRARY_NAME}-${PROJECT_VERSION_MAJOR})

INSTALL(FILES ${WebKit_THEME}
        DESTINATION ${DATA_INSTALL_DIR}/themes)

INCLUDE_DIRECTORIES(${THIRDPARTY_DIR}/gtest/include)

SET(EWKUnitTests_LIBRARIES
    ${WTF_LIBRARY_NAME}
    ${JavaScriptCore_LIBRARY_NAME}
    ${WebCore_LIBRARY_NAME}
    ${WebKit_LIBRARY_NAME}
    ${ECORE_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${EDJE_LIBRARIES}
    gtest
)

SET(EWKUnitTests_INCLUDE_DIRECTORIES
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

SET(EWKUnitTests_LINK_FLAGS
    ${ECORE_LDFLAGS}
    ${ECORE_EVAS_LDFLAGS}
    ${EVAS_LDFLAGS}
    ${EDJE_LDFLAGS}
)

IF (ENABLE_GLIB_SUPPORT)
    LIST(APPEND EWKUnitTests_INCLUDE_DIRECTORIES "${WTF_DIR}/wtf/gobject")
    LIST(APPEND EWKUnitTests_LIBRARIES
        ${GLIB_LIBRARIES}
        ${GLIB_GTHREAD_LIBRARIES}
    )
ENDIF ()

SET(DEFAULT_TEST_PAGE_DIR ${CMAKE_SOURCE_DIR}/Source/WebKit/efl/tests/resources)

ADD_DEFINITIONS(-DDEFAULT_TEST_PAGE_DIR=\"${DEFAULT_TEST_PAGE_DIR}\"
    -DDEFAULT_THEME_PATH=\"${THEME_BINARY_DIR}\"
    -DGTEST_LINKED_AS_SHARED_LIBRARY=1
)

ADD_LIBRARY(ewkTestUtils
    ${WEBKIT_DIR}/efl/tests/UnitTestUtils/EWKTestBase.cpp
    ${WEBKIT_DIR}/efl/tests/UnitTestUtils/EWKTestView.cpp
)
TARGET_LINK_LIBRARIES(ewkTestUtils ${EWKUnitTests_LIBRARIES})

SET(WEBKIT_EFL_TEST_DIR "${WEBKIT_DIR}/efl/tests/")

SET(EWKUnitTests_BINARIES
    test_ewk_view
)

IF (ENABLE_API_TESTS)
    FOREACH (testName ${EWKUnitTests_BINARIES})
        ADD_EXECUTABLE(${testName} ${WEBKIT_EFL_TEST_DIR}/${testName}.cpp ${WEBKIT_EFL_TEST_DIR}/test_runner.cpp)
        ADD_TEST(${testName} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${testName})
        SET_TESTS_PROPERTIES(${testName} PROPERTIES TIMEOUT 60)
        TARGET_LINK_LIBRARIES(${testName} ${EWKUnitTests_LIBRARIES} ewkTestUtils)
        ADD_TARGET_PROPERTIES(${testName} LINK_FLAGS "${EWKUnitTests_LINK_FLAGS}")
        SET_TARGET_PROPERTIES(${testName} PROPERTIES FOLDER "WebKit")
    ENDFOREACH ()
ENDIF ()

IF (ENABLE_INSPECTOR)
    SET(WEB_INSPECTOR_DIR ${CMAKE_BINARY_DIR}/WebKit/efl/webinspector)
    ADD_DEFINITIONS(-DWEB_INSPECTOR_DIR="${WEB_INSPECTOR_DIR}")
    ADD_DEFINITIONS(-DWEB_INSPECTOR_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/webinspector")
    ADD_CUSTOM_TARGET(
        web-inspector-resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${WEBCORE_DIR}/inspector/front-end ${WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${WEBCORE_DIR}/English.lproj/localizedStrings.js ${WEB_INSPECTOR_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorBackendCommands.js ${WEB_INSPECTOR_DIR}/InspectorBackendCommands.js
        DEPENDS ${WebCore_LIBRARY_NAME}
    )
    INSTALL(DIRECTORY ${WEB_INSPECTOR_DIR}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}
        FILES_MATCHING PATTERN "*.js"
                       PATTERN "*.html"
                       PATTERN "*.css"
                       PATTERN "*.gif"
                       PATTERN "*.png")
ENDIF ()
