/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2013 Gustavo Noronha Silva <gns@gnome.org>.
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
#include "WebKitWebViewBase.h"

#include "APIPageConfiguration.h"
#include "DrawingAreaProxyImpl.h"
#include "InputMethodFilter.h"
#include "KeyBindingTranslator.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "RedirectedXCompositeWindow.h"
#include "ViewState.h"
#include "WebEventFactory.h"
#include "WebFullScreenClientGtk.h"
#include "WebInspectorProxy.h"
#include "WebKitAuthenticationDialog.h"
#include "WebKitPrivate.h"
#include "WebKitWebViewBaseAccessible.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PasteboardHelper.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/Region.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if PLATFORM(X11)
#include <gdk/gdkx.h>
#endif

#if PLATFORM(WAYLAND)
#include <gdk/gdkwayland.h>
#endif

// gtk_widget_get_scale_factor() appeared in GTK 3.10, but we also need
// to make sure we have cairo new enough to support cairo_surface_set_device_scale
#define HAVE_GTK_SCALE_FACTOR HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE && GTK_CHECK_VERSION(3, 10, 0)

using namespace WebKit;
using namespace WebCore;

struct ClickCounter {
public:
    void reset()
    {
        currentClickCount = 0;
        previousClickPoint = IntPoint();
        previousClickTime = 0;
        previousClickButton = 0;
    }

    int currentClickCountForGdkButtonEvent(GdkEventButton* buttonEvent)
    {
        GdkEvent* event = reinterpret_cast<GdkEvent*>(buttonEvent);
        int doubleClickDistance = 250;
        int doubleClickTime = 5;
        g_object_get(gtk_settings_get_for_screen(gdk_event_get_screen(event)),
            "gtk-double-click-distance", &doubleClickDistance, "gtk-double-click-time", &doubleClickTime, nullptr);

        // GTK+ only counts up to triple clicks, but WebCore wants to know about
        // quadruple clicks, quintuple clicks, ad infinitum. Here, we replicate the
        // GDK logic for counting clicks.
        guint32 eventTime = gdk_event_get_time(event);
        if (!eventTime) {
            // Real events always have a non-zero time, but events synthesized
            // by the WTR do not and we must calculate a time manually. This time
            // is not calculated in the WTR, because GTK+ does not work well with
            // anything other than GDK_CURRENT_TIME on synthesized events.
            GTimeVal timeValue;
            g_get_current_time(&timeValue);
            eventTime = (timeValue.tv_sec * 1000) + (timeValue.tv_usec / 1000);
        }

        if ((event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
            || ((std::abs(buttonEvent->x - previousClickPoint.x()) < doubleClickDistance)
                && (std::abs(buttonEvent->y - previousClickPoint.y()) < doubleClickDistance)
                && (eventTime - previousClickTime < static_cast<unsigned>(doubleClickTime))
                && (buttonEvent->button == previousClickButton)))
            currentClickCount++;
        else
            currentClickCount = 1;

        double x, y;
        gdk_event_get_coords(event, &x, &y);
        previousClickPoint = IntPoint(x, y);
        previousClickButton = buttonEvent->button;
        previousClickTime = eventTime;

        return currentClickCount;
    }

private:
    int currentClickCount;
    IntPoint previousClickPoint;
    unsigned previousClickButton;
    int previousClickTime;
};

typedef HashMap<GtkWidget*, IntRect> WebKitWebViewChildrenMap;
typedef HashMap<uint32_t, GUniquePtr<GdkEvent>> TouchEventsMap;

struct _WebKitWebViewBasePrivate {
    _WebKitWebViewBasePrivate()
        : updateViewStateTimer(RunLoop::main(), this, &_WebKitWebViewBasePrivate::updateViewStateTimerFired)
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
        , clearRedirectedWindowSoonTimer(RunLoop::main(), this, &_WebKitWebViewBasePrivate::clearRedirectedWindowSoonTimerFired)
#endif
    {
    }

    void updateViewStateTimerFired()
    {
        if (!pageProxy)
            return;
        pageProxy->viewStateDidChange(viewStateFlagsToUpdate);
        viewStateFlagsToUpdate = ViewState::NoFlags;
    }

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    void clearRedirectedWindowSoonTimerFired()
    {
        if (redirectedWindow)
            redirectedWindow->resize(IntSize());
    }
#endif

    WebKitWebViewChildrenMap children;
    std::unique_ptr<PageClientImpl> pageClient;
    RefPtr<WebPageProxy> pageProxy;
    bool shouldForwardNextKeyEvent;
    ClickCounter clickCounter;
    CString tooltipText;
    IntRect tooltipArea;
    GRefPtr<AtkObject> accessible;
    GtkWidget* authenticationDialog;
    GtkWidget* inspectorView;
    AttachmentSide inspectorAttachmentSide;
    unsigned inspectorViewSize;
    GUniquePtr<GdkEvent> contextMenuEvent;
    WebContextMenuProxyGtk* activeContextMenuProxy;
    InputMethodFilter inputMethodFilter;
    KeyBindingTranslator keyBindingTranslator;
    TouchEventsMap touchEvents;

    GtkWindow* toplevelOnScreenWindow;
    unsigned long toplevelFocusInEventID;
    unsigned long toplevelFocusOutEventID;
    unsigned long toplevelWindowStateEventID;

    // View State.
    ViewState::Flags viewState;
    ViewState::Flags viewStateFlagsToUpdate;
    RunLoop::Timer<WebKitWebViewBasePrivate> updateViewStateTimer;

    WebKitWebViewBaseDownloadRequestHandler downloadHandler;

#if ENABLE(FULLSCREEN_API)
    bool fullScreenModeActive;
    WebFullScreenClientGtk fullScreenClient;
    GRefPtr<GDBusProxy> screenSaverProxy;
    GRefPtr<GCancellable> screenSaverInhibitCancellable;
    unsigned screenSaverCookie;
#endif

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    std::unique_ptr<RedirectedXCompositeWindow> redirectedWindow;
    RunLoop::Timer<WebKitWebViewBasePrivate> clearRedirectedWindowSoonTimer;
#endif

#if ENABLE(DRAG_SUPPORT)
    std::unique_ptr<DragAndDropHandler> dragAndDropHandler;
#endif

#if HAVE(GTK_GESTURES)
    std::unique_ptr<GestureController> gestureController;
#endif
};

WEBKIT_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_CONTAINER)

