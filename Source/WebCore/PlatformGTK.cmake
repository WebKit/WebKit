list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/cairo"
    "${WEBCORE_DIR}/platform/geoclue"
    "${WEBCORE_DIR}/platform/gtk"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gtk"
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/linux"
    "${WEBCORE_DIR}/platform/mediastream/gstreamer"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/gtk"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/gtk"
    "${WEBCORE_DIR}/platform/text/icu"
)

list(APPEND WebCore_SOURCES
    editing/SmartReplace.cpp

    loader/soup/CachedRawResourceSoup.cpp
    loader/soup/SubresourceLoaderSoup.cpp

    platform/Cursor.cpp
    platform/PlatformStrategies.cpp

    platform/audio/gstreamer/AudioDestinationGStreamer.cpp
    platform/audio/gstreamer/AudioFileReaderGStreamer.cpp
    platform/audio/gstreamer/FFTFrameGStreamer.cpp
    platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp

    platform/geoclue/GeolocationProviderGeoclue1.cpp
    platform/geoclue/GeolocationProviderGeoclue2.cpp

    platform/graphics/GraphicsContext3DPrivate.cpp
    platform/graphics/OpenGLShims.cpp
    platform/graphics/WOFFFileFormat.cpp

    platform/graphics/cairo/BitmapImageCairo.cpp
    platform/graphics/cairo/CairoUtilities.cpp
    platform/graphics/cairo/DrawingBufferCairo.cpp
    platform/graphics/cairo/FloatRectCairo.cpp
    platform/graphics/cairo/FontCairo.cpp
    platform/graphics/cairo/FontCairoHarfbuzzNG.cpp
    platform/graphics/cairo/GradientCairo.cpp
    platform/graphics/cairo/GraphicsContext3DCairo.cpp
    platform/graphics/cairo/ImageBufferCairo.cpp
    platform/graphics/cairo/ImageCairo.cpp
    platform/graphics/cairo/IntRectCairo.cpp
    platform/graphics/cairo/OwnPtrCairo.cpp
    platform/graphics/cairo/PathCairo.cpp
    platform/graphics/cairo/PatternCairo.cpp
    platform/graphics/cairo/PlatformContextCairo.cpp
    platform/graphics/cairo/PlatformPathCairo.cpp
    platform/graphics/cairo/RefPtrCairo.cpp
    platform/graphics/cairo/TileCairo.cpp
    platform/graphics/cairo/TiledBackingStoreBackendCairo.cpp
    platform/graphics/cairo/TransformationMatrixCairo.cpp

    platform/graphics/freetype/FontCacheFreeType.cpp
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
    platform/graphics/freetype/SimpleFontDataFreeType.cpp

    platform/graphics/gstreamer/AudioTrackPrivateGStreamer.cpp
    platform/graphics/gstreamer/GRefPtrGStreamer.cpp
    platform/graphics/gstreamer/GStreamerUtilities.cpp
    platform/graphics/gstreamer/ImageGStreamerCairo.cpp
    platform/graphics/gstreamer/InbandTextTrackPrivateGStreamer.cpp
    platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp
    platform/graphics/gstreamer/MediaPlayerPrivateGStreamerBase.cpp
    platform/graphics/gstreamer/MediaSourceGStreamer.cpp
    platform/graphics/gstreamer/SourceBufferPrivateGStreamer.cpp
    platform/graphics/gstreamer/TextCombinerGStreamer.cpp
    platform/graphics/gstreamer/TextSinkGStreamer.cpp
    platform/graphics/gstreamer/TrackPrivateBaseGStreamer.cpp
    platform/graphics/gstreamer/VideoSinkGStreamer.cpp
    platform/graphics/gstreamer/VideoTrackPrivateGStreamer.cpp
    platform/graphics/gstreamer/WebKitMediaSourceGStreamer.cpp
    platform/graphics/gstreamer/WebKitWebSourceGStreamer.cpp

    platform/graphics/harfbuzz/HarfBuzzFace.cpp
    platform/graphics/harfbuzz/HarfBuzzFaceCairo.cpp
    platform/graphics/harfbuzz/HarfBuzzShaper.cpp

    platform/graphics/opengl/Extensions3DOpenGL.cpp
    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/Extensions3DOpenGLES.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/gtk/GamepadsGtk.cpp

    platform/image-decoders/cairo/ImageDecoderCairo.cpp

    platform/linux/GamepadDeviceLinux.cpp

    platform/mediastream/gstreamer/MediaStreamCenterGStreamer.cpp

    platform/network/soup/AuthenticationChallengeSoup.cpp
    platform/network/soup/CertificateInfo.cpp
    platform/network/soup/CookieJarSoup.cpp
    platform/network/soup/CookieStorageSoup.cpp
    platform/network/soup/CredentialStorageSoup.cpp
    platform/network/soup/DNSSoup.cpp
    platform/network/soup/NetworkStorageSessionSoup.cpp
    platform/network/soup/ProxyServerSoup.cpp
    platform/network/soup/ResourceErrorSoup.cpp
    platform/network/soup/ResourceHandleSoup.cpp
    platform/network/soup/ResourceRequestSoup.cpp
    platform/network/soup/ResourceResponseSoup.cpp
    platform/network/soup/SocketStreamHandleSoup.cpp
    platform/network/soup/SoupNetworkSession.cpp
    platform/network/soup/SynchronousLoaderClientSoup.cpp

    platform/soup/SharedBufferSoup.cpp
    platform/soup/URLSoup.cpp

    platform/text/icu/UTextProvider.cpp
    platform/text/icu/UTextProviderLatin1.cpp
    platform/text/icu/UTextProviderUTF16.cpp
    platform/text/LocaleICU.cpp
    platform/text/TextCodecICU.cpp
    platform/text/TextEncodingDetectorICU.cpp

    platform/text/enchant/TextCheckerEnchant.cpp

    platform/text/gtk/TextBreakIteratorInternalICUGtk.cpp

    platform/network/gtk/CredentialBackingStore.cpp

    plugins/PluginPackageNone.cpp
    plugins/PluginViewNone.cpp
)

