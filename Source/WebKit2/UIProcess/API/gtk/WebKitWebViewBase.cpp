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

#include "DrawingAreaProxyImpl.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "ViewState.h"
#include "WebContext.h"
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
#include "WebUserContentControllerProxy.h"
#include "WebViewBaseInputMethodFilter.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkClickCounter.h>
#include <WebCore/GtkTouchContextHelper.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PasteboardHelper.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/Region.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
#include <WebCore/RedirectedXCompositeWindow.h>
#endif

// gtk_widget_get_scale_factor() appeared in GTK 3.10, but we also need
// to make sure we have cairo new enough to support cairo_surface_set_device_scale
#define HAVE_GTK_SCALE_FACTOR HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE && GTK_CHECK_VERSION(3, 10, 0)

using namespace WebKit;
using namespace WebCore;

typedef HashMap<GtkWidget*, IntRect> WebKitWebViewChildrenMap;

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
void redirectedWindowDamagedCallback(void* data);
#endif

struct _WebKitWebViewBasePrivate {
    WebKitWebViewChildrenMap children;
    std::unique_ptr<PageClientImpl> pageClient;
    RefPtr<WebPageProxy> pageProxy;
    bool shouldForwardNextKeyEvent;
    GtkClickCounter clickCounter;
    CString tooltipText;
    IntRect tooltipArea;
#if !GTK_CHECK_VERSION(3, 13, 4)
    IntSize resizerSize;
#endif
    GRefPtr<AtkObject> accessible;
    bool needsResizeOnMap;
    GtkWidget* authenticationDialog;
    GtkWidget* inspectorView;
    AttachmentSide inspectorAttachmentSide;
    unsigned inspectorViewSize;
    GUniquePtr<GdkEvent> contextMenuEvent;
    WebContextMenuProxyGtk* activeContextMenuProxy;
    WebViewBaseInputMethodFilter inputMethodFilter;
    GtkTouchContextHelper touchContext;

    GtkWindow* toplevelOnScreenWindow;
#if !GTK_CHECK_VERSION(3, 13, 4)
    unsigned long toplevelResizeGripVisibilityID;
#endif
    unsigned long toplevelFocusInEventID;
    unsigned long toplevelFocusOutEventID;
    unsigned long toplevelVisibilityEventID;

    // View State.
    bool isInWindowActive : 1;
    bool isFocused : 1;
    bool isVisible : 1;
    bool isWindowVisible : 1;

    WebKitWebViewBaseDownloadRequestHandler downloadHandler;

#if ENABLE(FULLSCREEN_API)
    bool fullScreenModeActive;
    WebFullScreenClientGtk fullScreenClient;
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
    OwnPtr<RedirectedXCompositeWindow> redirectedWindow;
#endif

#if ENABLE(DRAG_SUPPORT)
    std::unique_ptr<DragAndDropHandler> dragAndDropHandler;
#endif
};

WEBKIT_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_CONTAINER)

#if !GTK_CHECK_VERSION(3, 13, 4)
static void webkitWebViewBaseNotifyResizerSize(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->toplevelOnScreenWindow)
        return;

    gboolean resizerVisible;
    g_object_get(G_OBJECT(priv->toplevelOnScreenWindow), "resize-grip-visible", &resizerVisible, NULL);

    IntSize resizerSize;
    if (resizerVisible) {
        GdkRectangle resizerRect;
        gtk_window_get_resize_grip_area(priv->toplevelOnScreenWindow, &resizerRect);
        GdkRectangle allocation;
        gtk_widget_get_allocation(GTK_WIDGET(webViewBase), &allocation);
        if (gdk_rectangle_intersect(&resizerRect, &allocation, 0))
            resizerSize = IntSize(resizerRect.width, resizerRect.height);
    }

    if (resizerSize != priv->resizerSize) {
        priv->resizerSize = resizerSize;
        priv->pageProxy->setWindowResizerSize(resizerSize);
    }
}

static void toplevelWindowResizeGripVisibilityChanged(GObject*, GParamSpec*, WebKitWebViewBase* webViewBase)
{
    webkitWebViewBaseNotifyResizerSize(webViewBase);
}
#endif