static void webkitWebViewBaseScheduleUpdateViewState(WebKitWebViewBase* webViewBase, ViewState::Flags flagsToUpdate)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->viewStateFlagsToUpdate |= flagsToUpdate;
    if (priv->updateViewStateTimer.isActive())
        return;

    priv->updateViewStateTimer.startOneShot(0);
}

static gboolean toplevelWindowFocusInEvent(GtkWidget*, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->viewState & ViewState::WindowIsActive)
        return FALSE;

    priv->viewState |= ViewState::WindowIsActive;
    webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::WindowIsActive);

    return FALSE;
}

static gboolean toplevelWindowFocusOutEvent(GtkWidget*, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(priv->viewState & ViewState::WindowIsActive))
        return FALSE;

    priv->viewState &= ~ViewState::WindowIsActive;
    webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::WindowIsActive);

    return FALSE;
}

static gboolean toplevelWindowStateEvent(GtkWidget*, GdkEventWindowState* event, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(event->changed_mask & GDK_WINDOW_STATE_ICONIFIED))
        return FALSE;

    bool visible = !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED);
    if ((visible && priv->viewState & ViewState::IsVisible) || (!visible && !(priv->viewState & ViewState::IsVisible)))
        return FALSE;

    if (visible)
        priv->viewState |= ViewState::IsVisible;
    else
        priv->viewState &= ~ViewState::IsVisible;
    webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::IsVisible);

    return FALSE;
}

static void webkitWebViewBaseSetToplevelOnScreenWindow(WebKitWebViewBase* webViewBase, GtkWindow* window)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->toplevelOnScreenWindow == window)
        return;

    if (priv->toplevelFocusInEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelFocusInEventID);
        priv->toplevelFocusInEventID = 0;
    }
    if (priv->toplevelFocusOutEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelFocusOutEventID);
        priv->toplevelFocusOutEventID = 0;
    }
    if (priv->toplevelWindowStateEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelWindowStateEventID);
        priv->toplevelWindowStateEventID = 0;
    }

    priv->toplevelOnScreenWindow = window;
    if (!(priv->viewState & ViewState::IsInWindow)) {
        priv->viewState |= ViewState::IsInWindow;
        webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::IsInWindow);
    }
    if (!priv->toplevelOnScreenWindow)
        return;

    priv->toplevelFocusInEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-in-event",
                         G_CALLBACK(toplevelWindowFocusInEvent), webViewBase);
    priv->toplevelFocusOutEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-out-event",
                         G_CALLBACK(toplevelWindowFocusOutEvent), webViewBase);
    priv->toplevelWindowStateEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "window-state-event", G_CALLBACK(toplevelWindowStateEvent), webViewBase);
}

static void webkitWebViewBaseRealize(GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webView->priv;

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11) {
        ASSERT(!priv->redirectedWindow);
        priv->redirectedWindow = RedirectedXCompositeWindow::create(
            gtk_widget_get_parent_window(widget),
            [webView] {
                DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(webView->priv->pageProxy->drawingArea());
                if (drawingArea && drawingArea->isInAcceleratedCompositingMode())
                    gtk_widget_queue_draw(GTK_WIDGET(webView));
            });
        if (priv->redirectedWindow) {
            if (DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(priv->pageProxy->drawingArea()))
                drawingArea->setNativeSurfaceHandleForCompositing(priv->redirectedWindow->windowID());
        }
    }
#endif

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
    attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK
        | GDK_EXPOSURE_MASK
        | GDK_BUTTON_PRESS_MASK
        | GDK_BUTTON_RELEASE_MASK
        | GDK_SCROLL_MASK
        | GDK_SMOOTH_SCROLL_MASK
        | GDK_POINTER_MOTION_MASK
        | GDK_KEY_PRESS_MASK
        | GDK_KEY_RELEASE_MASK
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON1_MOTION_MASK
        | GDK_BUTTON2_MOTION_MASK
        | GDK_BUTTON3_MOTION_MASK
        | GDK_TOUCH_MASK;

    gint attributesMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    GdkWindow* window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributesMask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);

#if USE(TEXTURE_MAPPER) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(priv->pageProxy->drawingArea()))
        drawingArea->setNativeSurfaceHandleForCompositing(GDK_WINDOW_XID(window));
#endif

    gtk_style_context_set_background(gtk_widget_get_style_context(widget), window);

    gtk_im_context_set_client_window(priv->inputMethodFilter.context(), window);

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (widgetIsOnscreenToplevelWindow(toplevel))
        webkitWebViewBaseSetToplevelOnScreenWindow(webView, GTK_WINDOW(toplevel));
}

static void webkitWebViewBaseUnrealize(GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
#if USE(TEXTURE_MAPPER) && PLATFORM(X11)
    if (DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(webView->priv->pageProxy->drawingArea()))
        drawingArea->destroyNativeSurfaceHandleForCompositing();
#endif
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    webView->priv->redirectedWindow = nullptr;
#endif
    gtk_im_context_set_client_window(webView->priv->inputMethodFilter.context(), nullptr);

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unrealize(widget);
}

static bool webkitWebViewChildIsInternalWidget(WebKitWebViewBase* webViewBase, GtkWidget* widget)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    return widget == priv->inspectorView || widget == priv->authenticationDialog;
}

static void webkitWebViewBaseContainerAdd(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;

    // Internal widgets like the web inspector and authentication dialog have custom
    // allocations so we don't need to add them to our list of children.
    if (!webkitWebViewChildIsInternalWidget(webView, widget)) {
        GtkAllocation childAllocation;
        gtk_widget_get_allocation(widget, &childAllocation);
        priv->children.set(widget, childAllocation);
    }

    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

void webkitWebViewBaseAddAuthenticationDialog(WebKitWebViewBase* webViewBase, GtkWidget* dialog)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->authenticationDialog = dialog;
    gtk_container_add(GTK_CONTAINER(webViewBase), dialog);
    gtk_widget_show(dialog);

    // We need to draw the shadow over the widget.
    gtk_widget_queue_draw(GTK_WIDGET(webViewBase));
}

