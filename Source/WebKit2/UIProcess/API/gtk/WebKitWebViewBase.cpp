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
#include "WebKitWebViewBase.h"

#include "DrawingAreaProxyImpl.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "WebContext.h"
#include "WebEventFactory.h"
#include "WebFullScreenClientGtk.h"
#include "WebKitPrivate.h"
#include "WebKitWebViewBaseAccessible.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebCore/ClipboardGtk.h>
#include <WebCore/ClipboardUtilitiesGtk.h>
#include <WebCore/DataObjectGtk.h>
#include <WebCore/DragData.h>
#include <WebCore/DragIcon.h>
#include <WebCore/GtkClickCounter.h>
#include <WebCore/GtkDragAndDropHelper.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PasteboardHelper.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/Region.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <wtf/HashMap.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

using namespace WebKit;
using namespace WebCore;

typedef HashMap<GtkWidget*, IntRect> WebKitWebViewChildrenMap;

struct _WebKitWebViewBasePrivate {
    WebKitWebViewChildrenMap children;
    OwnPtr<PageClientImpl> pageClient;
    RefPtr<WebPageProxy> pageProxy;
    bool isPageActive;
    bool shouldForwardNextKeyEvent;
    GRefPtr<GtkIMContext> imContext;
    GtkClickCounter clickCounter;
    CString tooltipText;
    GtkDragAndDropHelper dragAndDropHelper;
    DragIcon dragIcon;
    IntSize resizerSize;
    GRefPtr<AtkObject> accessible;
    bool needsResizeOnMap;
#if ENABLE(FULLSCREEN_API)
    bool fullScreenModeActive;
    WebFullScreenClientGtk fullScreenClient;
#endif
};

G_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_CONTAINER)

static void webkitWebViewBaseNotifyResizerSizeForWindow(WebKitWebViewBase* webViewBase, GtkWindow* window)
{
    gboolean resizerVisible;
    g_object_get(G_OBJECT(window), "resize-grip-visible", &resizerVisible, NULL);

    IntSize resizerSize;
    if (resizerVisible) {
        GdkRectangle resizerRect;
        gtk_window_get_resize_grip_area(window, &resizerRect);
        GdkRectangle allocation;
        gtk_widget_get_allocation(GTK_WIDGET(webViewBase), &allocation);
        if (gdk_rectangle_intersect(&resizerRect, &allocation, 0))
            resizerSize = IntSize(resizerRect.width, resizerRect.height);
    }

    if (resizerSize != webViewBase->priv->resizerSize) {
        webViewBase->priv->resizerSize = resizerSize;
        webViewBase->priv->pageProxy->setWindowResizerSize(resizerSize);
    }
}

static void toplevelWindowResizeGripVisibilityChanged(GObject* object, GParamSpec*, WebKitWebViewBase* webViewBase)
{
    webkitWebViewBaseNotifyResizerSizeForWindow(webViewBase, GTK_WINDOW(object));
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
        | GDK_POINTER_MOTION_MASK
        | GDK_KEY_PRESS_MASK
        | GDK_KEY_RELEASE_MASK
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON1_MOTION_MASK
        | GDK_BUTTON2_MOTION_MASK
        | GDK_BUTTON3_MOTION_MASK;

    gint attributesMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    GdkWindow* window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributesMask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);

    gtk_style_context_set_background(gtk_widget_get_style_context(widget), window);

    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webView->priv;
    gtk_im_context_set_client_window(priv->imContext.get(), window);

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (widgetIsOnscreenToplevelWindow(toplevel)) {
        webkitWebViewBaseNotifyResizerSizeForWindow(webView, GTK_WINDOW(toplevel));
        g_signal_connect(toplevel, "notify::resize-grip-visible",
                         G_CALLBACK(toplevelWindowResizeGripVisibilityChanged), webView);
    }
}

static void webkitWebViewBaseContainerAdd(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;

    GtkAllocation childAllocation;
    gtk_widget_get_allocation(widget, &childAllocation);
    priv->children.set(widget, childAllocation);

    gtk_widget_set_parent(widget, GTK_WIDGET(container));
}

