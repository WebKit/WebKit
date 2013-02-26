list(INSERT WebKit_INCLUDE_DIRECTORIES 0
    "${BLACKBERRY_THIRD_PARTY_DIR}" # For <unicode.h>, which is included from <sys/keycodes.h>.
    "${BLACKBERRY_THIRD_PARTY_DIR}/icu"
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${WEBCORE_DIR}/bindings/cpp"
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/history/blackberry"
    "${WEBCORE_DIR}/html/parser" # For HTMLParserIdioms.h
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/platform/blackberry"
    "${WEBCORE_DIR}/platform/graphics/gpu"
    "${WEBCORE_DIR}/platform/graphics/blackberry"
    "${WEBCORE_DIR}/platform/network/blackberry"
    "${WEBCORE_DIR}/testing/js"
    "${WEBCORE_DIR}/Modules/filesystem"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${WEBCORE_DIR}/Modules/indexeddb"
    "${WEBCORE_DIR}/Modules/networkinfo"
    "${WEBCORE_DIR}/Modules/vibration"
    "${WEBCORE_DIR}/Modules/websockets"
    "${WEBKIT_DIR}/blackberry/Api"
    "${WEBKIT_DIR}/blackberry/WebCoreSupport"
    "${WEBKIT_DIR}/blackberry/WebKitSupport"
    "${CMAKE_SOURCE_DIR}/Source" # For JavaScriptCore API headers
    "${CMAKE_SOURCE_DIR}"
)

if (ENABLE_NOTIFICATIONS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/notifications"
    )
endif ()
if (WTF_USE_SKIA)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/chromium"
        "${WEBCORE_DIR}/platform/graphics/blackberry/skia"
        "${WEBCORE_DIR}/platform/graphics/skia"
    )
else ()
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/image-encoders"
    )
endif ()

if (NOT PUBLIC_BUILD)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        # needed for DRT for now
        "${WEBCORE_DIR}/platform/mock"
        "${WEBCORE_DIR}/svg/animation"
        "${WEBCORE_DIR}/workers"
        "${TOOLS_DIR}"
        "${TOOLS_DIR}/DumpRenderTree"
        "${TOOLS_DIR}/DumpRenderTree/blackberry"
    )
endif ()

if (ENABLE_BATTERY_STATUS)
    list(APPEND WebKit_INCLUDE_DIRECTORIES ${WEBCORE_DIR}/Modules/battery)
    list(APPEND WebKit_SOURCES blackberry/WebCoreSupport/BatteryClientBlackBerry.cpp)
endif ()

if (ENABLE_NAVIGATOR_CONTENT_UTILS)
  list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/navigatorcontentutils"
  )
endif ()

if (ENABLE_MEDIA_STREAM)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/mediastream"
        "${WEBCORE_DIR}/platform/mediastream"
    )
    list(APPEND WebKit_SOURCES
        blackberry/WebCoreSupport/UserMediaClientImpl.cpp
    )
endif ()

add_definitions(-DUSER_PROCESSES)

