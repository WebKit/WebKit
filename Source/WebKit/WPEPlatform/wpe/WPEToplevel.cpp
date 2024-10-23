/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WPEToplevel.h"

#include "WPEBufferDMABufFormats.h"
#include "WPEDisplay.h"
#include "WPEViewPrivate.h"
#include <wtf/HashSet.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

/**
 * WPEToplevel:
 *
 */
struct _WPEToplevelPrivate {
    GWeakPtr<WPEDisplay> display;
    HashSet<WPEView*> views;

    int width;
    int height;
    gdouble scale { 1 };
    WPEToplevelState state;
    bool closed;
#if USE(LIBDRM)
    GRefPtr<WPEBufferDMABufFormats> overridenDMABufFormats;
#endif
};

WEBKIT_DEFINE_TYPE(WPEToplevel, wpe_toplevel, G_TYPE_OBJECT)

enum {
    PROP_0,

    PROP_DISPLAY,

    N_PROPERTIES
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

static void wpeToplevelSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* toplevel = WPE_TOPLEVEL(object);

    switch (propId) {
    case PROP_DISPLAY:
        toplevel->priv->display.reset(WPE_DISPLAY(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeToplevelGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* toplevel = WPE_TOPLEVEL(object);

    switch (propId) {
    case PROP_DISPLAY:
        g_value_set_object(value, wpe_toplevel_get_display(toplevel));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpe_toplevel_class_init(WPEToplevelClass* toplevelClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(toplevelClass);
    objectClass->set_property = wpeToplevelSetProperty;
    objectClass->get_property = wpeToplevelGetProperty;

    /**
     * WPEToplevel:display:
     *
     * The #WPEDisplay of the toplevel.
     */
    sObjProperties[PROP_DISPLAY] =
        g_param_spec_object(
            "display",
            nullptr, nullptr,
            WPE_TYPE_DISPLAY,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
}

void wpeToplevelAddView(WPEToplevel* toplevel, WPEView* view)
{
    toplevel->priv->views.add(view);
}

void wpeToplevelRemoveView(WPEToplevel* toplevel, WPEView* view)
{
    toplevel->priv->views.remove(view);
}

/**
 * wpe_toplevel_get_display:
 * @toplevel: a #WPEToplevel
 *
 * Get the #WPEDisplay of @toplevel
 *
 * Returns: (transfer none) (nullable): a #WPEDisplay or %NULL
 */
WPEDisplay* wpe_toplevel_get_display(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), nullptr);

    return toplevel->priv->display.get();
}

/**
 * wpe_toplevel_set_title:
 * @toplevel: a #WPEToplevel
 * @title: (nullable): title to set, or %NULL
 *
 * Set the @toplevel title
 */
void wpe_toplevel_set_title(WPEToplevel* toplevel, const char* title)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    if (toplevelClass->set_title)
        toplevelClass->set_title(toplevel, title);
}

/**
 * wpe_toplevel_get_max_views:
 * @toplevel: a #WPEToplevel
 *
 * Get the maximum number of #WPEView that @toplevel can contain.
 *
 * Returns: the maximum number of views supported.
 */
guint wpe_toplevel_get_max_views(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), 0);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->get_max_views ? toplevelClass->get_max_views(toplevel) : 1;
}

/**
 * wpe_toplevel_get_n_views:
 * @toplevel: a #WPEToplevel
 *
 * Get the number of #WPEView contained by @toplevel
 *
 * Returns: the number of view in @toplevel
 */
guint wpe_toplevel_get_n_views(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), 0);

    return toplevel->priv->views.size();
}

/**
 * wpe_toplevel_foreach_view:
 * @toplevel: a #WPEToplevel
 * @func: (scope call): the function to call for each #WPEView
 * @user_data: user data to pass to the function
 *
 * Call @func for each #WPEView of @toplevel
 */
void wpe_toplevel_foreach_view(WPEToplevel* toplevel, WPEToplevelForeachViewFunc func, gpointer userData)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views)) {
        if (func(toplevel, view.get(), userData))
            return;
    }
}

/**
 * wpe_toplevel_closed:
 * @toplevel: a #WPEToplevel
 *
 * Set @toplevel as closed if not already closed.
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_closed(WPEToplevel* toplevel)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    if (toplevel->priv->closed)
        return;

    toplevel->priv->closed = true;
    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views))
        wpe_view_closed(view.get());
}

/**
 * wpe_toplevel_get_size:
 * @toplevel: a #WPEToplevel
 * @width: (out) (nullable): return location for width, or %NULL
 * @height: (out) (nullable): return location for width, or %NULL
 *
 * Get the @vtoplevel size in logical coordinates
 */
