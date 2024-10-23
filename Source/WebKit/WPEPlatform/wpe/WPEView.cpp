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
#include "WPEView.h"

#include "WPEBuffer.h"
#include "WPEBufferDMABufFormats.h"
#include "WPEDisplayPrivate.h"
#include "WPEEnumTypes.h"
#include "WPEEvent.h"
#include "WPEGestureControllerImpl.h"
#include "WPEToplevelPrivate.h"
#include "WPEViewPrivate.h"
#include <optional>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEView:
 *
 */
struct _WPEViewPrivate {
    GRefPtr<WPEDisplay> display;
    GRefPtr<WPEToplevel> toplevel;
    int width;
    int height;
    gdouble scale { 1 };
    WPEToplevelState state;
    bool closed;
    bool visible { true };
    bool mapped;
    bool hasFocus;

    struct {
        unsigned pressCount { 0 };
        gdouble x { 0 };
        gdouble y { 0 };
        guint button { 0 };
        guint32 time { 0 };
    } lastButtonPress;
    std::optional<GRefPtr<WPEGestureController>> gestureController;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEView, wpe_view, G_TYPE_OBJECT)

/**
 * wpe_view_error_quark:
 *
 * Gets the WPEView Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK(wpe-view-error-quark, wpe_view_error)

enum {
    PROP_0,

    PROP_DISPLAY,
    PROP_TOPLEVEL,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_SCALE,
    PROP_TOPLEVEL_STATE,
    PROP_SCREEN,
    PROP_VISIBLE,
    PROP_MAPPED,
    PROP_HAS_FOCUS,

    N_PROPERTIES
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

enum {
    CLOSED,
    RESIZED,
    BUFFER_RENDERED,
    BUFFER_RELEASED,
    EVENT,
    TOPLEVEL_STATE_CHANGED,
    PREFERRED_DMA_BUF_FORMATS_CHANGED,

    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void wpeViewSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* view = WPE_VIEW(object);

    switch (propId) {
    case PROP_DISPLAY:
        view->priv->display = WPE_DISPLAY(g_value_get_object(value));
        break;
    case PROP_TOPLEVEL:
        wpe_view_set_toplevel(view, WPE_TOPLEVEL(g_value_get_object(value)));
        break;
    case PROP_VISIBLE:
        wpe_view_set_visible(view, g_value_get_boolean(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeViewGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* view = WPE_VIEW(object);

    switch (propId) {
    case PROP_DISPLAY:
        g_value_set_object(value, wpe_view_get_display(view));
        break;
    case PROP_TOPLEVEL:
        g_value_set_object(value, wpe_view_get_toplevel(view));
        break;
    case PROP_WIDTH:
        g_value_set_int(value, wpe_view_get_width(view));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, wpe_view_get_height(view));
        break;
    case PROP_SCALE:
        g_value_set_double(value, wpe_view_get_scale(view));
        break;
    case PROP_TOPLEVEL_STATE:
        g_value_set_flags(value, wpe_view_get_toplevel_state(view));
        break;
    case PROP_SCREEN:
        g_value_set_object(value, wpe_view_get_screen(view));
        break;
    case PROP_VISIBLE:
        g_value_set_boolean(value, wpe_view_get_visible(view));
        break;
    case PROP_MAPPED:
        g_value_set_boolean(value, wpe_view_get_mapped(view));
        break;
    case PROP_HAS_FOCUS:
        g_value_set_boolean(value, wpe_view_get_has_focus(view));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeViewConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_view_parent_class)->constructed(object);

    // FIXME: add API to set the default view size.
    auto* priv = WPE_VIEW(object)->priv;
    priv->width = 1024;
    priv->height = 768;
}

static void wpeViewDispose(GObject* object)
{
    wpe_view_set_toplevel(WPE_VIEW(object), nullptr);

    G_OBJECT_CLASS(wpe_view_parent_class)->dispose(object);
}

static void wpe_view_class_init(WPEViewClass* viewClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewClass);
    objectClass->set_property = wpeViewSetProperty;
    objectClass->get_property = wpeViewGetProperty;
    objectClass->constructed = wpeViewConstructed;
    objectClass->dispose = wpeViewDispose;

    /**
     * WPEView:display:
     *
     * The #WPEDisplay of the view.
     */
    sObjProperties[PROP_DISPLAY] =
        g_param_spec_object(
            "display",
            nullptr, nullptr,
            WPE_TYPE_DISPLAY,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEView:toplevel:
     *
     * The #WPEToplevel of the view
     */
    sObjProperties[PROP_TOPLEVEL] =
        g_param_spec_object(
            "toplevel",
            nullptr, nullptr,
            WPE_TYPE_TOPLEVEL,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEView:width:
     *
     * The view width in logical coordinates
     */
    sObjProperties[PROP_WIDTH] =
        g_param_spec_int(
            "width",
            nullptr, nullptr,
            0, G_MAXINT, 0,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:height:
     *
     * The view height in logical coordinates
     */
    sObjProperties[PROP_HEIGHT] =
        g_param_spec_int(
            "height",
            nullptr, nullptr,
            0, G_MAXINT, 0,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:scale:
     *
     * The view scale
     */
    sObjProperties[PROP_SCALE] =
        g_param_spec_double(
            "scale",
            nullptr, nullptr,
            0., G_MAXDOUBLE, 1.,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:toplevel-state:
     *
     * The view's toplevel state
     */
    sObjProperties[PROP_TOPLEVEL_STATE] =
        g_param_spec_flags(
            "toplevel-state",
            nullptr, nullptr,
            WPE_TYPE_TOPLEVEL_STATE,
            WPE_TOPLEVEL_STATE_NONE,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:screen:
     *
     * The current #WPEScreen of the view.
     */
    sObjProperties[PROP_SCREEN] =
        g_param_spec_object(
            "screen",
            nullptr, nullptr,
            WPE_TYPE_SCREEN,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:visible:
     *
     * Whether the view should be visible or not. This property
     * can be used to show or hide a view, but setting to %TRUE (which
     * is the default) doesn't mean it will always be shown, because
     * the visbility also depends on the status of its toplevel (for
     * example if the toplevel is minimized the view will be hidden).
     * To know whether the view is actually visible, you can check
     * #WPEView:mapped property.
     */
    sObjProperties[PROP_VISIBLE] =
        g_param_spec_boolean(
            "visible",
            nullptr, nullptr,
            TRUE,
            WEBKIT_PARAM_READWRITE);

    /**
     * WPEView:mapped:
     *
     * Whether the view is mapped or not. A view is mapped when #WPEView:visible
     * is %TRUE and it's not hidden for other reasons, like for example when its
     * toplevel is minimized. This is a readonly property that can be used to
     * monitor when a #WPEView is shown or hidden.
     */
    sObjProperties[PROP_MAPPED] =
        g_param_spec_boolean(
            "mapped",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:has-focus:
     *
     * Whether the view has the keyboard focus.
     */
    sObjProperties[PROP_HAS_FOCUS] =
        g_param_spec_boolean(
            "has-focus",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);

    /**
     * WPEView::closed:
     * @view: a #WPEView
     *
     * Emitted when @view has been closed.
     */
    signals[CLOSED] = g_signal_new(
        "closed",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEView::resized:
     * @view: a #WPEView
     *
     * Emitted when @view is resized
     */
    signals[RESIZED] = g_signal_new(
        "resized",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEView::buffer-rendered:
     * @view: a #WPEView
     * @buffer: a #WPEBuffer
     *
     * Emitted to notify that the buffer has been rendered in the view.
     */
    signals[BUFFER_RENDERED] = g_signal_new(
        "buffer-rendered",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_BUFFER);

    /**
     * WPEView::buffer-released:
     * @view: a #WPEView
     * @buffer: a #WPEBuffer
     *
     * Emitted to notify that the buffer is no longer used by the view
     * and can be destroyed or reused.
     */
    signals[BUFFER_RELEASED] = g_signal_new(
        "buffer-released",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_BUFFER);

    /**
     * WPEView::event
     * @view: a #WPEView
     * @event: a #WPEEvent
     *
     * Emitted when an input event is received on @view.
     *
     * Returns: %TRUE to indicate that the event has been handled
     */
    signals[EVENT] = g_signal_new(
        "event",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled,
        nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WPE_TYPE_EVENT);

    /**
     * WPEView::toplevel-state-changed:
     * @view: a #WPEView
     * @previous_state: a #WPEToplevelState
     *
     * Emitted when @view's toplevel state changes
     */
    signals[TOPLEVEL_STATE_CHANGED] = g_signal_new(
        "toplevel-state-changed",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_TOPLEVEL_STATE);

    /**
     * WPEView::preferred-dma-buf-formats-changed:
     * @view: a #WPEView
     *
     * Emitted when the list of preferred DMA-BUF formats has changed.
     * This can happen whe the @view becomes a candidate for scanout.
     */
    signals[PREFERRED_DMA_BUF_FORMATS_CHANGED] = g_signal_new(
        "preferred-dma-buf-formats-changed",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);
}

void wpeViewToplevelStateChanged(WPEView* view, WPEToplevelState state)
{
    if (view->priv->state == state)
        return;

    auto previousState = view->priv->state;
    view->priv->state = state;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_TOPLEVEL_STATE]);
    g_signal_emit(view, signals[TOPLEVEL_STATE_CHANGED], 0, previousState);
}

void wpeViewScaleChanged(WPEView* view, double scale)
{
    if (view->priv->scale == scale)
        return;

    view->priv->scale = scale;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_SCALE]);
}

void wpeViewScreenChanged(WPEView* view)
{
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_SCREEN]);
}

void wpeViewPreferredDMABufFormatsChanged(WPEView* view)
{
    g_signal_emit(view, signals[PREFERRED_DMA_BUF_FORMATS_CHANGED], 0);
}

/**
 * wpe_view_new:
 * @display: a #WPEDisplay
 *
 * Create a new #WPEView for @display
 *
 * Returns: (transfer full): a #WPEView
 */
WPEView* wpe_view_new(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    return wpeDisplayCreateView(display);
}

/**
 * wpe_view_get_display:
 * @view: a #WPEView
 *
 * Get the #WPEDisplay of @view
 *
 * Returns: (transfer none): a #WPEDisplay
 */
WPEDisplay* wpe_view_get_display(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return view->priv->display.get();
}

/**
 * wpe_view_get_toplevel:
 * @view: a #WPEView
 *
 * Get the #WPEToplevel of @view
 *
 * Returns: (transfer none) (nullable): a #WPEToplevel
 */
WPEToplevel* wpe_view_get_toplevel(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return view->priv->toplevel.get();
}

/**
 * wpe_view_set_toplevel:
 * @view: a #WPEView
 * @toplevel: (nullable): a #WPEToplevel, or %NULL
 *
 * Set the current toplevel of @view.
 * If @toplevel has already reached the maximum number of views (see wpe_toplevel_get_max_views())
 * this function does nothing.
 */
void wpe_view_set_toplevel(WPEView* view, WPEToplevel* toplevel)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(!toplevel || (WPE_IS_TOPLEVEL(toplevel) && wpe_toplevel_get_display(toplevel) == view->priv->display.get()));

    auto* priv = view->priv;
    if (priv->toplevel == toplevel)
        return;

    if (toplevel && wpe_toplevel_get_n_views(toplevel) == wpe_toplevel_get_max_views(toplevel))
        return;

    if (priv->toplevel)
        wpeToplevelRemoveView(priv->toplevel.get(), view);

    priv->toplevel = toplevel;

    if (priv->toplevel) {
        wpeToplevelAddView(priv->toplevel.get(), view);
        wpeViewScaleChanged(view, wpe_toplevel_get_scale(priv->toplevel.get()));
        wpeViewToplevelStateChanged(view, wpe_toplevel_get_state(priv->toplevel.get()));
        wpeViewScreenChanged(view);
        wpeViewPreferredDMABufFormatsChanged(view);
    }

    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_TOPLEVEL]);
}