void webkitWebViewBaseAddWebInspector(WebKitWebViewBase* webViewBase, GtkWidget* inspector, AttachmentSide attachmentSide)
{
    if (webViewBase->priv->inspectorView == inspector && webViewBase->priv->inspectorAttachmentSide == attachmentSide)
        return;

    webViewBase->priv->inspectorAttachmentSide = attachmentSide;

    if (webViewBase->priv->inspectorView == inspector) {
        gtk_widget_queue_resize(GTK_WIDGET(webViewBase));
        return;
    }

    webViewBase->priv->inspectorView = inspector;
    gtk_container_add(GTK_CONTAINER(webViewBase), inspector);
}

static void webkitWebViewBaseContainerRemove(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;
    GtkWidget* widgetContainer = GTK_WIDGET(container);

    gboolean wasVisible = gtk_widget_get_visible(widget);
    gtk_widget_unparent(widget);

    if (priv->inspectorView == widget) {
        priv->inspectorView = 0;
        priv->inspectorViewSize = 0;
    } else if (priv->authenticationDialog == widget) {
        priv->authenticationDialog = 0;
    } else {
        ASSERT(priv->children.contains(widget));
        priv->children.remove(widget);
    }
    if (wasVisible && gtk_widget_get_visible(widgetContainer))
        gtk_widget_queue_resize(widgetContainer);
}

static void webkitWebViewBaseContainerForall(GtkContainer* container, gboolean includeInternals, GtkCallback callback, gpointer callbackData)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;

    Vector<GtkWidget*> children;
    copyKeysToVector(priv->children, children);
    for (const auto& child : children) {
        if (priv->children.contains(child))
            (*callback)(child, callbackData);
    }

    if (includeInternals && priv->inspectorView)
        (*callback)(priv->inspectorView, callbackData);

    if (includeInternals && priv->authenticationDialog)
        (*callback)(priv->authenticationDialog, callbackData);
}

void webkitWebViewBaseChildMoveResize(WebKitWebViewBase* webView, GtkWidget* child, const IntRect& childRect)
{
    const IntRect& geometry = webView->priv->children.get(child);
    if (geometry == childRect)
        return;

    webView->priv->children.set(child, childRect);
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webView));
}

static void webkitWebViewBaseDispose(GObject* gobject)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(gobject);
    g_cancellable_cancel(webView->priv->screenSaverInhibitCancellable.get());
    webkitWebViewBaseSetToplevelOnScreenWindow(webView, nullptr);
    webView->priv->pageProxy->close();
    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->dispose(gobject);
}

static void webkitWebViewBaseConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->constructed(object);

    GtkWidget* viewWidget = GTK_WIDGET(object);
    gtk_widget_set_can_focus(viewWidget, TRUE);
    gtk_drag_dest_set(viewWidget, static_cast<GtkDestDefaults>(0), nullptr, 0,
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_PRIVATE));
    gtk_drag_dest_set_target_list(viewWidget, PasteboardHelper::singleton().targetList());

    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(object)->priv;
    priv->pageClient = std::make_unique<PageClientImpl>(viewWidget);
    priv->authenticationDialog = 0;
}

#if USE(TEXTURE_MAPPER)
static bool webkitWebViewRenderAcceleratedCompositingResults(WebKitWebViewBase* webViewBase, DrawingAreaProxyImpl* drawingArea, cairo_t* cr, GdkRectangle* clipRect)
{
    ASSERT(drawingArea);

    if (!drawingArea->isInAcceleratedCompositingMode())
        return false;

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    // To avoid flashes when initializing accelerated compositing for the first
    // time, we wait until we know there's a frame ready before rendering.
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->redirectedWindow)
        return false;

    priv->redirectedWindow->setDeviceScaleFactor(webViewBase->priv->pageProxy->deviceScaleFactor());
    priv->redirectedWindow->resize(drawingArea->size());

    if (cairo_surface_t* surface = priv->redirectedWindow->surface()) {
        cairo_rectangle(cr, clipRect->x, clipRect->y, clipRect->width, clipRect->height);
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_fill(cr);
    }

    return true;
#else
    UNUSED_PARAM(webViewBase);
    UNUSED_PARAM(cr);
    UNUSED_PARAM(clipRect);
    return false;
#endif
}
#endif

static gboolean webkitWebViewBaseDraw(GtkWidget* widget, cairo_t* cr)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(webViewBase->priv->pageProxy->drawingArea());
    if (!drawingArea)
        return FALSE;

    GdkRectangle clipRect;
    if (!gdk_cairo_get_clip_rectangle(cr, &clipRect))
        return FALSE;

#if USE(TEXTURE_MAPPER)
    if (webkitWebViewRenderAcceleratedCompositingResults(webViewBase, drawingArea, cr, &clipRect))
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->draw(widget, cr);
#endif

    WebCore::Region unpaintedRegion; // This is simply unused.
    drawingArea->paint(cr, clipRect, unpaintedRegion);

    if (webViewBase->priv->authenticationDialog) {
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
        cairo_paint(cr);
    }

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->draw(widget, cr);

    return FALSE;
}

static void webkitWebViewBaseChildAllocate(GtkWidget* child, gpointer userData)
{
    if (!gtk_widget_get_visible(child))
        return;

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(userData);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    const IntRect& geometry = priv->children.get(child);
    if (geometry.isEmpty())
        return;

    GtkAllocation childAllocation = geometry;
    gtk_widget_size_allocate(child, &childAllocation);
    priv->children.set(child, IntRect());
}

static void webkitWebViewBaseSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, allocation);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    gtk_container_foreach(GTK_CONTAINER(webViewBase), webkitWebViewBaseChildAllocate, webViewBase);

    IntRect viewRect(allocation->x, allocation->y, allocation->width, allocation->height);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->inspectorView) {
        GtkAllocation childAllocation = viewRect;

        if (priv->inspectorAttachmentSide == AttachmentSide::Bottom) {
            int inspectorViewHeight = std::min(static_cast<int>(priv->inspectorViewSize), allocation->height);
            childAllocation.x = 0;
            childAllocation.y = allocation->height - inspectorViewHeight;
            childAllocation.height = inspectorViewHeight;
            viewRect.setHeight(std::max(allocation->height - inspectorViewHeight, 1));
        } else {
            int inspectorViewWidth = std::min(static_cast<int>(priv->inspectorViewSize), allocation->width);
            childAllocation.y = 0;
            childAllocation.x = allocation->width - inspectorViewWidth;
            childAllocation.width = inspectorViewWidth;
            viewRect.setWidth(std::max(allocation->width - inspectorViewWidth, 1));
        }

        gtk_widget_size_allocate(priv->inspectorView, &childAllocation);
    }

    // The authentication dialog is centered in the view rect, which means that it
    // never overlaps the web inspector. Thus, we need to calculate the allocation here
    // after calculating the inspector allocation.
    if (priv->authenticationDialog) {
        GtkRequisition naturalSize;
        gtk_widget_get_preferred_size(priv->authenticationDialog, 0, &naturalSize);

        GtkAllocation childAllocation = {
            (viewRect.width() - naturalSize.width) / 2,
            (viewRect.height() - naturalSize.height) / 2,
            naturalSize.width,
            naturalSize.height
        };
        gtk_widget_size_allocate(priv->authenticationDialog, &childAllocation);
    }

    DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(priv->pageProxy->drawingArea());
    if (!drawingArea)
        return;


