list(APPEND DumpRenderTree_SOURCES
    atk/AccessibilityCallbacksAtk.cpp
    atk/AccessibilityControllerAtk.cpp
    atk/AccessibilityNotificationHandlerAtk.cpp
    atk/AccessibilityUIElementAtk.cpp

    cairo/PixelDumpSupportCairo.cpp

    efl/AccessibilityControllerEfl.cpp
    efl/DumpHistoryItem.cpp
    efl/DumpRenderTree.cpp
    efl/DumpRenderTreeChrome.cpp
    efl/DumpRenderTreeView.cpp
    efl/EditingCallbacks.cpp
    efl/EventSender.cpp
    efl/FontManagement.cpp
    efl/GCControllerEfl.cpp
    efl/JSStringUtils.cpp
    efl/PixelDumpSupportEfl.cpp
    efl/TestRunnerEfl.cpp
    efl/TextInputController.cpp
    efl/WorkQueueItemEfl.cpp
)

list(APPEND DumpRenderTree_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_FILE_LIBRARIES}
    ${ECORE_INPUT_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EO_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES} -lm
    ${SQLITE_LIBRARIES}
)

list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
    "${WEBKIT_DIR}/efl/ewk"
    ${WEBKIT_DIR}/efl
    ${WEBKIT_DIR}/efl/WebCoreSupport
    ${WEBCORE_DIR}/platform/graphics/cairo
    ${WEBCORE_DIR}/platform/network/soup
    ${WTF_DIR}/wtf/efl
    ${TOOLS_DIR}/DumpRenderTree/atk
    ${TOOLS_DIR}/DumpRenderTree/cairo
    ${TOOLS_DIR}/DumpRenderTree/efl
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_FILE_INCLUDE_DIRS}
    ${ECORE_INPUT_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIR}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

if (ENABLE_ACCESSIBILITY)
    list(APPEND DumpRenderTree_INCLUDE_DIRECTORIES
        ${TOOLS_DIR}/DumpRenderTree/atk
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND DumpRenderTree_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

# FIXME: DOWNLOADED_FONTS_DIR should not hardcode the directory
# structure. See <https://bugs.webkit.org/show_bug.cgi?id=81475>.
add_definitions(-DFONTS_CONF_DIR="${TOOLS_DIR}/DumpRenderTree/gtk/fonts"
                -DDOWNLOADED_FONTS_DIR="${CMAKE_SOURCE_DIR}/WebKitBuild/Dependencies/Source/webkitgtk-test-fonts")
