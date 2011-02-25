/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebViewWidget.h"

#include "GOwnPtrGtk.h"
#include "GtkVersioning.h"
#include "NotImplemented.h"
#include "RefPtrCairo.h"

using namespace WebKit;
using namespace WebCore;

static gpointer webViewWidgetParentClass = 0;

struct _WebViewWidgetPrivate {
    WebView* webViewInstance;
    GtkIMContext* imContext;
    gint currentClickCount;
    IntPoint previousClickPoint;
    guint previousClickButton;
    guint32 previousClickTime;
};

static void webViewWidgetRealize(GtkWidget* widget)
{
    gtk_widget_set_realized(widget, TRUE);

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    GdkWindowAttr attributes;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = allocation.x;
    attributes.y = allocation.y;
    attributes.width = allocation.width;
    attributes.height = allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
#ifdef GTK_API_VERSION_2
    attributes.colormap = gtk_widget_get_colormap(widget);
#endif
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK
        | GDK_EXPOSURE_MASK
        | GDK_BUTTON_PRESS_MASK
        | GDK_BUTTON_RELEASE_MASK
        | GDK_POINTER_MOTION_MASK
        | GDK_KEY_PRESS_MASK
        | GDK_KEY_RELEASE_MASK
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON1_MOTION_MASK
        | GDK_BUTTON2_MOTION_MASK
        | GDK_BUTTON3_MOTION_MASK;

    gint attributesMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
#ifdef GTK_API_VERSION_2
    attributesMask |= GDK_WA_COLORMAP;
#endif
    GdkWindow* window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributesMask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);

#ifdef GTK_API_VERSION_2
#if GTK_CHECK_VERSION(2, 20, 0)
    gtk_widget_style_attach(widget);
#else
    widget->style = gtk_style_attach(gtk_widget_get_style(widget), window);
#endif
    gtk_style_set_background(gtk_widget_get_style(widget), window, GTK_STATE_NORMAL);
#else
    gtk_style_context_set_background(gtk_widget_get_style_context(widget), window);
#endif

    WebViewWidget* webView = WEB_VIEW_WIDGET(widget);
    WebViewWidgetPrivate* priv = webView->priv;
    gtk_im_context_set_client_window(priv->imContext, window);
}

static void webViewWidgetContainerAdd(GtkContainer* container, GtkWidget* widget)
{
    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void webViewWidgetDispose(GObject* gobject)
{
    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(gobject);
    WebViewWidgetPrivate* priv = webViewWidget->priv;

    if (priv->imContext) {
        g_object_unref(priv->imContext);
        priv->imContext = 0;
    }

    G_OBJECT_CLASS(webViewWidgetParentClass)->dispose(gobject);
}

static void webViewWidgetInit(WebViewWidget* webViewWidget)
{
    WebViewWidgetPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webViewWidget, WEB_VIEW_TYPE_WIDGET, WebViewWidgetPrivate);
    webViewWidget->priv = priv;

    gtk_widget_set_can_focus(GTK_WIDGET(webViewWidget), TRUE);
    priv->imContext = gtk_im_multicontext_new();

    priv->currentClickCount = 0;
    priv->previousClickButton = 0;
    priv->previousClickTime = 0;
}

#ifdef GTK_API_VERSION_2
static gboolean webViewExpose(GtkWidget* widget, GdkEventExpose* event)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    GdkRectangle clipRect;
    gdk_region_get_clipbox(event->region, &clipRect);

    GdkWindow* window = gtk_widget_get_window(widget);
    RefPtr<cairo_t> cr = adoptRef(gdk_cairo_create(window));

    webView->paint(widget, clipRect, cr.get());

    return FALSE;
}
#else
static gboolean webViewDraw(GtkWidget* widget, cairo_t* cr)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    GdkRectangle clipRect;

    if (!gdk_cairo_get_clip_rectangle(cr, &clipRect))
        return FALSE;

    webView->paint(widget, clipRect, cr);

    return FALSE;
}
#endif