void wpe_toplevel_get_size(WPEToplevel* toplevel, int* width, int* height)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    if (width)
        *width = toplevel->priv->width;
    if (height)
        *height = toplevel->priv->height;
}

/**
 * wpe_toplevel_resize:
 * @toplevel: a #WPEToplevel
 * @width: width in logical coordinates
 * @height: height in logical coordinates
 *
 * Request that the @toplevel is resized at @width x @height.
 *
 * Returns: %TRUE if resizing is supported and given dimensions are
 *    different than current size, otherwise %FALSE
 */
gboolean wpe_toplevel_resize(WPEToplevel* toplevel, int width, int height)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* priv = toplevel->priv;
    if (priv->width == width && priv->height == height)
        return FALSE;

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->resize ? toplevelClass->resize(toplevel, width, height) : FALSE;
}

/**
 * wpe_toplevel_resized:
 * @toplevel: a #WPEToplevel
 * @width: width in logical coordinates
 * @height: height in logical coordinates
 *
 * Update @toplevel size.
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_resized(WPEToplevel* toplevel, int width, int height)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    auto* priv = toplevel->priv;
    priv->width = width;
    priv->height = height;
}

/**
 * wpe_toplevel_get_state:
 * @toplevel: a #WPEToplevel
 *
 * Get the current state of @toplevel
 *
 * Returns: a #WPEToplevelState
 */
WPEToplevelState wpe_toplevel_get_state(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), WPE_TOPLEVEL_STATE_NONE);

    return toplevel->priv->state;
}

/**
 * wpe_toplevel_state_changed:
 * @toplevel: a #WPEToplevel
 * @state: a set of #WPEToplevelState
 *
 * Update the current state of @toplevel
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_state_changed(WPEToplevel* toplevel, WPEToplevelState state)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    if (toplevel->priv->state == state)
        return;

    toplevel->priv->state = state;
    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views))
        wpeViewToplevelStateChanged(view.get(), state);
}

/**
 * wpe_toplevel_get_scale:
 * @toplevel: a #WPEToplevel
 *
 * Get the @toplevel scale
 *
 * Returns: the toplevel scale
 */
gdouble wpe_toplevel_get_scale(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), 1.);

    return toplevel->priv->scale;
}

/**
 * wpe_toplevel_scale_changed:
 * @toplevel: a #WPEToplevel
 * @scale: the new scale
 *
 * Update the @toplevel scale.
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_scale_changed(WPEToplevel* toplevel, gdouble scale)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));
    g_return_if_fail(scale > 0);

    if (toplevel->priv->scale == scale)
        return;

    toplevel->priv->scale = scale;
    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views))
        wpeViewScaleChanged(view.get(), scale);
}

/**
 * wpe_toplevel_get_screen:
 * @toplevel: a #WPEToplevel
 *
 * Get current #WPEScreen of @toplevel
 *
 * Returns: (transfer none) (nullable): a #WPEScreen, or %NULL
 */
WPEScreen* wpe_toplevel_get_screen(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), nullptr);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->get_screen ? toplevelClass->get_screen(toplevel) : nullptr;
}

/**
 * wpe_toplevel_screen_changed:
 * @toplevel: a #WPEToplevel
 *
 * Notify that @toplevel screen has changed.
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_screen_changed(WPEToplevel* toplevel)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views))
        wpeViewScreenChanged(view.get());
}

/**
 * wpe_toplevel_fullscreen:
 * @toplevel: a #WPEToplevel
 *
 * Request that the @toplevel goes into a fullscreen state.
 *
 * Returns: %TRUE if fullscreen is supported, otherwise %FALSE
 */
gboolean wpe_toplevel_fullscreen(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->set_fullscreen ? toplevelClass->set_fullscreen(toplevel, TRUE) : FALSE;
}

/**
 * wpe_toplevel_unfullscreen:
 * @toplevel: a #WPEToplevel
 *
 * Request that the @toplevel leaves a fullscreen state.
 *
 * Returns: %TRUE if unfullscreen is supported, otherwise %FALSE
 */
gboolean wpe_toplevel_unfullscreen(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->set_fullscreen ? toplevelClass->set_fullscreen(toplevel, FALSE) : FALSE;
}

/**
 * wpe_toplevel_maximize:
 * @toplevel: a #WPEToplevel
 *
 * Request that the @toplevel is maximized. If the toplevel is already maximized this function
 * does nothing.
 *
 * Returns: %TRUE if maximize is supported, otherwise %FALSE
 */
