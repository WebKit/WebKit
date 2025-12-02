/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDisplay.h"

#include "WPEDRMDevicePrivate.h"
#include "WPEDisplayPrivate.h"
#include "WPEEGLError.h"
#include "WPEExtensions.h"
#include "WPEInputMethodContextNone.h"
#include <epoxy/egl.h>
#include <gio/gio.h>
#include <mutex>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#include <xf86drm.h>
#endif

#if USE(MANETTE)
#include "WPEGamepadManagerManette.h"
#endif

/**
 * WPEDisplay:
 *
 */
struct _WPEDisplayPrivate {
    bool eglInitialized;
    EGLDisplay eglDisplay;
    GUniqueOutPtr<GError> eglDisplayError;
    HashMap<String, bool> extensionsMap;
    GRefPtr<WPEBufferDMABufFormats> preferredDMABufFormats;
    GRefPtr<WPEKeymap> keymap;
    GRefPtr<WPEClipboard> clipboard;
    GRefPtr<WPESettings> settings;
    WPEAvailableInputDevices availableInputDevices;
    GRefPtr<WPEDRMDevice> overridenDRMDevice;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEDisplay, wpe_display, G_TYPE_OBJECT)

enum {
    PROP_0,

    PROP_AVAILABLE_INPUT_DEVICES,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

enum {
    SCREEN_ADDED,
    SCREEN_REMOVED,

    DISCONNECTED,

    LAST_SIGNAL
};

static std::array<unsigned, LAST_SIGNAL> signals;

/**
 * wpe_display_error_quark:
 *
 * Gets the WPEDisplay Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK(wpe-display-error-quark, wpe_display_error)

static GWeakPtr<WPEDisplay> s_primaryDisplay;

static void wpeDisplayConstructed(GObject* object)
{
    if (!s_primaryDisplay)
        s_primaryDisplay.reset(WPE_DISPLAY(object));

    G_OBJECT_CLASS(wpe_display_parent_class)->constructed(object);
}

static void wpeDisplayDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY(object)->priv;

    if (priv->eglDisplay) {
        eglTerminate(priv->eglDisplay);
        priv->eglDisplay = nullptr;
    }

    G_OBJECT_CLASS(wpe_display_parent_class)->dispose(object);
}

static void wpeDisplaySetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* display = WPE_DISPLAY(object);

    switch (propId) {
    case PROP_AVAILABLE_INPUT_DEVICES:
        wpe_display_set_available_input_devices(display, static_cast<WPEAvailableInputDevices>(g_value_get_flags(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeDisplayGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* display = WPE_DISPLAY(object);

    switch (propId) {
    case PROP_AVAILABLE_INPUT_DEVICES:
        g_value_set_flags(value, wpe_display_get_available_input_devices(display));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpe_display_class_init(WPEDisplayClass* displayClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayClass);
    objectClass->constructed = wpeDisplayConstructed;
    objectClass->dispose = wpeDisplayDispose;
    objectClass->set_property = wpeDisplaySetProperty;
    objectClass->get_property = wpeDisplayGetProperty;

    /**
     * WPEDisplay:available-input-devices:
     *
     * The input devices (e.g. mouse, keyboard or touchscreen) available to use for this display.
     *
     * This property can be used by creators to adjust their UI based on the available interactions.
     */
    sObjProperties[PROP_AVAILABLE_INPUT_DEVICES] =
        g_param_spec_flags(
            "available-input-devices",
            nullptr, nullptr,
            WPE_TYPE_AVAILABLE_INPUT_DEVICES,
            WPE_AVAILABLE_INPUT_DEVICE_NONE,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());