/**
 * wpe_view_get_width:
 * @view: a #WPEView
 *
 * Get the @view width in logical coordinates
 *
 * Returns: the view width
 */
int wpe_view_get_width(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), 0);

    return view->priv->width;
}

/**
 * wpe_view_get_height:
 * @view: a #WPEView
 *
 * Get the @view height in logical coordinates
 *
 * Returns: the view height
 */
int wpe_view_get_height(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), 0);

    return view->priv->height;
}

/**
 * wpe_view_closed:
 * @view: a #WPEView
 *
 * Emit #WPEView::closed signal if @view is not already closed.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_closed(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    auto* priv = view->priv;
    if (priv->closed)
        return;

    priv->closed = true;
    g_signal_emit(view, signals[CLOSED], 0);
}

/**
 * wpe_view_resized:
 * @view: a #WPEView
 * @width: width in logical coordinates
 * @height: height in logical coordinates
 *
 * Update @view size and emit #WPEView::resized if size changed.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_resized(WPEView* view, int width, int height)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    auto* priv = view->priv;
    if (priv->width == width && priv->height == height)
        return;

    g_object_freeze_notify(G_OBJECT(view));
    if (priv->width != width) {
        priv->width = width;
        g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_WIDTH]);
    }
    if (priv->height != height) {
        priv->height = height;
        g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_HEIGHT]);
    }
    g_object_thaw_notify(G_OBJECT(view));

    g_signal_emit(view, signals[RESIZED], 0);
}

/**
 * wpe_view_get_scale:
 * @view: a #WPEView
 *
 * Get the @view scale
 *
 * Returns: the view scale
 */