static void webkitWebViewBaseContainerRemove(GtkContainer* container, GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;
    GtkWidget* widgetContainer = GTK_WIDGET(container);

    ASSERT(priv->children.contains(widget));
    gboolean wasVisible = gtk_widget_get_visible(widget);
    gtk_widget_unparent(widget);

    priv->children.remove(widget);
    if (wasVisible && gtk_widget_get_visible(widgetContainer))
        gtk_widget_queue_resize(widgetContainer);
}

static void webkitWebViewBaseContainerForall(GtkContainer* container, gboolean includeInternals, GtkCallback callback, gpointer callbackData)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(container);
    WebKitWebViewBasePrivate* priv = webView->priv;

    WebKitWebViewChildrenMap children = priv->children;
    WebKitWebViewChildrenMap::const_iterator end = children.end();
    for (WebKitWebViewChildrenMap::const_iterator current = children.begin(); current != end; ++current)
        (*callback)(current->first, callbackData);
}

void webkitWebViewBaseChildMoveResize(WebKitWebViewBase* webView, GtkWidget* child, const IntRect& childRect)
{
    const IntRect& geometry = webView->priv->children.get(child);

    if (geometry == childRect)
        return;

    webView->priv->children.set(child, childRect);
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webView));
}

static void webkitWebViewBaseFinalize(GObject* gobject)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(gobject);
    webkitWebViewBase->priv->pageProxy->close();

    webkitWebViewBase->priv->~WebKitWebViewBasePrivate();
    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->finalize(gobject);
}

static void webkit_web_view_base_init(WebKitWebViewBase* webkitWebViewBase)
{
    WebKitWebViewBasePrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webkitWebViewBase, WEBKIT_TYPE_WEB_VIEW_BASE, WebKitWebViewBasePrivate);
    webkitWebViewBase->priv = priv;
    new (priv) WebKitWebViewBasePrivate();

    priv->isPageActive = TRUE;
    priv->shouldForwardNextKeyEvent = FALSE;

    GtkWidget* viewWidget = GTK_WIDGET(webkitWebViewBase);
    gtk_widget_set_double_buffered(viewWidget, FALSE);
    gtk_widget_set_can_focus(viewWidget, TRUE);
    priv->imContext = adoptGRef(gtk_im_multicontext_new());

    priv->pageClient = PageClientImpl::create(viewWidget);

    priv->dragAndDropHelper.setWidget(viewWidget);

    gtk_drag_dest_set(viewWidget, static_cast<GtkDestDefaults>(0), 0, 0,
                      static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_PRIVATE));
    gtk_drag_dest_set_target_list(viewWidget, PasteboardHelper::defaultPasteboardHelper()->targetList());
}

static gboolean webkitWebViewBaseDraw(GtkWidget* widget, cairo_t* cr)
{
    DrawingAreaProxy* drawingArea = WEBKIT_WEB_VIEW_BASE(widget)->priv->pageProxy->drawingArea();
    if (!drawingArea)
        return FALSE;

    GdkRectangle clipRect;
    if (!gdk_cairo_get_clip_rectangle(cr, &clipRect))
        return FALSE;

    WebCore::Region unpaintedRegion; // This is simply unused.
    static_cast<DrawingAreaProxyImpl*>(drawingArea)->paint(cr, clipRect, unpaintedRegion);

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

static void resizeWebKitWebViewBaseFromAllocation(WebKitWebViewBase* webViewBase, GtkAllocation* allocation)
{
    gtk_container_foreach(GTK_CONTAINER(webViewBase), webkitWebViewBaseChildAllocate, webViewBase);

    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageProxy->drawingArea())
        priv->pageProxy->drawingArea()->setSize(IntSize(allocation->width, allocation->height), IntSize());

    GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(webViewBase));
    if (widgetIsOnscreenToplevelWindow(toplevel))
        webkitWebViewBaseNotifyResizerSizeForWindow(webViewBase, GTK_WINDOW(toplevel));
}