    /**
     * WPEDisplay::screen-added:
     * @display: a #WPEDisplay
     * @screen: the #WPEScreen added
     *
     * Emitted when a screen is added
     */
    signals[SCREEN_ADDED] = g_signal_new(
        "screen-added",
        G_TYPE_FROM_CLASS(displayClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_SCREEN);

    /**
     * WPEDisplay::screen-removed:
     * @display: a #WPEDisplay
     * @screen: the #WPEScreen removed
     *
     * Emitted after a screen is removed.
     * Note that the screen is always invalidated before this signal is emitted.
     */
    signals[SCREEN_REMOVED] = g_signal_new(
        "screen-removed",
        G_TYPE_FROM_CLASS(displayClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_SCREEN);

    /**
     * WPEDisplay::disconnected:
     * @display: a #WPEDisplay
     * @error: (nullable): a #GError or %NULL
     *
     * Emitted when the @display is disconnected from the platform display.
     */
    signals[DISCONNECTED] = g_signal_new(
        "disconnected",
        G_TYPE_FROM_CLASS(displayClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        G_TYPE_ERROR);
}

WPEView* wpeDisplayCreateView(WPEDisplay* display)
{
    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->create_view(display);
}

bool wpeDisplayCheckEGLExtension(WPEDisplay* display, const char* extensionName)
{
    auto addResult = display->priv->extensionsMap.ensure(ASCIILiteral::fromLiteralUnsafe(extensionName), [&] {
        auto* eglDisplay = wpe_display_get_egl_display(display, nullptr);
        return eglDisplay ? epoxy_has_egl_extension(eglDisplay, extensionName) : false;
    });
    return addResult.iterator->value;
}

WPEInputMethodContext* wpeDisplayCreateInputMethodContext(WPEDisplay* display, WPEView* view)
{
    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);

    auto* inputMethodContext = wpeDisplayClass->create_input_method_context ? wpeDisplayClass->create_input_method_context(display, view) : nullptr;
    if (!inputMethodContext)
        inputMethodContext = wpeInputMethodContextNoneNew(view);
    return inputMethodContext;
}

/**
 * wpe_display_get_default:
 *
 * Get the default display
 *
 * Returns: (nullable) (transfer none): the default display or %NULL
 */
WPEDisplay* wpe_display_get_default(void)
{
    static GRefPtr<WPEDisplay> s_defaultDisplay;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        wpeEnsureExtensionPointsLoaded();
        auto* extensionPoint = g_io_extension_point_lookup(WPE_DISPLAY_EXTENSION_POINT_NAME);

        const char* extensionName = g_getenv("WPE_DISPLAY");
        if (extensionName && *extensionName) {
            if (auto* extension = g_io_extension_point_get_extension_by_name(extensionPoint, extensionName)) {
                GUniqueOutPtr<GError> error;
                GRefPtr<WPEDisplay> display = adoptGRef(WPE_DISPLAY(g_object_new(g_io_extension_get_type(extension), nullptr)));
                if (wpe_display_connect(display.get(), &error.outPtr())) {
                    s_defaultDisplay = WTFMove(display);
                    return;
                }
                g_error("Failed to connect to display of type %s: %s", extensionName, error->message);
            } else
                g_error("Display of type %s was not found", extensionName);
            return;
        }

        auto* extensionList = g_io_extension_point_get_extensions(extensionPoint);
        for (auto* i = extensionList; i; i = g_list_next(i)) {
            auto* extension = static_cast<GIOExtension*>(i->data);
            GRefPtr<WPEDisplay> display = adoptGRef(WPE_DISPLAY(g_object_new(g_io_extension_get_type(extension), nullptr)));
            if (wpe_display_connect(display.get(), nullptr)) {
                s_defaultDisplay = WTFMove(display);
                return;
            }
        }
    });
    return s_defaultDisplay.get();
}

/**
 * wpe_display_get_primary:
 *
 * Get the primary display. By default, the first #WPEDisplay that is
 * created is set as primary display. This is the desired behavior in
 * most of the cases, but you can set any #WPEDisplay as primary
 * calling wpe_display_set_primary() if needed.
 *
 * Returns: (nullable) (transfer none): the primary display or %NULL
 */
WPEDisplay* wpe_display_get_primary()
{
    return s_primaryDisplay.get();
}

/**
 * wpe_display_set_primary:
 * @display: a #WPEDisplay
 *
 * Set @display as the primary display.
 *
 * In most of the cases you don't need to call this, because
 * the first #WPEDisplay that is created is set as primary
 * automatically.
 */
void wpe_display_set_primary(WPEDisplay* display)
{
    s_primaryDisplay.reset(display);
}

/**
 * wpe_display_connect
 * @display: a #WPEDisplay
 * @error: return location for error or %NULL to ignore
 *
 * Connect the display to the native system.
 *
 * Returns: %TRUE if connection succeeded, or %FALSE in case of error.
 */
gboolean wpe_display_connect(WPEDisplay* display, GError** error)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), FALSE);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->connect(display, error);
}