gdouble wpe_view_get_scale(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), 1.);

    return view->priv->scale;
}

/**
 * wpe_view_get_visible:
 * @view: a #WPEView
 *
 * Get the visibility of @view.
 * This might return %TRUE even if the view is not mapped,
 * for example when it's minimized.
 *
 * Returns: %TRUE if the view is visible, or %FALSE otherwise
 */
gboolean wpe_view_get_visible(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    return view->priv->visible;
}

/**
 * wpe_view_set_visible:
 * @view: a #WPEView
 * @visible: whether the view should be visible
 *
 * Set the visibility of @view. See #WPEView:visible property
 * for more information.
 */
void wpe_view_set_visible(WPEView* view, gboolean visible)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (view->priv->visible == visible)
        return;

    view->priv->visible = visible;
    if (view->priv->visible)
        wpe_view_map(view);
    else
        wpe_view_unmap(view);
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_VISIBLE]);
}

/**
 * wpe_view_get_mapped:
 * @view: a #WPEView
 *
 * Get whether @view is mapped. A #WPEView isa mapped if
 * #WPEView:visible is %TRUE and it's not hidden for other
 * reasons, like for example when its toplevel is minimized.
 * You can connect to notify::mapped signal of @view to
 * monitor the visibility.
 *
 * Returns: %TRUE if the view is mapped, or %FALSE otherwise
 */
