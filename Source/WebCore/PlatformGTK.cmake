include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Linux.cmake)

if (USE_TEXTURE_MAPPER)
    include(platform/TextureMapper.cmake)
endif ()

set(WebCore_OUTPUT_NAME WebCoreGTK)

list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/accessibility/atk"
    "${WEBCORE_DIR}/editing/atk"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/cairo"
    "${WEBCORE_DIR}/platform/gamepad"
    "${WEBCORE_DIR}/platform/gamepad/deprecated"
    "${WEBCORE_DIR}/platform/gamepad/glib"
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
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WEBCORE_DIR}/platform/mediastream/gtk"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/gtk"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/platform/text/gtk"
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

    editing/atk/FrameSelectionAtk.cpp

    loader/soup/CachedRawResourceSoup.cpp
    loader/soup/SubresourceLoaderSoup.cpp

    page/linux/ResourceUsageOverlayLinux.cpp
    page/linux/ResourceUsageThreadLinux.cpp

    platform/KillRingNone.cpp
    platform/StaticPasteboard.cpp
    platform/UserAgentQuirks.cpp

    platform/audio/glib/AudioBusGLib.cpp

    platform/gamepad/glib/GamepadsGlib.cpp

    platform/geoclue/GeolocationProviderGeoclue1.cpp
    platform/geoclue/GeolocationProviderGeoclue2.cpp

    platform/glib/EventLoopGlib.cpp
    platform/glib/FileSystemGlib.cpp
    platform/glib/KeyedDecoderGlib.cpp
    platform/glib/KeyedEncoderGlib.cpp
    platform/glib/MainThreadSharedTimerGLib.cpp
    platform/glib/SharedBufferGlib.cpp

    platform/graphics/GLContext.cpp
    platform/graphics/GraphicsContext3DPrivate.cpp

    platform/graphics/cairo/BackingStoreBackendCairoImpl.cpp
    platform/graphics/cairo/BackingStoreBackendCairoX11.cpp
    platform/graphics/cairo/CairoUtilities.cpp
    platform/graphics/cairo/FloatRectCairo.cpp
    platform/graphics/cairo/FontCairo.cpp
    platform/graphics/cairo/FontCairoHarfbuzzNG.cpp
    platform/graphics/cairo/GradientCairo.cpp
    platform/graphics/cairo/GraphicsContext3DCairo.cpp
    platform/graphics/cairo/GraphicsContextCairo.cpp
    platform/graphics/cairo/ImageBufferCairo.cpp
    platform/graphics/cairo/ImageCairo.cpp
    platform/graphics/cairo/IntRectCairo.cpp
    platform/graphics/cairo/NativeImageCairo.cpp
    platform/graphics/cairo/PathCairo.cpp
    platform/graphics/cairo/PatternCairo.cpp
    platform/graphics/cairo/PlatformContextCairo.cpp
    platform/graphics/cairo/PlatformPathCairo.cpp
    platform/graphics/cairo/RefPtrCairo.cpp
    platform/graphics/cairo/TransformationMatrixCairo.cpp

    platform/graphics/egl/GLContextEGL.cpp
    platform/graphics/egl/GLContextEGLWayland.cpp
    platform/graphics/egl/GLContextEGLX11.cpp

    platform/graphics/freetype/FontCacheFreeType.cpp
    platform/graphics/freetype/FontCustomPlatformDataFreeType.cpp
    platform/graphics/freetype/FontPlatformDataFreeType.cpp
    platform/graphics/freetype/GlyphPageTreeNodeFreeType.cpp
    platform/graphics/freetype/SimpleFontDataFreeType.cpp

    platform/graphics/glx/GLContextGLX.cpp

    platform/graphics/gstreamer/ImageGStreamerCairo.cpp

    platform/graphics/harfbuzz/ComplexTextControllerHarfBuzz.cpp
    platform/graphics/harfbuzz/HarfBuzzFace.cpp
    platform/graphics/harfbuzz/HarfBuzzFaceCairo.cpp
    platform/graphics/harfbuzz/HarfBuzzShaper.cpp

    platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
    platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
    platform/graphics/opengl/TemporaryOpenGLSetting.cpp

    platform/graphics/opentype/OpenTypeVerticalData.cpp

    platform/graphics/wayland/PlatformDisplayWayland.cpp

    platform/graphics/x11/PlatformDisplayX11.cpp
    platform/graphics/x11/XErrorTrapper.cpp
    platform/graphics/x11/XUniqueResource.cpp

    platform/gtk/DragDataGtk.cpp
    platform/gtk/ErrorsGtk.cpp
    platform/gtk/MIMETypeRegistryGtk.cpp
    platform/gtk/PasteboardGtk.cpp
    platform/gtk/ScrollAnimatorGtk.cpp
    platform/gtk/SelectionData.cpp
    platform/gtk/TemporaryLinkStubs.cpp
    platform/gtk/UserAgentGtk.cpp

    platform/image-decoders/cairo/ImageBackingStoreCairo.cpp

    platform/network/soup/AuthenticationChallengeSoup.cpp
    platform/network/soup/CertificateInfo.cpp
    platform/network/soup/CookieJarSoup.cpp
    platform/network/soup/CookieStorageSoup.cpp
    platform/network/soup/CredentialStorageSoup.cpp
    platform/network/soup/DNSSoup.cpp
    platform/network/soup/GRefPtrSoup.cpp
    platform/network/soup/NetworkStorageSessionSoup.cpp
    platform/network/soup/ProxyServerSoup.cpp
    platform/network/soup/ResourceErrorSoup.cpp
    platform/network/soup/ResourceHandleSoup.cpp
    platform/network/soup/ResourceRequestSoup.cpp
    platform/network/soup/ResourceResponseSoup.cpp
    platform/network/soup/SocketStreamHandleImplSoup.cpp
    platform/network/soup/SoupNetworkSession.cpp
    platform/network/soup/SynchronousLoaderClientSoup.cpp
    platform/network/soup/WebKitSoupRequestGeneric.cpp

    platform/soup/PublicSuffixSoup.cpp
    platform/soup/SharedBufferSoup.cpp
    platform/soup/URLSoup.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/text/enchant/TextCheckerEnchant.cpp

    platform/text/hyphen/HyphenationLibHyphen.cpp

    platform/unix/LoggingUnix.cpp
)