list(APPEND WebCorePlatformGTK_SOURCES
    accessibility/atk/AXObjectCacheAtk.cpp
    accessibility/atk/AccessibilityObjectAtk.cpp
    accessibility/atk/WebKitAccessibleHyperlink.cpp
    accessibility/atk/WebKitAccessibleInterfaceAction.cpp
    accessibility/atk/WebKitAccessibleInterfaceComponent.cpp
    accessibility/atk/WebKitAccessibleInterfaceDocument.cpp
    accessibility/atk/WebKitAccessibleInterfaceEditableText.cpp
    accessibility/atk/WebKitAccessibleInterfaceHyperlinkImpl.cpp
    accessibility/atk/WebKitAccessibleInterfaceHypertext.cpp
    accessibility/atk/WebKitAccessibleInterfaceImage.cpp
    accessibility/atk/WebKitAccessibleInterfaceSelection.cpp
    accessibility/atk/WebKitAccessibleInterfaceTable.cpp
    accessibility/atk/WebKitAccessibleInterfaceTableCell.cpp
    accessibility/atk/WebKitAccessibleInterfaceText.cpp
    accessibility/atk/WebKitAccessibleInterfaceValue.cpp
    accessibility/atk/WebKitAccessibleUtil.cpp
    accessibility/atk/WebKitAccessibleWrapperAtk.cpp

    editing/atk/FrameSelectionAtk.cpp

    page/gtk/DragControllerGtk.cpp
    page/gtk/EventHandlerGtk.cpp

    platform/audio/gtk/AudioBusGtk.cpp

    platform/cairo/WidgetBackingStoreCairo.cpp

    platform/graphics/GLContext.cpp

    platform/graphics/cairo/GraphicsContextCairo.cpp

    platform/graphics/egl/GLContextEGL.cpp

    platform/graphics/freetype/FontPlatformDataFreeType.cpp

    platform/graphics/glx/GLContextGLX.cpp

    platform/graphics/gtk/ColorGtk.cpp
    platform/graphics/gtk/GdkCairoUtilities.cpp
    platform/graphics/gtk/IconGtk.cpp
    platform/graphics/gtk/ImageBufferGtk.cpp
    platform/graphics/gtk/ImageGtk.cpp
    platform/graphics/gtk/IntPointGtk.cpp
    platform/graphics/gtk/IntRectGtk.cpp

    platform/gtk/ClipboardUtilitiesGtk.cpp
    platform/gtk/ContextMenuGtk.cpp
    platform/gtk/ContextMenuItemGtk.cpp
    platform/gtk/CursorGtk.cpp
    platform/gtk/DataObjectGtk.cpp
    platform/gtk/DragDataGtk.cpp
    platform/gtk/DragIcon.cpp
    platform/gtk/DragImageGtk.cpp
    platform/gtk/ErrorsGtk.cpp
    platform/gtk/EventLoopGtk.cpp
    platform/gtk/FileSystemGtk.cpp
    platform/gtk/GRefPtrGtk.cpp
    platform/gtk/GtkClickCounter.cpp
    platform/gtk/GtkDragAndDropHelper.cpp
    platform/gtk/GtkInputMethodFilter.cpp
    platform/gtk/GtkPluginWidget.cpp
    platform/gtk/GtkPopupMenu.cpp
    platform/gtk/GtkTouchContextHelper.cpp
    platform/gtk/GtkUtilities.cpp
    platform/gtk/GtkVersioning.c
    platform/gtk/KeyBindingTranslator.cpp
    platform/gtk/LanguageGtk.cpp
    platform/gtk/LocalizedStringsGtk.cpp
    platform/gtk/LoggingGtk.cpp
    platform/gtk/MIMETypeRegistryGtk.cpp
    platform/gtk/MainFrameScrollbarGtk.cpp
    platform/gtk/PasteboardGtk.cpp
    platform/gtk/PasteboardHelper.cpp
    platform/gtk/PlatformKeyboardEventGtk.cpp
    platform/gtk/PlatformMouseEventGtk.cpp
    platform/gtk/PlatformScreenGtk.cpp
    platform/gtk/PlatformWheelEventGtk.cpp
    platform/gtk/PopupMenuGtk.cpp
    platform/gtk/RedirectedXCompositeWindow.cpp
    platform/gtk/RenderThemeGtk.cpp
    platform/gtk/RenderThemeGtk2.cpp
    platform/gtk/RenderThemeGtk3.cpp
    platform/gtk/ScrollViewGtk.cpp
    platform/gtk/ScrollbarThemeGtk.cpp
    platform/gtk/ScrollbarThemeGtk2.cpp
    platform/gtk/ScrollbarThemeGtk3.cpp
    platform/gtk/SearchPopupMenuGtk.cpp
    platform/gtk/SharedBufferGtk.cpp
    platform/gtk/SharedTimerGtk.cpp
    platform/gtk/SoundGtk.cpp
    platform/gtk/TemporaryLinkStubs.cpp
    platform/gtk/UserAgentGtk.cpp
    platform/gtk/WebKitAuthenticationWidget.cpp
    platform/gtk/WidgetBackingStoreGtkX11.cpp
    platform/gtk/WidgetGtk.cpp
    platform/gtk/WidgetRenderingContext.cpp
)