gboolean wpe_view_get_mapped(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    return view->priv->mapped;
}

/**
 * wpe_view_map:
 * @view: a #WPEView
 *
 * Make @view to be mapped. If #WPEView:visible is %TRUE and
 * the view can be shown (determined by #WPEViewClass::can_be_mapped)
 * #WPEView:mapped will be set to %TRUE.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_map(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (view->priv->mapped || !view->priv->visible)
        return;

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->can_be_mapped && !viewClass->can_be_mapped(view))
        return;

    view->priv->mapped = TRUE;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_MAPPED]);
}

/**
 * wpe_view_unmap:
 * @view: a #WPEView
 *
 * Make @view to be unmapped. This always sets the #WPEView:mapped
 * property to %FALSE and the @view is considered to be hidden even
 * if #WPEView:visible is %TRUE.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_unmap(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (!view->priv->mapped)
        return;

    view->priv->mapped = FALSE;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_MAPPED]);
}

/**
 * wpe_view_lock_pointer:
 * @view: a #WPEView
 *
 * Disable the movements of the pointer in @view, locking it to a particular
 * area; while the pointer is locked, mouse events are relative instead of
 * absolute motions.
 *
 * Returns: %TRUE if the pointer is locked, or %FALSE otherwise
 */
gboolean wpe_view_lock_pointer(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->lock_pointer)
        return viewClass->lock_pointer(view);

    return FALSE;
}

/**
 * wpe_view_unlock_pointer:
 * @view: a #WPEView
 *
 * Unlock the pointer in @view if it has been locked.
 *
 * Returns: %TRUE if the pointer is unlocked, or %FALSE otherwise
 */