static gboolean toplevelWindowFocusInEvent(GtkWidget*, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->isInWindowActive) {
        priv->isInWindowActive = true;
        priv->pageProxy->viewStateDidChange(ViewState::WindowIsActive);
    }

    return FALSE;
}

static gboolean toplevelWindowFocusOutEvent(GtkWidget*, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->isInWindowActive) {
        priv->isInWindowActive = false;
        priv->pageProxy->viewStateDidChange(ViewState::WindowIsActive);
    }

    return FALSE;
}

static gboolean toplevelWindowVisibilityEvent(GtkWidget*, GdkEventVisibility* visibilityEvent, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    bool isWindowVisible = visibilityEvent->state != GDK_VISIBILITY_FULLY_OBSCURED;
    if (priv->isWindowVisible != isWindowVisible) {
        priv->isWindowVisible = isWindowVisible;
        priv->pageProxy->viewStateDidChange(ViewState::IsVisible);
    }

    return FALSE;
}

static void webkitWebViewBaseSetToplevelOnScreenWindow(WebKitWebViewBase* webViewBase, GtkWindow* window)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->toplevelOnScreenWindow == window)
        return;

#if !GTK_CHECK_VERSION(3, 13, 4)
    if (priv->toplevelResizeGripVisibilityID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelResizeGripVisibilityID);
        priv->toplevelResizeGripVisibilityID = 0;
    }
#endif
    if (priv->toplevelFocusInEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelFocusInEventID);
        priv->toplevelFocusInEventID = 0;
    }
    if (priv->toplevelFocusOutEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelFocusOutEventID);
        priv->toplevelFocusOutEventID = 0;
    }
    if (priv->toplevelVisibilityEventID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelVisibilityEventID);
        priv->toplevelVisibilityEventID = 0;
    }

    priv->toplevelOnScreenWindow = window;
    priv->pageProxy->viewStateDidChange(ViewState::IsInWindow);
    if (!priv->toplevelOnScreenWindow)
        return;

#if !GTK_CHECK_VERSION(3, 13, 4)
    webkitWebViewBaseNotifyResizerSize(webViewBase);

    priv->toplevelResizeGripVisibilityID =
        g_signal_connect(priv->toplevelOnScreenWindow, "notify::resize-grip-visible",
                         G_CALLBACK(toplevelWindowResizeGripVisibilityChanged), webViewBase);
#endif
    priv->toplevelFocusInEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-in-event",
                         G_CALLBACK(toplevelWindowFocusInEvent), webViewBase);
    priv->toplevelFocusOutEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-out-event",
                         G_CALLBACK(toplevelWindowFocusOutEvent), webViewBase);
    priv->toplevelVisibilityEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "visibility-notify-event",
                         G_CALLBACK(toplevelWindowVisibilityEvent), webViewBase);
}

static void webkitWebViewBaseRealize(GtkWidget* widget)
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

    gtk_style_context_set_background(gtk_widget_get_style_context(widget), window);

    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (widgetIsOnscreenToplevelWindow(toplevel))
        webkitWebViewBaseSetToplevelOnScreenWindow(webView, GTK_WINDOW(toplevel));
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
    gtk_drag_dest_set_target_list(viewWidget, PasteboardHelper::defaultPasteboardHelper()->targetList());

    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(object)->priv;
    priv->pageClient = PageClientImpl::create(viewWidget);

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
    GdkDisplay* display = gdk_display_manager_get_default_display(gdk_display_manager_get());
    if (GDK_IS_X11_DISPLAY(display)) {
        priv->redirectedWindow = RedirectedXCompositeWindow::create(IntSize(1, 1), RedirectedXCompositeWindow::DoNotCreateGLContext);
        if (priv->redirectedWindow)
            priv->redirectedWindow->setDamageNotifyCallback(redirectedWindowDamagedCallback, object);
    }
#endif

    priv->authenticationDialog = 0;
}