if (WTF_USE_GEOCLUE2)
    list(APPEND WebCore_SOURCES
        ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c
    )
    execute_process(COMMAND pkg-config --variable dbus_interface geoclue-2.0 OUTPUT_VARIABLE GEOCLUE_DBUS_INTERFACE)
    add_custom_command(
         OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.h
         COMMAND gdbus-codegen --interface-prefix org.freedesktop.GeoClue2. --c-namespace Geoclue --generate-c-code ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface ${GEOCLUE_DBUS_INTERFACE}
    )
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsGtk.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsApple.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsGtk.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/gtk/RenderThemeGtk.cpp)

list(APPEND WebCore_LIBRARIES
    ${ATK_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${ENCHANT_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GEOCLUE_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GUDEV_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${ICU_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${WEBP_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xt_LIB}
    ${ZLIB_LIBRARIES}
)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GUDEV_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${WEBP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        ${WEBCORE_DIR}/platform/graphics/gstreamer
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTREAMER_BASE_INCLUDE_DIRS}
        ${GSTREAMER_APP_INCLUDE_DIRS}
        ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
    )

    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_APP_LIBRARIES}
        ${GSTREAMER_BASE_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTREAMER_PBUTILS_LIBRARIES}
    )
    # Avoiding a GLib deprecation warning due to GStreamer API using deprecated classes.
    set_source_files_properties(platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp PROPERTIES COMPILE_DEFINITIONS "GLIB_DISABLE_DEPRECATION_WARNINGS=1")