gboolean wpe_view_unlock_pointer(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->unlock_pointer)
        return viewClass->unlock_pointer(view);

    return FALSE;
}

/**
 * wpe_view_set_cursor_from_name:
 * @view: a #WPEView
 * @name: a cursor name
 *
 * Set the @view cursor by looking up @name in the current cursor theme.
 */
void wpe_view_set_cursor_from_name(WPEView* view, const char* name)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(name);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->set_cursor_from_name)
        viewClass->set_cursor_from_name(view, name);
}

/**
 * wpe_view_set_cursor_from_bytes:
 * @view: a #WPEView
 * @bytes: the cursor image data
 * @width: the cursor width
 * @height: the cursor height
 * @stride: the cursor image data stride
 * @hotspot_x: the cursor hotspot x coordinate
 * @hotspot_y: the cursor hotspot y coordinate
 *
 * Set the @view cursor from the given @bytes. Pixel format must be %WPE_PIXEL_FORMAT_ARGB8888.
 */
void wpe_view_set_cursor_from_bytes(WPEView* view, GBytes* bytes, guint width, guint height, guint stride, guint hotspotX, guint hotspotY)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(bytes);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->set_cursor_from_bytes)
        viewClass->set_cursor_from_bytes(view, bytes, width, height, stride, hotspotX, hotspotY);
}

/**
 * wpe_view_get_toplevel_state:
 * @view: a #WPEView
 *
 * Get the current state of @view's toplevel
 *
 * Returns: the view's toplevel state
 */
WPEToplevelState wpe_view_get_toplevel_state(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), WPE_TOPLEVEL_STATE_NONE);

    return view->priv->state;
}

/**
 * wpe_view_get_screen:
 * @view: a #WPEView
 *
 * Get current #WPEScreen of @view
 *
 * Returns: (transfer none) (nullable): a #WPEScreen, or %NULL
 */
WPEScreen* wpe_view_get_screen(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return view->priv->toplevel ? wpe_toplevel_get_screen(view->priv->toplevel.get()) : nullptr;
}

/**
 * wpe_view_render_buffer:
 * @view: a #WPEView
 * @buffer: a #WPEBuffer to render
 * @damage_rects: (nullable) (array length=n_damage_rects): damage rectangles
 * @n_damage_rects: number of rectangles in @damage_rects
 * @error: return location for error or %NULL to ignore
 *
 * Render the given @buffer into @view.
 * If this function returns %TRUE you must call wpe_view_buffer_rendered() when the buffer
 * is rendered and wpe_view_buffer_released() when it's no longer used by the view.
 *
 * Returns: %TRUE if buffer will be rendered, or %FALSE otherwise
 */
gboolean wpe_view_render_buffer(WPEView* view, WPEBuffer* buffer, const WPERectangle* damageRects, guint nDamageRects, GError** error)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->render_buffer(view, buffer, damageRects, nDamageRects, error);
}

/**
 * wpe_view_buffer_rendered:
 * @view: a #WPEView
 * @buffer: a #WPEBuffer
 *
 * Emit #WPEView::buffer-rendered signal to notify that @buffer has been rendered.
 */
void wpe_view_buffer_rendered(WPEView* view, WPEBuffer* buffer)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(WPE_IS_BUFFER(buffer));

    g_signal_emit(view, signals[BUFFER_RENDERED], 0, buffer);
}

/**
 * wpe_view_buffer_released:
 * @view: a #WPEView
 * @buffer: a #WPEBuffer
 *
 * Emit #WPEView::buffer-released signal to notify that @buffer is no longer used and
 * can be destroyed or reused.
 */
void wpe_view_buffer_released(WPEView* view, WPEBuffer* buffer)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(WPE_IS_BUFFER(buffer));

    g_signal_emit(view, signals[BUFFER_RELEASED], 0, buffer);
}

/**
 * wpe_view_event:
 * @view: a #WPEView
 * @event: a #WPEEvent
 *
 * Emit #WPEView::event signal to notify about @event
 */
void wpe_view_event(WPEView* view, WPEEvent* event)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(event);

    gboolean handled;
    g_signal_emit(view, signals[EVENT], 0, event, &handled);
}