#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (priv->redirectedWindow && drawingArea->isInAcceleratedCompositingMode())
        priv->redirectedWindow->resize(viewRect.size());
#endif

    drawingArea->setSize(viewRect.size(), IntSize(), IntSize());
}

static void webkitWebViewBaseMap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->map(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->viewState & ViewState::IsVisible)
        return;

    priv->viewState |= ViewState::IsVisible;
    webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::IsVisible);
}

static void webkitWebViewBaseUnmap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unmap(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(priv->viewState & ViewState::IsVisible))
        return;

    priv->viewState &= ~ViewState::IsVisible;
    webkitWebViewBaseScheduleUpdateViewState(webViewBase, ViewState::IsVisible);
}

static gboolean webkitWebViewBaseFocusInEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    webkitWebViewBaseSetFocus(webViewBase, true);
    webViewBase->priv->inputMethodFilter.notifyFocusedIn();

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_in_event(widget, event);
}

static gboolean webkitWebViewBaseFocusOutEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    webkitWebViewBaseSetFocus(webViewBase, false);
    webViewBase->priv->inputMethodFilter.notifyFocusedOut();

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_out_event(widget, event);
}

static gboolean webkitWebViewBaseKeyPressEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_press_event(widget, event);

#if ENABLE(FULLSCREEN_API)
    if (priv->fullScreenModeActive) {
        switch (event->keyval) {
        case GDK_KEY_Escape:
        case GDK_KEY_f:
        case GDK_KEY_F:
            priv->pageProxy->fullScreenManager()->requestExitFullScreen();
            return TRUE;
        default:
            break;
        }
    }
#endif

    // Since WebProcess key event handling is not synchronous, handle the event in two passes.
    // When WebProcess processes the input event, it will call PageClientImpl::doneWithKeyEvent
    // with event handled status which determines whether to pass the input event to parent or not
    // using gtk_main_do_event().
    if (priv->shouldForwardNextKeyEvent) {
        priv->shouldForwardNextKeyEvent = FALSE;
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_press_event(widget, event);
    }

    priv->inputMethodFilter.filterKeyEvent(event, [priv, event](const WebCore::CompositionResults& compositionResults, InputMethodFilter::EventFakedForComposition faked) {
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event), compositionResults, faked,
            !compositionResults.compositionUpdated() ? priv->keyBindingTranslator.commandsForKeyEvent(event) : Vector<String>()));
    });

    return TRUE;
}

static gboolean webkitWebViewBaseKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->shouldForwardNextKeyEvent) {
        priv->shouldForwardNextKeyEvent = FALSE;
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_release_event(widget, event);
    }

    priv->inputMethodFilter.filterKeyEvent(event, [priv, event](const WebCore::CompositionResults& compositionResults, InputMethodFilter::EventFakedForComposition faked) {
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event), compositionResults, faked, { }));
    });

    return TRUE;
}

static gboolean webkitWebViewBaseButtonPressEvent(GtkWidget* widget, GdkEventButton* buttonEvent)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return TRUE;

    gtk_widget_grab_focus(widget);

    priv->inputMethodFilter.notifyMouseButtonPress();

    // For double and triple clicks GDK sends both a normal button press event
    // and a specific type (like GDK_2BUTTON_PRESS). If we detect a special press
    // coming up, ignore this event as it certainly generated the double or triple
    // click. The consequence of not eating this event is two DOM button press events
    // are generated.
    GUniquePtr<GdkEvent> nextEvent(gdk_event_peek());
    if (nextEvent && (nextEvent->any.type == GDK_2BUTTON_PRESS || nextEvent->any.type == GDK_3BUTTON_PRESS))
        return TRUE;

    // If it's a right click event save it as a possible context menu event.
    if (buttonEvent->button == 3)
        priv->contextMenuEvent.reset(gdk_event_copy(reinterpret_cast<GdkEvent*>(buttonEvent)));

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(buttonEvent),
        priv->clickCounter.currentClickCountForGdkButtonEvent(buttonEvent)));
    return TRUE;
}

static gboolean webkitWebViewBaseButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return TRUE;

    gtk_widget_grab_focus(widget);
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return TRUE;
}

static gboolean webkitWebViewBaseScrollEvent(GtkWidget* widget, GdkEventScroll* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return TRUE;

    priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(reinterpret_cast<GdkEvent*>(event)));

    return TRUE;
}

static gboolean webkitWebViewBaseMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->motion_notify_event(widget, event);

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return FALSE;
}

#if ENABLE(TOUCH_EVENTS)
static void appendTouchEvent(Vector<WebPlatformTouchPoint>& touchPoints, const GdkEvent* event, WebPlatformTouchPoint::TouchPointState state)
{
    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);

    gdouble xRoot, yRoot;
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    uint32_t identifier = GPOINTER_TO_UINT(gdk_event_get_event_sequence(event));
    touchPoints.uncheckedAppend(WebPlatformTouchPoint(identifier, state, IntPoint(xRoot, yRoot), IntPoint(x, y)));
}

static inline WebPlatformTouchPoint::TouchPointState touchPointStateForEvents(const GdkEvent* current, const GdkEvent* event)
{
    if (gdk_event_get_event_sequence(current) != gdk_event_get_event_sequence(event))
        return WebPlatformTouchPoint::TouchStationary;

    switch (current->type) {
    case GDK_TOUCH_UPDATE:
        return WebPlatformTouchPoint::TouchMoved;
    case GDK_TOUCH_BEGIN:
        return WebPlatformTouchPoint::TouchPressed;
    case GDK_TOUCH_END:
        return WebPlatformTouchPoint::TouchReleased;
    default:
        return WebPlatformTouchPoint::TouchStationary;
    }
}

