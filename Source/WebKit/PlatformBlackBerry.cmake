LIST(INSERT WebKit_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}" # For <unicode.h>, which is included from <sys/keycodes.h>.
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)

LIST(APPEND WebKit_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/wtf/text"
    "${WEBCORE_DIR}/bindings/cpp"
    "${WEBCORE_DIR}/history/blackberry"
    "${WEBCORE_DIR}/html/canvas"
    "${WEBCORE_DIR}/html/parser" # For HTMLParserIdioms.h
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/platform/blackberry"
    "${WEBCORE_DIR}/platform/graphics/blackberry"
    "${WEBCORE_DIR}/platform/graphics/blackberry/skia"
    "${WEBCORE_DIR}/platform/graphics/skia"
    "${WEBCORE_DIR}/platform/network/blackberry"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/vibration"
    "${WEBCORE_DIR}/Modules/websockets"
    "${WEBKIT_DIR}/blackberry/Api"
    "${WEBKIT_DIR}/blackberry/WebCoreSupport"
    "${WEBKIT_DIR}/blackberry/WebKitSupport"
    "${CMAKE_SOURCE_DIR}/Source" # For JavaScriptCore API headers
)
IF (NOT PUBLIC_BUILD)
    LIST(APPEND WebKit_INCLUDE_DIRECTORIES
        # needed for DRT for now
        "${JAVASCRIPTCORE_DIR}/wtf"
        "${WEBCORE_DIR}/platform/mock"
        "${WEBCORE_DIR}/svg/animation"
        "${WEBCORE_DIR}/workers"
        "${TOOLS_DIR}"
        "${TOOLS_DIR}/DumpRenderTree"
        "${TOOLS_DIR}/DumpRenderTree/blackberry"
    )
ENDIF ()

IF (ENABLE_BATTERY_STATUS)
    LIST(APPEND WebKit_INCLUDE_DIRECTORIES ${WEBCORE_DIR}/Modules/battery)
    LIST(APPEND WebKit_SOURCES blackberry/WebCoreSupport/BatteryClientBlackBerry.cpp)
ENDIF ()

IF (ENABLE_MEDIA_STREAM)
    LIST(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/mediastream"
        "${WEBCORE_DIR}/platform/mediastream"
    )
    LIST(APPEND WebKit_SOURCES
        blackberry/WebCoreSupport/UserMediaClientImpl.cpp
    )
ENDIF ()

ADD_DEFINITIONS(-DUSER_PROCESSES)

