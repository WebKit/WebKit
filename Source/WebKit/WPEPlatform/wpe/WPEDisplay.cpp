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

#include "WPEBufferDMABufFormat.h"
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
    GList* preferredDMABufFormats;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEDisplay, wpe_display, G_TYPE_OBJECT)

/**
 * wpe_display_error_quark:
 *
 * Gets the WPEDisplay Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK(wpe-display-error-quark, wpe_display_error)

static void wpeDisplayDispose(GObject* object)
{
    auto* priv = WPE_DISPLAY(object)->priv;

    g_clear_list(&priv->preferredDMABufFormats, reinterpret_cast<GDestroyNotify>(wpe_buffer_dma_buf_format_free));
    g_clear_pointer(&priv->eglDisplay, eglTerminate);

    G_OBJECT_CLASS(wpe_display_parent_class)->dispose(object);
}

static void wpe_display_class_init(WPEDisplayClass* displayClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayClass);
    objectClass->dispose = wpeDisplayDispose;
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
static GList* wpeDisplayPreferredDMABufFormats(WPEDisplay* display)
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

    GList* preferredFormats = nullptr;
    for (auto format : formats) {
        GRefPtr<GArray> dmabufModifiers = adoptGRef(g_array_sized_new(FALSE, TRUE, sizeof(guint64), 1));
        guint64 invalidModifier = DRM_FORMAT_MOD_INVALID;
        g_array_append_val(dmabufModifiers.get(), invalidModifier);
        if (s_eglQueryDmaBufModifiersEXT) {
            EGLint modifiersCount;
            if (s_eglQueryDmaBufModifiersEXT(eglDisplay, format, 0, nullptr, nullptr, &modifiersCount) && modifiersCount) {
                Vector<EGLuint64KHR> modifiers(modifiersCount);
                if (s_eglQueryDmaBufModifiersEXT(eglDisplay, format, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.data()), nullptr, &modifiersCount)) {
                    g_array_set_size(dmabufModifiers.get(), modifiersCount);
                    for (int i = 0; i < modifiersCount; ++i) {
                        guint64* modifier = &g_array_index(dmabufModifiers.get(), guint64, i);
                        *modifier = modifiers[i];
                    }
                }
            }
        }
        preferredFormats = g_list_prepend(preferredFormats, wpe_buffer_dma_buf_format_new(WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING, static_cast<guint32>(format), dmabufModifiers.get()));
    }
    return g_list_reverse(preferredFormats);
}
#endif

/**
 * wpe_display_get_preferred_dma_buf_formats:
 * @display: a #WPEDisplay
 *
 * Get the list of preferred DMA-BUF buffer formats for @display.
 *
 * Returns: (transfer none) (element-type WPEBufferDMABufFormat) (nullable): a #GList of #WPEBufferDMABufFormat
 */
GList* wpe_display_get_preferred_dma_buf_formats(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    auto* priv = display->priv;
    if (!priv->preferredDMABufFormats) {
        auto* wpeDisplayClass = WPE_DISPLAY_GET_CLASS(display);
        if (wpeDisplayClass->get_preferred_dma_buf_formats)
            priv->preferredDMABufFormats = wpeDisplayClass->get_preferred_dma_buf_formats(display);

#if USE(LIBDRM)
        if (!priv->preferredDMABufFormats)
            priv->preferredDMABufFormats = wpeDisplayPreferredDMABufFormats(display);
#endif
    }

    return display->priv->preferredDMABufFormats;
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

static CString& renderNodeFile()
{
    static NeverDestroyed<CString> renderNode;
    return renderNode.get();
}

#if USE(LIBDRM)
static const CString wpeDRMRenderNodeFile()
{
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0)
        return { };

    CString renderNodeDeviceFile;
    for (int i = 0; i < numDevices && renderNodeDeviceFile.isNull(); ++i) {
        drmDevice* device = devices[i];
        if (!(device->available_nodes & (1 << DRM_NODE_RENDER)))
            continue;

        renderNodeDeviceFile = device->nodes[DRM_NODE_RENDER];
    }
    drmFreeDevices(devices, numDevices);

    return renderNodeDeviceFile;
}
#endif

/**
 * wpe_render_node_device:
 *
 * Get the render node device to be used for rendering.
 *
 * Returns: (transfer none) (nullable): a render node device, or %NULL.
 */
const char* wpe_render_node_device()
{
    auto& renderNode = renderNodeFile();
    if (renderNode.isNull() && !isSotfwareRast()) {
        const char* envDeviceFile = getenv("WPE_RENDER_NODE_DEVICE");
        if (envDeviceFile && *envDeviceFile)
            renderNode = envDeviceFile;
#if USE(LIBDRM)
        else
            renderNode = wpeDRMRenderNodeFile();
#endif
    }
    return renderNode.data();
}

static CString& renderDeviceFile()
{
    static NeverDestroyed<CString> renderDevice;
    return renderDevice.get();
}

#if USE(LIBDRM)
static const CString wpeDRMRenderDeviceFile()
{
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0)
        return { };

    CString renderDeviceFile;
    for (int i = 0; i < numDevices && renderDeviceFile.isNull(); ++i) {
        drmDevice* device = devices[i];
        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY | 1 << DRM_NODE_RENDER)))
            continue;

        renderDeviceFile = device->nodes[DRM_NODE_PRIMARY];
    }
    drmFreeDevices(devices, numDevices);

    return renderDeviceFile;
}
#endif

/**
 * wpe_render_device:
 *
 * Get the render device to be used for rendering.
 *
 * Returns: (transfer none) (nullable): a render device, or %NULL.
 */
const char* wpe_render_device()
{
    auto& renderDevice = renderDeviceFile();
    if (renderDevice.isNull() && !isSotfwareRast()) {
        const char* envDeviceFile = getenv("WPE_RENDER_DEVICE");
        if (envDeviceFile && *envDeviceFile)
            renderDevice = envDeviceFile;
#if USE(LIBDRM)
        else
            renderDevice = wpeDRMRenderDeviceFile();
#endif
    }
    return renderDevice.data();
}