static void webkitWebViewBaseGetTouchPointsForEvent(WebKitWebViewBase* webViewBase, GdkEvent* event, Vector<WebPlatformTouchPoint>& touchPoints)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    touchPoints.reserveInitialCapacity(event->type == GDK_TOUCH_END ? priv->touchEvents.size() + 1 : priv->touchEvents.size());

    for (const auto& it : priv->touchEvents)
        appendTouchEvent(touchPoints, it.value.get(), touchPointStateForEvents(it.value.get(), event));

    // Touch was already removed from the TouchEventsMap, add it here.
    if (event->type == GDK_TOUCH_END)
        appendTouchEvent(touchPoints, event, WebPlatformTouchPoint::TouchReleased);
}

static gboolean webkitWebViewBaseTouchEvent(GtkWidget* widget, GdkEventTouch* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->authenticationDialog)
        return TRUE;

    GdkEvent* touchEvent = reinterpret_cast<GdkEvent*>(event);

#if HAVE(GTK_GESTURES)
    GestureController& gestureController = webkitWebViewBaseGestureController(webViewBase);
    if (gestureController.isProcessingGestures()) {
        // If we are already processing gestures is because the WebProcess didn't handle the
        // BEGIN touch event, so pass subsequent events to the GestureController.
        gestureController.handleEvent(touchEvent);
        return TRUE;
    }
#endif

    uint32_t sequence = GPOINTER_TO_UINT(gdk_event_get_event_sequence(touchEvent));
    switch (touchEvent->type) {
    case GDK_TOUCH_BEGIN: {
        ASSERT(!priv->touchEvents.contains(sequence));
        GUniquePtr<GdkEvent> event(gdk_event_copy(touchEvent));
        priv->touchEvents.add(sequence, WTFMove(event));
        break;
    }
    case GDK_TOUCH_UPDATE: {
        auto it = priv->touchEvents.find(sequence);
        ASSERT(it != priv->touchEvents.end());
        it->value.reset(gdk_event_copy(touchEvent));
        break;
    }
    case GDK_TOUCH_END:
        ASSERT(priv->touchEvents.contains(sequence));
        priv->touchEvents.remove(sequence);
        break;
    default:
        break;
    }

    Vector<WebPlatformTouchPoint> touchPoints;
    webkitWebViewBaseGetTouchPointsForEvent(webViewBase, touchEvent, touchPoints);
    priv->pageProxy->handleTouchEvent(NativeWebTouchEvent(reinterpret_cast<GdkEvent*>(event), WTFMove(touchPoints)));

    return TRUE;
}
#endif // ENABLE(TOUCH_EVENTS)

#if HAVE(GTK_GESTURES)
GestureController& webkitWebViewBaseGestureController(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->gestureController)
        priv->gestureController = std::make_unique<GestureController>(*priv->pageProxy);
    return *priv->gestureController;
}
#endif

static gboolean webkitWebViewBaseQueryTooltip(GtkWidget* widget, gint /* x */, gint /* y */, gboolean keyboardMode, GtkTooltip* tooltip)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;

    if (keyboardMode) {
        // TODO: https://bugs.webkit.org/show_bug.cgi?id=61732.
        notImplemented();
        return FALSE;
    }

    if (priv->tooltipText.length() <= 0)
        return FALSE;

    if (!priv->tooltipArea.isEmpty()) {
        GdkRectangle area = priv->tooltipArea;
        gtk_tooltip_set_tip_area(tooltip, &area);
    } else
        gtk_tooltip_set_tip_area(tooltip, 0);
    gtk_tooltip_set_text(tooltip, priv->tooltipText.data());

    return TRUE;
}

#if ENABLE(DRAG_SUPPORT)
static void webkitWebViewBaseDragDataGet(GtkWidget* widget, GdkDragContext* context, GtkSelectionData* selectionData, guint info, guint /* time */)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    ASSERT(priv->dragAndDropHandler);
    priv->dragAndDropHandler->fillDragData(context, selectionData, info);
}

static void webkitWebViewBaseDragEnd(GtkWidget* widget, GdkDragContext* context)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    ASSERT(priv->dragAndDropHandler);
    priv->dragAndDropHandler->finishDrag(context);
}

static void webkitWebViewBaseDragDataReceived(GtkWidget* widget, GdkDragContext* context, gint /* x */, gint /* y */, GtkSelectionData* selectionData, guint info, guint time)
{
    webkitWebViewBaseDragAndDropHandler(WEBKIT_WEB_VIEW_BASE(widget)).dragEntered(context, selectionData, info, time);
}
#endif // ENABLE(DRAG_SUPPORT)

static AtkObject* webkitWebViewBaseGetAccessible(GtkWidget* widget)
{
    // If the socket has already been created and embedded a plug ID, return it.
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (priv->accessible && atk_socket_is_occupied(ATK_SOCKET(priv->accessible.get())))
        return priv->accessible.get();

    // Create the accessible object and associate it to the widget.
    if (!priv->accessible) {
        priv->accessible = adoptGRef(ATK_OBJECT(webkitWebViewBaseAccessibleNew(widget)));

        // Set the parent not to break bottom-up navigation.
        GtkWidget* parentWidget = gtk_widget_get_parent(widget);
        AtkObject* axParent = parentWidget ? gtk_widget_get_accessible(parentWidget) : 0;
        if (axParent)
            atk_object_set_parent(priv->accessible.get(), axParent);
    }

    // Try to embed the plug in the socket, if posssible.
    String plugID = priv->pageProxy->accessibilityPlugID();
    if (plugID.isNull())
        return priv->accessible.get();

    atk_socket_embed(ATK_SOCKET(priv->accessible.get()), const_cast<gchar*>(plugID.utf8().data()));

    return priv->accessible.get();
}

#if ENABLE(DRAG_SUPPORT)
static gboolean webkitWebViewBaseDragMotion(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time)
{
    webkitWebViewBaseDragAndDropHandler(WEBKIT_WEB_VIEW_BASE(widget)).dragMotion(context, IntPoint(x, y), time);
    return TRUE;
}