/**
 * wpe_display_disconnected:
 * @display: a #WPEDisplay
 * @error: (nullable): a #GError or %NULL
 *
 * Emit the signal #WPEDisplay::disconnected with the given @error
 *
 * This function should only be called by platform implementations.
 */
void wpe_display_disconnected(WPEDisplay* display, GError* error)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));
    g_return_if_fail(!error || g_error_matches(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_LOST));

    g_signal_emit(display, signals[DISCONNECTED], 0, error);
}

/**
 * wpe_display_get_egl_display: (skip)
 * @display: a #WPEDisplay
 * @error: return location for error or %NULL to ignore
 *
 * Get the `EGLDisplay` of @display
 *
 * Retrurns: (transfer none) (nullable): a `EGLDisplay` or %NULL
 */
gpointer wpe_display_get_egl_display(WPEDisplay* display, GError** error)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->eglInitialized) {
        priv->eglInitialized = true;

        auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
        if (!wpeDisplayClass->get_egl_display) {
            g_set_error_literal(&priv->eglDisplayError.outPtr(), WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "Operation not supported");
            if (error)
                *error = g_error_copy(priv->eglDisplayError.get());
            return nullptr;
        }
        auto* eglDisplay = wpeDisplayClass->get_egl_display(display, &priv->eglDisplayError.outPtr());
        if (!eglDisplay) {
            if (error)
                *error = g_error_copy(priv->eglDisplayError.get());
            return nullptr;
        }

        if (!eglInitialize(eglDisplay, nullptr, nullptr)) {
            g_set_error(&priv->eglDisplayError.outPtr(), WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: eglInitialize failed with error %#04x", eglGetError());
            if (error)
                *error = g_error_copy(priv->eglDisplayError.get());
            return nullptr;
        }

        priv->eglDisplay = eglDisplay;
    }

    if (error && priv->eglDisplayError)
        *error = g_error_copy(priv->eglDisplayError.get());
    return priv->eglDisplay;
}

/**
 * wpe_display_get_keymap:
 * @display: a #WPEDisplay
 *
 * Get the #WPEKeymap of @display
 *
 * As a fallback, a #WPEKeymapXKB for the pc105 "US" layout is returned if the actual display
 * implementation does not provide a keymap itself.
 *
 * Returns: (transfer none): a #WPEKeymap
 */
WPEKeymap* wpe_display_get_keymap(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->keymap) {
        auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
        if (wpeDisplayClass->get_keymap)
            priv->keymap = wpeDisplayClass->get_keymap(display);

        if (!priv->keymap)
            priv->keymap = adoptGRef(wpe_keymap_xkb_new());
    }
    return priv->keymap.get();
}

/**
 * wpe_display_get_clipboard:
 * @display: a #WPEDisplay
 *
 * Get the #WPEClipboard of @display. If the platform doesn't
 * support clipboard, a local #WPEClipboard is created.
 *
 * Returns: (transfer none): a #WPEClipboard
 */
WPEClipboard* wpe_display_get_clipboard(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->clipboard) {
        auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
        if (wpeDisplayClass->get_clipboard)
            priv->clipboard = wpeDisplayClass->get_clipboard(display);

        if (!priv->clipboard)
            priv->clipboard = adoptGRef(wpe_clipboard_new(display));
    }
    return priv->clipboard.get();
}

/**
 * wpe_display_get_settings:
 * @display: a #WPEDisplay
 *
 * Get the #WPESettings of @display
 *
 * Returns: (transfer none): a #WPESettings
 */
WPESettings* wpe_display_get_settings(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->settings)
        priv->settings = adoptGRef(WPE_SETTINGS(g_object_new(WPE_TYPE_SETTINGS, nullptr)));

    return priv->settings.get();
}

