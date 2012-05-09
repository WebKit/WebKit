LIST(APPEND WebCore_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
)

LIST(APPEND WebCore_INCLUDE_DIRECTORIES
  "${JAVASCRIPTCORE_DIR}/wtf/gobject"
  "${WEBCORE_DIR}/accessibility/efl"
  "${WEBCORE_DIR}/page/efl"
  "${WEBCORE_DIR}/platform/efl"
  "${WEBCORE_DIR}/platform/graphics/efl"
  "${WEBCORE_DIR}/platform/network/soup"
  "${WEBCORE_DIR}/platform/text/efl"
  "${WEBCORE_DIR}/plugins/efl"
  "${WEBKIT_DIR}/efl/WebCoreSupport"
  "${WEBKIT_DIR}/efl/ewk"
)

LIST(APPEND WebCore_SOURCES
  accessibility/efl/AccessibilityObjectEfl.cpp
  bindings/js/ScriptControllerEfl.cpp
  page/efl/DragControllerEfl.cpp
  page/efl/EventHandlerEfl.cpp
  platform/Cursor.cpp
  platform/efl/ClipboardEfl.cpp
  platform/efl/ColorChooserEfl.cpp
  platform/efl/ContextMenuEfl.cpp
  platform/efl/ContextMenuItemEfl.cpp
  platform/efl/CursorEfl.cpp
  platform/efl/DragDataEfl.cpp
  platform/efl/DragImageEfl.cpp
  platform/efl/EflKeyboardUtilities.cpp
  platform/efl/EflScreenUtilities.cpp
  platform/efl/EventLoopEfl.cpp
  platform/efl/FileSystemEfl.cpp
  platform/efl/KURLEfl.cpp
  platform/efl/LanguageEfl.cpp
  platform/efl/LocalizedStringsEfl.cpp
  platform/efl/LoggingEfl.cpp
  platform/efl/MIMETypeRegistryEfl.cpp
  platform/efl/PasteboardEfl.cpp
  platform/efl/PlatformKeyboardEventEfl.cpp
  platform/efl/PlatformMouseEventEfl.cpp
  platform/efl/PlatformScreenEfl.cpp
  platform/efl/PlatformTouchEventEfl.cpp
  platform/efl/PlatformTouchPointEfl.cpp
  platform/efl/PlatformWheelEventEfl.cpp
  platform/efl/PopupMenuEfl.cpp
  platform/efl/RefPtrEfl.cpp
  platform/efl/RenderThemeEfl.cpp
  platform/efl/RunLoopEfl.cpp
  platform/efl/ScrollViewEfl.cpp
  platform/efl/ScrollbarEfl.cpp
  platform/efl/ScrollbarThemeEfl.cpp
  platform/efl/SearchPopupMenuEfl.cpp
  platform/efl/SharedBufferEfl.cpp
  platform/efl/SharedTimerEfl.cpp
  platform/efl/SoundEfl.cpp
  platform/efl/SystemTimeEfl.cpp
  platform/efl/TemporaryLinkStubs.cpp
  platform/efl/WidgetEfl.cpp
  platform/graphics/ImageSource.cpp
  platform/graphics/efl/GraphicsLayerEfl.cpp
  platform/graphics/efl/IconEfl.cpp
  platform/graphics/efl/ImageEfl.cpp
  platform/graphics/efl/IntPointEfl.cpp
  platform/graphics/efl/IntRectEfl.cpp
  platform/image-decoders/ImageDecoder.cpp
  platform/image-decoders/bmp/BMPImageDecoder.cpp
  platform/image-decoders/bmp/BMPImageReader.cpp
  platform/image-decoders/gif/GIFImageDecoder.cpp
  platform/image-decoders/gif/GIFImageReader.cpp
  platform/image-decoders/ico/ICOImageDecoder.cpp
  platform/image-decoders/jpeg/JPEGImageDecoder.cpp
  platform/image-decoders/png/PNGImageDecoder.cpp
  platform/image-decoders/webp/WEBPImageDecoder.cpp
  platform/network/soup/CookieJarSoup.cpp
  platform/network/soup/CredentialStorageSoup.cpp
  platform/network/soup/DNSSoup.cpp
  platform/network/soup/GOwnPtrSoup.cpp
  platform/network/soup/ProxyServerSoup.cpp
  platform/network/soup/ResourceHandleSoup.cpp
  platform/network/soup/ResourceRequestSoup.cpp
  platform/network/soup/ResourceResponseSoup.cpp
  platform/network/soup/SocketStreamHandleSoup.cpp
  platform/network/soup/SoupURIUtils.cpp
  platform/posix/FileSystemPOSIX.cpp
  platform/text/efl/TextBreakIteratorInternalICUEfl.cpp
)

