list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}"
    "${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector"
    "${JAVASCRIPTCORE_DIR}"
    "${JAVASCRIPTCORE_DIR}/ForwardingHeaders"
    "${JAVASCRIPTCORE_DIR}/API"
    "${JAVASCRIPTCORE_DIR}/assembler"
    "${JAVASCRIPTCORE_DIR}/bytecode"
    "${JAVASCRIPTCORE_DIR}/bytecompiler"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${JAVASCRIPTCORE_DIR}/disassembler"
    "${JAVASCRIPTCORE_DIR}/heap"
    "${JAVASCRIPTCORE_DIR}/debugger"
    "${JAVASCRIPTCORE_DIR}/interpreter"
    "${JAVASCRIPTCORE_DIR}/jit"
    "${JAVASCRIPTCORE_DIR}/llint"
    "${JAVASCRIPTCORE_DIR}/parser"
    "${JAVASCRIPTCORE_DIR}/profiler"
    "${JAVASCRIPTCORE_DIR}/runtime"
    "${JAVASCRIPTCORE_DIR}/yarr"
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/efl"
    "${WEBCORE_DIR}/page/scrolling/coordinatedgraphics"
    "${WEBCORE_DIR}/platform/cairo"
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/geoclue"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/efl"
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/surfaces"
    "${WEBCORE_DIR}/platform/graphics/surfaces/efl"
    "${WEBCORE_DIR}/platform/graphics/surfaces/glx"
    "${WEBCORE_DIR}/platform/graphics/texmap"
    "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WEBCORE_DIR}/platform/linux"
    "${WEBCORE_DIR}/platform/mediastream/openwebrtc"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/efl"
    "${WEBCORE_DIR}/plugins/efl"
    "${WTF_DIR}"
    "${WTF_DIR}/wtf/efl"
)