#if USE(TEXTURE_MAPPER_GL)
static bool webkitWebViewRenderAcceleratedCompositingResults(WebKitWebViewBase* webViewBase, DrawingAreaProxyImpl* drawingArea, cairo_t* cr, GdkRectangle* clipRect)
{
    if (!drawingArea->isInAcceleratedCompositingMode())
        return false;

#if PLATFORM(X11)
    // To avoid flashes when initializing accelerated compositing for the first
    // time, we wait until we know there's a frame ready before rendering.
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->redirectedWindow)
        return false;

    cairo_rectangle(cr, clipRect->x, clipRect->y, clipRect->width, clipRect->height);
    cairo_surface_t* surface = priv->redirectedWindow->cairoSurfaceForWidget(GTK_WIDGET(webViewBase));
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_fill(cr);
    return true;
#else
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

#if USE(TEXTURE_MAPPER_GL)
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

static void resizeWebKitWebViewBaseFromAllocation(WebKitWebViewBase* webViewBase, GtkAllocation* allocation, bool sizeChanged)
{
    gtk_container_foreach(GTK_CONTAINER(webViewBase), webkitWebViewBaseChildAllocate, webViewBase);

    IntRect viewRect(allocation->x, allocation->y, allocation->width, allocation->height);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->inspectorView) {
        GtkAllocation childAllocation = viewRect;

        if (priv->inspectorAttachmentSide == AttachmentSideBottom) {
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

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
    if (sizeChanged && webViewBase->priv->redirectedWindow)
        webViewBase->priv->redirectedWindow->resize(viewRect.size());
#endif

    if (priv->pageProxy->drawingArea())
        priv->pageProxy->drawingArea()->setSize(viewRect.size(), IntSize(), IntSize());

#if !GTK_CHECK_VERSION(3, 13, 4)
    webkitWebViewBaseNotifyResizerSize(webViewBase);
#endif
}

static void webkitWebViewBaseSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    bool sizeChanged = gtk_widget_get_allocated_width(widget) != allocation->width
                       || gtk_widget_get_allocated_height(widget) != allocation->height;

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, allocation);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    if (sizeChanged && !gtk_widget_get_mapped(widget)) {
        webViewBase->priv->needsResizeOnMap = true;
        return;
    }

    resizeWebKitWebViewBaseFromAllocation(webViewBase, allocation, sizeChanged);
}

static void webkitWebViewBaseMap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->map(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->isVisible) {
        priv->isVisible = true;
        priv->pageProxy->viewStateDidChange(ViewState::IsVisible);
    }

    if (!priv->needsResizeOnMap)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    resizeWebKitWebViewBaseFromAllocation(webViewBase, &allocation, true /* sizeChanged */);
    priv->needsResizeOnMap = false;
}

static void webkitWebViewBaseUnmap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unmap(widget);

    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (priv->isVisible) {
        priv->isVisible = false;
        priv->pageProxy->viewStateDidChange(ViewState::IsVisible);
    }
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
    priv->inputMethodFilter.filterKeyEvent(event);
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
    priv->inputMethodFilter.filterKeyEvent(event);
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

    if (!priv->clickCounter.shouldProcessButtonEvent(buttonEvent))
        return TRUE;

    // If it's a right click event save it as a possible context menu event.
    if (buttonEvent->button == 3)
        priv->contextMenuEvent.reset(gdk_event_copy(reinterpret_cast<GdkEvent*>(buttonEvent)));
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(buttonEvent),
                                                     priv->clickCounter.clickCountForGdkButtonEvent(widget, buttonEvent)));
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
        return TRUE;

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return TRUE;
}

static gboolean webkitWebViewBaseTouchEvent(GtkWidget* widget, GdkEventTouch* event)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;

    if (priv->authenticationDialog)
        return TRUE;

    priv->touchContext.handleEvent(reinterpret_cast<GdkEvent*>(event));
    priv->pageProxy->handleTouchEvent(NativeWebTouchEvent(reinterpret_cast<GdkEvent*>(event), priv->touchContext));

    return TRUE;
}

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
    widgetClass->touch_event = webkitWebViewBaseTouchEvent;
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