endif ()

if (ENABLE_VIDEO)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        ${GSTREAMER_TAG_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_TAG_LIBRARIES}
        ${GSTREAMER_VIDEO_LIBRARIES}
    )
endif ()

if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        ${WEBCORE_DIR}/platform/audio/gstreamer
        ${GSTREAMER_AUDIO_INCLUDE_DIRS}
        ${GSTREAMER_FFT_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_AUDIO_LIBRARIES}
        ${GSTREAMER_FFT_LIBRARIES}
    )
endif ()

if (ENABLE_TEXTURE_MAPPER)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/texmap"
    )
    list(APPEND WebCore_SOURCES
        platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
        platform/graphics/texmap/TextureMapperGL.cpp
        platform/graphics/texmap/TextureMapperShaderProgram.cpp
    )
endif ()

if (WTF_USE_EGL)
    list(APPEND WebCore_LIBRARIES
        ${EGL_LIBRARY}
    )
endif ()

install(FILES "${WEBCORE_DIR}/Resources/textAreaResizeCorner.png"
              "${WEBCORE_DIR}/Resources/nullPlugin.png"
              "${WEBCORE_DIR}/Resources/urlIcon.png"
              "${WEBCORE_DIR}/Resources/missingImage.png"
              "${WEBCORE_DIR}/Resources/panIcon.png"
              "${WEBCORE_DIR}/Resources/deleteButton.png"
              "${WEBCORE_DIR}/Resources/inputSpeech.png"
        DESTINATION "${DATA_INSTALL_DIR}/images"
)

if (ENABLE_WEB_AUDIO)
    install(FILES "${WEBCORE_DIR}/platform/audio/resources/Composite.wav"
            DESTINATION "${DATA_INSTALL_DIR}/resources/audio"
    )
endif ()

if (ENABLE_WEBKIT2)
    # WebKit2 needs a version of WebCore compiled against GTK+2, so we've isolated all the GTK+
    # dependent files into a separate library which can be used to construct a GTK+2 WebCore
    # for the plugin process.
    add_library(WebCorePlatformGTK2 ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
    add_dependencies(WebCorePlatformGTK2 WebCore)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCorePlatformGTK2)
    set_property(TARGET WebCorePlatformGTK2
        APPEND
        PROPERTY COMPILE_DEFINITIONS GTK_API_VERSION_2=1
    )
    set_property(
        TARGET WebCorePlatformGTK2
        APPEND
        PROPERTY INCLUDE_DIRECTORIES
            ${WebCore_INCLUDE_DIRECTORIES}
            ${GTK2_INCLUDE_DIRS}
            ${GDK2_INCLUDE_DIRS}
    )
    target_link_libraries(WebCorePlatformGTK2
         ${WebCore_LIBRARIES}
         ${GTK2_LIBRARIES}
         ${GDK2_LIBRARIES}
    )
