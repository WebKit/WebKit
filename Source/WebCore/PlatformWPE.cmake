include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)
include(PlatformGLib.cmake)

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
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/audio/glib"
    "${WEBCORE_DIR}/platform/glib"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/gbm"
    "${WEBCORE_DIR}/platform/graphics/glx"
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
    "${WEBCORE_DIR}/platform/wpe"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/atspi/AccessibilityAtspi.h
    accessibility/atspi/AccessibilityAtspiEnums.h
    accessibility/atspi/AccessibilityObjectAtspi.h
    accessibility/atspi/AccessibilityRootAtspi.h

    platform/glib/ApplicationGLib.h

    platform/graphics/wayland/PlatformDisplayWayland.h
    platform/graphics/wayland/WlUniquePtr.h
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/wpe/RenderThemeWPE.cpp)

list(APPEND WebCore_LIBRARIES
    WPE::libwpe
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
)

if (USE_WPE_VIDEO_PLANE_DISPLAY_DMABUF OR USE_WPEBACKEND_FDO_AUDIO_EXTENSION)
    list(APPEND WebCore_LIBRARIES ${WPEBACKEND_FDO_LIBRARIES})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${WPE_INCLUDE_DIRS})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${WPEBACKEND_FDO_INCLUDE_DIRS})
endif ()

if (USE_OPENXR)
    list(APPEND WebCore_LIBRARIES ${OPENXR_LIBRARIES})
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES ${OPENXR_INCLUDE_DIRS})
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

if (USE_LIBGBM)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GBM_INCLUDE_DIR}
        ${LIBDRM_INCLUDE_DIR}
    )
    list(APPEND WebCore_LIBRARIES
        ${GBM_LIBRARIES}
        ${LIBDRM_LIBRARIES}
    )
endif ()

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/gamepad/libwpe"
    )

    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/gamepad/libwpe/GamepadProviderLibWPE.h
    )
endif ()