list(APPEND WebCorePlatformGTK_SOURCES
    editing/gtk/EditorGtk.cpp

    page/gtk/DragControllerGtk.cpp
    page/gtk/EventHandlerGtk.cpp

    platform/graphics/PlatformDisplay.cpp

    platform/graphics/gtk/ColorGtk.cpp
    platform/graphics/gtk/GdkCairoUtilities.cpp
    platform/graphics/gtk/IconGtk.cpp
    platform/graphics/gtk/ImageBufferGtk.cpp
    platform/graphics/gtk/ImageGtk.cpp

    platform/gtk/CursorGtk.cpp
    platform/gtk/DragImageGtk.cpp
    platform/gtk/GRefPtrGtk.cpp
    platform/gtk/GtkUtilities.cpp
    platform/gtk/GtkVersioning.c
    platform/gtk/LocalizedStringsGtk.cpp
    platform/gtk/PasteboardHelper.cpp
    platform/gtk/PlatformKeyboardEventGtk.cpp
    platform/gtk/PlatformMouseEventGtk.cpp
    platform/gtk/PlatformPasteboardGtk.cpp
    platform/gtk/PlatformScreenGtk.cpp
    platform/gtk/PlatformWheelEventGtk.cpp
    platform/gtk/RenderThemeGadget.cpp
    platform/gtk/ScrollbarThemeGtk.cpp
    platform/gtk/SoundGtk.cpp
    platform/gtk/WidgetGtk.cpp

    rendering/RenderThemeGtk.cpp
)