list(APPEND WebCore_SOURCES
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

    editing/SmartReplace.cpp

    editing/atk/FrameSelectionAtk.cpp

    editing/efl/EditorEfl.cpp

    html/shadow/MediaControlsApple.cpp

    loader/soup/CachedRawResourceSoup.cpp
    loader/soup/SubresourceLoaderSoup.cpp

    page/efl/DragControllerEfl.cpp
    page/efl/EventHandlerEfl.cpp

    page/scrolling/AxisScrollSnapOffsets.cpp

    page/scrolling/coordinatedgraphics/ScrollingCoordinatorCoordinatedGraphics.cpp
    page/scrolling/coordinatedgraphics/ScrollingStateNodeCoordinatedGraphics.cpp

    platform/KillRingNone.cpp

    platform/audio/efl/AudioBusEfl.cpp

    platform/audio/gstreamer/AudioDestinationGStreamer.cpp
    platform/audio/gstreamer/AudioFileReaderGStreamer.cpp
    platform/audio/gstreamer/AudioSourceProviderGStreamer.cpp
    platform/audio/gstreamer/FFTFrameGStreamer.cpp
    platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp

    platform/efl/BatteryProviderEfl.cpp
    platform/efl/CursorEfl.cpp
    platform/efl/DragDataEfl.cpp
    platform/efl/DragImageEfl.cpp
    platform/efl/EflInspectorUtilities.cpp
    platform/efl/EflKeyboardUtilities.cpp
    platform/efl/EflScreenUtilities.cpp
    platform/efl/ErrorsEfl.cpp
    platform/efl/EventLoopEfl.cpp
    platform/efl/FileSystemEfl.cpp
    platform/efl/GamepadsEfl.cpp
    platform/efl/LanguageEfl.cpp
    platform/efl/LocalizedStringsEfl.cpp
    platform/efl/LoggingEfl.cpp
    platform/efl/MIMETypeRegistryEfl.cpp
    platform/efl/MainThreadSharedTimerEfl.cpp
    platform/efl/PasteboardEfl.cpp
    platform/efl/PlatformKeyboardEventEfl.cpp
    platform/efl/PlatformMouseEventEfl.cpp
    platform/efl/PlatformScreenEfl.cpp
    platform/efl/PlatformWheelEventEfl.cpp
    platform/efl/ScrollbarThemeEfl.cpp
    platform/efl/SoundEfl.cpp
    platform/efl/TemporaryLinkStubs.cpp
    platform/efl/UserAgentEfl.cpp
    platform/efl/WidgetEfl.cpp

    platform/geoclue/GeolocationProviderGeoclue1.cpp
    platform/geoclue/GeolocationProviderGeoclue2.cpp

    platform/glib/KeyedDecoderGlib.cpp
    platform/glib/KeyedEncoderGlib.cpp

    platform/graphics/ImageSource.cpp
    platform/graphics/PlatformDisplay.cpp
    platform/graphics/WOFFFileFormat.cpp

    platform/graphics/cairo/BackingStoreBackendCairoImpl.cpp
    platform/graphics/cairo/BitmapImageCairo.cpp
    platform/graphics/cairo/CairoUtilities.cpp
    platform/graphics/cairo/FontCairo.cpp
    platform/graphics/cairo/FontCairoHarfbuzzNG.cpp
    platform/graphics/cairo/GradientCairo.cpp
    platform/graphics/cairo/GraphicsContextCairo.cpp
    platform/graphics/cairo/ImageBufferCairo.cpp
    platform/graphics/cairo/ImageCairo.cpp
    platform/graphics/cairo/IntRectCairo.cpp
    platform/graphics/cairo/PathCairo.cpp
    platform/graphics/cairo/PatternCairo.cpp
    platform/graphics/cairo/PlatformContextCairo.cpp
    platform/graphics/cairo/PlatformPathCairo.cpp
    platform/graphics/cairo/RefPtrCairo.cpp
    platform/graphics/cairo/TransformationMatrixCairo.cpp

    platform/graphics/efl/CairoUtilitiesEfl.cpp
    platform/graphics/efl/EvasGLContext.cpp
    platform/graphics/efl/EvasGLSurface.cpp
    platform/graphics/efl/GraphicsContext3DEfl.cpp
    platform/graphics/efl/GraphicsContext3DPrivate.cpp
    platform/graphics/efl/IconEfl.cpp
    platform/graphics/efl/ImageBufferEfl.cpp
    platform/graphics/efl/ImageEfl.cpp
    platform/graphics/efl/IntPointEfl.cpp
    platform/graphics/efl/IntRectEfl.cpp

    platform/graphics/freetype/FontCacheFreeType.cpp
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/FontPlatformDataFreeType.cpp
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

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/GLPlatformContext.cpp
    platform/graphics/opengl/GLPlatformSurface.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/graphics/surfaces/GLTransportSurface.cpp
    platform/graphics/surfaces/GraphicsSurface.cpp

    platform/graphics/surfaces/efl/GraphicsSurfaceCommon.cpp

    platform/graphics/surfaces/glx/X11Helper.cpp

    platform/graphics/texmap/BitmapTexture.cpp
    platform/graphics/texmap/BitmapTextureGL.cpp
    platform/graphics/texmap/BitmapTexturePool.cpp
    platform/graphics/texmap/ClipStack.cpp
    platform/graphics/texmap/GraphicsLayerTextureMapper.cpp
    platform/graphics/texmap/TextureMapperGL.cpp
    platform/graphics/texmap/TextureMapperShaderProgram.cpp

    platform/graphics/texmap/coordinated/AreaAllocator.cpp
    platform/graphics/texmap/coordinated/CompositingCoordinator.cpp
    platform/graphics/texmap/coordinated/CoordinatedGraphicsLayer.cpp
    platform/graphics/texmap/coordinated/CoordinatedImageBacking.cpp
    platform/graphics/texmap/coordinated/CoordinatedSurface.cpp
    platform/graphics/texmap/coordinated/Tile.cpp
    platform/graphics/texmap/coordinated/TiledBackingStore.cpp
    platform/graphics/texmap/coordinated/UpdateAtlas.cpp

    platform/graphics/x11/PlatformDisplayX11.cpp
    platform/graphics/x11/XUniqueResource.cpp

    platform/image-encoders/JPEGImageEncoder.cpp

    platform/image-decoders/ImageDecoder.cpp

    platform/image-decoders/bmp/BMPImageDecoder.cpp
    platform/image-decoders/bmp/BMPImageReader.cpp

    platform/image-decoders/cairo/ImageDecoderCairo.cpp

    platform/image-decoders/gif/GIFImageDecoder.cpp
    platform/image-decoders/gif/GIFImageReader.cpp

    platform/image-decoders/ico/ICOImageDecoder.cpp

    platform/image-decoders/jpeg/JPEGImageDecoder.cpp

    platform/image-decoders/png/PNGImageDecoder.cpp

    platform/image-decoders/webp/WEBPImageDecoder.cpp

    platform/linux/GamepadDeviceLinux.cpp
    platform/linux/MemoryPressureHandlerLinux.cpp

    platform/mediastream/openwebrtc/OpenWebRTCUtilities.cpp
    platform/mediastream/openwebrtc/RealtimeMediaSourceCenterOwr.cpp

    platform/network/efl/NetworkStateNotifierEfl.cpp

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
    platform/network/soup/WebKitSoupRequestGeneric.cpp

    platform/posix/FileSystemPOSIX.cpp
    platform/posix/SharedBufferPOSIX.cpp

    platform/soup/PublicSuffixSoup.cpp
    platform/soup/SharedBufferSoup.cpp
    platform/soup/URLSoup.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/text/efl/TextBreakIteratorInternalICUEfl.cpp

    platform/text/enchant/TextCheckerEnchant.cpp

    platform/text/hyphen/HyphenationLibHyphen.cpp

    rendering/RenderThemeEfl.cpp
)