static void webkitWebViewBaseDragLeave(GtkWidget* widget, GdkDragContext* context, guint /* time */)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    ASSERT(priv->dragAndDropHandler);
    priv->dragAndDropHandler->dragLeave(context);
}

static gboolean webkitWebViewBaseDragDrop(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    ASSERT(priv->dragAndDropHandler);
    return priv->dragAndDropHandler->drop(context, IntPoint(x, y), time);
}
#endif // ENABLE(DRAG_SUPPORT)

static void webkitWebViewBaseParentSet(GtkWidget* widget, GtkWidget* /* oldParent */)
{
    if (!gtk_widget_get_parent(widget))
        webkitWebViewBaseSetToplevelOnScreenWindow(WEBKIT_WEB_VIEW_BASE(widget), 0);
}

static gboolean webkitWebViewBaseFocus(GtkWidget* widget, GtkDirectionType direction)
{
    // If the authentication dialog is active, we need to forward focus events there. This
    // ensures that you can tab between elements in the box.
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (priv->authenticationDialog) {
        gboolean returnValue;
        g_signal_emit_by_name(priv->authenticationDialog, "focus", direction, &returnValue);
        return returnValue;
    }

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus(widget, direction);
}

static void webkitWebViewBaseDestroy(GtkWidget* widget)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (priv->authenticationDialog)
        gtk_widget_destroy(priv->authenticationDialog);

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->destroy(widget);
}

static void webkit_web_view_base_class_init(WebKitWebViewBaseClass* webkitWebViewBaseClass)
{
    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webkitWebViewBaseClass);
    widgetClass->realize = webkitWebViewBaseRealize;
    widgetClass->unrealize = webkitWebViewBaseUnrealize;
    widgetClass->draw = webkitWebViewBaseDraw;
    widgetClass->size_allocate = webkitWebViewBaseSizeAllocate;
    widgetClass->map = webkitWebViewBaseMap;
    widgetClass->unmap = webkitWebViewBaseUnmap;
    widgetClass->focus = webkitWebViewBaseFocus;
    widgetClass->focus_in_event = webkitWebViewBaseFocusInEvent;
    widgetClass->focus_out_event = webkitWebViewBaseFocusOutEvent;
    widgetClass->key_press_event = webkitWebViewBaseKeyPressEvent;
    widgetClass->key_release_event = webkitWebViewBaseKeyReleaseEvent;
    widgetClass->button_press_event = webkitWebViewBaseButtonPressEvent;
    widgetClass->button_release_event = webkitWebViewBaseButtonReleaseEvent;
    widgetClass->scroll_event = webkitWebViewBaseScrollEvent;
    widgetClass->motion_notify_event = webkitWebViewBaseMotionNotifyEvent;
#if ENABLE(TOUCH_EVENTS)
    widgetClass->touch_event = webkitWebViewBaseTouchEvent;
#endif
    widgetClass->query_tooltip = webkitWebViewBaseQueryTooltip;
#if ENABLE(DRAG_SUPPORT)
    widgetClass->drag_end = webkitWebViewBaseDragEnd;
    widgetClass->drag_data_get = webkitWebViewBaseDragDataGet;
    widgetClass->drag_motion = webkitWebViewBaseDragMotion;
    widgetClass->drag_leave = webkitWebViewBaseDragLeave;
    widgetClass->drag_drop = webkitWebViewBaseDragDrop;
    widgetClass->drag_data_received = webkitWebViewBaseDragDataReceived;
#endif // ENABLE(DRAG_SUPPORT)
    widgetClass->get_accessible = webkitWebViewBaseGetAccessible;
    widgetClass->parent_set = webkitWebViewBaseParentSet;
    widgetClass->destroy = webkitWebViewBaseDestroy;

    GObjectClass* gobjectClass = G_OBJECT_CLASS(webkitWebViewBaseClass);
    gobjectClass->constructed = webkitWebViewBaseConstructed;
    gobjectClass->dispose = webkitWebViewBaseDispose;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webkitWebViewBaseClass);
    containerClass->add = webkitWebViewBaseContainerAdd;
    containerClass->remove = webkitWebViewBaseContainerRemove;
    containerClass->forall = webkitWebViewBaseContainerForall;
}

WebKitWebViewBase* webkitWebViewBaseCreate(const API::PageConfiguration& configuration)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(g_object_new(WEBKIT_TYPE_WEB_VIEW_BASE, nullptr));
    webkitWebViewBaseCreateWebPage(webkitWebViewBase, configuration.copy());
    return webkitWebViewBase;
}

GtkIMContext* webkitWebViewBaseGetIMContext(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->inputMethodFilter.context();
}

WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->pageProxy.get();
}

#if HAVE(GTK_SCALE_FACTOR)
static void deviceScaleFactorChanged(WebKitWebViewBase* webkitWebViewBase)
{
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (webkitWebViewBase->priv->redirectedWindow)
        webkitWebViewBase->priv->redirectedWindow->setDeviceScaleFactor(webkitWebViewBase->priv->pageProxy->deviceScaleFactor());
#endif
    webkitWebViewBase->priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
}
#endif // HAVE(GTK_SCALE_FACTOR)

void webkitWebViewBaseCreateWebPage(WebKitWebViewBase* webkitWebViewBase, Ref<API::PageConfiguration>&& configuration)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    WebProcessPool* context = configuration->processPool();
    priv->pageProxy = context->createWebPage(*priv->pageClient, WTFMove(configuration));
    priv->pageProxy->initializeWebPage();

    priv->inputMethodFilter.setPage(priv->pageProxy.get());

#if HAVE(GTK_SCALE_FACTOR)
    // We attach this here, because changes in scale factor are passed directly to the page proxy.
    priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
    g_signal_connect(webkitWebViewBase, "notify::scale-factor", G_CALLBACK(deviceScaleFactorChanged), nullptr);
#endif
}

void webkitWebViewBaseSetTooltipText(WebKitWebViewBase* webViewBase, const char* tooltip)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (tooltip && tooltip[0] != '\0') {
        priv->tooltipText = tooltip;
        gtk_widget_set_has_tooltip(GTK_WIDGET(webViewBase), TRUE);
    } else {
        priv->tooltipText = "";
        gtk_widget_set_has_tooltip(GTK_WIDGET(webViewBase), FALSE);
    }

    gtk_widget_trigger_tooltip_query(GTK_WIDGET(webViewBase));
}