static void webViewSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    GTK_WIDGET_CLASS(webViewWidgetParentClass)->size_allocate(widget, allocation);
    webView->setSize(widget, IntSize(allocation->width, allocation->height));
}

static gboolean webViewFocusInEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(widget);
    WebView* webView = webViewWidgetGetWebViewInstance(webViewWidget);

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (gtk_widget_is_toplevel(toplevel) && gtk_window_has_toplevel_focus(GTK_WINDOW(toplevel))) {
        gtk_im_context_focus_in(webViewWidgetGetIMContext(webViewWidget));
        webView->handleFocusInEvent(widget);
    }

    return GTK_WIDGET_CLASS(webViewWidgetParentClass)->focus_in_event(widget, event);
}

static gboolean webViewFocusOutEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(widget);
    WebView* webView = webViewWidgetGetWebViewInstance(webViewWidget);

    webView->handleFocusOutEvent(widget);
    GtkIMContext* imContext = webViewWidgetGetIMContext(webViewWidget);
    if (imContext)
        gtk_im_context_focus_out(imContext);

    return GTK_WIDGET_CLASS(webViewWidgetParentClass)->focus_out_event(widget, event);
}

static gboolean webViewKeyPressEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    webView->handleKeyboardEvent(event);

    return GTK_WIDGET_CLASS(webViewWidgetParentClass)->key_press_event(widget, event);
}

static gboolean webViewKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(widget);
    WebView* webView = webViewWidgetGetWebViewInstance(webViewWidget);

    if (gtk_im_context_filter_keypress(webViewWidgetGetIMContext(webViewWidget), event))
        return TRUE;

    webView->handleKeyboardEvent(event);

    return GTK_WIDGET_CLASS(webViewWidgetParentClass)->key_release_event(widget, event);
}

// Copied from webkitwebview.cpp
static guint32 getEventTime(GdkEvent* event)
{
    guint32 time = gdk_event_get_time(event);
    if (time)
        return time;

    // Real events always have a non-zero time, but events synthesized
    // by the DRT do not and we must calculate a time manually. This time
    // is not calculated in the DRT, because GTK+ does not work well with
    // anything other than GDK_CURRENT_TIME on synthesized events.
    GTimeVal timeValue;
    g_get_current_time(&timeValue);
    return (timeValue.tv_sec * 1000) + (timeValue.tv_usec / 1000);
}

static gboolean webViewButtonPressEvent(GtkWidget* widget, GdkEventButton* buttonEvent)
{
    if (buttonEvent->button == 3) {
        // FIXME: [GTK] Add context menu support for Webkit2.
        // https://bugs.webkit.org/show_bug.cgi?id=54827
        notImplemented();
        return FALSE;
    }

    gtk_widget_grab_focus(widget);

    // For double and triple clicks GDK sends both a normal button press event
    // and a specific type (like GDK_2BUTTON_PRESS). If we detect a special press
    // coming up, ignore this event as it certainly generated the double or triple
    // click. The consequence of not eating this event is two DOM button press events
    // are generated.
    GOwnPtr<GdkEvent> nextEvent(gdk_event_peek());
    if (nextEvent && (nextEvent->any.type == GDK_2BUTTON_PRESS || nextEvent->any.type == GDK_3BUTTON_PRESS))
        return TRUE;

    gint doubleClickDistance = 250;
    gint doubleClickTime = 5;
    GtkSettings* settings = gtk_settings_get_for_screen(gtk_widget_get_screen(widget));
    g_object_get(settings,
                 "gtk-double-click-distance", &doubleClickDistance,
                 "gtk-double-click-time", &doubleClickTime, NULL);

    // GTK+ only counts up to triple clicks, but WebCore wants to know about
    // quadruple clicks, quintuple clicks, ad infinitum. Here, we replicate the
    // GDK logic for counting clicks.
    GdkEvent* event(reinterpret_cast<GdkEvent*>(buttonEvent));
    guint32 eventTime = getEventTime(event);
    WebViewWidget* webViewWidget = WEB_VIEW_WIDGET(widget);
    WebViewWidgetPrivate* priv = webViewWidget->priv;
    if ((event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
        || ((abs(buttonEvent->x - priv->previousClickPoint.x()) < doubleClickDistance)
            && (abs(buttonEvent->y - priv->previousClickPoint.y()) < doubleClickDistance)
            && (eventTime - priv->previousClickTime < static_cast<guint>(doubleClickTime))
            && (buttonEvent->button == priv->previousClickButton)))
        priv->currentClickCount++;
    else
        priv->currentClickCount = 1;

    WebView* webView = webViewWidgetGetWebViewInstance(webViewWidget);
    webView->handleMouseEvent(event, priv->currentClickCount);

    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);
    priv->previousClickPoint = IntPoint(x, y);
    priv->previousClickButton = buttonEvent->button;
    priv->previousClickTime = eventTime;

    return FALSE;
}