/**
 * wpe_view_compute_press_count:
 * @view: a #WPEView
 * @x: the x coordinate of a button press event
 * @y: the y coordinate of a button press event
 * @button: the button number of a button press event
 * @time: the timestamp of a button press event
 *
 * Compute the number of presses for a button press event
 *
 * Returns: the number of button presses
 */
guint wpe_view_compute_press_count(WPEView* view, gdouble x, gdouble y, guint button, guint32 time)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), 0);

    auto* priv = view->priv;
    unsigned pressCount = 1;
    if (priv->lastButtonPress.pressCount) {
        // FIXME: make these configurable.
        static const int doubleClickDistance = 5;
        static const unsigned doubleClickTime = 400;
        if (std::abs(x - priv->lastButtonPress.x) < doubleClickDistance
            && std::abs(y - priv->lastButtonPress.y) < doubleClickDistance
            && button == priv->lastButtonPress.button
            && time - priv->lastButtonPress.time < doubleClickTime)
            pressCount = priv->lastButtonPress.pressCount + 1;
    }

    priv->lastButtonPress = { pressCount, x, y, button, time };
    return pressCount;
}

/**
 * wpe_view_focus_in:
 * @view: a #WPEView
 *
 * Make @view gain the keyboard focus.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_focus_in(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (view->priv->hasFocus)
        return;

    view->priv->hasFocus = true;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_HAS_FOCUS]);
}

/**
 * wpe_view_focus_out:
 * @view: a #WPEView
 *
 * Make @view lose the keyboard focus.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_focus_out(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (!view->priv->hasFocus)
        return;

    view->priv->hasFocus = false;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_HAS_FOCUS]);
}

/**
 * wpe_view_get_has_focus:
 * @view: a #WPEView
 *
 * Get whether @view has the keyboard focus.
 *
 * Returns: %TRUE if view has the keyboard focus, or %FALSE otherwise
 */
gboolean wpe_view_get_has_focus(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    return !!view->priv->hasFocus;
}

/**
 * wpe_view_get_preferred_dma_buf_formats:
 * @view: a #WPEView
 *
 * Get the list of preferred DMA-BUF buffer formats for @view.
 *
 * Returns: (transfer none) (nullable): a #WPEBufferDMABufFormats
 */
WPEBufferDMABufFormats* wpe_view_get_preferred_dma_buf_formats(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    if (!view->priv->toplevel)
        return nullptr;

    return wpe_toplevel_get_preferred_dma_buf_formats(view->priv->toplevel.get());
}

/**
 * wpe_view_set_opaque_rectangles:
 * @view: a #WPEView
 * @rects: (nullable) (array length=n_rects): opaque rectangles in view-local coordinates
 * @n_rects: the total number of elements in @rects
 *
 * Set the rectangles of @view that contain opaque content.
 * This is an optimization hint that is automatically set by WebKit when the
 * web view background color is opaque.
 */
void wpe_view_set_opaque_rectangles(WPEView* view, WPERectangle* rects, guint rectsCount)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(!rects || rectsCount > 0);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->set_opaque_rectangles)
        viewClass->set_opaque_rectangles(view, rects, rectsCount);
}

/**
 * wpe_view_set_gesture_controller:
 * @view: a #WPEView
 * @controller: (nullable): a #WPEGestureController, or %NULL
 *
 * Set @controller as #WPEGestureController of @view.
 * When supplied with %NULL, the default #WPEGestureController will be removed
 * and thus default gesture handling will be disabled for the @view.
 */
void wpe_view_set_gesture_controller(WPEView* view, WPEGestureController* controller)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(WPE_IS_GESTURE_CONTROLLER(controller));

    view->priv->gestureController = controller;
}

/**
 * wpe_view_get_gesture_controller:
 * @view: a #WPEView
 *
 * Get the #WPEGestureController of @view.
 *
 * Returns: (transfer none) (nullable): a #WPEGestureController or %NULL
 */
WPEGestureController* wpe_view_get_gesture_controller(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    if (!view->priv->gestureController)
        view->priv->gestureController = adoptGRef(wpeGestureControllerImplNew());

    return view->priv->gestureController->get();
}