static void webkitWebViewBaseSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, allocation);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    if (!gtk_widget_get_mapped(GTK_WIDGET(webViewBase)) && !webViewBase->priv->pageProxy->drawingArea()->size().isEmpty()) {
        webViewBase->priv->needsResizeOnMap = true;
        return;
    }
    resizeWebKitWebViewBaseFromAllocation(webViewBase, allocation);
}

static void webkitWebViewBaseMap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->map(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    if (!webViewBase->priv->needsResizeOnMap)
        return;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    resizeWebKitWebViewBaseFromAllocation(webViewBase, &allocation);
    webViewBase->priv->needsResizeOnMap = false;
}

static gboolean webkitWebViewBaseFocusInEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (widgetIsOnscreenToplevelWindow(toplevel) && gtk_window_has_toplevel_focus(GTK_WINDOW(toplevel))) {
        gtk_im_context_focus_in(priv->imContext.get());
        if (!priv->isPageActive) {
            priv->isPageActive = TRUE;
            priv->pageProxy->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
        }
    }

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_in_event(widget, event);
}

static gboolean webkitWebViewBaseFocusOutEvent(GtkWidget* widget, GdkEventFocus* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->isPageActive = FALSE;
    priv->pageProxy->viewStateDidChange(WebPageProxy::ViewWindowIsActive);
    if (priv->imContext)
        gtk_im_context_focus_out(priv->imContext.get());

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus_out_event(widget, event);
}

static gboolean webkitWebViewBaseKeyPressEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

#if ENABLE(FULLSCREEN_API)
    if (priv->fullScreenModeActive) {
        switch (event->keyval) {
        case GDK_KEY_Escape:
        case GDK_KEY_f:
        case GDK_KEY_F:
            webkitWebViewBaseExitFullScreen(webViewBase);
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
    priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event)));
    return TRUE;
}

static gboolean webkitWebViewBaseKeyReleaseEvent(GtkWidget* widget, GdkEventKey* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (gtk_im_context_filter_keypress(priv->imContext.get(), event))
        return TRUE;

    if (priv->shouldForwardNextKeyEvent) {
        priv->shouldForwardNextKeyEvent = FALSE;
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_release_event(widget, event);
    }
    priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(event)));
    return TRUE;
}

static gboolean webkitWebViewBaseButtonPressEvent(GtkWidget* widget, GdkEventButton* buttonEvent)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    gtk_widget_grab_focus(widget);

    if (!priv->clickCounter.shouldProcessButtonEvent(buttonEvent))
        return TRUE;
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(buttonEvent),
                                                     priv->clickCounter.clickCountForGdkButtonEvent(widget, buttonEvent)));
    return FALSE;
}

static gboolean webkitWebViewBaseButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    gtk_widget_grab_focus(widget);
    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return FALSE;
}

static gboolean webkitWebViewBaseScrollEvent(GtkWidget* widget, GdkEventScroll* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(reinterpret_cast<GdkEvent*>(event)));

    return FALSE;
}

static gboolean webkitWebViewBaseMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(reinterpret_cast<GdkEvent*>(event), 0 /* currentClickCount */));

    return FALSE;
}

static gboolean webkitWebViewBaseQueryTooltip(GtkWidget* widget, gint x, gint y, gboolean keyboardMode, GtkTooltip* tooltip)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;

    if (keyboardMode) {
        // TODO: https://bugs.webkit.org/show_bug.cgi?id=61732.
        notImplemented();
        return FALSE;
    }

    if (priv->tooltipText.length() <= 0)
        return FALSE;

    // TODO: set the tip area when WKPageMouseDidMoveOverElementCallback
    // receives a hit test result.
    gtk_tooltip_set_text(tooltip, priv->tooltipText.data());
    return TRUE;
}

static void webkitWebViewBaseDragDataGet(GtkWidget* widget, GdkDragContext* context, GtkSelectionData* selectionData, guint info, guint time)
{
    WEBKIT_WEB_VIEW_BASE(widget)->priv->dragAndDropHelper.handleGetDragData(context, selectionData, info);
}