IF (ENABLE_NETSCAPE_PLUGIN_API)
  LIST(APPEND WebCore_SOURCES
    plugins/PluginDatabase.cpp
    plugins/PluginDebug.cpp
    plugins/PluginPackage.cpp
    plugins/PluginStream.cpp
    plugins/PluginView.cpp

    plugins/efl/PluginDataEfl.cpp
    plugins/efl/PluginPackageEfl.cpp
    plugins/efl/PluginViewEfl.cpp
  )
ELSE ()
  LIST(APPEND WebCore_SOURCES
    plugins/PluginDataNone.cpp
    plugins/PluginPackageNone.cpp
    plugins/PluginViewNone.cpp
  )
ENDIF ()

LIST(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsEfl.css
)

IF (WTF_USE_CAIRO)
  LIST(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/cairo"
    "${WEBCORE_DIR}/platform/graphics/cairo"
  )
  LIST(APPEND WebCore_SOURCES
    platform/cairo/WidgetBackingStoreCairo.cpp
    platform/graphics/cairo/BitmapImageCairo.cpp
    platform/graphics/cairo/CairoUtilities.cpp
    platform/graphics/cairo/FontCairo.cpp
    platform/graphics/cairo/GradientCairo.cpp
    platform/graphics/cairo/GraphicsContextCairo.cpp
    platform/graphics/cairo/ImageBufferCairo.cpp
    platform/graphics/cairo/ImageCairo.cpp
    platform/graphics/cairo/NativeImageCairo.cpp
    platform/graphics/cairo/OwnPtrCairo.cpp
    platform/graphics/cairo/PathCairo.cpp
    platform/graphics/cairo/PatternCairo.cpp
    platform/graphics/cairo/PlatformContextCairo.cpp
    platform/graphics/cairo/PlatformPathCairo.cpp
    platform/graphics/cairo/RefPtrCairo.cpp
    platform/graphics/cairo/TransformationMatrixCairo.cpp

    platform/image-decoders/cairo/ImageDecoderCairo.cpp
  )

  IF (WTF_USE_FREETYPE)
    LIST(APPEND WebCore_INCLUDE_DIRECTORIES
      "${WEBCORE_DIR}/platform/graphics/freetype"
    )
    LIST(APPEND WebCore_SOURCES
      platform/graphics/WOFFFileFormat.cpp
      platform/graphics/efl/FontEfl.cpp
      platform/graphics/freetype/FontCacheFreeType.cpp
      platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
      platform/graphics/freetype/FontPlatformDataFreeType.cpp
      platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
      platform/graphics/freetype/SimpleFontDataFreeType.cpp
    )
  ENDIF ()

  IF (WTF_USE_PANGO)
    LIST(APPEND WebCore_INCLUDE_DIRECTORIES
      "${WEBCORE_DIR}/platform/graphics/pango"
      ${Pango_INCLUDE_DIRS}
    )
    LIST(APPEND WebCore_SOURCES
      platform/graphics/pango/FontPango.cpp
      platform/graphics/pango/FontCachePango.cpp
      platform/graphics/pango/FontCustomPlatformDataPango.cpp
      platform/graphics/pango/FontPlatformDataPango.cpp
      platform/graphics/pango/GlyphPageTreeNodePango.cpp
      platform/graphics/pango/SimpleFontDataPango.cpp
      platform/graphics/pango/PangoUtilities.cpp
    )
    LIST(APPEND WebCore_LIBRARIES
      ${Pango_LIBRARY}
      ${Pango_Cairo_LIBRARY}
    )
  ENDIF ()
ENDIF ()

IF (WTF_USE_ICU_UNICODE)
  LIST(APPEND WebCore_SOURCES
    editing/SmartReplaceICU.cpp
    platform/text/TextEncodingDetectorICU.cpp
    platform/text/TextBreakIteratorICU.cpp
    platform/text/TextCodecICU.cpp
  )
ENDIF ()