list(APPEND WebKit_SOURCES
    blackberry/Api/BackingStore.cpp
    blackberry/Api/BlackBerryGlobal.cpp
    blackberry/Api/InRegionScroller.cpp
    blackberry/Api/WebAnimation.cpp
    blackberry/Api/WebCookieJar.cpp
    blackberry/Api/WebKitMIMETypeConverter.cpp
    blackberry/Api/WebKitTextCodec.cpp
    blackberry/Api/WebOverlay.cpp
    blackberry/Api/WebOverlayOverride.cpp
    blackberry/Api/WebPage.cpp
    blackberry/Api/WebPageCompositor.cpp
    blackberry/Api/WebPageGroupLoadDeferrer.cpp
    blackberry/Api/WebSettings.cpp
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
    blackberry/WebCoreSupport/ExternalExtension.cpp
    blackberry/WebCoreSupport/FrameLoaderClientBlackBerry.cpp
    blackberry/WebCoreSupport/FrameNetworkingContextBlackBerry.cpp
    blackberry/WebCoreSupport/GeolocationClientBlackBerry.cpp
    blackberry/WebCoreSupport/IconDatabaseClientBlackBerry.cpp
    blackberry/WebCoreSupport/InspectorClientBlackBerry.cpp
    blackberry/WebCoreSupport/NetworkInfoClientBlackBerry.cpp
    blackberry/WebCoreSupport/NotificationClientBlackBerry.cpp
    blackberry/WebCoreSupport/PagePopupBlackBerry.cpp
    blackberry/WebCoreSupport/NavigatorContentUtilsClientBlackBerry.cpp
    blackberry/WebCoreSupport/SelectPopupClient.cpp
    blackberry/WebCoreSupport/SuggestionBoxHandler.cpp
    blackberry/WebCoreSupport/SuggestionBoxElement.cpp
    blackberry/WebCoreSupport/VibrationClientBlackBerry.cpp
    blackberry/WebCoreSupport/DatePickerClient.cpp
    blackberry/WebCoreSupport/ColorPickerClient.cpp
    blackberry/WebKitSupport/AboutData.cpp
    blackberry/WebKitSupport/BackingStoreTile.cpp
    blackberry/WebKitSupport/BackingStoreClient.cpp
    blackberry/WebKitSupport/BackingStoreVisualizationViewportAccessor.cpp
    blackberry/WebKitSupport/DefaultTapHighlight.cpp
    blackberry/WebKitSupport/DOMSupport.cpp
    blackberry/WebKitSupport/FrameLayers.cpp
    blackberry/WebKitSupport/InPageSearchManager.cpp
    blackberry/WebKitSupport/InputHandler.cpp
    blackberry/WebKitSupport/InRegionScrollableArea.cpp
    blackberry/WebKitSupport/InspectorOverlayBlackBerry.cpp
    blackberry/WebKitSupport/NotificationManager.cpp
    blackberry/WebKitSupport/RenderQueue.cpp
    blackberry/WebKitSupport/SelectionHandler.cpp
    blackberry/WebKitSupport/SelectionOverlay.cpp
    blackberry/WebKitSupport/SpellingHandler.cpp
    blackberry/WebKitSupport/SurfacePool.cpp
    blackberry/WebKitSupport/TouchEventHandler.cpp
    blackberry/WebKitSupport/FatFingers.cpp
    blackberry/WebKitSupport/WebKitThreadViewportAccessor.cpp
)

if (ENABLE_WEBGL)
    add_definitions(-DWTF_USE_OPENGL_ES_2=1)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        ${OPENGL_INCLUDE_DIR}
        ${THIRDPARTY_DIR}/ANGLE/src
        ${THIRDPARTY_DIR}/ANGLE/include/GLSLANG
    )
    list(APPEND WebKit_LIBRARIES
        ${OPENGL_gl_LIBRARY}
    )
endif (ENABLE_WEBGL)

if (NOT PUBLIC_BUILD)
    # DumpRenderTree sources
    list(APPEND WebKit_SOURCES
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
        ${WTF_DIR}/wtf/MD5.cpp
    )
endif ()

set(WebKit_LINK_FLAGS ${BLACKBERRY_LINK_FLAGS})

list(APPEND WebKit_LIBRARIES
    ${CURL_LIBRARY}
    ${FONTCONFIG_LIBRARY}
    ${ICUData_LIBRARY}
    ${ICUI18N_LIBRARY}
    ${ICUUC_LIBRARY}
    ${INTL_LIBRARY}
    ${JPEG_LIBRARY}
    ${JavaScriptCore_LIBRARY_NAME}
    ${LEVELDB_LIBRARY}
    ${MMR_LIBRARY}
    ${M_LIBRARY}
    ${OTS_LIBRARY}
    ${PNG_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${WebKitPlatform_LIBRARY}
    ${XML2_LIBRARY}
    ${XSLT_LIBRARY}
    ${Z_LIBRARY}
)

if (WTF_USE_SKIA)
    list(APPEND WebKit_LIBRARIES
        ${FREETYPE_LIBRARY}
        ${HARFBUZZ_LIBRARY}
        ${Skia_LIBRARY}
        ${Skia_QNX_LIBRARY}
    )
else ()
    list(APPEND WebKit_LIBRARIES
        ${ITYPE_LIBRARY}
        ${WTLE_LIBRARY}
    )
endif ()

if (PROFILING)
    list(APPEND WebKit_LIBRARIES
        ${PROFILING_LIBRARY}
    )
endif ()

if (WTF_USE_ACCELERATED_COMPOSITING)
    list(APPEND WebKit_SOURCES
        blackberry/WebKitSupport/GLES2Context.cpp
    )
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gles2"
    )
    list(APPEND WebKit_LIBRARIES
        ${GLESv2_LIBRARY}
        ${EGL_LIBRARY}
    )