static void webkitWebViewBaseDragEnd(GtkWidget* widget, GdkDragContext* context)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    if (!webViewBase->priv->dragAndDropHelper.handleDragEnd(context))
        return;

    GdkDevice* device = gdk_drag_context_get_device(context);
    int x = 0, y = 0;
    gdk_device_get_window_at_position(device, &x, &y);
    int xRoot = 0, yRoot = 0;
    gdk_device_get_position(device, 0, &xRoot, &yRoot);
    webViewBase->priv->pageProxy->dragEnded(IntPoint(x, y), IntPoint(xRoot, yRoot),
                                            gdkDragActionToDragOperation(gdk_drag_context_get_selected_action(context)));
}

static void webkitWebViewBaseDragDataReceived(GtkWidget* widget, GdkDragContext* context, gint x, gint y, GtkSelectionData* selectionData, guint info, guint time)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    OwnPtr<DragData> dragData(webViewBase->priv->dragAndDropHelper.handleDragDataReceived(context, selectionData, info));
    if (!dragData)
        return;

    webViewBase->priv->pageProxy->resetDragOperation();
    webViewBase->priv->pageProxy->dragEntered(dragData.get());
    DragOperation operation = webViewBase->priv->pageProxy->dragSession().operation;
    gdk_drag_status(context, dragOperationToSingleGdkDragAction(operation), time);
}

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

static gboolean webkitWebViewBaseDragMotion(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    OwnPtr<DragData> dragData(webViewBase->priv->dragAndDropHelper.handleDragMotion(context, IntPoint(x, y), time));
    if (!dragData)
        return TRUE;

    webViewBase->priv->pageProxy->dragUpdated(dragData.get());
    DragOperation operation = webViewBase->priv->pageProxy->dragSession().operation;
    gdk_drag_status(context, dragOperationToSingleGdkDragAction(operation), time);
    return TRUE;
}

static void dragExitedCallback(GtkWidget* widget, DragData* dragData, bool dropHappened)
{
    // Don't call dragExited if we have just received a drag-drop signal. This
    // happens in the case of a successful drop onto the view.
    if (dropHappened)
        return;

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    webViewBase->priv->pageProxy->dragExited(dragData);
    webViewBase->priv->pageProxy->resetDragOperation();
}

static void webkitWebViewBaseDragLeave(GtkWidget* widget, GdkDragContext* context, guint time)
{
    WEBKIT_WEB_VIEW_BASE(widget)->priv->dragAndDropHelper.handleDragLeave(context, dragExitedCallback);
}

static gboolean webkitWebViewBaseDragDrop(GtkWidget* widget, GdkDragContext* context, gint x, gint y, guint time)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    OwnPtr<DragData> dragData(webViewBase->priv->dragAndDropHelper.handleDragDrop(context, IntPoint(x, y)));
    if (!dragData)
        return FALSE;

    SandboxExtension::Handle handle;
    SandboxExtension::HandleArray sandboxExtensionForUpload;
    webViewBase->priv->pageProxy->performDrag(dragData.get(), String(), handle, sandboxExtensionForUpload);
    gtk_drag_finish(context, TRUE, FALSE, time);
    return TRUE;
}