if (USE_GEOCLUE2)
    list(APPEND WebCore_DERIVED_SOURCES
        ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c
    )
    execute_process(COMMAND pkg-config --variable dbus_interface geoclue-2.0 OUTPUT_VARIABLE GEOCLUE_DBUS_INTERFACE)
    add_custom_command(
         OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.h
         COMMAND gdbus-codegen --interface-prefix org.freedesktop.GeoClue2. --c-namespace Geoclue --generate-c-code ${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface ${GEOCLUE_DBUS_INTERFACE}
    )
    set_source_files_properties(${DERIVED_SOURCES_WEBCORE_DIR}/Geoclue2Interface.c PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/mediaControlsGtk.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WEBCORE_DIR}/English.lproj/mediaControlsLocalizedStrings.js
    ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.js
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
    ${LIBGCRYPT_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${HYPHEN_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xt_LIB}
    ${ZLIB_LIBRARIES}
    WTF
)

list(APPEND WebCoreTestSupport_LIBRARIES WTF)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${ENCHANT_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GUDEV_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBGCRYPT_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

if (USE_OPENGL_ES_2)
    list(APPEND WebCore_SOURCES
        platform/graphics/opengl/Extensions3DOpenGLES.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
    )
endif ()

if (USE_OPENGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/OpenGLShims.cpp

        platform/graphics/opengl/Extensions3DOpenGL.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
    )
endif ()

if (ENABLE_PLUGIN_PROCESS_GTK2)
    # WebKitPluginProcess2 needs a version of WebCore compiled against GTK+2, so we've isolated all the GTK+
    # dependent files into a separate library which can be used to construct a GTK+2 WebCore
    # for the plugin process.
    add_library(WebCorePlatformGTK2 ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
    add_dependencies(WebCorePlatformGTK2 WebCore)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCorePlatformGTK2)
    set_property(TARGET WebCorePlatformGTK2
        APPEND
        PROPERTY COMPILE_DEFINITIONS GTK_API_VERSION_2=1
    )
    target_include_directories(WebCorePlatformGTK2 PRIVATE
        ${WebCore_INCLUDE_DIRECTORIES}
        ${GTK2_INCLUDE_DIRS}
        ${GDK2_INCLUDE_DIRS}
    )
    target_include_directories(WebCorePlatformGTK2 SYSTEM PRIVATE
        ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
    )
    target_link_libraries(WebCorePlatformGTK2
         ${WebCore_LIBRARIES}
         ${GTK2_LIBRARIES}
         ${GDK2_LIBRARIES}
    )
endif ()

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WAYLAND_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${WAYLAND_LIBRARIES}
    )
endif ()

add_library(WebCorePlatformGTK ${WebCore_LIBRARY_TYPE} ${WebCorePlatformGTK_SOURCES})
add_dependencies(WebCorePlatformGTK WebCore)
WEBKIT_SET_EXTRA_COMPILER_FLAGS(WebCorePlatformGTK)
target_include_directories(WebCorePlatformGTK PRIVATE
    ${WebCore_INCLUDE_DIRECTORIES}
)
target_include_directories(WebCorePlatformGTK SYSTEM PRIVATE
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
    ${GTK_INCLUDE_DIRS}
    ${GDK_INCLUDE_DIRS}
)
target_link_libraries(WebCorePlatformGTK
    ${WebCore_LIBRARIES}
    ${GTK_LIBRARIES}
    ${GDK_LIBRARIES}
)

include_directories(
    ${WebCore_INCLUDE_DIRECTORIES}
    "${WEBCORE_DIR}/bindings/gobject/"
)

include_directories(SYSTEM
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)

add_definitions(-DBUILDING_WEBKIT)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
    )
endif ()

if (ENABLE_SUBTLE_CRYPTO)
    list(APPEND WebCore_SOURCES
        crypto/CryptoAlgorithm.cpp
        crypto/CryptoAlgorithmRegistry.cpp
        crypto/CryptoKey.cpp
        crypto/SubtleCrypto.cpp
        crypto/WebKitSubtleCrypto.cpp

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

        crypto/gcrypt/CryptoAlgorithmHMACGCrypt.cpp

        crypto/gnutls/CryptoAlgorithmAES_CBCGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmAES_KWGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSAES_PKCS1_v1_5GnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSASSA_PKCS1_v1_5GnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRSA_OAEPGnuTLS.cpp
        crypto/gnutls/CryptoAlgorithmRegistryGnuTLS.cpp
        crypto/gnutls/CryptoKeyRSAGnuTLS.cpp
        crypto/gnutls/SerializedCryptoKeyWrapGnuTLS.cpp

        crypto/keys/CryptoKeyAES.cpp
        crypto/keys/CryptoKeyDataOctetSequence.cpp
        crypto/keys/CryptoKeyDataRSAComponents.cpp
        crypto/keys/CryptoKeyHMAC.cpp
        crypto/keys/CryptoKeyRSA.cpp
        crypto/keys/CryptoKeySerializationRaw.cpp
    )
endif ()