WebKitWebViewBase* webkitWebViewBaseCreate(WebContext* context, WebPreferences* preferences, WebPageGroup* pageGroup, WebUserContentControllerProxy* userContentController, WebPageProxy* relatedPage)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(g_object_new(WEBKIT_TYPE_WEB_VIEW_BASE, NULL));
    webkitWebViewBaseCreateWebPage(webkitWebViewBase, context, preferences, pageGroup, userContentController, relatedPage);
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

void webkitWebViewBaseUpdatePreferences(WebKitWebViewBase* webkitWebViewBase)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
    if (priv->redirectedWindow)
        return;
#endif

    priv->pageProxy->preferences().setAcceleratedCompositingEnabled(false);
}

#if HAVE(GTK_SCALE_FACTOR)
static void deviceScaleFactorChanged(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
}
#endif // HAVE(GTK_SCALE_FACTOR)

void webkitWebViewBaseCreateWebPage(WebKitWebViewBase* webkitWebViewBase, WebContext* context, WebPreferences* preferences, WebPageGroup* pageGroup, WebUserContentControllerProxy* userContentController, WebPageProxy* relatedPage)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;

    WebPageConfiguration webPageConfiguration;
    webPageConfiguration.preferences = preferences;
    webPageConfiguration.pageGroup = pageGroup;
    webPageConfiguration.relatedPage = relatedPage;
    webPageConfiguration.userContentController = userContentController;
    priv->pageProxy = context->createWebPage(*priv->pageClient, WTF::move(webPageConfiguration));
    priv->pageProxy->initializeWebPage();

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
    if (priv->redirectedWindow)
        priv->pageProxy->setAcceleratedCompositingWindowId(priv->redirectedWindow->windowId());
#endif

#if HAVE(GTK_SCALE_FACTOR)
    // We attach this here, because changes in scale factor are passed directly to the page proxy.
    priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
    g_signal_connect(webkitWebViewBase, "notify::scale-factor", G_CALLBACK(deviceScaleFactorChanged), nullptr);
#endif

    webkitWebViewBaseUpdatePreferences(webkitWebViewBase);

    // This must happen here instead of the instance initializer, because the input method
    // filter must have access to the page.
    priv->inputMethodFilter.setWebView(webkitWebViewBase);
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

void webkitWebViewBaseSetActiveContextMenuProxy(WebKitWebViewBase* webkitWebViewBase, WebContextMenuProxyGtk* contextMenuProxy)
{
    webkitWebViewBase->priv->activeContextMenuProxy = contextMenuProxy;
}

WebContextMenuProxyGtk* webkitWebViewBaseGetActiveContextMenuProxy(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->activeContextMenuProxy;
}

GdkEvent* webkitWebViewBaseTakeContextMenuEvent(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->contextMenuEvent.release();
}

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(X11)
void redirectedWindowDamagedCallback(void* data)
{
    gtk_widget_queue_draw(GTK_WIDGET(data));
}
#endif

void webkitWebViewBaseSetFocus(WebKitWebViewBase* webViewBase, bool focused)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->isFocused == focused)
        return;

    unsigned viewStateFlags = ViewState::IsFocused;
    priv->isFocused = focused;

    // If the view has received the focus and the window is not active
    // mark the current window as active now. This can happen if the
    // toplevel window is a GTK_WINDOW_POPUP and the focus has been
    // set programatically like WebKitTestRunner does, because POPUP
    // can't be focused.
    if (priv->isFocused && !priv->isInWindowActive) {
        priv->isInWindowActive = true;
        viewStateFlags |= ViewState::WindowIsActive;
    }
    priv->pageProxy->viewStateDidChange(viewStateFlags);
}

bool webkitWebViewBaseIsInWindowActive(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->isInWindowActive;
}

bool webkitWebViewBaseIsFocused(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->isFocused;
}

bool webkitWebViewBaseIsVisible(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->isVisible;
}

bool webkitWebViewBaseIsInWindow(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->toplevelOnScreenWindow;
}

bool webkitWebViewBaseIsWindowVisible(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->isWindowVisible;
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
    webkitWebViewBase->priv->inputMethodFilter.setCursorRect(webkitWebViewBase->priv->pageProxy->editorState().cursorRect);
}

void webkitWebViewBaseResetClickCounter(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->clickCounter.reset();
}