static void webkit_web_view_base_class_init(WebKitWebViewBaseClass* webkitWebViewBaseClass)
{
    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webkitWebViewBaseClass);
    widgetClass->realize = webkitWebViewBaseRealize;
    widgetClass->draw = webkitWebViewBaseDraw;
    widgetClass->size_allocate = webkitWebViewBaseSizeAllocate;
    widgetClass->map = webkitWebViewBaseMap;
    widgetClass->focus_in_event = webkitWebViewBaseFocusInEvent;
    widgetClass->focus_out_event = webkitWebViewBaseFocusOutEvent;
    widgetClass->key_press_event = webkitWebViewBaseKeyPressEvent;
    widgetClass->key_release_event = webkitWebViewBaseKeyReleaseEvent;
    widgetClass->button_press_event = webkitWebViewBaseButtonPressEvent;
    widgetClass->button_release_event = webkitWebViewBaseButtonReleaseEvent;
    widgetClass->scroll_event = webkitWebViewBaseScrollEvent;
    widgetClass->motion_notify_event = webkitWebViewBaseMotionNotifyEvent;
    widgetClass->query_tooltip = webkitWebViewBaseQueryTooltip;
    widgetClass->drag_end = webkitWebViewBaseDragEnd;
    widgetClass->drag_data_get = webkitWebViewBaseDragDataGet;
    widgetClass->drag_motion = webkitWebViewBaseDragMotion;
    widgetClass->drag_leave = webkitWebViewBaseDragLeave;
    widgetClass->drag_drop = webkitWebViewBaseDragDrop;
    widgetClass->drag_data_received = webkitWebViewBaseDragDataReceived;
    widgetClass->get_accessible = webkitWebViewBaseGetAccessible;

    GObjectClass* gobjectClass = G_OBJECT_CLASS(webkitWebViewBaseClass);
    gobjectClass->finalize = webkitWebViewBaseFinalize;

    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webkitWebViewBaseClass);
    containerClass->add = webkitWebViewBaseContainerAdd;
    containerClass->remove = webkitWebViewBaseContainerRemove;
    containerClass->forall = webkitWebViewBaseContainerForall;

    g_type_class_add_private(webkitWebViewBaseClass, sizeof(WebKitWebViewBasePrivate));
}

WebKitWebViewBase* webkitWebViewBaseCreate(WebContext* context, WebPageGroup* pageGroup)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(g_object_new(WEBKIT_TYPE_WEB_VIEW_BASE, NULL));
    webkitWebViewBaseCreateWebPage(webkitWebViewBase, toAPI(context), toAPI(pageGroup));
    return webkitWebViewBase;
}

GtkIMContext* webkitWebViewBaseGetIMContext(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->imContext.get();
}

WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->pageProxy.get();
}

void webkitWebViewBaseCreateWebPage(WebKitWebViewBase* webkitWebViewBase, WKContextRef context, WKPageGroupRef pageGroup)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;

    priv->pageProxy = toImpl(context)->createWebPage(priv->pageClient.get(), toImpl(pageGroup));
    priv->pageProxy->initializeWebPage();

#if ENABLE(FULLSCREEN_API)
    priv->pageProxy->fullScreenManager()->setWebView(webkitWebViewBase);
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

void webkitWebViewBaseStartDrag(WebKitWebViewBase* webViewBase, const DragData& dragData, PassRefPtr<ShareableBitmap> dragImage)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    RefPtr<DataObjectGtk> dataObject(dragData.platformData());
    GRefPtr<GtkTargetList> targetList(PasteboardHelper::defaultPasteboardHelper()->targetListForDataObject(dataObject.get()));
    GOwnPtr<GdkEvent> currentEvent(gtk_get_current_event());
    GdkDragContext* context = gtk_drag_begin(GTK_WIDGET(webViewBase),
                                             targetList.get(),
                                             dragOperationToGdkDragActions(dragData.draggingSourceOperationMask()),
                                             1, /* button */
                                             currentEvent.get());
    priv->dragAndDropHelper.startedDrag(context, dataObject.get());


    // A drag starting should prevent a double-click from happening. This might
    // happen if a drag is followed very quickly by another click (like in the DRT).
    priv->clickCounter.reset();

    if (dragImage) {
        RefPtr<cairo_surface_t> image(dragImage->createCairoSurface());
        priv->dragIcon.setImage(image.get());
        priv->dragIcon.useForDrag(context);
    } else
        gtk_drag_set_icon_default(context);
}

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

void webkitWebViewBaseInitializeFullScreenClient(WebKitWebViewBase* webkitWebViewBase, const WKFullScreenClientGtk* wkClient)
{
    webkitWebViewBase->priv->fullScreenClient.initialize(wkClient);
}