if (USE_GEOCLUE2)
    list(APPEND WebCore_SOURCES
        ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c
    )
    execute_process(COMMAND pkg-config --variable dbus_interface geoclue-2.0 OUTPUT_VARIABLE GEOCLUE_DBUS_INTERFACE)
    add_custom_command(
         OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.h
         COMMAND gdbus-codegen --interface-prefix org.freedesktop.GeoClue2. --c-namespace Geoclue --generate-c-code ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface ${GEOCLUE_DBUS_INTERFACE}
    )
endif ()

if (ENABLE_GAMEPAD_DEPRECATED)
    # FIXME: GAMEPAD_DEPRECATED is legacy implementation. Need to be removed.
    list(REMOVE_ITEM WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad"
    )
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitVersion.h
    MAIN_DEPENDENCY ${WEBKIT_DIR}/scripts/generate-webkitversion.pl
    DEPENDS ${WEBKIT_DIR}/mac/Configurations/Version.xcconfig
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/scripts/generate-webkitversion.pl --config ${WEBKIT_DIR}/mac/Configurations/Version.xcconfig --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}
    VERBATIM)
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitVersion.h)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/efl/RenderThemeEfl.cpp)

list(APPEND WebCore_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_FILE_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EEZE_LIBRARIES}
    ${EINA_LIBRARIES}
    ${ELDBUS_LIBRARIES}
    ${EO_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GEOCLUE_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${HYPHEN_LIBRARIES}
    ${PNG_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${WEBP_LIBRARIES}
    ${X11_X11_LIB}
    ${ZLIB_LIBRARIES}
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_FILE_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EO_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EEZE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${ELDBUS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${WEBP_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
)

if (ENABLE_MEDIA_STREAM)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${OPENWEBRTC_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${OPENWEBRTC_LIBRARIES}
    )
endif ()

if (ENABLE_VIDEO OR ENABLE_WEB_AUDIO)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gstreamer"
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
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
        ${GSTREAMER_AUDIO_LIBRARIES}
    )
    # Avoiding a GLib deprecation warning due to GStreamer API using deprecated classes.
    set_source_files_properties(platform/audio/gstreamer/WebKitWebAudioSourceGStreamer.cpp PROPERTIES COMPILE_DEFINITIONS "GLIB_DISABLE_DEPRECATION_WARNINGS=1")
endif ()

if (ENABLE_VIDEO)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_TAG_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_TAG_LIBRARIES}
        ${GSTREAMER_VIDEO_LIBRARIES}
    )

    if (USE_GSTREAMER_MPEGTS)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${GSTREAMER_MPEGTS_INCLUDE_DIRS}
        )

        list(APPEND WebCore_LIBRARIES
            ${GSTREAMER_MPEGTS_LIBRARIES}
        )
    endif ()
endif ()

if (USE_EGL)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/surfaces/egl"
    )
endif ()

if (USE_EGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/surfaces/egl/EGLConfigSelector.cpp
        platform/graphics/surfaces/egl/EGLContext.cpp
        platform/graphics/surfaces/egl/EGLHelper.cpp
        platform/graphics/surfaces/egl/EGLSurface.cpp
        platform/graphics/surfaces/egl/EGLXSurface.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/surfaces/glx/GLXContext.cpp
        platform/graphics/surfaces/glx/GLXSurface.cpp
    )
endif ()

if (USE_OPENGL_ES_2)
    list(APPEND WebCore_SOURCES
        platform/graphics/opengl/Extensions3DOpenGLES.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    )