void webkitWebViewBaseSetTooltipArea(WebKitWebViewBase* webViewBase, const IntRect& tooltipArea)
{
    webViewBase->priv->tooltipArea = tooltipArea;
}

#if ENABLE(DRAG_SUPPORT)
DragAndDropHandler& webkitWebViewBaseDragAndDropHandler(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->dragAndDropHandler)
        priv->dragAndDropHandler = std::make_unique<DragAndDropHandler>(*priv->pageProxy);
    return *priv->dragAndDropHandler;
}
#endif // ENABLE(DRAG_SUPPORT)

void webkitWebViewBaseForwardNextKeyEvent(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->shouldForwardNextKeyEvent = TRUE;
}

#if ENABLE(FULLSCREEN_API)
static void screenSaverInhibitedCallback(GDBusProxy* screenSaverProxy, GAsyncResult* result, WebKitWebViewBase* webViewBase)
{
    GRefPtr<GVariant> returnValue = adoptGRef(g_dbus_proxy_call_finish(screenSaverProxy, result, nullptr));
    if (returnValue)
        g_variant_get(returnValue.get(), "(u)", &webViewBase->priv->screenSaverCookie);
    webViewBase->priv->screenSaverInhibitCancellable = nullptr;
}

static void webkitWebViewBaseSendInhibitMessageToScreenSaver(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    ASSERT(priv->screenSaverProxy);
    priv->screenSaverCookie = 0;
    if (!priv->screenSaverInhibitCancellable)
        priv->screenSaverInhibitCancellable = adoptGRef(g_cancellable_new());
    g_dbus_proxy_call(priv->screenSaverProxy.get(), "Inhibit", g_variant_new("(ss)", g_get_prgname(), _("Website running in fullscreen mode")),
        G_DBUS_CALL_FLAGS_NONE, -1, priv->screenSaverInhibitCancellable.get(), reinterpret_cast<GAsyncReadyCallback>(screenSaverInhibitedCallback), webViewBase);
}

static void screenSaverProxyCreatedCallback(GObject*, GAsyncResult* result, WebKitWebViewBase* webViewBase)
{
    // WebKitWebViewBase cancels the proxy creation on dispose, which means this could be called
    // after the web view has been destroyed and g_dbus_proxy_new_for_bus_finish will return nullptr.
    // So, make sure we don't use the web view unless we have a valid proxy.
    // See https://bugs.webkit.org/show_bug.cgi?id=151653.
    GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, nullptr));
    if (!proxy)
        return;

    webViewBase->priv->screenSaverProxy = proxy;
    webkitWebViewBaseSendInhibitMessageToScreenSaver(webViewBase);
}

static void webkitWebViewBaseInhibitScreenSaver(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->screenSaverCookie) {
        // Already inhibited.
        return;
    }

    if (priv->screenSaverProxy) {
        webkitWebViewBaseSendInhibitMessageToScreenSaver(webViewBase);
        return;
    }

    priv->screenSaverInhibitCancellable = adoptGRef(g_cancellable_new());
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION, static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, "org.freedesktop.ScreenSaver", "/ScreenSaver", "org.freedesktop.ScreenSaver", priv->screenSaverInhibitCancellable.get(),
        reinterpret_cast<GAsyncReadyCallback>(screenSaverProxyCreatedCallback), webViewBase);
}

static void webkitWebViewBaseUninhibitScreenSaver(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->screenSaverCookie) {
        // Not inhibited or it's being inhibited.
        g_cancellable_cancel(priv->screenSaverInhibitCancellable.get());
        return;
    }

    // If we have a cookie we should have a proxy.
    ASSERT(priv->screenSaverProxy);
    g_dbus_proxy_call(priv->screenSaverProxy.get(), "UnInhibit", g_variant_new("(u)", priv->screenSaverCookie), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
    priv->screenSaverCookie = 0;
}
#endif

void webkitWebViewBaseEnterFullScreen(WebKitWebViewBase* webkitWebViewBase)
{
#if ENABLE(FULLSCREEN_API)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (priv->fullScreenModeActive)
        return;

    if (!priv->fullScreenClient.willEnterFullScreen())
        return;

    WebFullScreenManagerProxy* fullScreenManagerProxy = priv->pageProxy->fullScreenManager();
    fullScreenManagerProxy->willEnterFullScreen();

    GtkWidget* topLevelWindow = gtk_widget_get_toplevel(GTK_WIDGET(webkitWebViewBase));
    if (gtk_widget_is_toplevel(topLevelWindow))
        gtk_window_fullscreen(GTK_WINDOW(topLevelWindow));
    fullScreenManagerProxy->didEnterFullScreen();
    priv->fullScreenModeActive = true;
    webkitWebViewBaseInhibitScreenSaver(webkitWebViewBase);
#endif
}

void webkitWebViewBaseExitFullScreen(WebKitWebViewBase* webkitWebViewBase)
{
#if ENABLE(FULLSCREEN_API)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (!priv->fullScreenModeActive)
        return;

    if (!priv->fullScreenClient.willExitFullScreen())
        return;

    WebFullScreenManagerProxy* fullScreenManagerProxy = priv->pageProxy->fullScreenManager();
    fullScreenManagerProxy->willExitFullScreen();

    GtkWidget* topLevelWindow = gtk_widget_get_toplevel(GTK_WIDGET(webkitWebViewBase));
    if (gtk_widget_is_toplevel(topLevelWindow))
        gtk_window_unfullscreen(GTK_WINDOW(topLevelWindow));
    fullScreenManagerProxy->didExitFullScreen();
    priv->fullScreenModeActive = false;
    webkitWebViewBaseUninhibitScreenSaver(webkitWebViewBase);
#endif
}

void webkitWebViewBaseInitializeFullScreenClient(WebKitWebViewBase* webkitWebViewBase, const WKFullScreenClientGtkBase* wkClient)
{
    webkitWebViewBase->priv->fullScreenClient.initialize(wkClient);
}

void webkitWebViewBaseSetInspectorViewSize(WebKitWebViewBase* webkitWebViewBase, unsigned size)
{
    if (webkitWebViewBase->priv->inspectorViewSize == size)
        return;
    webkitWebViewBase->priv->inspectorViewSize = size;
    if (webkitWebViewBase->priv->inspectorView)
        gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webkitWebViewBase));
}