endif ()

add_library(WebCorePlatformGTK ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
add_dependencies(WebCorePlatformGTK WebCore)
WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCorePlatformGTK)
set_property(
    TARGET WebCorePlatformGTK
    APPEND
    PROPERTY INCLUDE_DIRECTORIES
        ${WebCore_INCLUDE_DIRECTORIES}
        ${GTK_INCLUDE_DIRS}
        ${GDK_INCLUDE_DIRS}
)
target_link_libraries(WebCorePlatformGTK
    ${WebCore_LIBRARIES}
    ${GTK_LIBRARIES}
    ${GDK_LIBRARIES}
)

include_directories(
    "${WebCore_INCLUDE_DIRECTORIES}"
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}"
)

list(APPEND GObjectDOMBindings_SOURCES
    bindings/gobject/ConvertToUTF8String.cpp
    bindings/gobject/DOMObjectCache.cpp
    bindings/gobject/GObjectEventListener.cpp
    bindings/gobject/WebKitDOMCustom.cpp
    bindings/gobject/WebKitDOMDeprecated.cpp
    bindings/gobject/WebKitDOMEventTarget.cpp
    bindings/gobject/WebKitDOMHTMLPrivate.cpp
    bindings/gobject/WebKitDOMObject.cpp
    bindings/gobject/WebKitDOMPrivate.cpp
    ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdomdefines.h
    ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdom.h
)