LIST(APPEND WebKit_SOURCES
    blackberry/Api/BackingStore.cpp
    blackberry/Api/BlackBerryGlobal.cpp
    blackberry/Api/InRegionScroller.cpp
    blackberry/Api/WebAnimation.cpp
    blackberry/Api/WebKitMIMETypeConverter.cpp
    blackberry/Api/WebKitTextCodec.cpp
    blackberry/Api/WebOverlay.cpp
    blackberry/Api/WebOverlayOverride.cpp
    blackberry/Api/WebPage.cpp
    blackberry/Api/WebPageCompositor.cpp
    blackberry/Api/WebPageGroupLoadDeferrer.cpp
    blackberry/Api/WebSettings.cpp
    blackberry/Api/WebString.cpp
    blackberry/Api/WebViewportArguments.cpp
    blackberry/Api/JavaScriptVariant.cpp
    blackberry/WebCoreSupport/AutofillManager.cpp
    blackberry/WebCoreSupport/CacheClientBlackBerry.cpp
    blackberry/WebCoreSupport/ChromeClientBlackBerry.cpp
    blackberry/WebCoreSupport/ClientExtension.cpp
    blackberry/WebCoreSupport/ContextMenuClientBlackBerry.cpp
    blackberry/WebCoreSupport/CredentialManager.cpp
    blackberry/WebCoreSupport/CredentialTransformData.cpp
    blackberry/WebCoreSupport/DeviceMotionClientBlackBerry.cpp
    blackberry/WebCoreSupport/DeviceOrientationClientBlackBerry.cpp
    blackberry/WebCoreSupport/DragClientBlackBerry.cpp
    blackberry/WebCoreSupport/EditorClientBlackBerry.cpp
    blackberry/WebCoreSupport/FrameLoaderClientBlackBerry.cpp
    blackberry/WebCoreSupport/FrameNetworkingContextBlackBerry.cpp
    blackberry/WebCoreSupport/GeolocationControllerClientBlackBerry.cpp
    blackberry/WebCoreSupport/IconDatabaseClientBlackBerry.cpp
    blackberry/WebCoreSupport/InspectorClientBlackBerry.cpp
    blackberry/WebCoreSupport/JavaScriptDebuggerBlackBerry.cpp
    blackberry/WebCoreSupport/NotificationPresenterImpl.cpp
    blackberry/WebCoreSupport/VibrationClientBlackBerry.cpp
    blackberry/WebCoreSupport/PagePopupBlackBerry.cpp
    blackberry/WebCoreSupport/SelectPopupClient.cpp
    blackberry/WebCoreSupport/DatePickerClient.cpp
    blackberry/WebKitSupport/AboutData.cpp
    blackberry/WebKitSupport/BackingStoreCompositingSurface.cpp
    blackberry/WebKitSupport/BackingStoreTile.cpp
    blackberry/WebKitSupport/BackingStoreClient.cpp
    blackberry/WebKitSupport/DefaultTapHighlight.cpp
    blackberry/WebKitSupport/DOMSupport.cpp
    blackberry/WebKitSupport/FrameLayers.cpp
    blackberry/WebKitSupport/InPageSearchManager.cpp
    blackberry/WebKitSupport/InputHandler.cpp
    blackberry/WebKitSupport/InRegionScrollableArea.cpp
    blackberry/WebKitSupport/InspectorOverlay.cpp
    blackberry/WebKitSupport/RenderQueue.cpp
    blackberry/WebKitSupport/SelectionHandler.cpp
    blackberry/WebKitSupport/SelectionOverlay.cpp
    blackberry/WebKitSupport/SurfacePool.cpp
    blackberry/WebKitSupport/TouchEventHandler.cpp
    blackberry/WebKitSupport/FatFingers.cpp
)

IF (ENABLE_WEBGL)
    ADD_DEFINITIONS (-DWTF_USE_OPENGL_ES_2=1)
    LIST(APPEND WebKit_INCLUDE_DIRECTORIES
        ${OPENGL_INCLUDE_DIR}
        ${THIRDPARTY_DIR}/ANGLE/src
        ${THIRDPARTY_DIR}/ANGLE/include/GLSLANG
    )
    LIST(APPEND WebKit_LIBRARIES
        ${OPENGL_gl_LIBRARY}
    )
ENDIF (ENABLE_WEBGL)

IF (NOT PUBLIC_BUILD)
    # DumpRenderTree sources
    LIST(APPEND WebKit_SOURCES
        blackberry/WebKitSupport/DumpRenderTreeSupport.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/AccessibilityControllerBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/AccessibilityUIElementBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/DumpRenderTree.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/EventSender.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/GCControllerBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/TestRunnerBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/PixelDumpSupportBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/PNGImageEncoder.cpp
        ${TOOLS_DIR}/DumpRenderTree/blackberry/WorkQueueItemBlackBerry.cpp
        ${TOOLS_DIR}/DumpRenderTree/AccessibilityController.cpp
        ${TOOLS_DIR}/DumpRenderTree/AccessibilityUIElement.cpp
        ${TOOLS_DIR}/DumpRenderTree/AccessibilityTextMarker.cpp
        ${TOOLS_DIR}/DumpRenderTree/TestRunner.cpp
        ${TOOLS_DIR}/DumpRenderTree/CyclicRedundancyCheck.cpp
        ${TOOLS_DIR}/DumpRenderTree/PixelDumpSupport.cpp
        ${TOOLS_DIR}/DumpRenderTree/WorkQueue.cpp
        ${TOOLS_DIR}/DumpRenderTree/GCController.cpp
    )
ENDIF ()

