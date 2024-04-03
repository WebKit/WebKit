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
#include "WPEBufferDMABufFormat.h"
#include "WPEDisplayPrivate.h"
#include "WPEEnumTypes.h"
#include "WPEEvent.h"
#include <wtf/SetForScope.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/WTFString.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

/**
 * WPEView:
 *
 */
struct _WPEViewPrivate {
    GRefPtr<WPEDisplay> display;
    int width;
    int height;
    gdouble scale { 1 };
    WPEViewState state;
#if USE(LIBDRM)
    GList* overridenDMABufFormats;
#endif

    struct {
        unsigned pressCount { 0 };
        gdouble x { 0 };
        gdouble y { 0 };
        guint button { 0 };
        guint32 time { 0 };
    } lastButtonPress;
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
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_SCALE,
    PROP_STATE,
    PROP_MONITOR,

    N_PROPERTIES
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

enum {
    RESIZED,
    BUFFER_RENDERED,
    BUFFER_RELEASED,
    EVENT,
    FOCUS_IN,
    FOCUS_OUT,
    STATE_CHANGED,
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
    case PROP_WIDTH:
        g_value_set_int(value, wpe_view_get_width(view));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, wpe_view_get_height(view));
        break;
    case PROP_SCALE:
        g_value_set_double(value, wpe_view_get_scale(view));
        break;
    case PROP_STATE:
        g_value_set_flags(value, wpe_view_get_state(view));
        break;
    case PROP_MONITOR:
        g_value_set_object(value, wpe_view_get_monitor(view));
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

#if USE(LIBDRM)
static void wpeViewDispose(GObject* object)
{
    auto* priv = WPE_VIEW(object)->priv;

    g_clear_list(&priv->overridenDMABufFormats, reinterpret_cast<GDestroyNotify>(wpe_buffer_dma_buf_format_free));

    G_OBJECT_CLASS(wpe_view_parent_class)->dispose(object);
}
#endif

static void wpe_view_class_init(WPEViewClass* viewClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewClass);
    objectClass->set_property = wpeViewSetProperty;
    objectClass->get_property = wpeViewGetProperty;
    objectClass->constructed = wpeViewConstructed;
#if USE(LIBDRM)
    objectClass->dispose = wpeViewDispose;
#endif

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
            1., G_MAXDOUBLE, 1.,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:state:
     *
     * The view state
     */
    sObjProperties[PROP_STATE] =
        g_param_spec_flags(
            "flags",
            nullptr, nullptr,
            WPE_TYPE_VIEW_STATE,
            WPE_VIEW_STATE_NONE,
            WEBKIT_PARAM_READABLE);

    /**
     * WPEView:monitor:
     *
     * The current #WPEMonitor of the view.
     */
    sObjProperties[PROP_MONITOR] =
        g_param_spec_object(
            "monitor",
            nullptr, nullptr,
            WPE_TYPE_MONITOR,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);

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
     * WPEView::focus-in:
     * @view: a #WPEView
     *
     * Emitted when @view gets the keyboard focus
     */
    signals[FOCUS_IN] = g_signal_new(
        "focus-in",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEView::focus-out:
     * @view: a #WPEView
     *
     * Emitted when @view loses the keyboard focus
     */
    signals[FOCUS_OUT] = g_signal_new(
        "focus-out",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 0);

    /**
     * WPEView::state-changed:
     * @view: a #WPEView
     * @previous_state: a #WPEViewState
     *
     * Emitted when @view state changes
     */
    signals[STATE_CHANGED] = g_signal_new(
        "state-changed",
        G_TYPE_FROM_CLASS(viewClass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 1,
        WPE_TYPE_VIEW_STATE);

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

/**
 * wpe_view_new:
 * @display: a #WPEDisplay
 *
 * Create a new #WPEView for @display
 *
 * Returns: (transfer full) (nullable): a #WPEView, or %NULL
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
 * wpe_view_resize:
 * @view: a #WPEView
 * @width: width in logical coordinates
 * @height: height in logical coordinates
 *
 * Request that the @view is resized at @width x @height.
 *
 * Signal #WPEView::resized will be emitted when the resize is performed.
 *
 * Returns: %TRUE if resizing is supported and given dimensions are
 *    different than current size, otherwise %FALSE
 */
gboolean wpe_view_resize(WPEView* view, int width, int height)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* priv = view->priv;
    if (priv->width == width && priv->height == height)
        return FALSE;

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->resize ? viewClass->resize(view, width, height) : FALSE;
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
 * wpe_view_scale_changed:
 * @view: a #WPEView
 * @scale: the new scale
 *
 * Update the @view scale.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_scale_changed(WPEView* view, gdouble scale)
{
    g_return_if_fail(WPE_IS_VIEW(view));
    g_return_if_fail(scale > 0);

    if (view->priv->scale == scale)
        return;

    view->priv->scale = scale;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_SCALE]);
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
 * wpe_view_get_state:
 * @view: a #WPEView
 *
 * Get the current state of @view
 *
 * Returns: the view's state
 */
WPEViewState wpe_view_get_state(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), WPE_VIEW_STATE_NONE);

    return view->priv->state;
}

/**
 * wpe_view_state_changed:
 * @view: a #WPEView
 * @state: a set of #WPEViewState
 *
 * Update the current state of @view and emit @WPEView::state-changed if changed.
 *
 * This function should only be called by #WPEView derived classes
 * in platform implementations.
 */
void wpe_view_state_changed(WPEView* view, WPEViewState state)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    if (view->priv->state == state)
        return;

    auto previousState = view->priv->state;
    view->priv->state = state;
    g_object_notify_by_pspec(G_OBJECT(view), sObjProperties[PROP_STATE]);
    g_signal_emit(view, signals[STATE_CHANGED], 0, previousState);
}

