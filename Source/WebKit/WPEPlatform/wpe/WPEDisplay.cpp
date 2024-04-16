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

#include "WPEDisplayPrivate.h"
#include "WPEEGLError.h"
#include "WPEExtensions.h"
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
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEDisplay, wpe_display, G_TYPE_OBJECT)

enum {
    MONITOR_ADDED,
    MONITOR_REMOVED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

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

    g_clear_pointer(&priv->eglDisplay, eglTerminate);

    G_OBJECT_CLASS(wpe_display_parent_class)->dispose(object);
}

static void wpe_display_class_init(WPEDisplayClass* displayClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayClass);
    objectClass->constructed = wpeDisplayConstructed;
    objectClass->dispose = wpeDisplayDispose;

    /**
     * WPEDisplay::monitor-added:
     * @display: a #WPEDisplay
     * @monitor: the #WPEMonitor added
     *
     * Emitted when a monitor is added
     */
    signals[MONITOR_ADDED] = g_signal_new(
        "monitor-added",
        G_TYPE_FROM_CLASS(displayClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_MONITOR);

    /**
     * WPEDisplay::monitor-removed:
     * @display: a #WPEDisplay
     * @monitor: the #WPEMonitor removed
     *
     * Emitted after a monitor is removed.
     * Note that the monitor is always invalidated before this signal is emitted.
     */
    signals[MONITOR_REMOVED] = g_signal_new(
        "monitor-removed",
        G_TYPE_FROM_CLASS(displayClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_MONITOR);
}

WPEView* wpeDisplayCreateView(WPEDisplay* display)
{
    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->create_view ? wpeDisplayClass->create_view(display) : nullptr;
}

bool wpeDisplayCheckEGLExtension(WPEDisplay* display, const char* extensionName)
{
    auto addResult = display->priv->extensionsMap.ensure(ASCIILiteral::fromLiteralUnsafe(extensionName), [&] {
        auto* eglDisplay = wpe_display_get_egl_display(display, nullptr);
        return eglDisplay ? epoxy_has_egl_extension(eglDisplay, extensionName) : false;
    });
    return addResult.iterator->value;
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
                g_warning("Failed to connect to display of type %s: %s", extensionName, error->message);
            } else
                g_warning("Display of type %s was not found", extensionName);
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
 * @error: return location for error or %NULL to ignore
 *
 * Get the #WPEKeymap of @display
 *
 * Returns: (transfer none): a #WPEKeymap or %NULL in case of error
 */
WPEKeymap* wpe_display_get_keymap(WPEDisplay* display, GError** error)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    if (!wpeDisplayClass->get_keymap) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_NOT_SUPPORTED, "Operation not supported");
        return nullptr;
    }

    return wpeDisplayClass->get_keymap(display, error);
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
    if (!s_eglQueryDmaBufFormatsEXT(eglDisplay, formatsCount, reinterpret_cast<EGLint*>(formats.data()), &formatsCount))
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
        if (!s_eglQueryDmaBufModifiersEXT(eglDisplay, format, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.data()), nullptr, &modifiersCount))
            return { DRM_FORMAT_MOD_INVALID };

        return modifiers;
    };

    auto* builder = wpe_buffer_dma_buf_formats_builder_new(wpe_display_get_drm_render_node(display));
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
 * wpe_display_get_n_monitors:
 * @display: a #WPEDisplay
 *
 * Get the number of monitors of @display
 *
 * Returns: the number of monitors
 */
guint wpe_display_get_n_monitors(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), 0);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    if (!wpeDisplayClass->get_n_monitors)
        return 0;

    return wpeDisplayClass->get_n_monitors(display);
}

/**
 * wpe_display_get_monitor:
 * @display: a #WPEDisplay
 * @index: the number of the monitor
 *
 * Get the monitor of @display at @index
 *
 * Returns: (transfer none) (nullable): a #WPEMonitor, or %NULL
 */
WPEMonitor* wpe_display_get_monitor(WPEDisplay* display, guint index)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    if (!wpeDisplayClass->get_monitor)
        return nullptr;

    return wpeDisplayClass->get_monitor(display, index);
}

/**
 * wpe_display_monitor_added:
 * @display: a #WPEDisplay
 * @monitor: the #WPEMonitor added
 *
 * Emit the signal #WPEDisplay::monitor-added.
 */
void wpe_display_monitor_added(WPEDisplay* display, WPEMonitor* monitor)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));
    g_return_if_fail(WPE_IS_MONITOR(monitor));

    g_signal_emit(display, signals[MONITOR_ADDED], 0, monitor);
}

/**
 * wpe_display_monitor_removed:
 * @display: a #WPEDisplay
 * @monitor: the #WPEMonitor removed
 *
 * Emit the signal #WPEDisplay::monitor-removed.
 * Note that wpe_monitor_invalidate() is called before the signal is emitted.
 */
void wpe_display_monitor_removed(WPEDisplay* display, WPEMonitor* monitor)
{
    g_return_if_fail(WPE_IS_DISPLAY(display));
    g_return_if_fail(WPE_IS_MONITOR(monitor));

    wpe_monitor_invalidate(monitor);
    g_signal_emit(display, signals[MONITOR_REMOVED], 0, monitor);
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
 * Get the DRM device of @display.
 *
 * Returns: (transfer none) (nullable): the filename of the DRM device node, or %NULL
 */
const char* wpe_display_get_drm_device(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    if (isSotfwareRast())
        return nullptr;

    static const char* envDeviceFile = getenv("WPE_DRM_DEVICE");
    if (envDeviceFile && *envDeviceFile)
        return envDeviceFile;

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->get_drm_device ? wpeDisplayClass->get_drm_device(display) : nullptr;
}

/**
 * wpe_display_get_drm_render_node:
 * @display: a #WPEDisplay
 *
 * Get the DRM render node of @display.
 *
 * Returns: (transfer none) (nullable): the filename of the DRM render node, or %NULL
 */
const char* wpe_display_get_drm_render_node(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    if (isSotfwareRast())
        return nullptr;

    static const char* envDeviceFile = getenv("WPE_DRM_RENDER_NODE");
    if (envDeviceFile && *envDeviceFile)
        return envDeviceFile;

    auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
    return wpeDisplayClass->get_drm_render_node ? wpeDisplayClass->get_drm_render_node(display) : nullptr;
}