#if USE(LIBDRM)
static GRefPtr<WPEBufferDMABufFormats> wpeDisplayPreferredDMABufFormats(WPEDisplay* display)
{
    auto eglDisplay = static_cast<EGLDisplay>(wpe_display_get_egl_display(display, nullptr));
    if (!eglDisplay)
        return nullptr;

    if (!wpeDisplayCheckEGLExtension(display, "EXT_image_dma_buf_import"))
        return nullptr;

    // Epoxy requires a current context for the symbol resolver to work automatically.
    static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
    if (!s_eglQueryDmaBufFormatsEXT)
        return nullptr;

    EGLint formatsCount;
    if (!s_eglQueryDmaBufFormatsEXT(eglDisplay, 0, nullptr, &formatsCount) || !formatsCount)
        return nullptr;

    Vector<EGLint> formats(formatsCount);
    if (!s_eglQueryDmaBufFormatsEXT(eglDisplay, formatsCount, reinterpret_cast<EGLint*>(formats.mutableSpan().data()), &formatsCount))
        return nullptr;

    static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT = wpeDisplayCheckEGLExtension(display, "EXT_image_dma_buf_import_modifiers") ?
        reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT")) : nullptr;

    auto modifiersForFormat = [&](EGLint format) -> Vector<EGLuint64KHR> {
        if (!s_eglQueryDmaBufModifiersEXT)
            return { DRM_FORMAT_MOD_INVALID };

        EGLint modifiersCount;
        if (!s_eglQueryDmaBufModifiersEXT(eglDisplay, format, 0, nullptr, nullptr, &modifiersCount) || !modifiersCount)
            return { DRM_FORMAT_MOD_INVALID };

        Vector<EGLuint64KHR> modifiers(modifiersCount);
        if (!s_eglQueryDmaBufModifiersEXT(eglDisplay, format, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.mutableSpan().data()), nullptr, &modifiersCount))
            return { DRM_FORMAT_MOD_INVALID };

        return modifiers;
    };

    auto* builder = wpe_buffer_dma_buf_formats_builder_new(wpe_display_get_drm_device(display));
    wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING);
    for (auto format : formats) {
        auto modifiers = modifiersForFormat(format);
        for (auto modifier : modifiers)
            wpe_buffer_dma_buf_formats_builder_append_format(builder, format, modifier);
    }
    return adoptGRef(wpe_buffer_dma_buf_formats_builder_end(builder));
}
#endif

/**
 * wpe_display_get_preferred_dma_buf_formats:
 * @display: a #WPEDisplay
 *
 * Get the list of preferred DMA-BUF buffer formats for @display.
 *
 * Returns: (transfer none) (nullable): a #WPEBufferDMABufFormats
 */
WPEBufferDMABufFormats* wpe_display_get_preferred_dma_buf_formats(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->preferredDMABufFormats) {
        auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
        if (wpeDisplayClass->get_preferred_dma_buf_formats)
            priv->preferredDMABufFormats = adoptGRef(wpeDisplayClass->get_preferred_dma_buf_formats(display));

#if USE(LIBDRM)
        if (!priv->preferredDMABufFormats)
            priv->preferredDMABufFormats = wpeDisplayPreferredDMABufFormats(display);
#endif
    }

    return display->priv->preferredDMABufFormats.get();
}

/**
 * wpe_display_get_n_screens:
 * @display: a #WPEDisplay
 *
 * Get the number of screens of @display
 *
 * Returns: the number of screens
 */
guint wpe_display_get_n_screens(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), 0);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    if (!wpeDisplayClass->get_n_screens)
        return 0;

    return wpeDisplayClass->get_n_screens(display);
}

/**
 * wpe_display_get_screen:
 * @display: a #WPEDisplay
 * @index: the number of the screen
 *
 * Get the screen of @display at @index
 *
 * Returns: (transfer none) (nullable): a #WPEScreen, or %NULL
 */
WPEScreen* wpe_display_get_screen(WPEDisplay* display, guint index)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    if (!wpeDisplayClass->get_screen)
        return nullptr;

    return wpeDisplayClass->get_screen(display, index);
}

/**
 * wpe_display_screen_added:
 * @display: a #WPEDisplay
 * @screen: the #WPEScreen added
 *
 * Emit the signal #WPEDisplay::screen-added.
 */
void wpe_display_screen_added(WPEDisplay* display, WPEScreen* screen)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));
    g_return_if_fail(WPE_IS_SCREEN(screen));

    g_signal_emit(display, signals[SCREEN_ADDED], 0, screen);
}

/**
 * wpe_display_screen_removed:
 * @display: a #WPEDisplay
 * @screen: the #WPEScreen removed
 *
 * Emit the signal #WPEDisplay::screen-removed.
 * Note that wpe_screen_invalidate() is called before the signal is emitted.
 */