list(APPEND GObjectDOMBindings_IDL_FILES
    Modules/battery/BatteryManager.idl
    Modules/gamepad/Gamepad.idl
    Modules/gamepad/GamepadList.idl
    Modules/geolocation/Geolocation.idl
    Modules/mediasource/VideoPlaybackQuality.idl
    Modules/quota/StorageInfo.idl
    Modules/quota/StorageQuota.idl
    Modules/webdatabase/Database.idl

    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSStyleSheet.idl
    css/CSSValue.idl
    css/DOMWindowCSS.idl
    css/MediaList.idl
    css/MediaQueryList.idl
    css/StyleMedia.idl
    css/StyleSheet.idl
    css/StyleSheetList.idl

    dom/Attr.idl
    dom/CDATASection.idl
    dom/CharacterData.idl
    dom/Comment.idl
    dom/DOMImplementation.idl
    dom/DOMNamedFlowCollection.idl
    dom/DOMStringList.idl
    dom/DOMStringMap.idl
    dom/Document.idl
    dom/DocumentFragment.idl
    dom/DocumentType.idl
    dom/Element.idl
    dom/EntityReference.idl
    dom/Event.idl
    dom/KeyboardEvent.idl
    dom/MessagePort.idl
    dom/MouseEvent.idl
    dom/NamedNodeMap.idl
    dom/Node.idl
    dom/NodeFilter.idl
    dom/NodeIterator.idl
    dom/NodeList.idl
    dom/ProcessingInstruction.idl
    dom/Range.idl
    dom/Text.idl
    dom/Touch.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/WebKitNamedFlow.idl
    dom/WheelEvent.idl

    fileapi/Blob.idl
    fileapi/File.idl
    fileapi/FileList.idl

    html/DOMSettableTokenList.idl
    html/DOMTokenList.idl
    html/HTMLAnchorElement.idl
    html/HTMLAppletElement.idl
    html/HTMLAreaElement.idl
    html/HTMLBRElement.idl
    html/HTMLBaseElement.idl
    html/HTMLBaseFontElement.idl
    html/HTMLBodyElement.idl
    html/HTMLButtonElement.idl
    html/HTMLCanvasElement.idl
    html/HTMLCollection.idl
    html/HTMLDListElement.idl
    html/HTMLDetailsElement.idl
    html/HTMLDirectoryElement.idl
    html/HTMLDivElement.idl
    html/HTMLDocument.idl
    html/HTMLElement.idl
    html/HTMLEmbedElement.idl
    html/HTMLFieldSetElement.idl
    html/HTMLFontElement.idl
    html/HTMLFormElement.idl
    html/HTMLFrameElement.idl
    html/HTMLFrameSetElement.idl
    html/HTMLHRElement.idl
    html/HTMLHeadElement.idl
    html/HTMLHeadingElement.idl
    html/HTMLHtmlElement.idl
    html/HTMLIFrameElement.idl
    html/HTMLImageElement.idl
    html/HTMLInputElement.idl
    html/HTMLKeygenElement.idl
    html/HTMLLIElement.idl
    html/HTMLLabelElement.idl
    html/HTMLLegendElement.idl
    html/HTMLLinkElement.idl
    html/HTMLMapElement.idl
    html/HTMLMarqueeElement.idl
    html/HTMLMediaElement.idl
    html/HTMLMenuElement.idl
    html/HTMLMetaElement.idl
    html/HTMLModElement.idl
    html/HTMLOListElement.idl
    html/HTMLObjectElement.idl
    html/HTMLOptGroupElement.idl
    html/HTMLOptionElement.idl
    html/HTMLOptionsCollection.idl
    html/HTMLParagraphElement.idl
    html/HTMLParamElement.idl
    html/HTMLPreElement.idl
    html/HTMLQuoteElement.idl
    html/HTMLScriptElement.idl
    html/HTMLSelectElement.idl
    html/HTMLStyleElement.idl
    html/HTMLTableCaptionElement.idl
    html/HTMLTableCellElement.idl
    html/HTMLTableColElement.idl
    html/HTMLTableElement.idl
    html/HTMLTableRowElement.idl
    html/HTMLTableSectionElement.idl
    html/HTMLTextAreaElement.idl
    html/HTMLTitleElement.idl
    html/HTMLUListElement.idl
    html/MediaController.idl
    html/MediaError.idl
    html/TimeRanges.idl
    html/ValidityState.idl

    loader/appcache/DOMApplicationCache.idl

    page/BarProp.idl
    page/DOMSecurityPolicy.idl
    page/DOMSelection.idl
    page/DOMWindow.idl
    page/History.idl
    page/Location.idl
    page/Navigator.idl
    page/Performance.idl
    page/PerformanceEntry.idl
    page/PerformanceEntryList.idl
    page/PerformanceNavigation.idl
    page/PerformanceTiming.idl
    page/Screen.idl
    page/WebKitPoint.idl

    plugins/DOMMimeType.idl
    plugins/DOMMimeTypeArray.idl
    plugins/DOMPlugin.idl
    plugins/DOMPluginArray.idl

    storage/Storage.idl

    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
)

if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    list(APPEND GObjectDOMBindings_IDL_FILES
        html/HTMLAudioElement.idl
        html/HTMLVideoElement.idl

        html/track/AudioTrack.idl
        html/track/AudioTrackList.idl
        html/track/TextTrack.idl
        html/track/TextTrackCue.idl
        html/track/TextTrackCueList.idl
        html/track/TextTrackList.idl
        html/track/TrackEvent.idl
        html/track/VideoTrack.idl
        html/track/VideoTrackList.idl
    )
endif ()

set(GObjectDOMBindings_CLASS_LIST Custom EventTarget Object Deprecated)
set(GObjectDOMBindings_INSTALLED_HEADERS
     ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdomdefines.h
     ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdom.h
     ${WEBCORE_DIR}/bindings/gobject/WebKitDOMCustom.h
     ${WEBCORE_DIR}/bindings/gobject/WebKitDOMDeprecated.h
     ${WEBCORE_DIR}/bindings/gobject/WebKitDOMEventTarget.h
     ${WEBCORE_DIR}/bindings/gobject/WebKitDOMObject.h
)
file(GLOB GObjectDOMBindingsSymbolsFiles
    "${WEBCORE_DIR}/bindings/gobject/WebKitDOM*.symbols"
)

