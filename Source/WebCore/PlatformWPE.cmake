include(platform/Adwaita.cmake)

if (USE_CAIRO)
    include(platform/Cairo.cmake)
    include(platform/FreeType.cmake)
elseif (USE_SKIA)
    include(platform/Skia.cmake)
endif ()

include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)

if (USE_EXTERNAL_HOLEPUNCH)
    include(platform/HolePunch.cmake)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesWPE.txt"

    "platform/SourcesGLib.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atspi"
    "${WEBCORE_DIR}/crypto/openssl"
    "${WEBCORE_DIR}/platform/audio/glib"
    "${WEBCORE_DIR}/platform/glib"
    "${WEBCORE_DIR}/platform/graphics/android"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/gbm"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/libwpe"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediastream/gstreamer"
    "${WEBCORE_DIR}/platform/network/glib"
    "${WEBCORE_DIR}/platform/text/icu"
    "${WEBCORE_DIR}/platform/video-codecs"
    "${WEBCORE_DIR}/platform/wpe"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/atspi/AccessibilityAtspi.h
    accessibility/atspi/AccessibilityAtspiEnums.h
    accessibility/atspi/AccessibilityObjectAtspi.h
    accessibility/atspi/AccessibilityRootAtspi.h

    platform/glib/ApplicationGLib.h
    platform/glib/SelectionData.h
    platform/glib/SystemSettings.h

    platform/graphics/android/GraphicsContextGLTextureMapperAndroid.h
    platform/graphics/android/PlatformDisplayAndroid.h

    platform/graphics/egl/PlatformDisplayDefault.h
    platform/graphics/egl/PlatformDisplaySurfaceless.h

    platform/graphics/gbm/GBMVersioning.h
    platform/graphics/gbm/PlatformDisplayGBM.h

    platform/graphics/libwpe/PlatformDisplayLibWPE.h
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/wpe/RenderThemeWPE.cpp)

list(APPEND WebCore_PRIVATE_LIBRARIES
    Tasn1::Tasn1
)

list(APPEND WebCore_LIBRARIES
    GLib::Module
    WPE::libwpe
    ${UPOWERGLIB_LIBRARIES}
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${UPOWERGLIB_INCLUDE_DIRS}
)

if (USE_OPENXR)
    list(APPEND WebCore_LIBRARIES ${OPENXR_LIBRARIES})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${OPENXR_INCLUDE_DIRS})
endif ()

if (USE_SKIA AND ENABLE_DRAG_SUPPORT)
    list(APPEND WebCore_SOURCES
        platform/skia/DragImageSkia.cpp
    )
endif ()

if (USE_ATSPI)
    set(WebCore_AtspiInterfaceFiles
        ${WEBCORE_DIR}/accessibility/atspi/xml/Accessible.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Action.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Application.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Cache.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Collection.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Component.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/DeviceEventController.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/DeviceEventListener.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Document.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/EditableText.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Event.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Hyperlink.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Hypertext.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Image.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Registry.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Selection.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Socket.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/TableCell.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Table.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Text.xml
        ${WEBCORE_DIR}/accessibility/atspi/xml/Value.xml
    )

    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/AccessibilityAtspiInterfaces.h ${WebCore_DERIVED_SOURCES_DIR}/AccessibilityAtspiInterfaces.c
        DEPENDS ${WebCore_AtspiInterfaceFiles}
        COMMAND gdbus-codegen --interface-prefix=org.a11y.atspi --c-namespace=webkit --pragma-once --interface-info-header --output=${WebCore_DERIVED_SOURCES_DIR}/AccessibilityAtspiInterfaces.h ${WebCore_AtspiInterfaceFiles}
        COMMAND gdbus-codegen --interface-prefix=org.a11y.atspi --c-namespace=webkit --interface-info-body --output=${WebCore_DERIVED_SOURCES_DIR}/AccessibilityAtspiInterfaces.c ${WebCore_AtspiInterfaceFiles}
        VERBATIM
    )

    list(APPEND WebCore_SOURCES
        ${WebCore_DERIVED_SOURCES_DIR}/AccessibilityAtspiInterfaces.c
    )
endif ()

if (USE_GBM)
    list(APPEND WebCore_LIBRARIES GBM::GBM)
elseif (USE_LIBDRM)
    list(APPEND WebCore_LIBRARIES LibDRM::LibDRM)
endif ()

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/gamepad/libwpe"
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/gamepad/libwpe/GamepadProviderLibWPE.h
    )
endif ()

# This sets the maximum amount of memory that BitmapTexturePool can hold before being more
# aggressive trying to release the unused textures.
# Use a big value as the default size limit (80MB, enough for ten 1920x1080 layers).
# Embedded users will want to set this limit depending on the capabilities of their platform.
list(APPEND WebCore_PRIVATE_DEFINITIONS
     BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB=80
)