SET(WebKit_LINK_FLAGS ${BLACKBERRY_LINK_FLAGS})

LIST(APPEND WebKit_LIBRARIES
    ${CURL_LIBRARY}
    ${FONTCONFIG_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${HARFBUZZ_LIBRARY}
    ${ICUData_LIBRARY}
    ${ICUI18N_LIBRARY}
    ${ICUUC_LIBRARY}
    ${INTL_LIBRARY}
    ${JPEG_LIBRARY}
    ${JavaScriptCore_LIBRARY_NAME}
    ${MMR_LIBRARY}
    ${M_LIBRARY}
    ${OTS_LIBRARY}
    ${PNG_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${Skia_LIBRARY}
    ${Skia_QNX_LIBRARY}
    ${UUID_LIBRARY}
    ${WebKitPlatform_LIBRARY}
    ${XML2_LIBRARY}
    ${XSLT_LIBRARY}
    ${Z_LIBRARY}
)

IF (PROFILING)
    LIST(APPEND WebKit_LIBRARIES
        ${PROFILING_LIBRARY}
    )
ENDIF ()

IF (WTF_USE_ACCELERATED_COMPOSITING)
    LIST(APPEND WebKit_SOURCES
        blackberry/WebKitSupport/GLES2Context.cpp
    )
    LIST(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gles2"
    )
    LIST(APPEND WebKit_LIBRARIES
        ${GLESv2_LIBRARY}
        ${EGL_LIBRARY}
    )
ENDIF ()

FILE(GLOB BBWebKit_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/blackberry/Api/*.h")

INSTALL(FILES ${BBWebKit_HEADERS} DESTINATION usr/include/browser/webkit)

IF (NOT PUBLIC_BUILD)
    INSTALL(FILES ${TOOLS_DIR}/DumpRenderTree/blackberry/DumpRenderTreeBlackBerry.h
            DESTINATION usr/include/browser/webkit)
ENDIF ()

SET(WebKit_INSTALL_DIR "${CMAKE_SYSTEM_PROCESSOR}/usr/lib/torch-webkit")

# Get the JavaScript file names from inspector.html, in order to keep the JavaScript files
# generated in the correct order, and to keep the file names in-sync with the changes of inspector.html
FILE (STRINGS ${WEBCORE_DIR}/inspector/front-end/inspector.html SCRIPT_TAGS REGEX "<script.* src=\".*js\".*></script>")
FOREACH (_line IN LISTS SCRIPT_TAGS)
    STRING (STRIP ${_line} _stripped_line)
    STRING (REGEX REPLACE "<script.* src=\"(.*\\.js)\".*></script>" "\\1" _js_file ${_stripped_line})
    STRING (COMPARE EQUAL ${_js_file} "InspectorBackendCommands.js" _comp_result)
    IF ( ${_comp_result} )
        # InspectorBackendCommands.js was generated with the build, should get it from DERIVED_SOURCES_WEBCORE_DIR.
        SET (_js_file "${DERIVED_SOURCES_WEBCORE_DIR}/${_js_file}")
    ELSE ()
        SET (_js_file "${WEBCORE_DIR}/inspector/front-end/${_js_file}")
    ENDIF ()
    SET (JS_FILES ${JS_FILES} ${_js_file})
ENDFOREACH ()
SET (JS_FILES ${JS_FILES} ${WEBKIT_DIR}/blackberry/WebCoreSupport/inspectorBB.js)

ADD_CUSTOM_TARGET (
    inspector ALL
    COMMAND cat ${JS_FILES} > ${DERIVED_SOURCES_WEBCORE_DIR}/javascript.js
    DEPENDS ${WebCore_LIBRARY_NAME}
    COMMENT "Web Inspector resources building..."
)

# Generate contents for AboutData.cpp
ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataHaveFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl HAVE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataHaveFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp
)

ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataEnableFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl ENABLE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataEnableFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp
)

ADD_CUSTOM_COMMAND(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataUseFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl USE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataUseFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
)

ADD_CUSTOM_TARGET(
    aboutFeatures ALL
    DEPENDS ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
)