foreach (file ${GObjectDOMBindings_IDL_FILES})
    get_filename_component(classname ${file} NAME_WE)
    list(APPEND GObjectDOMBindings_CLASS_LIST ${classname})
    list(APPEND GObjectDOMBindings_INSTALLED_HEADERS ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/WebKitDOM${classname}.h)
    list(APPEND GObjectDOMBindingsSymbolsFiles ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/WebKitDOM${classname}.symbols)
endforeach ()

set(GOBJECT_DOM_BINDINGS_FEATURES_DEFINES "LANGUAGE_GOBJECT=1 ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
string(REPLACE "ENABLE_INDEXED_DATABASE=1" "" GOBJECT_DOM_BINDINGS_FEATURES_DEFINES ${GOBJECT_DOM_BINDINGS_FEATURES_DEFINES})
string(REPLACE REGEX "ENABLE_SVG[A-Z_]+=1" "" GOBJECT_DOM_BINDINGS_FEATURES_DEFINES ${GOBJECT_DOM_BINDINGS_FEATURES_DEFINES})

file(MAKE_DIRECTORY ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR})

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdomdefines.h
    COMMAND echo ${GObjectDOMBindings_CLASS_LIST} | ${PERL_EXECUTABLE} ${WEBCORE_DIR}/bindings/scripts/gobject-generate-headers.pl defines > ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdomdefines.h
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdom.h
    COMMAND echo ${GObjectDOMBindings_CLASS_LIST} | ${PERL_EXECUTABLE} ${WEBCORE_DIR}/bindings/scripts/gobject-generate-headers.pl gdom > ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/webkitdom.h
)

add_custom_target(fake-installed-webkitdom-headers
    COMMAND ln -n -s -f ${WEBCORE_DIR}/bindings/gobject/*.h ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}
)

GENERATE_BINDINGS(GObjectDOMBindings_SOURCES
    "${GObjectDOMBindings_IDL_FILES}"
    "${WEBCORE_DIR}"
    "${IDL_INCLUDES}"
    "${GOBJECT_DOM_BINDINGS_FEATURES_DEFINES}"
    ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}
    WebKitDOM GObject
    ${IDL_ATTRIBUTES_FILE}
    ${SUPPLEMENTAL_DEPENDENCY_FILE}
    ${WINDOW_CONSTRUCTORS_FILE}
    ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${SHAREDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE})

add_definitions(-DBUILDING_WEBKIT)

add_library(GObjectDOMBindings STATIC ${GObjectDOMBindings_SOURCES})

WEBKIT_SET_EXTRA_COMPILER_FLAGS(GObjectDOMBindings)

add_dependencies(GObjectDOMBindings
    WebCore
    fake-installed-webkitdom-headers
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkitdom.cfg
    "[webkitdomgtk]\n"
    "pkgconfig_file=${WebKit2_PKGCONFIG_FILE}\n"
    "namespace=webkit_dom\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBCORE_DIR}/bindings\n"
    "       -I${WEBCORE_DIR}/bindings/gobject\n"
    "       -I${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}\n"
    "doc_dir=${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}/docs\n"
    "source_dirs=${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}\n"
    "            ${WEBCORE_DIR}/bindings/gobject\n"
    "headers=${GObjectDOMBindings_INSTALLED_HEADERS}\n"
)

install(FILES ${GObjectDOMBindings_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkitdom"
)

# Some installed headers are not on the list of headers used for gir generation.
set(GObjectDOMBindings_GIR_HEADERS ${GObjectDOMBindings_INSTALLED_HEADERS})
list(REMOVE_ITEM GObjectDOMBindings_GIR_HEADERS
     bindings/gobject/WebKitDOMDeprecated.h
     bindings/gobject/WebKitDOMEventTarget.h
     bindings/gobject/WebKitDOMObject.h
)

# Propagate this variable to the parent scope, so that it can be used in other parts of the build.
set(GObjectDOMBindings_GIR_HEADERS ${GObjectDOMBindings_GIR_HEADERS} PARENT_SCOPE)
