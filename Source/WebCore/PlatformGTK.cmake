include(platform/Cairo.cmake)
include(platform/FreeType.cmake)
include(platform/GCrypt.cmake)
include(platform/GStreamer.cmake)
include(platform/ImageDecoders.cmake)
include(platform/Soup.cmake)
include(platform/TextureMapper.cmake)
include(PlatformGLib.cmake)

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "SourcesGTK.txt"

    "platform/SourcesGLib.txt"
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/accessibility/atspi"
    "${WEBCORE_DIR}/crypto/openssl"
    "${WEBCORE_DIR}/page/gtk"
    "${WEBCORE_DIR}/platform/adwaita"
    "${WEBCORE_DIR}/platform/audio/glib"
    "${WEBCORE_DIR}/platform/generic"
    "${WEBCORE_DIR}/platform/glib"
    "${WEBCORE_DIR}/platform/gtk"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/epoxy"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gbm"
    "${WEBCORE_DIR}/platform/graphics/gstreamer"
    "${WEBCORE_DIR}/platform/graphics/gtk"
    "${WEBCORE_DIR}/platform/graphics/libwpe"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/wayland"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediastream/gtk"
    "${WEBCORE_DIR}/platform/mediastream/gstreamer"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network/glib"
    "${WEBCORE_DIR}/platform/text/gtk"
)

list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
    accessibility/atspi/AccessibilityAtspi.h
    accessibility/atspi/AccessibilityAtspiEnums.h
    accessibility/atspi/AccessibilityObjectAtspi.h
    accessibility/atspi/AccessibilityRootAtspi.h

    platform/adwaita/ScrollbarThemeAdwaita.h

    platform/glib/ApplicationGLib.h

    platform/graphics/x11/PlatformDisplayX11.h
    platform/graphics/x11/XErrorTrapper.h
    platform/graphics/x11/XUniquePtr.h
    platform/graphics/x11/XUniqueResource.h

    platform/gtk/GRefPtrGtk.h
    platform/gtk/GUniquePtrGtk.h
    platform/gtk/GtkUtilities.h
    platform/gtk/GtkVersioning.h
    platform/gtk/ScrollbarThemeGtk.h
    platform/gtk/SelectionData.h

    platform/text/enchant/TextCheckerEnchant.h

    rendering/RenderThemeAdwaita.h
)

set(CSS_VALUE_PLATFORM_DEFINES "HAVE_OS_DARK_MODE_SUPPORT=1")

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/themeAdwaita.css
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.css
)

set(WebCore_USER_AGENT_SCRIPTS
    ${WebCore_DERIVED_SOURCES_DIR}/ModernMediaControls.js
)

set(WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/rendering/RenderThemeAdwaita.cpp)

list(APPEND WebCore_LIBRARIES
    ${ENCHANT_LIBRARIES}
    ${GLIB_GIO_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${LIBSECRET_LIBRARIES}
    ${LIBTASN1_LIBRARIES}
    ${HYPHEN_LIBRARIES}
    ${UPOWERGLIB_LIBRARIES}
    ${X11_X11_LIB}
    ${X11_Xcomposite_LIB}
    ${X11_Xdamage_LIB}
    ${X11_Xrender_LIB}
    ${X11_Xt_LIB}
    GTK::GTK
)

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ENCHANT_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBTASN1_INCLUDE_DIRS}
    ${UPOWERGLIB_INCLUDE_DIRS}
)

if (USE_OPENGL AND NOT USE_LIBEPOXY)
    list(APPEND WebCore_SOURCES
        platform/graphics/OpenGLShims.cpp
    )
endif ()

if (ENABLE_WAYLAND_TARGET)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/graphics/wayland/PlatformDisplayWayland.h
        platform/graphics/wayland/WlUniquePtr.h
    )
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${WAYLAND_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${WAYLAND_LIBRARIES}
        WPE::libwpe
    )
endif ()

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_PRIVATE_FRAMEWORK_HEADERS
        platform/gamepad/manette/ManetteGamepadProvider.h
    )
    list(APPEND WebCore_LIBRARIES
        Manette::Manette
    )
endif ()

if (ENABLE_BUBBLEWRAP_SANDBOX)
    list(APPEND WebCore_LIBRARIES Libseccomp::Libseccomp)
endif ()

include_directories(SYSTEM
    ${WebCore_SYSTEM_INCLUDE_DIRECTORIES}
)

list(APPEND WebCoreTestSupport_LIBRARIES PRIVATE GTK::GTK)

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
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