/**
 * wpe_view_get_monitor:
 * @view: a #WPEView
 *
 * Get current #WPEMonitor of @view
 *
 * Returns: (transfer none) (nullable): a #WPEMonitor, or %NULL
 */
WPEMonitor* wpe_view_get_monitor(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->get_monitor ? viewClass->get_monitor(view) : nullptr;
}

/**
 * wpe_view_fullscreen:
 * @view: a #WPEView
 *
 * Request that the @view goes into a fullscreen state.
 *
 * To track the state see #WPEView::state-changed
 *
 * Returns: %TRUE if fullscreen is supported, otherwise %FALSE
 */
gboolean wpe_view_fullscreen(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->set_fullscreen ? viewClass->set_fullscreen(view, TRUE) : FALSE;
}

/**
 * wpe_view_unfullscreen:
 * @view: a #WPEView
 *
 * Request that the @view leaves a fullscreen state.
 *
 * To track the state see #WPEView::state-changed
 *
 * Returns: %TRUE if unfullscreen is supported, otherwise %FALSE
 */
gboolean wpe_view_unfullscreen(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->set_fullscreen ? viewClass->set_fullscreen(view, FALSE) : FALSE;
}

/**
 * wpe_view_maximize:
 * @view: a #WPEView
 *
 * Request that the @view is maximized. If the view is already maximized this function
 * does nothing.
 *
 * To track the state see #WPEView::state-changed
 *
 * Returns: %TRUE if maximize is supported, otherwise %FALSE
 */
gboolean wpe_view_maximize(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->set_maximized ? viewClass->set_maximized(view, TRUE) : FALSE;
}

/**
 * wpe_view_unmaximize:
 * @view: a #WPEView
 *
 * Request that the @view is unmaximized. If the view is not maximized this function
 * does nothing.
 *
 * To track the state see #WPEView::state-changed
 *
 * Returns: %TRUE if maximize is supported, otherwise %FALSE
 */
gboolean wpe_view_unmaximize(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->set_maximized ? viewClass->set_maximized(view, FALSE) : FALSE;
}

/**
 * wpe_view_render_buffer:
 * @view: a #WPEView
 * @buffer: a #WPEBuffer to render
 * @error: return location for error or %NULL to ignore
 *
 * Render the given @buffer into @view.
 * If this function returns %TRUE you must call wpe_view_buffer_rendered() when the buffer
 * is rendered and wpe_view_buffer_released() when it's no longer used by the view.
 *
 * Returns: %TRUE if buffer will be rendered, or %FALSE otherwise
 */
gboolean wpe_view_render_buffer(WPEView* view, WPEBuffer* buffer, GError** error)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), FALSE);
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), FALSE);

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    return viewClass->render_buffer(view, buffer, error);
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
 * Emit #WPEView::focus-in signal to notify that @view has gained the keyboard focus.
 */
void wpe_view_focus_in(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    g_signal_emit(view, signals[FOCUS_IN], 0);
}

/**
 * wpe_view_focus_out:
 * @view: a #WPEView
 *
 * Emit #WPEView::focus-out signal to notify that @view has lost the keyboard focus.
 */
void wpe_view_focus_out(WPEView* view)
{
    g_return_if_fail(WPE_IS_VIEW(view));

    g_signal_emit(view, signals[FOCUS_OUT], 0);
}

/**
 * wpe_view_get_preferred_dma_buf_formats:
 * @view: a #WPEView
 *
 * Get the list of preferred DMA-BUF buffer formats for @view.
 *
 * Returns: (transfer none) (element-type WPEBufferDMABufFormat) (nullable): a #GList of #WPEBufferDMABufFormat
 */
GList* wpe_view_get_preferred_dma_buf_formats(WPEView* view)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

#if USE(LIBDRM)
    if (view->priv->overridenDMABufFormats)
        return view->priv->overridenDMABufFormats;

    const char* formatString = getenv("WPE_DMABUF_BUFFER_FORMAT");
    if (formatString && *formatString) {
        auto tokens = String::fromUTF8(formatString).split(':');
        if (!tokens.isEmpty() && tokens[0].length() >= 2 && tokens[0].length() <= 4) {
            guint32 format = fourcc_code(tokens[0][0], tokens[0][1], tokens[0].length() > 2 ? tokens[0][2] : ' ', tokens[0].length() > 3 ? tokens[0][3] : ' ');
            guint64 modifier = tokens.size() > 1 ? g_ascii_strtoull(tokens[1].ascii().data(), nullptr, 10) : DRM_FORMAT_MOD_INVALID;
            GRefPtr<GArray> modifiers = adoptGRef(g_array_sized_new(FALSE, TRUE, sizeof(guint64), 1));
            g_array_append_val(modifiers.get(), modifier);
            WPEBufferDMABufFormatUsage usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
            if (tokens.size() > 2) {
                if (tokens[2] == "rendering"_s)
                    usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING;
                else if (tokens[2] == "mapping"_s)
                    usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING;
                else if (tokens[2] == "scanout"_s)
                    usage = WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT;
            }
            view->priv->overridenDMABufFormats = g_list_prepend(view->priv->overridenDMABufFormats, wpe_buffer_dma_buf_format_new(usage, format, modifiers.get()));
            return view->priv->overridenDMABufFormats;
        }

        WTFLogAlways("Invalid format %s set in WPE_DMABUF_BUFFER_FORMAT, ignoring...", formatString);
    }
#endif

    auto* viewClass = WPE_VIEW_GET_CLASS(view);
    if (viewClass->get_preferred_dma_buf_formats)
        return viewClass->get_preferred_dma_buf_formats(view);

    return wpe_display_get_preferred_dma_buf_formats(view->priv->display.get());
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