static void activeContextMenuUnmapped(GtkMenu* menu, WebKitWebViewBase* webViewBase)
{
    if (webViewBase->priv->activeContextMenuProxy && webViewBase->priv->activeContextMenuProxy->gtkMenu() == menu)
        webViewBase->priv->activeContextMenuProxy = nullptr;
}

void webkitWebViewBaseSetActiveContextMenuProxy(WebKitWebViewBase* webkitWebViewBase, WebContextMenuProxyGtk* contextMenuProxy)
{
    webkitWebViewBase->priv->activeContextMenuProxy = contextMenuProxy;
    g_signal_connect_object(contextMenuProxy->gtkMenu(), "unmap", G_CALLBACK(activeContextMenuUnmapped), webkitWebViewBase, static_cast<GConnectFlags>(0));
}

WebContextMenuProxyGtk* webkitWebViewBaseGetActiveContextMenuProxy(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->activeContextMenuProxy;
}

GdkEvent* webkitWebViewBaseTakeContextMenuEvent(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->contextMenuEvent.release();
}

void webkitWebViewBaseSetFocus(WebKitWebViewBase* webViewBase, bool focused)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if ((focused && priv->viewState & ViewState::IsFocused) || (!focused && !(priv->viewState & ViewState::IsFocused)))
        return;

    ViewState::Flags flagsToUpdate = ViewState::IsFocused;
    if (focused) {
        priv->viewState |= ViewState::IsFocused;

        // If the view has received the focus and the window is not active
        // mark the current window as active now. This can happen if the
        // toplevel window is a GTK_WINDOW_POPUP and the focus has been
        // set programatically like WebKitTestRunner does, because POPUP
        // can't be focused.
        if (!(priv->viewState & ViewState::WindowIsActive)) {
            priv->viewState |= ViewState::WindowIsActive;
            flagsToUpdate |= ViewState::WindowIsActive;
        }
    } else
        priv->viewState &= ~ViewState::IsFocused;

    webkitWebViewBaseScheduleUpdateViewState(webViewBase, flagsToUpdate);
}

bool webkitWebViewBaseIsInWindowActive(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->viewState & ViewState::WindowIsActive;
}

bool webkitWebViewBaseIsFocused(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->viewState & ViewState::IsFocused;
}

bool webkitWebViewBaseIsVisible(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->viewState & ViewState::IsVisible;
}

bool webkitWebViewBaseIsInWindow(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->viewState & ViewState::IsInWindow;
}

void webkitWebViewBaseSetDownloadRequestHandler(WebKitWebViewBase* webViewBase, WebKitWebViewBaseDownloadRequestHandler downloadHandler)
{
    webViewBase->priv->downloadHandler = downloadHandler;
}

void webkitWebViewBaseHandleDownloadRequest(WebKitWebViewBase* webViewBase, DownloadProxy* download)
{
    if (webViewBase->priv->downloadHandler)
        webViewBase->priv->downloadHandler(webViewBase, download);
}

void webkitWebViewBaseSetInputMethodState(WebKitWebViewBase* webkitWebViewBase, bool enabled)
{
    webkitWebViewBase->priv->inputMethodFilter.setEnabled(enabled);
}

void webkitWebViewBaseUpdateTextInputState(WebKitWebViewBase* webkitWebViewBase)
{
    const auto& editorState = webkitWebViewBase->priv->pageProxy->editorState();
    if (!editorState.isMissingPostLayoutData)
        webkitWebViewBase->priv->inputMethodFilter.setCursorRect(editorState.postLayoutData().caretRectAtStart);
}

void webkitWebViewBaseResetClickCounter(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->clickCounter.reset();
}

#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
static void webkitWebViewBaseClearRedirectedWindowSoon(WebKitWebViewBase* webkitWebViewBase)
{
    static const double clearRedirectedWindowSoonDelay = 2;
    webkitWebViewBase->priv->clearRedirectedWindowSoonTimer.startOneShot(clearRedirectedWindowSoonDelay);
}
#endif

void webkitWebViewBaseWillEnterAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase)
{
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (!priv->redirectedWindow)
        return;
    DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(priv->pageProxy->drawingArea());
    if (!drawingArea)
        return;

    priv->redirectedWindow->setDeviceScaleFactor(webkitWebViewBase->priv->pageProxy->deviceScaleFactor());
    priv->redirectedWindow->resize(drawingArea->size());

    // Clear the redirected window if we don't enter AC mode in the end.
    webkitWebViewBaseClearRedirectedWindowSoon(webkitWebViewBase);
#else
    UNUSED_PARAM(webkitWebViewBase);
#endif
}

void webkitWebViewBaseEnterAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase)
{
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    webkitWebViewBase->priv->clearRedirectedWindowSoonTimer.stop();
#else
    UNUSED_PARAM(webkitWebViewBase);
#endif
}

void webkitWebViewBaseExitAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase)
{
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    // Resize the window later to ensure we have already rendered the
    // non composited contents and avoid flickering. We can also avoid the
    // window resize entirely if we switch back to AC mode quickly.
    webkitWebViewBaseClearRedirectedWindowSoon(webkitWebViewBase);
#else
    UNUSED_PARAM(webkitWebViewBase);
#endif
}

void webkitWebViewBaseDidRelaunchWebProcess(WebKitWebViewBase* webkitWebViewBase)
{
    // Queue a resize to ensure the new DrawingAreaProxy is resized.
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webkitWebViewBase));

#if PLATFORM(X11) && USE(TEXTURE_MAPPER)
    if (PlatformDisplay::sharedDisplay().type() != PlatformDisplay::Type::X11)
        return;

    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    DrawingAreaProxyImpl* drawingArea = static_cast<DrawingAreaProxyImpl*>(priv->pageProxy->drawingArea());
    ASSERT(drawingArea);
#if USE(REDIRECTED_XCOMPOSITE_WINDOW)
    if (!priv->redirectedWindow)
        return;
    uint64_t windowID = priv->redirectedWindow->windowID();
#else
    if (!gtk_widget_get_realized(GTK_WIDGET(webkitWebViewBase)))
        return;
    uint64_t windowID = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(webkitWebViewBase)));
#endif
    drawingArea->setNativeSurfaceHandleForCompositing(windowID);
#else
    UNUSED_PARAM(webkitWebViewBase);
#endif
}