else ()
    list(APPEND WebCore_SOURCES
        platform/graphics/OpenGLShims.cpp

        platform/graphics/opengl/Extensions3DOpenGL.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    )
endif ()

if (NOT USE_EGL AND X11_Xcomposite_FOUND AND X11_Xrender_FOUND)
    list(APPEND WebCore_LIBRARIES
        ${X11_Xcomposite_LIB}
        ${X11_Xrender_LIB}
    )
endif ()

if (ENABLE_WEB_AUDIO)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/audio/gstreamer"
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_AUDIO_INCLUDE_DIRS}
        ${GSTREAMER_FFT_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GSTREAMER_FFT_LIBRARIES}
    )
    set(WEB_AUDIO_DIR ${CMAKE_INSTALL_PREFIX}/${DATA_INSTALL_DIR}/webaudio/resources)
    file(GLOB WEB_AUDIO_DATA "${WEBCORE_DIR}/platform/audio/resources/*.wav")
    install(FILES ${WEB_AUDIO_DATA} DESTINATION ${WEB_AUDIO_DIR})
    add_definitions(-DUNINSTALLED_AUDIO_RESOURCES_DIR="${WEBCORE_DIR}/platform/audio/resources")
endif ()

if (ENABLE_SPELLCHECK)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${ENCHANT_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${ENCHANT_LIBRARIES}
    )
endif ()

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/accessibility/atk"
    )
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${ATK_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${ATK_LIBRARIES}
    )
endif ()

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
        platform/ScrollAnimatorSmooth.cpp
    )
endif ()

if (ENABLE_SPEECH_SYNTHESIS)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${ESPEAK_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${ESPEAK_LIBRARIES}
    )
    list(APPEND WebCore_SOURCES
        platform/efl/PlatformSpeechSynthesisProviderEfl.cpp
        platform/efl/PlatformSpeechSynthesizerEfl.cpp
    )
endif ()

if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND WebCore_SOURCES
        crypto/CryptoAlgorithm.cpp
        crypto/CryptoAlgorithmDescriptionBuilder.cpp
        crypto/CryptoAlgorithmRegistry.cpp
        crypto/CryptoKey.cpp
        crypto/CryptoKeyPair.cpp
        crypto/SubtleCrypto.cpp

        crypto/algorithms/CryptoAlgorithmAES_CBC.cpp
        crypto/algorithms/CryptoAlgorithmAES_KW.cpp
        crypto/algorithms/CryptoAlgorithmHMAC.cpp
        crypto/algorithms/CryptoAlgorithmRSAES_PKCS1_v1_5.cpp
        crypto/algorithms/CryptoAlgorithmRSASSA_PKCS1_v1_5.cpp
        crypto/algorithms/CryptoAlgorithmRSA_OAEP.cpp
        crypto/algorithms/CryptoAlgorithmSHA1.cpp
        crypto/algorithms/CryptoAlgorithmSHA224.cpp
        crypto/algorithms/CryptoAlgorithmSHA256.cpp
        crypto/algorithms/CryptoAlgorithmSHA384.cpp
        crypto/algorithms/CryptoAlgorithmSHA512.cpp

        crypto/gnutls/CryptoAlgorithmAES_CBCGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmAES_KWGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmHMACGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSAES_PKCS1_v1_5GnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSASSA_PKCS1_v1_5GnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSA_OAEPGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRegistryGnuTLS.cpp
        crypto/gnutls/CryptoDigestGnuTLS.cpp
        crypto/gnutls/CryptoKeyRSAGnuTLS.cpp
        crypto/gnutls/SerializedCryptoKeyWrapGnuTLS.cpp

        crypto/keys/CryptoKeyAES.cpp
        crypto/keys/CryptoKeyDataOctetSequence.cpp
        crypto/keys/CryptoKeyDataRSAComponents.cpp
        crypto/keys/CryptoKeyHMAC.cpp
        crypto/keys/CryptoKeySerializationRaw.cpp
    )

    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GNUTLS_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GNUTLS_LIBRARIES}
    )
endif ()

if (DEVELOPER_MODE)
    if (USE_LIBHYPHEN AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/WebKitBuild/DependenciesEFL)
        add_definitions(-DTEST_HYPHENATAION_PATH=\"${CMAKE_SOURCE_DIR}/WebKitBuild/DependenciesEFL/Root/webkitgtk-test-dicts\")
    endif ()
endif ()