static gboolean webViewButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    gtk_widget_grab_focus(widget);
    webView->handleMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */);

    return FALSE;
}

static gboolean webViewScrollEvent(GtkWidget* widget, GdkEventScroll* event)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    webView->handleWheelEvent(event);

    return FALSE;
}

static gboolean webViewMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event)
{
    WebView* webView = webViewWidgetGetWebViewInstance(WEB_VIEW_WIDGET(widget));
    webView->handleMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */);

    return FALSE;
}

static void webViewWidgetClassInit(WebViewWidgetClass* webViewWidgetClass)
{
    webViewWidgetParentClass = g_type_class_peek_parent(webViewWidgetClass);

    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webViewWidgetClass);
    widgetClass->realize = webViewWidgetRealize;
#ifdef GTK_API_VERSION_2
    widgetClass->expose_event = webViewExpose;
#else
    widgetClass->draw = webViewDraw;
#endif
    widgetClass->size_allocate = webViewSizeAllocate;
    widgetClass->focus_in_event = webViewFocusInEvent;
    widgetClass->focus_out_event = webViewFocusOutEvent;
    widgetClass->key_press_event = webViewKeyPressEvent;
    widgetClass->key_release_event = webViewKeyReleaseEvent;
    widgetClass->button_press_event = webViewButtonPressEvent;
    widgetClass->button_release_event = webViewButtonReleaseEvent;
    widgetClass->scroll_event = webViewScrollEvent;
    widgetClass->motion_notify_event = webViewMotionNotifyEvent;

    GObjectClass* gobjectClass = G_OBJECT_CLASS(webViewWidgetClass);
    gobjectClass->dispose = webViewWidgetDispose;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webViewWidgetClass);
    containerClass->add = webViewWidgetContainerAdd;

    g_type_class_add_private(webViewWidgetClass, sizeof(WebViewWidgetPrivate));
}

GType webViewWidgetGetType()
{
    static volatile gsize gDefineTypeIdVolatile = 0;

    if (!g_once_init_enter(&gDefineTypeIdVolatile))
        return gDefineTypeIdVolatile;

    GType gDefineTypeId = g_type_register_static_simple(GTK_TYPE_CONTAINER,
                                                        g_intern_static_string("WebViewWidget"),
                                                        sizeof(WebViewWidgetClass),
                                                        reinterpret_cast<GClassInitFunc>(webViewWidgetClassInit),
                                                        sizeof(WebViewWidget),
                                                        reinterpret_cast<GInstanceInitFunc>(webViewWidgetInit),
                                                        static_cast<GTypeFlags>(0));
    g_once_init_leave(&gDefineTypeIdVolatile, gDefineTypeId);

    return gDefineTypeIdVolatile;
}

WebView* webViewWidgetGetWebViewInstance(WebViewWidget* webViewWidget)
{
    return webViewWidget->priv->webViewInstance;
}

void webViewWidgetSetWebViewInstance(WebViewWidget* webViewWidget, WebView* webViewInstance)
{
    webViewWidget->priv->webViewInstance = webViewInstance;
}

GtkIMContext* webViewWidgetGetIMContext(WebViewWidget* webViewWidget)
{
    return webViewWidget->priv->imContext;
}