LIST(APPEND WebCore_LIBRARIES
  ${CAIRO_LIBRARIES}
  ${ECORE_X_LIBRARIES}
  ${EFLDEPS_LIBRARIES}
  ${EVAS_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${ICU_LIBRARIES}
  ${JPEG_LIBRARY}
  ${LIBXML2_LIBRARIES}
  ${LIBXSLT_LIBRARIES}
  ${PNG_LIBRARY}
  ${SQLITE_LIBRARIES}
  ${Glib_LIBRARIES}
  ${LIBSOUP24_LIBRARIES}
  ${ZLIB_LIBRARIES}
)

LIST(APPEND WebCore_INCLUDE_DIRECTORIES
  ${CAIRO_INCLUDE_DIRS}
  ${ECORE_X_INCLUDE_DIRS}
  ${EFLDEPS_INCLUDE_DIRS}
  ${EVAS_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
  ${ICU_INCLUDE_DIRS}
  ${LIBXML2_INCLUDE_DIR}
  ${LIBXSLT_INCLUDE_DIR}
  ${SQLITE_INCLUDE_DIR}
  ${Glib_INCLUDE_DIRS}
  ${LIBSOUP24_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

IF (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
  LIST(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/graphics/gstreamer"

    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_INTERFACES_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
  )
  LIST(APPEND WebCore_SOURCES
    platform/graphics/gstreamer/GRefPtrGStreamer.cpp
    platform/graphics/gstreamer/GStreamerUtilities.cpp
    platform/graphics/gstreamer/GStreamerVersioning.cpp
  )
  LIST(APPEND WebCore_LIBRARIES
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_INTERFACES_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
  )
ENDIF ()

IF (ENABLE_VIDEO)
  LIST(APPEND WebCore_INCLUDE_DIRECTORIES
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
  )
  LIST(APPEND WebCore_SOURCES
    platform/graphics/gstreamer/GStreamerGWorld.cpp
    platform/graphics/gstreamer/ImageGStreamerCairo.cpp
    platform/graphics/gstreamer/MediaPlayerPrivateGStreamer.cpp
    platform/graphics/gstreamer/PlatformVideoWindowEfl.cpp
    platform/graphics/gstreamer/VideoSinkGStreamer.cpp
    platform/graphics/gstreamer/WebKitWebSourceGStreamer.cpp
  )
  LIST(APPEND WebCore_LIBRARIES
    ${GSTREAMER_VIDEO_LIBRARIES}
  )
ENDIF ()

IF (ENABLE_WEBGL)
  LIST(APPEND WebCore_INCLUDE_DIRECTORIES
    ${OPENGL_INCLUDE_DIR}
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/opengl"
  )
  LIST(APPEND WebCore_LIBRARIES
    ${OPENGL_gl_LIBRARY}
  )
  LIST(APPEND WebCore_SOURCES
    platform/graphics/cairo/DrawingBufferCairo.cpp
    platform/graphics/cairo/GraphicsContext3DCairo.cpp
    platform/graphics/glx/GraphicsContext3DPrivate.cpp
    platform/graphics/OpenGLShims.cpp
    platform/graphics/opengl/Extensions3DOpenGL.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
  )
ENDIF ()

ADD_DEFINITIONS(-DWTF_USE_CROSS_PLATFORM_CONTEXT_MENUS=1
                -DDATA_DIR="${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}")

IF (ENABLE_WEB_AUDIO)
  LIST(APPEND WebCore_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/audio/gstreamer"

    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_FFT_INCLUDE_DIRS}
  )
  LIST(APPEND WebCore_SOURCES
    platform/audio/efl/AudioBusEfl.cpp
    platform/audio/gstreamer/AudioDestinationGStreamer.cpp
    platform/audio/gstreamer/AudioFileReaderGStreamer.cpp
    platform/audio/gstreamer/FFTFrameGStreamer.cpp
    platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp
  )
  LIST(APPEND WebCore_LIBRARIES
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${GSTREAMER_FFT_LIBRARIES}
  )
  SET(WEB_AUDIO_DIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/webaudio/resources)
  FILE(GLOB WEB_AUDIO_DATA "${WEBCORE_DIR}/platform/audio/resources/*.wav")
  INSTALL(FILES ${WEB_AUDIO_DATA} DESTINATION ${WEB_AUDIO_DIR})
  ADD_DEFINITIONS(-DUNINSTALLED_AUDIO_RESOURCES_DIR="${WEBCORE_DIR}/platform/audio/resources")
ENDIF ()