endif ()

file(GLOB BBWebKit_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/blackberry/Api/*.h")

install(FILES ${BBWebKit_HEADERS}
        DESTINATION ../../usr/include/browser/webkit)

if (NOT PUBLIC_BUILD)
    install(FILES ${TOOLS_DIR}/DumpRenderTree/blackberry/DumpRenderTreeBlackBerry.h
            DESTINATION ../../usr/include/browser/webkit)
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebKit_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/html/track"
    )
endif ()

install(DIRECTORY ${WEBCORE_DIR}/inspector/front-end/
        DESTINATION ../../usr/share/webkit/inspector/
        FILES_MATCHING PATTERN "*.js")
install(DIRECTORY ${WEBCORE_DIR}/inspector/front-end/
        DESTINATION ../../usr/share/webkit/inspector/
        FILES_MATCHING PATTERN "*.css")
install(DIRECTORY ${WEBCORE_DIR}/inspector/front-end/
        DESTINATION ../../usr/share/webkit/inspector/
        FILES_MATCHING PATTERN "*.png")
install(DIRECTORY ${WEBCORE_DIR}/inspector/front-end/
        DESTINATION ../../usr/share/webkit/inspector/
        FILES_MATCHING PATTERN "*.jpg")
install(DIRECTORY ${WEBCORE_DIR}/inspector/front-end/
        DESTINATION ../../usr/share/webkit/inspector/
        FILES_MATCHING PATTERN "*.gif")
install(FILES ${DERIVED_SOURCES_WEBCORE_DIR}/inspectorBB.html
              ${WEBKIT_DIR}/blackberry/WebCoreSupport/inspectorBB.js
              ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorBackendCommands.js
        DESTINATION ../../usr/share/webkit/inspector/)

if (NOT PUBLIC_BUILD)
    # Add the custom target to build the host-side ImageDiff binary.
    # Reuse the Qt version.
    add_custom_target(
        ImageDiff ALL
        WORKING_DIRECTORY ${TOOLS_DIR}/DumpRenderTree/blackberry/
        COMMAND ./build
        DEPENDS ${TOOLS_DIR}/DumpRenderTree/qt/ImageDiff.cpp
        COMMENT "ImageDiff building..."
    )
endif ()

add_custom_target(
    inspector ALL
    command cp ${WEBCORE_DIR}/inspector/front-end/inspector.html ${DERIVED_SOURCES_WEBCORE_DIR}/inspectorBB.html && echo '<script src="inspectorBB.js"></script>'  >> ${DERIVED_SOURCES_WEBCORE_DIR}/inspectorBB.html
    DEPENDS ${WebCore_LIBRARY_NAME}
    COMMENT "Web Inspector resources building..."
)

# Generate contents for AboutData.cpp
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataHaveFeatures.in
    DEPENDS ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataHaveFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl HAVE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataHaveFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataEnableFeatures.in
    DEPENDS ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataEnableFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl ENABLE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataEnableFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataUseFeatures.in
    DEPENDS ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataUseFeatures.in ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/blackberry/WebCoreSupport/generateAboutDataFeatures.pl USE ${WEBKIT_DIR}/blackberry/WebCoreSupport/AboutDataUseFeatures.in ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
)

add_custom_target(
    aboutFeatures ALL
    DEPENDS ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataHaveFeatures.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataEnableFeatures.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/AboutDataUseFeatures.cpp
)