void wpe_display_screen_removed(WPEDisplay* display, WPEScreen* screen)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));
    g_return_if_fail(WPE_IS_SCREEN(screen));

    wpe_screen_invalidate(screen);
    g_signal_emit(display, signals[SCREEN_REMOVED], 0, screen);
}

static bool isSotfwareRast()
{
    static bool swrast;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const char* envAlwaysSoftware = getenv("LIBGL_ALWAYS_SOFTWARE");
        if (envAlwaysSoftware
            && (!strcmp(envAlwaysSoftware, "1")
                || !strcasecmp(envAlwaysSoftware, "y")
                || !strcasecmp(envAlwaysSoftware, "yes")
                || !strcasecmp(envAlwaysSoftware, "t")
                || !strcasecmp(envAlwaysSoftware, "true"))) {
            swrast = true;
        }
    });
    return swrast;
}

/**
 * wpe_display_get_drm_device:
 * @display: a #WPEDisplay
 *
 * Get the DRM device of @display. This is the main device that
 * will be used to initialize the EGL display and allocate GBM
 * buffers by default. The DRM device required to allocate GBM
 * buffers for direct scanout will be set as main or group device
 * in #WPEBufferDMABufFormats.
 *
 * Returns: (transfer none) (nullable): a #WPEDRMDevice or %NULL
 */
WPEDRMDevice* wpe_display_get_drm_device(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    if (isSotfwareRast())
        return nullptr;

    if (display->priv->overridenDRMDevice)
        return display->priv->overridenDRMDevice.get();

    static const char* envDeviceFile = getenv("WPE_DRM_DEVICE");
    if (envDeviceFile && *envDeviceFile) {
        display->priv->overridenDRMDevice = wpeDRMDeviceCreateForDevice(envDeviceFile);
        if (display->priv->overridenDRMDevice)
            return display->priv->overridenDRMDevice.get();

        g_warning("Invalid device %s set in WPE_DRM_DEVICE, ignoring...", envDeviceFile);
    }

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->get_drm_device ? wpeDisplayClass->get_drm_device(display) : nullptr;
}

/**
 * wpe_display_use_explicit_sync:
 * @display: a #WPEDisplay
 *
 * Get whether explicit sync should be used with @display for
 * supported buffers.
 *
 * Returns: %TRUE if explicit sync should be used, or %FALSE otherwise
 */
gboolean wpe_display_use_explicit_sync(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), FALSE);

    static const char* envExplicitSync = getenv("WPE_USE_EXPLICIT_SYNC");
    if (envExplicitSync && !strcmp(envExplicitSync, "0"))
        return false;

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->use_explicit_sync ? wpeDisplayClass->use_explicit_sync(display) : FALSE;
}

/**
 * wpe_display_get_available_input_devices:
 * @display: a #WPEDisplay
 *
 * Get the available input devices of @display that can be used by creators to adjust their UI based on the available interactions.
 *
 * Returns: a #WPEAvailableInputDevices
 */
WPEAvailableInputDevices wpe_display_get_available_input_devices(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), WPE_AVAILABLE_INPUT_DEVICE_NONE);

    return display->priv->availableInputDevices;
}

/**
 * wpe_display_set_available_input_devices:
 * @display: a #WPEDisplay
 * @devices: a #WPEAvailableInputDevices
 *
 * Sets the available input devices for a @display.
 *
 * This function should only be called by platform implementations.
 */
void wpe_display_set_available_input_devices(WPEDisplay* display, WPEAvailableInputDevices devices)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));

    if (display->priv->availableInputDevices == devices)
        return;

    display->priv->availableInputDevices = devices;
    g_object_notify_by_pspec(G_OBJECT(display), sObjProperties[PROP_AVAILABLE_INPUT_DEVICES]);
}

/**
 * wpe_display_create_gamepad_manager:
 * @display: a #WPEDisplay
 *
 * Create a #WPEGamepadManager to handle gamepads
 *
 * Returns: (transfer full) (nullable): a new #WPEGamepadManager or %NULL if not supported
 */
WPEGamepadManager* wpe_display_create_gamepad_manager(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    auto* manager = wpeDisplayClass->create_gamepad_manager ? wpeDisplayClass->create_gamepad_manager(display) : nullptr;
#if USE(MANETTE)
    if (!manager)
        manager = wpeGamepadManagerManetteCreate();
#endif
    return manager;
}