gboolean wpe_toplevel_maximize(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->set_maximized ? toplevelClass->set_maximized(toplevel, TRUE) : FALSE;
}

/**
 * wpe_toplevel_unmaximize:
 * @toplevel: a #WPEToplevel
 *
 * Request that the @toplevel is unmaximized. If the toplevel is not maximized this function
 * does nothing.
 *
 * Returns: %TRUE if maximize is supported, otherwise %FALSE
 */
gboolean wpe_toplevel_unmaximize(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->set_maximized ? toplevelClass->set_maximized(toplevel, FALSE) : FALSE;
}

/**
 * wpe_toplevel_minimize:
 * @toplevel: a #WPEToplevel
 *
 * Request that the @toplevel is minimized.
 *
 * Returns: %TRUE if minimize is supported, otherwise %FALSE
 */
gboolean wpe_toplevel_minimize(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), FALSE);

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    return toplevelClass->set_minimized ? toplevelClass->set_minimized(toplevel) : FALSE;
}

/**
 * wpe_toplevel_get_preferred_dma_buf_formats:
 * @toplevel: a #WPEToplevel
 *
 * Get the list of preferred DMA-BUF buffer formats for @toplevel.
 *
 * Returns: (transfer none) (nullable): a #WPEBufferDMABufFormats
 */
WPEBufferDMABufFormats* wpe_toplevel_get_preferred_dma_buf_formats(WPEToplevel* toplevel)
{
    g_return_val_if_fail(WPE_IS_TOPLEVEL(toplevel), nullptr);

    auto* priv = toplevel->priv;
#if USE(LIBDRM)
    if (priv->overridenDMABufFormats)
        return priv->overridenDMABufFormats.get();

    const char* formatString = getenv("WPE_DMABUF_BUFFER_FORMAT");
    if (formatString && *formatString) {
        auto tokens = String::fromUTF8(formatString).split(':');
        if (!tokens.isEmpty() && tokens[0].length() >= 2 && tokens[0].length() <= 4) {
            guint32 format = fourcc_code(tokens[0][0], tokens[0][1], tokens[0].length() > 2 ? tokens[0][2] : ' ', tokens[0].length() > 3 ? tokens[0][3] : ' ');
            char* endptr = nullptr;
            guint64 modifier = tokens.size() > 1 ? g_ascii_strtoull(tokens[1].ascii().data(), &endptr, 16) : DRM_FORMAT_MOD_INVALID;
            if (!(modifier == G_MAXUINT64 && errno == ERANGE) && !(!modifier && !endptr)) {
                WPEBufferDMABufFormatUsage usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
                if (tokens.size() > 2) {
                    if (tokens[2] == "rendering"_s)
                        usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
                    else if (tokens[2] == "mapping"_s)
                        usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING;
                    else if (tokens[2] == "scanout"_s)
                        usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT;
                }
                auto* builder = wpe_buffer_dma_buf_formats_builder_new(priv->display ? wpe_display_get_drm_render_node(priv->display.get()) : nullptr);
                wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, usage);
                wpe_buffer_dma_buf_formats_builder_append_format(builder, format, modifier);
                priv->overridenDMABufFormats = adoptGRef(wpe_buffer_dma_buf_formats_builder_end(builder));
                return priv->overridenDMABufFormats.get();
            }
        }

        WTFLogAlways("Invalid format %s set in WPE_DMABUF_BUFFER_FORMAT, ignoring...", formatString);
    }
#endif

    auto* toplevelClass = WPE_TOPLEVEL_GET_CLASS(toplevel);
    if (toplevelClass->get_preferred_dma_buf_formats)
        return toplevelClass->get_preferred_dma_buf_formats(toplevel);

    return priv->display ? wpe_display_get_preferred_dma_buf_formats(priv->display.get()) : nullptr;
}

/**
 * wpe_toplevel_preferred_dma_buf_formats_changed:
 * @toplevel: a #WPEToplevel
 *
 * Notify that @toplevel preferred DMA-BUF formats have changed.
 *
 * This function should only be called by #WPEToplevel derived classes
 * in platform implementations.
 */
void wpe_toplevel_preferred_dma_buf_formats_changed(WPEToplevel* toplevel)
{
    g_return_if_fail(WPE_IS_TOPLEVEL(toplevel));

    for (auto& view : copyToVectorOf<GRefPtr<WPEView>>(toplevel->priv->views))
        wpeViewPreferredDMABufFormatsChanged(view.get());
}
