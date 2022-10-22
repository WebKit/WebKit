/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2013 Gustavo Noronha Silva <gns@gnome.org>.
 * Copyright (C) 2011, 2020 Igalia S.L.
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
#include "AcceleratedBackingStore.h"
#include "DragSource.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "DropTarget.h"
#include "InputMethodFilter.h"
#include "KeyBindingTranslator.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "PageClientImpl.h"
#include "PointerLockManager.h"
#include "ViewGestureController.h"
#include "WebEventFactory.h"
#include "WebInspectorUIProxy.h"
#include "WebKit2Initialize.h"
#include "WebKitEmojiChooser.h"
#include "WebKitInitialize.h"
#include "WebKitInputMethodContextImplGtk.h"
#include "WebKitWebViewAccessible.h"
#include "WebKitWebViewBaseInternal.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPopupMenuProxyGtk.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"
#include <WebCore/ActivityState.h>
#include <WebCore/CairoUtilities.h>
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformDisplay.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PointerEvent.h>
#include <WebCore/RefPtrCairo.h>
#include <WebCore/Region.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <memory>
#include <pal/system/SleepDisabler.h>
#include <wtf/Compiler.h>
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if ENABLE(FULLSCREEN_API)
#include "WebFullScreenManagerProxy.h"
#endif

using namespace WebKit;
using namespace WebCore;

#if !USE(GTK4)
struct ClickCounter {
public:
    void reset()
    {
        currentClickCount = 0;
        previousClickPoint = IntPoint();
        previousClickTime = 0;
        previousClickButton = 0;
    }

    int currentClickCountForGdkButtonEvent(GdkEvent* event)
    {
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
            eventTime = g_get_monotonic_time() / 1000;
        }

        GdkEventType type;
        guint button;
        double x, y;
        gdk_event_get_coords(event, &x, &y);
        gdk_event_get_button(event, &button);
        type = gdk_event_get_event_type(event);

        if ((type == GDK_2BUTTON_PRESS || type == GDK_3BUTTON_PRESS)
            || ((std::abs(x - previousClickPoint.x()) < doubleClickDistance)
                && (std::abs(y - previousClickPoint.y()) < doubleClickDistance)
                && (eventTime - previousClickTime < static_cast<unsigned>(doubleClickTime))
                && (button == previousClickButton)))
            currentClickCount++;
        else
            currentClickCount = 1;

        previousClickPoint = IntPoint(x, y);
        previousClickButton = button;
        previousClickTime = eventTime;

        return currentClickCount;
    }

private:
    int currentClickCount;
    IntPoint previousClickPoint;
    unsigned previousClickButton;
    int previousClickTime;
};
#endif

struct MotionEvent {
    MotionEvent(const FloatPoint& position, const FloatPoint& globalPosition, WebMouseEventButton button, unsigned short buttons, OptionSet<WebEventModifier> modifiers)
        : position(position)
        , globalPosition(globalPosition)
        , button(button)
        , buttons(buttons)
        , modifiers(modifiers)
    {
    }

    MotionEvent(GtkWidget* widget, GdkEvent* event)
    {
        double x, y, xRoot, yRoot;
        GdkModifierType state;
        if (event) {
            gdk_event_get_coords(event, &x, &y);
            gdk_event_get_root_coords(event, &xRoot, &yRoot);
            gdk_event_get_state(event, &state);
        } else {
            auto* device = gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(widget)));
            widgetDevicePosition(widget, device, &x, &y, &state);
            auto rootPoint = widgetRootCoords(widget, x, y);
            xRoot = rootPoint.x();
            yRoot = rootPoint.y();
        }

        MotionEvent(FloatPoint(x, y), FloatPoint(xRoot, yRoot), state);
    }

    MotionEvent(FloatPoint&& position, FloatPoint&& globalPosition, GdkModifierType state)
        : position(WTFMove(position))
        , globalPosition(WTFMove(globalPosition))
    {
        if (state & GDK_CONTROL_MASK)
            modifiers.add(WebEventModifier::ControlKey);
        if (state & GDK_SHIFT_MASK)
            modifiers.add(WebEventModifier::ShiftKey);
        if (state & GDK_MOD1_MASK)
            modifiers.add(WebEventModifier::AltKey);
        if (state & GDK_META_MASK)
            modifiers.add(WebEventModifier::MetaKey);

        if (state & GDK_BUTTON1_MASK) {
            button = WebMouseEventButton::LeftButton;
            buttons |= 1;
        }
        if (state & GDK_BUTTON2_MASK) {
            button = WebMouseEventButton::MiddleButton;
            buttons |= 4;
        }
        if (state & GDK_BUTTON3_MASK) {
            button = WebMouseEventButton::RightButton;
            buttons |= 2;
        }
    }

    FloatSize delta(GdkEvent* event)
    {
        double x, y;
        gdk_event_get_root_coords(event, &x, &y);
        return FloatPoint(x, y) - globalPosition;
    }

    FloatPoint position;
    FloatPoint globalPosition;
    WebMouseEventButton button { WebMouseEventButton::NoButton };
    unsigned short buttons { 0 };
    OptionSet<WebEventModifier> modifiers;
};

#if !USE(GTK4)
typedef HashMap<GtkWidget*, IntRect> WebKitWebViewChildrenMap;
#endif
typedef HashMap<uint32_t, GUniquePtr<GdkEvent>> TouchEventsMap;

struct _WebKitWebViewBasePrivate {
    _WebKitWebViewBasePrivate()
        : updateActivityStateTimer(RunLoop::main(), this, &_WebKitWebViewBasePrivate::updateActivityStateTimerFired)
#if GTK_CHECK_VERSION(3, 24, 0)
        , releaseEmojiChooserTimer(RunLoop::main(), this, &_WebKitWebViewBasePrivate::releaseEmojiChooserTimerFired)
#endif
    {
#if GTK_CHECK_VERSION(3, 24, 0)
        releaseEmojiChooserTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
    }

    void updateActivityStateTimerFired()
    {
        if (!pageProxy)
            return;
        pageProxy->activityStateDidChange(activityStateFlagsToUpdate);
        activityStateFlagsToUpdate = { };
    }

#if GTK_CHECK_VERSION(3, 24, 0)
    void releaseEmojiChooserTimerFired()
    {
#if USE(GTK4)
        g_clear_pointer(&emojiChooser, gtk_widget_unparent);
#else
        if (emojiChooser) {
            gtk_widget_destroy(emojiChooser);
            emojiChooser = nullptr;
        }
#endif
    }
#endif

#if !USE(GTK4)
    WebKitWebViewChildrenMap children;
#endif
    std::unique_ptr<PageClientImpl> pageClient;
    RefPtr<WebPageProxy> pageProxy;
    IntSize viewSize { };
    bool shouldForwardNextKeyEvent { false };
    bool shouldForwardNextWheelEvent { false };
#if !USE(GTK4)
    ClickCounter clickCounter;
#endif
    CString tooltipText;
    IntRect tooltipArea;
    WebHitTestResultData::IsScrollbar mouseIsOverScrollbar;
#if !USE(GTK4)
    GRefPtr<AtkObject> accessible;
#endif
    GtkWidget* dialog { nullptr };
    GtkWidget* inspectorView { nullptr };
    AttachmentSide inspectorAttachmentSide { AttachmentSide::Bottom };
    unsigned inspectorViewSize { 0 };
    GUniquePtr<GdkEvent> contextMenuEvent;
    WebContextMenuProxyGtk* activeContextMenuProxy { nullptr };
    InputMethodFilter inputMethodFilter;
    KeyBindingTranslator keyBindingTranslator;
    TouchEventsMap touchEvents;
    IntSize contentsSize;
    std::optional<MotionEvent> lastMotionEvent;
    bool isBlank;
    bool shouldNotifyFocusEvents { true };

    GtkWindow* toplevelOnScreenWindow { nullptr };
#if USE(GTK4)
    unsigned long toplevelIsActiveID { 0 };
    unsigned long toplevelWindowStateChangedID { 0 };
    unsigned long toplevelWindowUnrealizedID { 0 };
#else
    unsigned long toplevelFocusInEventID { 0 };
    unsigned long toplevelFocusOutEventID { 0 };
    unsigned long toplevelWindowStateEventID { 0 };
#endif
    unsigned long toplevelWindowRealizedID { 0 };

    // View State.
    OptionSet<ActivityState::Flag> activityState;
    OptionSet<ActivityState::Flag> activityStateFlagsToUpdate;
    RunLoop::Timer<WebKitWebViewBasePrivate> updateActivityStateTimer;

#if ENABLE(FULLSCREEN_API)
    bool fullScreenModeActive { false };
    std::unique_ptr<PAL::SleepDisabler> sleepDisabler;
#endif

    std::unique_ptr<AcceleratedBackingStore> acceleratedBackingStore;

#if ENABLE(DRAG_SUPPORT)
    std::unique_ptr<DragSource> dragSource;
    std::unique_ptr<DropTarget> dropTarget;
#endif

    GtkGesture* touchGestureGroup;
    std::unique_ptr<ViewGestureController> viewGestureController;
    bool isBackForwardNavigationGestureEnabled { false };

#if GTK_CHECK_VERSION(3, 24, 0)
    GtkWidget* emojiChooser;
    CompletionHandler<void(String)> emojiChooserCompletionHandler;
    RunLoop::Timer<WebKitWebViewBasePrivate> releaseEmojiChooserTimer;
#endif

    // Touch gestures state
    FloatPoint dragOffset;
    bool isLongPressed;
    bool isBeingDragged;
    bool pageGrabbedTouch;

    std::unique_ptr<PointerLockManager> pointerLockManager;
};

/**
 * WebKitWebViewBase:
 *
 * Internal base class.
 */

#if USE(GTK4)
WEBKIT_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_WIDGET)
#else
WEBKIT_DEFINE_TYPE(WebKitWebViewBase, webkit_web_view_base, GTK_TYPE_CONTAINER)
#endif

static void webkitWebViewBaseScheduleUpdateActivityState(WebKitWebViewBase* webViewBase, OptionSet<ActivityState::Flag> flagsToUpdate)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->activityStateFlagsToUpdate.add(flagsToUpdate);
    if (priv->updateActivityStateTimer.isActive())
        return;

    priv->updateActivityStateTimer.startOneShot(0_s);
}

#if !USE(GTK4)
static gboolean toplevelWindowFocusInEvent(GtkWidget* widget, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    // Spurious focus in events can occur when the window is hidden.
    if (!gtk_widget_get_visible(widget))
        return FALSE;

    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->activityState & ActivityState::WindowIsActive)
        return FALSE;

    priv->activityState.add(ActivityState::WindowIsActive);
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::WindowIsActive);

    return FALSE;
}

static gboolean toplevelWindowFocusOutEvent(GtkWidget*, GdkEventFocus*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(priv->activityState & ActivityState::WindowIsActive))
        return FALSE;

    priv->activityState.remove(ActivityState::WindowIsActive);
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::WindowIsActive);

    return FALSE;
}

static gboolean toplevelWindowStateEvent(GtkWidget*, GdkEventWindowState* event, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(event->changed_mask & GDK_WINDOW_STATE_ICONIFIED))
        return FALSE;

    bool visible = !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED);
    if ((visible && priv->activityState & ActivityState::IsVisible) || (!visible && !(priv->activityState & ActivityState::IsVisible)))
        return FALSE;

    if (visible)
        priv->activityState.add(ActivityState::IsVisible);
    else
        priv->activityState.remove(ActivityState::IsVisible);
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::IsVisible);

    return FALSE;
}

static void toplevelWindowRealized(WebKitWebViewBase* webViewBase)
{
    gtk_widget_realize(GTK_WIDGET(webViewBase));

    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->toplevelWindowRealizedID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelWindowRealizedID);
        priv->toplevelWindowRealizedID = 0;
    }
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
    if (priv->toplevelWindowRealizedID) {
        g_signal_handler_disconnect(priv->toplevelOnScreenWindow, priv->toplevelWindowRealizedID);
        priv->toplevelWindowRealizedID = 0;
    }

    priv->toplevelOnScreenWindow = window;

    if (!priv->toplevelOnScreenWindow) {
        OptionSet<ActivityState::Flag> flagsToUpdate;
        if (priv->activityState & ActivityState::IsInWindow) {
            priv->activityState.remove(ActivityState::IsInWindow);
            flagsToUpdate.add(ActivityState::IsInWindow);
        }
        if (priv->activityState & ActivityState::WindowIsActive) {
            priv->activityState.remove(ActivityState::WindowIsActive);
            flagsToUpdate.add(ActivityState::IsInWindow);
        }
        if (flagsToUpdate)
            webkitWebViewBaseScheduleUpdateActivityState(webViewBase, flagsToUpdate);

        return;
    }

    priv->toplevelFocusInEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-in-event",
                         G_CALLBACK(toplevelWindowFocusInEvent), webViewBase);
    priv->toplevelFocusOutEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "focus-out-event",
                         G_CALLBACK(toplevelWindowFocusOutEvent), webViewBase);
    priv->toplevelWindowStateEventID =
        g_signal_connect(priv->toplevelOnScreenWindow, "window-state-event", G_CALLBACK(toplevelWindowStateEvent), webViewBase);

    if (gtk_widget_get_realized(GTK_WIDGET(window)))
        gtk_widget_realize(GTK_WIDGET(webViewBase));
    else
        priv->toplevelWindowRealizedID = g_signal_connect_swapped(window, "realize", G_CALLBACK(toplevelWindowRealized), webViewBase);
}
#endif

static void webkitWebViewBaseRealize(GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webView->priv;

#if USE(GTK4)
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->realize(widget);
#else
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
        | GDK_ENTER_NOTIFY_MASK
        | GDK_LEAVE_NOTIFY_MASK
        | GDK_KEY_PRESS_MASK
        | GDK_KEY_RELEASE_MASK
        | GDK_BUTTON_MOTION_MASK
        | GDK_BUTTON1_MOTION_MASK
        | GDK_BUTTON2_MOTION_MASK
        | GDK_BUTTON3_MOTION_MASK
        | GDK_TOUCH_MASK;
    attributes.event_mask |= GDK_TOUCHPAD_GESTURE_MASK;

    gint attributesMask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

    GdkWindow* window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributesMask);
    gtk_widget_set_window(widget, window);
    gdk_window_set_user_data(window, widget);
#endif

    auto* imContext = priv->inputMethodFilter.context();
    if (WEBKIT_IS_INPUT_METHOD_CONTEXT_IMPL_GTK(imContext))
        webkitInputMethodContextImplGtkSetClientWidget(WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(imContext), widget);

    if (priv->acceleratedBackingStore)
        priv->acceleratedBackingStore->realize();
}

static void webkitWebViewBaseUnrealize(GtkWidget* widget)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(widget);

    auto* imContext = webView->priv->inputMethodFilter.context();
    if (WEBKIT_IS_INPUT_METHOD_CONTEXT_IMPL_GTK(imContext))
        webkitInputMethodContextImplGtkSetClientWidget(WEBKIT_INPUT_METHOD_CONTEXT_IMPL_GTK(imContext), nullptr);

    if (webView->priv->acceleratedBackingStore)
        webView->priv->acceleratedBackingStore->unrealize();

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unrealize(widget);
}

#if !USE(GTK4)
static bool webkitWebViewChildIsInternalWidget(WebKitWebViewBase* webViewBase, GtkWidget* widget)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    return widget == priv->inspectorView || widget == priv->dialog || widget == priv->keyBindingTranslator.widget();
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
#endif

void webkitWebViewBaseAddDialog(WebKitWebViewBase* webViewBase, GtkWidget* dialog)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->dialog = dialog;
#if USE(GTK4)
    g_object_add_weak_pointer(G_OBJECT(dialog), reinterpret_cast<void**>(&priv->dialog));
#endif
    gtk_widget_set_parent(dialog, GTK_WIDGET(webViewBase));
    gtk_widget_show(dialog);

    // We need to draw the shadow over the widget.
    gtk_widget_queue_draw(GTK_WIDGET(webViewBase));
}

#if USE(GTK4)
static void webkitWebViewBaseRemoveDialog(WebKitWebViewBase* webViewBase, GtkWidget* dialog)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->dialog)
        return;

    g_object_remove_weak_pointer(G_OBJECT(dialog), reinterpret_cast<void**>(&priv->dialog));
    g_clear_pointer(&priv->dialog, gtk_widget_unparent);
}
#endif

#if !USE(GTK4)
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
    } else if (priv->dialog == widget) {
        priv->dialog = nullptr;
        if (gtk_widget_get_visible(widgetContainer))
            gtk_widget_grab_focus(widgetContainer);
    } else if (priv->keyBindingTranslator.widget() == widget)
        priv->keyBindingTranslator.invalidate();
    else {
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

    for (const auto& child : copyToVector(priv->children.keys())) {
        if (priv->children.contains(child))
            (*callback)(child, callbackData);
    }

    if (includeInternals && priv->keyBindingTranslator.widget())
        (*callback)(priv->keyBindingTranslator.widget(), callbackData);

    if (includeInternals && priv->inspectorView)
        (*callback)(priv->inspectorView, callbackData);

    if (includeInternals && priv->dialog)
        (*callback)(priv->dialog, callbackData);
}

void webkitWebViewBaseChildMoveResize(WebKitWebViewBase* webView, GtkWidget* child, const IntRect& childRect)
{
    const IntRect& geometry = webView->priv->children.get(child);
    if (geometry == childRect)
        return;

    webView->priv->children.set(child, childRect);
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webView));
}
#endif

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
    gtk_widget_set_parent(inspector, GTK_WIDGET(webViewBase));
}

void webkitWebViewBaseRemoveWebInspector(WebKitWebViewBase* webViewBase, GtkWidget* inspector)
{
    if (webViewBase->priv->inspectorView != inspector)
        return;

#if USE(GTK4)
    g_clear_pointer(&webViewBase->priv->inspectorView, gtk_widget_unparent);
#else
    gtk_container_remove(GTK_CONTAINER(webViewBase), inspector);
#endif
}

#if GTK_CHECK_VERSION(3, 24, 0)
static void webkitWebViewBaseCompleteEmojiChooserRequest(WebKitWebViewBase* webView, const String& text)
{
    if (auto completionHandler = std::exchange(webView->priv->emojiChooserCompletionHandler, nullptr))
        completionHandler(text);
}
#endif

static void webkitWebViewBaseDispose(GObject* gobject)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(gobject);
#if USE(GTK4)
    webkitWebViewBaseRemoveDialog(webView, webView->priv->dialog);
    webkitWebViewBaseRemoveWebInspector(webView, webView->priv->inspectorView);
    if (auto* widget = webView->priv->keyBindingTranslator.widget())
        gtk_widget_unparent(widget);
    g_clear_pointer(&webView->priv->emojiChooser, gtk_widget_unparent);
#else
    g_clear_pointer(&webView->priv->dialog, gtk_widget_destroy);
    webkitWebViewBaseSetToplevelOnScreenWindow(webView, nullptr);
#if ENABLE(ACCESSIBILITY)
    if (webView->priv->accessible)
        webkitWebViewAccessibleSetWebView(WEBKIT_WEB_VIEW_ACCESSIBLE(webView->priv->accessible.get()), nullptr);
#endif // ENABLE(ACCESSIBILITY)
#endif
#if GTK_CHECK_VERSION(3, 24, 0)
    webkitWebViewBaseCompleteEmojiChooserRequest(webView, emptyString());
#endif
    if (webView->priv->pointerLockManager) {
        webView->priv->pointerLockManager->unlock();
        webView->priv->pointerLockManager = nullptr;
    }
    webView->priv->inputMethodFilter.setContext(nullptr);
    webView->priv->pageProxy->close();
    webView->priv->acceleratedBackingStore = nullptr;
#if ENABLE(FULLSCREEN_API)
    webView->priv->sleepDisabler = nullptr;
#endif
    webView->priv->keyBindingTranslator.invalidate();
    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->dispose(gobject);
}

#if USE(GTK4)
static void webkitWebViewBaseSnapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    FloatSize widgetSize(gtk_widget_get_width(widget), gtk_widget_get_height(widget));
    if (widgetSize.isEmpty())
        return;

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(webViewBase->priv->pageProxy->drawingArea());
    if (!drawingArea)
        return;

    auto* pageSnapshot = gtk_snapshot_new();
    if (!webViewBase->priv->isBlank) {
        if (drawingArea->isInAcceleratedCompositingMode())
            webViewBase->priv->acceleratedBackingStore->snapshot(pageSnapshot);
        else {
            graphene_rect_t bounds = GRAPHENE_RECT_INIT(0, 0, widgetSize.width(), widgetSize.height());
            RefPtr<cairo_t> cr = adoptRef(gtk_snapshot_append_cairo(pageSnapshot, &bounds));
            WebCore::Region unpaintedRegion; // This is simply unused.
            drawingArea->paint(cr.get(), IntRect { { 0, 0 }, drawingArea->size() }, unpaintedRegion);
        }
    }

    if (auto* pageRenderNode = gtk_snapshot_free_to_node(pageSnapshot)) {
        bool showingNavigationSnapshot = webViewBase->priv->pageProxy->isShowingNavigationGestureSnapshot();
        auto* controller = webkitWebViewBaseViewGestureController(webViewBase);
        if (showingNavigationSnapshot && controller)
            controller->snapshot(snapshot, pageRenderNode);
        else
            gtk_snapshot_append_node(snapshot, pageRenderNode);

        gsk_render_node_unref(pageRenderNode);
    }

    if (webViewBase->priv->inspectorView)
        gtk_widget_snapshot_child(widget, webViewBase->priv->inspectorView, snapshot);

    if (webViewBase->priv->dialog)
        gtk_widget_snapshot_child(widget, webViewBase->priv->dialog, snapshot);
}
#else
static gboolean webkitWebViewBaseDraw(GtkWidget* widget, cairo_t* cr)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(webViewBase->priv->pageProxy->drawingArea());
    if (!drawingArea)
        return FALSE;

    GdkRectangle clipRect;
    if (!gdk_cairo_get_clip_rectangle(cr, &clipRect))
        return FALSE;

    if (!webViewBase->priv->isBlank) {
        bool showingNavigationSnapshot = webViewBase->priv->pageProxy->isShowingNavigationGestureSnapshot();
        if (showingNavigationSnapshot)
            cairo_push_group(cr);

        if (drawingArea->isInAcceleratedCompositingMode()) {
            ASSERT(webViewBase->priv->acceleratedBackingStore);
            webViewBase->priv->acceleratedBackingStore->paint(cr, clipRect);
        } else {
            WebCore::Region unpaintedRegion; // This is simply unused.
            drawingArea->paint(cr, clipRect, unpaintedRegion);
        }

        if (showingNavigationSnapshot) {
            RefPtr<cairo_pattern_t> group = adoptRef(cairo_pop_group(cr));
            if (auto* controller = webkitWebViewBaseViewGestureController(webViewBase))
                controller->draw(cr, group.get());
        }
    }

    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->draw(widget, cr);

    return FALSE;
}
#endif

#if !USE(GTK4)
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
#endif

#if USE(GTK4)
static void webkitWebViewBaseSizeAllocate(GtkWidget* widget, int width, int height, int baseline)
#else
static void webkitWebViewBaseSizeAllocate(GtkWidget* widget, GtkAllocation* allocation)
#endif
{
#if USE(GTK4)
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, width, height, baseline);
    GtkAllocation allocationStack = { 0, 0, width, height };
    GtkAllocation* allocation = &allocationStack;
#else
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->size_allocate(widget, allocation);
#endif

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
#if !USE(GTK4)
    gtk_container_foreach(GTK_CONTAINER(webViewBase), webkitWebViewBaseChildAllocate, webViewBase);
#endif

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

    // The dialogs are centered in the view rect, which means that it
    // never overlaps the web inspector. Thus, we need to calculate the allocation here
    // after calculating the inspector allocation.
    if (priv->dialog) {
        GtkRequisition minimumSize;
        gtk_widget_get_preferred_size(priv->dialog, &minimumSize, nullptr);

        GtkAllocation childAllocation = { 0, 0, std::max(minimumSize.width, viewRect.width()), std::max(minimumSize.height, viewRect.height()) };
        gtk_widget_size_allocate(priv->dialog, &childAllocation);
    }

#if USE(GTK4)
    for (auto* child = gtk_widget_get_first_child(widget); child; child = gtk_widget_get_next_sibling(child)) {
        if (GTK_IS_POPOVER(child))
            gtk_popover_present(GTK_POPOVER(child));
    }
#endif

    priv->viewSize = viewRect.size();

    if (auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(priv->pageProxy->drawingArea()))
        drawingArea->setSize(priv->viewSize);
}

#if USE(GTK4)
static void webkitWebViewBaseMeasure(GtkWidget* widget, GtkOrientation orientation, int, int* minimumSize, int* naturalSize, int*, int*)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    switch (orientation) {
    case GTK_ORIENTATION_HORIZONTAL:
        *naturalSize = priv->contentsSize.width();
        break;
    case GTK_ORIENTATION_VERTICAL:
        *naturalSize = priv->contentsSize.height();
        break;
    }

    *minimumSize = 0;
}
#else
static void webkitWebViewBaseGetPreferredWidth(GtkWidget* widget, gint* minimumSize, gint* naturalSize)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    *minimumSize = 0;
    *naturalSize = priv->contentsSize.width();
}

static void webkitWebViewBaseGetPreferredHeight(GtkWidget* widget, gint* minimumSize, gint* naturalSize)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    *minimumSize = 0;
    *naturalSize = priv->contentsSize.height();
}
#endif

static void webkitWebViewBaseMap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->map(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    OptionSet<ActivityState::Flag> flagsToUpdate;
    if (!(priv->activityState & ActivityState::IsVisible))
        flagsToUpdate.add(ActivityState::IsVisible);
    if (priv->toplevelOnScreenWindow) {
        if (!(priv->activityState & ActivityState::IsInWindow))
            flagsToUpdate.add(ActivityState::IsInWindow);

#if ENABLE(DEVELOPER_MODE)
        // Xvfb doesn't support toplevel focus, so gtk_window_is_active() always returns false. We consider
        // toplevel window to be always active since it's the only one.
        if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11) {
            if (!g_strcmp0(g_getenv("UNDER_XVFB"), "yes"))
                flagsToUpdate.add(ActivityState::WindowIsActive);
        }
#endif

        if (gtk_window_is_active(GTK_WINDOW(priv->toplevelOnScreenWindow)) && !(priv->activityState & ActivityState::WindowIsActive))
            flagsToUpdate.add(ActivityState::WindowIsActive);
    }
    if (!flagsToUpdate)
        return;

    priv->activityState.add(flagsToUpdate);
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, flagsToUpdate);
}

static void webkitWebViewBaseUnmap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unmap(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!(priv->activityState & ActivityState::IsVisible))
        return;

    priv->activityState.remove(ActivityState::IsVisible);
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::IsVisible);
}

#if !USE(GTK4)
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

static gboolean webkitWebViewBaseKeyPressEvent(GtkWidget* widget, GdkEventKey* keyEvent)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    // Since WebProcess key event handling is not synchronous, handle the event in two passes.
    // When WebProcess processes the input event, it will call PageClientImpl::doneWithKeyEvent
    // with event handled status which determines whether to pass the input event to parent or not
    // using gtk_main_do_event().
    if (priv->shouldForwardNextKeyEvent) {
        priv->shouldForwardNextKeyEvent = false;
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_press_event(widget, keyEvent);
    }

    GdkModifierType state;
    guint keyval;
    gdk_event_get_state(reinterpret_cast<GdkEvent*>(keyEvent), &state);
    gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(keyEvent), &keyval);

#if ENABLE(DEVELOPER_MODE) && OS(LINUX)
    if ((state & GDK_CONTROL_MASK) && (state & GDK_SHIFT_MASK) && keyval == GDK_KEY_G) {
        auto& preferences = priv->pageProxy->preferences();
        preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
        return GDK_EVENT_STOP;
    }
#endif

    if (priv->dialog)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->key_press_event(widget, keyEvent);

#if ENABLE(FULLSCREEN_API)
    if (priv->fullScreenModeActive) {
        switch (keyval) {
        case GDK_KEY_Escape:
        case GDK_KEY_f:
        case GDK_KEY_F:
            priv->pageProxy->fullScreenManager()->requestExitFullScreen();
            return GDK_EVENT_STOP;
        default:
            break;
        }
    }
#endif

    auto filterResult = priv->inputMethodFilter.filterKeyEvent(reinterpret_cast<GdkEvent*>(keyEvent));
    if (!filterResult.handled) {
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(keyEvent), filterResult.keyText,
            priv->keyBindingTranslator.commandsForKeyEvent(keyEvent)));
    }

    return GDK_EVENT_STOP;
}

static gboolean webkitWebViewBaseKeyReleaseEvent(GtkWidget* widget, GdkEventKey* keyEvent)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (!priv->inputMethodFilter.filterKeyEvent(reinterpret_cast<GdkEvent*>(keyEvent)).handled)
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(reinterpret_cast<GdkEvent*>(keyEvent), { }, { }));

    return GDK_EVENT_STOP;
}
#endif

#if USE(GTK4)
static void webkitWebViewBaseFocusEnter(WebKitWebViewBase* webViewBase, GtkEventController*)
{
    webkitWebViewBaseSetFocus(webViewBase, true);
    webViewBase->priv->inputMethodFilter.notifyFocusedIn();
}

static void webkitWebViewBaseFocusLeave(WebKitWebViewBase* webViewBase, GtkEventController*)
{
    webkitWebViewBaseSetFocus(webViewBase, false);
    webViewBase->priv->inputMethodFilter.notifyFocusedOut();
}

static gboolean webkitWebViewBaseKeyPressed(WebKitWebViewBase* webViewBase, unsigned keyval, unsigned, GdkModifierType state, GtkEventController* controller)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    // Since WebProcess key event handling is not synchronous, handle the event in two passes.
    // When WebProcess processes the input event, it will call PageClientImpl::doneWithKeyEvent
    // with event handled status which determines whether to pass the input event to parent or not
    // using gdk_display_put_event().
    if (priv->shouldForwardNextKeyEvent) {
        priv->shouldForwardNextKeyEvent = false;
        return GDK_EVENT_PROPAGATE;
    }

#if ENABLE(DEVELOPER_MODE) && OS(LINUX)
    if ((state & GDK_CONTROL_MASK) && (state & GDK_SHIFT_MASK) && keyval == GDK_KEY_G) {
        auto& preferences = priv->pageProxy->preferences();
        preferences.setResourceUsageOverlayVisible(!preferences.resourceUsageOverlayVisible());
        return GDK_EVENT_STOP;
    }
#endif

    if (priv->dialog)
        return gtk_event_controller_key_forward(GTK_EVENT_CONTROLLER_KEY(controller), priv->dialog);

#if ENABLE(FULLSCREEN_API)
    if (priv->fullScreenModeActive) {
        switch (keyval) {
        case GDK_KEY_Escape:
        case GDK_KEY_f:
        case GDK_KEY_F:
            priv->pageProxy->fullScreenManager()->requestExitFullScreen();
            return GDK_EVENT_STOP;
        default:
            break;
        }
    }
#endif

    auto* event = gtk_event_controller_get_current_event(controller);
    auto filterResult = priv->inputMethodFilter.filterKeyEvent(event);
    if (!filterResult.handled) {
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(event, filterResult.keyText,
            priv->keyBindingTranslator.commandsForKeyEvent(GTK_EVENT_CONTROLLER_KEY(controller))));
    }

    return GDK_EVENT_STOP;
}

static void webkitWebViewBaseKeyReleased(WebKitWebViewBase* webViewBase, unsigned, unsigned, GdkModifierType, GtkEventController* controller)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    auto* event = gtk_event_controller_get_current_event(controller);
    if (!priv->inputMethodFilter.filterKeyEvent(event).handled)
        priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(event, { }, { }));
}
#endif

#if !USE(GTK4)
static void webkitWebViewBaseHandleMouseEvent(WebKitWebViewBase* webViewBase, GdkEvent* event)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    ASSERT(!priv->dialog);

    int clickCount = 0;
    std::optional<FloatSize> movementDelta;
    GdkEventType eventType = gdk_event_get_event_type(event);
    switch (eventType) {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS: {
        // For double and triple clicks GDK sends both a normal button press event
        // and a specific type (like GDK_2BUTTON_PRESS). If we detect a special press
        // coming up, ignore this event as it certainly generated the double or triple
        // click. The consequence of not eating this event is two DOM button press events
        // are generated.
        GUniquePtr<GdkEvent> nextEvent(gdk_event_peek());
        if (nextEvent && (nextEvent->any.type == GDK_2BUTTON_PRESS || nextEvent->any.type == GDK_3BUTTON_PRESS))
            return;

        priv->inputMethodFilter.cancelComposition();

        guint button;
        gdk_event_get_button(event, &button);
        // If it's a right click event save it as a possible context menu event.
        if (button == GDK_BUTTON_SECONDARY)
            priv->contextMenuEvent.reset(gdk_event_copy(event));

        clickCount = priv->clickCounter.currentClickCountForGdkButtonEvent(event);
    }
        FALLTHROUGH;
    case GDK_BUTTON_RELEASE:
        gtk_widget_grab_focus(GTK_WIDGET(webViewBase));
        break;
    case GDK_MOTION_NOTIFY:
        // Pointer Lock. 7.1 Attributes: movementX/Y must be updated regardless of pointer lock state.
        if (priv->lastMotionEvent)
            movementDelta = priv->lastMotionEvent->delta(event);
        priv->lastMotionEvent = MotionEvent(GTK_WIDGET(webViewBase), event);
        break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(event, clickCount, movementDelta));
}

static gboolean webkitWebViewBaseButtonPressEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->dialog)
        return GDK_EVENT_STOP;

    if (gdk_device_get_source(gdk_event_get_source_device(reinterpret_cast<GdkEvent*>(event))) == GDK_SOURCE_TOUCHSCREEN)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->button_press_event(widget, event);

    webkitWebViewBaseHandleMouseEvent(webViewBase, reinterpret_cast<GdkEvent*>(event));

    return GDK_EVENT_STOP;
}

static gboolean webkitWebViewBaseButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->dialog)
        return GDK_EVENT_STOP;
    if (gdk_device_get_source(gdk_event_get_source_device(reinterpret_cast<GdkEvent*>(event))) == GDK_SOURCE_TOUCHSCREEN)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->button_release_event(widget, event);

    webkitWebViewBaseHandleMouseEvent(webViewBase, reinterpret_cast<GdkEvent*>(event));

    return GDK_EVENT_STOP;
}
#endif

#if USE(GTK4)
static void webkitWebViewBaseButtonPressed(WebKitWebViewBase* webViewBase, int clickCount, double x, double y, GtkGesture* gesture)
{
    if (gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture)))
        return;
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

    if (clickCount == 1)
        priv->inputMethodFilter.cancelComposition();
    gtk_widget_grab_focus(GTK_WIDGET(webViewBase));

    auto* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    auto button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    auto* event = gtk_gesture_get_last_event(gesture, sequence);

    // If it's a right click event save it as a possible context menu event.
    if (button == GDK_BUTTON_SECONDARY)
        priv->contextMenuEvent.reset(gdk_event_copy(event));

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(event, { clampToInteger(x), clampToInteger(y) }, clickCount, std::nullopt));
}

static void webkitWebViewBaseButtonReleased(WebKitWebViewBase* webViewBase, int clickCount, double x, double y, GtkGesture* gesture)
{
    if (gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture)))
        return;
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

    gtk_widget_grab_focus(GTK_WIDGET(webViewBase));

    auto* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(gtk_gesture_get_last_event(gesture, sequence), { clampToInteger(x), clampToInteger(y) }, clickCount, std::nullopt));
}
#endif

static bool shouldInvertDirectionForScrollEvent(WebHitTestResultData::IsScrollbar isScrollbar, bool isShiftPressed)
{
    // Shift+Wheel scrolls in the perpendicular direction, unless we are over a scrollbar
    // in which case we always want to follow the scrollbar direction.
    switch (isScrollbar) {
    case WebHitTestResultData::IsScrollbar::No:
        return isShiftPressed;
    case WebHitTestResultData::IsScrollbar::Horizontal:
        return true;
    case WebHitTestResultData::IsScrollbar::Vertical:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if !USE(GTK4)
static void webkitWebViewBaseHandleWheelEvent(WebKitWebViewBase* webViewBase, GdkEvent* event, std::optional<WebWheelEvent::Phase> phase = std::nullopt, std::optional<WebWheelEvent::Phase> momentum = std::nullopt)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
    if (controller && controller->isSwipeGestureEnabled()) {
        double deltaX, deltaY;
        gdk_event_get_scroll_deltas(event, &deltaX, &deltaY);
        FloatSize delta(deltaX, deltaY);

        int32_t eventTime = static_cast<int32_t>(gdk_event_get_time(event));

        GdkDevice* device = gdk_event_get_source_device(event);
        GdkInputSource source = gdk_device_get_source(device);

        bool isEnd = gdk_event_is_scroll_stop_event(event) ? true : false;

        PlatformGtkScrollData scrollData = { .delta = delta, .eventTime = eventTime, .source = source, .isEnd = isEnd };
        if (controller->handleScrollWheelEvent(&scrollData))
            return;
    }

    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    ASSERT(!priv->dialog);
    if (phase)
        priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(event, phase.value(), momentum.value_or(WebWheelEvent::Phase::PhaseNone)));
    else
        priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(event));
}

static gboolean webkitWebViewBaseScrollEvent(GtkWidget* widget, GdkEventScroll* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (std::exchange(priv->shouldForwardNextWheelEvent, false))
        return GDK_EVENT_PROPAGATE;

    if (priv->dialog)
        return GDK_EVENT_PROPAGATE;

    if (shouldInvertDirectionForScrollEvent(priv->mouseIsOverScrollbar, event->state & GDK_SHIFT_MASK)) {
        switch (event->direction) {
        case GDK_SCROLL_UP:
            event->direction = GDK_SCROLL_LEFT;
            break;
        case GDK_SCROLL_LEFT:
            event->direction = GDK_SCROLL_UP;
            break;
        case GDK_SCROLL_DOWN:
            event->direction = GDK_SCROLL_RIGHT;
            break;
        case GDK_SCROLL_RIGHT:
            event->direction = GDK_SCROLL_DOWN;
            break;
        case GDK_SCROLL_SMOOTH:
            std::swap(event->delta_x, event->delta_y);
            break;
        }
    }

    webkitWebViewBaseHandleWheelEvent(webViewBase, reinterpret_cast<GdkEvent*>(event));

    return GDK_EVENT_STOP;
}
#endif

#if USE(GTK4)
static gboolean handleScroll(WebKitWebViewBase* webViewBase, double deltaX, double deltaY, bool isEnd, GtkEventController* eventController)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return GDK_EVENT_PROPAGATE;

    auto* event = gtk_event_controller_get_current_event(eventController);

    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
    if (controller && controller->isSwipeGestureEnabled()) {
        FloatSize delta(deltaX, deltaY);

        int32_t eventTime = static_cast<int32_t>(gtk_event_controller_get_current_event_time(eventController));

        GdkDevice* device = gtk_event_controller_get_current_event_device(eventController);
        // We only have hold events on touchpads, so assume one if we get no event.
        GdkInputSource source = device ? gdk_device_get_source(device) : GDK_SOURCE_TOUCHPAD;

        PlatformGtkScrollData scrollData = { .delta = delta, .eventTime = eventTime, .source = source, .isEnd = isEnd };
        if (controller->handleScrollWheelEvent(&scrollData))
            return GDK_EVENT_STOP;
    }

    if (!isEnd && shouldInvertDirectionForScrollEvent(priv->mouseIsOverScrollbar, gdk_event_get_modifier_state(event) & GDK_SHIFT_MASK))
        std::swap(deltaX, deltaY);

    if (event)
        priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(event, priv->lastMotionEvent ? IntPoint(priv->lastMotionEvent->position) : IntPoint(), FloatSize(-deltaX, -deltaY)));

    return GDK_EVENT_STOP;
}

static void webkitWebViewBaseScrollBegin(WebKitWebViewBase* webViewBase, GtkEventController* controller)
{
    handleScroll(webViewBase, 0, 0, false, controller);
}

static gboolean webkitWebViewBaseScroll(WebKitWebViewBase* webViewBase, double deltaX, double deltaY, GtkEventController* eventController)
{
    return handleScroll(webViewBase, deltaX, deltaY, false, eventController);
}

static void webkitWebViewBaseScrollEnd(WebKitWebViewBase* webViewBase, GtkEventController* controller)
{
    handleScroll(webViewBase, 0, 0, true, controller);
}
#endif

#if !USE(GTK4)
static gboolean webkitWebViewBasePopupMenu(GtkWidget* widget)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    GdkEvent* currentEvent = gtk_get_current_event();
    if (!currentEvent)
        currentEvent = gdk_event_new(GDK_NOTHING);
    priv->contextMenuEvent.reset(currentEvent);
    priv->pageProxy->handleContextMenuKeyEvent();

    return TRUE;
}

static gboolean webkitWebViewBaseMotionNotifyEvent(GtkWidget* widget, GdkEventMotion* event)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->dialog) {
        auto* widgetClass = GTK_WIDGET_CLASS(webkit_web_view_base_parent_class);
        return widgetClass->motion_notify_event ? widgetClass->motion_notify_event(widget, event) : GDK_EVENT_PROPAGATE;
    }

    if (gdk_device_get_source(gdk_event_get_source_device(reinterpret_cast<GdkEvent*>(event))) == GDK_SOURCE_TOUCHSCREEN)
        return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->motion_notify_event(widget, event);

    if (priv->pointerLockManager) {
        double x, y;
        gdk_event_get_root_coords(reinterpret_cast<GdkEvent*>(event), &x, &y);
        priv->pointerLockManager->didReceiveMotionEvent(FloatPoint(x, y));
        return GDK_EVENT_STOP;
    }

    webkitWebViewBaseHandleMouseEvent(webViewBase, reinterpret_cast<GdkEvent*>(event));

    return GDK_EVENT_PROPAGATE;
}

static gboolean webkitWebViewBaseCrossingNotifyEvent(GtkWidget* widget, GdkEventCrossing* crossingEvent)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->dialog)
        return GDK_EVENT_PROPAGATE;

#if ENABLE(DEVELOPER_MODE)
    // Do not send mouse move events to the WebProcess for crossing events during testing.
    // WTR never generates crossing events and they can confuse tests.
    // https://bugs.webkit.org/show_bug.cgi?id=185072.
    if (UNLIKELY(priv->pageProxy->process().processPool().configuration().fullySynchronousModeIsAllowedForTesting()))
        return GDK_EVENT_PROPAGATE;
#endif

    // In the case of crossing events, it's very important the actual coordinates the WebProcess receives, because once the mouse leaves
    // the web view, the WebProcess won't receive more events until the mouse enters again in the web view. So, if the coordinates of the leave
    // event are not accurate, the WebProcess might not know the mouse left the view. This can happen because of double to integer conversion,
    // if the coordinates of the leave event are for example (25.2, -0.9), the WebProcess will receive (25, 0) and any hit test will succeed
    // because those coordinates are inside the web view.
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    double xEvent, yEvent;
    gdk_event_get_coords(reinterpret_cast<GdkEvent*>(crossingEvent), &xEvent, &yEvent);
    double width = allocation.width;
    double height = allocation.height;
    double x = xEvent;
    double y = yEvent;
    if (x < 0 && x > -1)
        x = -1;
    else if (x >= width && x < width + 1)
        x = width + 1;
    if (y < 0 && y > -1)
        y = -1;
    else if (y >= height && y < height + 1)
        y = height + 1;

    GdkEvent* event = reinterpret_cast<GdkEvent*>(crossingEvent);
    GUniquePtr<GdkEvent> copiedEvent;
    if (x != xEvent || y != yEvent) {
        copiedEvent.reset(gdk_event_copy(event));
        copiedEvent->crossing.x = x;
        copiedEvent->crossing.y = y;
    }

    webkitWebViewBaseHandleMouseEvent(webViewBase, copiedEvent ? copiedEvent.get() : event);

    return GDK_EVENT_PROPAGATE;
}
#endif

#if USE(GTK4)
static void webkitWebViewBaseEnter(WebKitWebViewBase* webViewBase, double x, double y, GdkCrossingMode, GtkEventController*)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

#if ENABLE(DEVELOPER_MODE)
    // Do not send mouse move events to the WebProcess for crossing events during testing.
    // WTR never generates crossing events and they can confuse tests.
    // https://bugs.webkit.org/show_bug.cgi?id=185072.
    if (UNLIKELY(priv->pageProxy->process().processPool().configuration().fullySynchronousModeIsAllowedForTesting()))
        return;
#endif

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent({ clampToInteger(x), clampToInteger(y) }));
}

static gboolean webkitWebViewBaseMotion(WebKitWebViewBase* webViewBase, double x, double y, GtkEventController* controller)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return GDK_EVENT_PROPAGATE;

    if (priv->pointerLockManager) {
        priv->pointerLockManager->didReceiveMotionEvent(FloatPoint(x, y));
        return GDK_EVENT_STOP;
    }

    auto* event = gtk_event_controller_get_current_event(controller);
    std::optional<FloatSize> movementDelta;
    MotionEvent motionEvent(FloatPoint(x, y), FloatPoint(x, y), gdk_event_get_modifier_state(event));
    if (priv->lastMotionEvent)
        movementDelta = motionEvent.position - priv->lastMotionEvent->position;
    priv->lastMotionEvent = WTFMove(motionEvent);

    webViewBase->priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(event, { clampToInteger(x), clampToInteger(y) }, 0, movementDelta));

    return GDK_EVENT_PROPAGATE;
}

static void webkitWebViewBaseLeave(WebKitWebViewBase* webViewBase, GdkCrossingMode, GtkEventController*)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

#if ENABLE(DEVELOPER_MODE)
    // Do not send mouse move events to the WebProcess for crossing events during testing.
    // WTR never generates crossing events and they can confuse tests.
    // https://bugs.webkit.org/show_bug.cgi?id=185072.
    if (UNLIKELY(priv->pageProxy->process().processPool().configuration().fullySynchronousModeIsAllowedForTesting()))
        return;
#endif

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent({ -1, -1 }));
}
#endif

#if ENABLE(TOUCH_EVENTS)
static void appendTouchEvent(GtkWidget* webViewBase, Vector<WebPlatformTouchPoint>& touchPoints, GdkEvent* event, WebPlatformTouchPoint::TouchPointState state)
{
    gdouble x, y;
    gdk_event_get_coords(event, &x, &y);
#if USE(GTK4)
    // Events in GTK4 are given in native surface coordinates
    gtk_widget_translate_coordinates(GTK_WIDGET(gtk_widget_get_native(webViewBase)),
        webViewBase, x, y, &x, &y);
#endif

    gdouble xRoot, yRoot;
    gdk_event_get_root_coords(event, &xRoot, &yRoot);

    uint32_t identifier = GPOINTER_TO_UINT(gdk_event_get_event_sequence(event));
    touchPoints.uncheckedAppend(WebPlatformTouchPoint(identifier, state, IntPoint(xRoot, yRoot), IntPoint(x, y)));
}

static inline WebPlatformTouchPoint::TouchPointState touchPointStateForEvents(GdkEvent* current, GdkEvent* event)
{
    if (gdk_event_get_event_sequence(current) != gdk_event_get_event_sequence(event))
        return WebPlatformTouchPoint::TouchStationary;

    switch (gdk_event_get_event_type(event)) {
    case GDK_TOUCH_UPDATE:
        return WebPlatformTouchPoint::TouchMoved;
    case GDK_TOUCH_BEGIN:
        return WebPlatformTouchPoint::TouchPressed;
    case GDK_TOUCH_END:
        return WebPlatformTouchPoint::TouchReleased;
    case GDK_TOUCH_CANCEL:
        return WebPlatformTouchPoint::TouchCancelled;
    default:
        return WebPlatformTouchPoint::TouchStationary;
    }
}

static void webkitWebViewBaseGetTouchPointsForEvent(WebKitWebViewBase* webViewBase, GdkEvent* event, Vector<WebPlatformTouchPoint>& touchPoints)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    GdkEventType type = gdk_event_get_event_type(event);
    bool touchEnd = (type == GDK_TOUCH_END) || (type == GDK_TOUCH_CANCEL);
    touchPoints.reserveInitialCapacity(touchEnd ? priv->touchEvents.size() + 1 : priv->touchEvents.size());

    GtkWidget* widget = GTK_WIDGET(webViewBase);
    for (const auto& it : priv->touchEvents)
        appendTouchEvent(widget, touchPoints, it.value.get(), touchPointStateForEvents(it.value.get(), event));

    // Touch was already removed from the TouchEventsMap, add it here.
    if (touchEnd)
        appendTouchEvent(widget, touchPoints, event, WebPlatformTouchPoint::TouchReleased);
}

#if USE(GTK4)
static gboolean webkitWebViewBaseTouchEvent(GtkWidget* widget, GdkEvent* event)
#else
static gboolean webkitWebViewBaseTouchEvent(GtkWidget* widget, GdkEventTouch* event)
#endif
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;

    if (priv->dialog)
        return GDK_EVENT_STOP;

    GdkEvent* touchEvent = reinterpret_cast<GdkEvent*>(event);
    uint32_t sequence = GPOINTER_TO_UINT(gdk_event_get_event_sequence(touchEvent));

    GdkEventType type = gdk_event_get_event_type(touchEvent);
    switch (type) {
    case GDK_TOUCH_BEGIN: {
        if (priv->touchEvents.isEmpty())
            priv->pageGrabbedTouch = false;
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
    case GDK_TOUCH_CANCEL:
        FALLTHROUGH;
    case GDK_TOUCH_END:
        ASSERT(priv->touchEvents.contains(sequence));
        priv->touchEvents.remove(sequence);
        break;
    default:
#if !USE(GTK4)
        ASSERT_NOT_REACHED();
#endif
        return GDK_EVENT_PROPAGATE;
    }

    Vector<WebPlatformTouchPoint> touchPoints;
    webkitWebViewBaseGetTouchPointsForEvent(webViewBase, touchEvent, touchPoints);
    priv->pageProxy->handleTouchEvent(NativeWebTouchEvent(reinterpret_cast<GdkEvent*>(event), WTFMove(touchPoints)));

#if USE(GTK4)
    return GDK_EVENT_PROPAGATE;
#else
    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->touch_event(widget, event);
#endif
}
#endif // ENABLE(TOUCH_EVENTS)

void webkitWebViewBaseSetEnableBackForwardNavigationGesture(WebKitWebViewBase* webViewBase, bool enabled)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->isBackForwardNavigationGestureEnabled = enabled;

    if (auto* controller = webkitWebViewBaseViewGestureController(webViewBase))
        controller->setSwipeGestureEnabled(enabled);

    priv->pageProxy->setShouldRecordNavigationSnapshots(enabled);
}

ViewGestureController* webkitWebViewBaseViewGestureController(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->viewGestureController.get();
}

bool webkitWebViewBaseBeginBackSwipeForTesting(WebKitWebViewBase* webViewBase)
{
    if (auto* gestureController = webkitWebViewBaseViewGestureController(webViewBase))
        return gestureController->beginSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);

    return FALSE;
}

bool webkitWebViewBaseCompleteBackSwipeForTesting(WebKitWebViewBase* webViewBase)
{
    if (auto* gestureController = webkitWebViewBaseViewGestureController(webViewBase))
        return gestureController->completeSimulatedSwipeInDirectionForTesting(ViewGestureController::SwipeDirection::Back);

    return FALSE;
}

GVariant* webkitWebViewBaseContentsOfUserInterfaceItem(WebKitWebViewBase* webViewBase, const char* userInterfaceItem)
{
    if (g_strcmp0(userInterfaceItem, "validationBubble"))
        return nullptr;

    WebPageProxy* page = webViewBase->priv->pageProxy.get();
    auto* validationBubble = page->validationBubble();
    String message = validationBubble ? validationBubble->message() : emptyString();
    double fontSize = validationBubble ? validationBubble->fontSize() : 0;

    GVariantBuilder subBuilder;
    g_variant_builder_init(&subBuilder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&subBuilder, "{sv}", "message", g_variant_new_string(message.utf8().data()));
    g_variant_builder_add(&subBuilder, "{sv}", "fontSize", g_variant_new_double(fontSize));

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&builder, "{sv}", userInterfaceItem, g_variant_builder_end(&subBuilder));

    return g_variant_builder_end(&builder);
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

#if !USE(GTK4) && ENABLE(ACCESSIBILITY)
static AtkObject* webkitWebViewBaseGetAccessible(GtkWidget* widget)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (!priv->accessible) {
        // Create the accessible object and associate it to the widget.
        priv->accessible = adoptGRef(ATK_OBJECT(webkitWebViewAccessibleNew(widget)));

        // Set the parent to not break bottom-up navigation.
        if (auto* parentWidget = gtk_widget_get_parent(widget)) {
            if (auto* axParent = gtk_widget_get_accessible(parentWidget))
                atk_object_set_parent(priv->accessible.get(), axParent);
        }
    }

    return priv->accessible.get();
}
#endif

#if USE(GTK4)
static void toplevelWindowIsActiveChanged(GtkWindow* window, GParamSpec*, WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (gtk_window_is_active(window)) {
        if (priv->activityState & ActivityState::WindowIsActive)
            return;
        priv->activityState.add(ActivityState::WindowIsActive);
        if (priv->activityState & ActivityState::IsFocused)
            priv->inputMethodFilter.notifyFocusedIn();
    } else {
        if (!(priv->activityState & ActivityState::WindowIsActive))
            return;
        priv->activityState.remove(ActivityState::WindowIsActive);
        if (priv->activityState & ActivityState::IsFocused)
            priv->inputMethodFilter.notifyFocusedOut();
    }

    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::WindowIsActive);
}

static void toplevelWindowStateChanged(GdkSurface* surface, GParamSpec*, WebKitWebViewBase* webViewBase)
{
    auto state = gdk_toplevel_get_state(GDK_TOPLEVEL(surface));
    bool visible = !(state & GDK_TOPLEVEL_STATE_MINIMIZED);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (visible) {
        if (priv->activityState & ActivityState::IsVisible)
            return;
        priv->activityState.add(ActivityState::IsVisible);
    } else {
        if (!(priv->activityState & ActivityState::IsVisible))
            return;
        priv->activityState.remove(ActivityState::IsVisible);
    }
    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, ActivityState::IsVisible);
}

static void toplevelWindowRealized(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    g_clear_signal_handler(&priv->toplevelWindowRealizedID, priv->toplevelOnScreenWindow);
    priv->toplevelWindowStateChangedID =
        g_signal_connect(gtk_native_get_surface(GTK_NATIVE(priv->toplevelOnScreenWindow)), "notify::state", G_CALLBACK(toplevelWindowStateChanged), webViewBase);
}

static void toplevelWindowUnrealized(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    g_clear_signal_handler(&priv->toplevelWindowUnrealizedID, priv->toplevelOnScreenWindow);
    g_clear_signal_handler(&priv->toplevelWindowStateChangedID, gtk_native_get_surface(GTK_NATIVE(priv->toplevelOnScreenWindow)));
}

static void webkitWebViewBaseRoot(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->root(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->toplevelOnScreenWindow = GTK_WINDOW(gtk_widget_get_root(widget));

    OptionSet<ActivityState::Flag> flagsToUpdate;
    if (!(priv->activityState & ActivityState::IsInWindow)) {
        priv->activityState.add(ActivityState::IsInWindow);
        flagsToUpdate.add(ActivityState::IsInWindow);
    }
    if (gtk_widget_get_visible(GTK_WIDGET(priv->toplevelOnScreenWindow)) && gtk_window_is_active(priv->toplevelOnScreenWindow)) {
        priv->activityState.add(ActivityState::WindowIsActive);
        flagsToUpdate.add(ActivityState::IsInWindow);
    }
    if (flagsToUpdate)
        webkitWebViewBaseScheduleUpdateActivityState(webViewBase, flagsToUpdate);

    priv->toplevelIsActiveID =
        g_signal_connect(priv->toplevelOnScreenWindow, "notify::is-active", G_CALLBACK(toplevelWindowIsActiveChanged), widget);
    if (gtk_widget_get_realized(GTK_WIDGET(priv->toplevelOnScreenWindow))) {
        priv->toplevelWindowStateChangedID =
            g_signal_connect(gtk_native_get_surface(GTK_NATIVE(priv->toplevelOnScreenWindow)), "notify::state", G_CALLBACK(toplevelWindowStateChanged), widget);
    } else {
        priv->toplevelWindowRealizedID =
            g_signal_connect_swapped(priv->toplevelOnScreenWindow, "realize", G_CALLBACK(toplevelWindowRealized), widget);
    }
    priv->toplevelWindowUnrealizedID =
        g_signal_connect_swapped(priv->toplevelOnScreenWindow, "unrealize", G_CALLBACK(toplevelWindowUnrealized), widget);
}

static void webkitWebViewBaseUnroot(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->unroot(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    g_clear_signal_handler(&priv->toplevelIsActiveID, priv->toplevelOnScreenWindow);
    g_clear_signal_handler(&priv->toplevelWindowRealizedID, priv->toplevelOnScreenWindow);
    g_clear_signal_handler(&priv->toplevelWindowUnrealizedID, priv->toplevelOnScreenWindow);
    if (gtk_widget_get_realized(GTK_WIDGET(priv->toplevelOnScreenWindow)))
        g_clear_signal_handler(&priv->toplevelWindowStateChangedID, gtk_native_get_surface(GTK_NATIVE(priv->toplevelOnScreenWindow)));
    priv->toplevelOnScreenWindow = nullptr;

    OptionSet<ActivityState::Flag> flagsToUpdate;
    if (priv->activityState & ActivityState::IsInWindow) {
        priv->activityState.remove(ActivityState::IsInWindow);
        flagsToUpdate.add(ActivityState::IsInWindow);
    }
    if (priv->activityState & ActivityState::WindowIsActive) {
        priv->activityState.remove(ActivityState::WindowIsActive);
        flagsToUpdate.add(ActivityState::IsInWindow);
    }
    if (flagsToUpdate)
        webkitWebViewBaseScheduleUpdateActivityState(webViewBase, flagsToUpdate);
}
#else
static void webkitWebViewBaseHierarchyChanged(GtkWidget* widget, GtkWidget* oldToplevel)
{
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (widgetIsOnscreenToplevelWindow(oldToplevel) && GTK_WINDOW(oldToplevel) == priv->toplevelOnScreenWindow) {
        webkitWebViewBaseSetToplevelOnScreenWindow(WEBKIT_WEB_VIEW_BASE(widget), nullptr);
        return;
    }

    if (!oldToplevel) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
        if (widgetIsOnscreenToplevelWindow(toplevel))
            webkitWebViewBaseSetToplevelOnScreenWindow(WEBKIT_WEB_VIEW_BASE(widget), GTK_WINDOW(toplevel));
    }
}
#endif

#if USE(GTK4)
static gboolean webkitWebViewBaseGrabFocus(GtkWidget* widget)
{
    gtk_root_set_focus(gtk_widget_get_root(widget), widget);
    return TRUE;
}
#endif

static gboolean webkitWebViewBaseFocus(GtkWidget* widget, GtkDirectionType direction)
{
    // If a dialog is active, we need to forward focus events there. This
    // ensures that you can tab between elements in the box.
    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(widget)->priv;
    if (priv->dialog) {
        gboolean returnValue;
        g_signal_emit_by_name(priv->dialog, "focus", direction, &returnValue);
        return returnValue;
    }

    return GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->focus(widget, direction);
}

#if USE(GTK4)
static void webkitWebViewBaseCssChanged(GtkWidget* widget, GtkCssStyleChange* change)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->css_changed(widget, change);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->pageProxy->accentColorDidChange();
}
#else
static void webkitWebViewBaseStyleUpdated(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(webkit_web_view_base_parent_class)->style_updated(widget);

    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(widget);
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->pageProxy->accentColorDidChange();
}
#endif

static void webkitWebViewBaseZoomBegin(WebKitWebViewBase* webViewBase, GdkEventSequence* sequence, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;

    double x, y;
    gtk_gesture_get_bounding_box_center(gesture, &x, &y);

    auto* event = gtk_gesture_get_last_event(gesture, sequence);

    webkitWebViewBaseSynthesizeWheelEvent(webViewBase, event, 0, 0, x, y, WheelEventPhase::Began, WheelEventPhase::NoPhase, true);

#if !USE(GTK4)
    GtkGesture* click = GTK_GESTURE(g_object_get_data(G_OBJECT(webViewBase), "wk-view-multi-press-gesture"));
    gtk_gesture_set_state(click, GTK_EVENT_SEQUENCE_DENIED);
#endif
}

static void webkitWebViewBaseZoomChanged(WebKitWebViewBase* webViewBase, gdouble scale, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;

    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
    if (!controller)
        return;

    double x, y;
    gtk_gesture_get_bounding_box_center(gesture, &x, &y);
    FloatPoint origin = FloatPoint(x, y);

    controller->setMagnification(scale, origin);
}

static void webkitWebViewBaseZoomEnd(WebKitWebViewBase* webViewBase, GdkEventSequence* sequence, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;

    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
    if (!controller)
        return;

    controller->endMagnification();
}

static void webkitWebViewBaseTouchLongPress(WebKitWebViewBase* webViewBase, gdouble x, gdouble y, GtkGesture*)
{
    webViewBase->priv->isLongPressed = true;
}

static void webkitWebViewBaseTouchPress(WebKitWebViewBase* webViewBase, int nPress, double x, double y, GtkGesture*)
{
    webViewBase->priv->isLongPressed = false;
}

static void webkitWebViewBaseTouchRelease(WebKitWebViewBase* webViewBase, int nPress, double x, double y, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;
    if (priv->isBeingDragged)
        return;

    unsigned button;
    unsigned buttons;
    if (priv->isLongPressed) {
        button = GDK_BUTTON_SECONDARY;
        buttons = GDK_BUTTON3_MASK;
    } else {
        button = GDK_BUTTON_PRIMARY;
        buttons = GDK_BUTTON1_MASK;
    }

    unsigned modifiers = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Motion, 0, 0, x, y, modifiers, nPress, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
    webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Press, button, 0, x, y, modifiers, nPress, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
    webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Release, button, buttons, x, y, modifiers, nPress, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
}

static void webkitWebViewBaseTouchDragBegin(WebKitWebViewBase* webViewBase, gdouble startX, gdouble startY, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->dragOffset.set(0, 0);
    priv->isBeingDragged = false;

    auto* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    auto* event = gtk_gesture_get_last_event(gesture, sequence);

    webkitWebViewBaseSynthesizeWheelEvent(webViewBase, event, 0, 0, startX, startY, WheelEventPhase::Began, WheelEventPhase::NoPhase, true);
}

static void webkitWebViewBaseTouchDragUpdate(WebKitWebViewBase* webViewBase, double offsetX, double offsetY, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;

    double x, y;
    gtk_gesture_drag_get_start_point(GTK_GESTURE_DRAG(gesture), &x, &y);

    auto* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    auto* event = gtk_gesture_get_last_event(gesture, sequence);

    unsigned modifiers = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    if (!priv->isBeingDragged) {
        if (!gtk_drag_check_threshold(GTK_WIDGET(webViewBase), 0, 0, static_cast<int>(offsetX), static_cast<int>(offsetY)))
            return;
        priv->isBeingDragged = true;
        gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

        if (priv->isLongPressed) {
            // Drag after long press forwards emulated mouse events (for e.g. text selection)
            webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Motion, 0, 0, x, y, modifiers, 1, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
            webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Press, GDK_BUTTON_PRIMARY, 0, x, y, modifiers, 0, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
        } else
            webkitWebViewBaseSynthesizeWheelEvent(webViewBase, event, 0, 0, x, y, WheelEventPhase::Began, WheelEventPhase::NoPhase, true);
    }

    if (priv->isLongPressed)
        webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Motion, GDK_BUTTON_PRIMARY, GDK_BUTTON1_MASK, x + offsetX, y + offsetY, modifiers, 0, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
    else {
        double deltaX = priv->dragOffset.x() - offsetX;
        double deltaY = priv->dragOffset.y() - offsetY;
        priv->dragOffset.set(offsetX, offsetY);

        ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
        if (controller && controller->isSwipeGestureEnabled()) {
            FloatSize delta(deltaX / Scrollbar::pixelsPerLineStep(), deltaY / Scrollbar::pixelsPerLineStep());
            int32_t eventTime = static_cast<int32_t>(gtk_event_controller_get_current_event_time(GTK_EVENT_CONTROLLER(gesture)));
            PlatformGtkScrollData scrollData = { .delta = delta, .eventTime = eventTime, .source = GDK_SOURCE_TOUCHSCREEN, .isEnd = false };
            if (controller->handleScrollWheelEvent(&scrollData))
                return;
        }

        webkitWebViewBaseSynthesizeWheelEvent(webViewBase, event, -deltaX, -deltaY, x, y, WheelEventPhase::Changed, WheelEventPhase::NoPhase, true);
    }
}

static void webkitWebViewBaseTouchDragEnd(WebKitWebViewBase* webViewBase, gdouble offsetX, gdouble offsetY, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch)
        return;

    if (priv->isLongPressed) {
        double x, y;
        gtk_gesture_drag_get_start_point(GTK_GESTURE_DRAG(gesture), &x, &y);
        unsigned modifiers = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
        webkitWebViewBaseSynthesizeMouseEvent(webViewBase, MouseEventType::Release, GDK_BUTTON_PRIMARY, GDK_BUTTON1_MASK, x + offsetX, y + offsetY, modifiers, 0, mousePointerEventType(), PlatformMouseEvent::IsTouch::Yes);
    }
}

static void webkitWebViewBaseTouchDragCancel(WebKitWebViewBase* webViewBase, GdkEventSequence*, GtkGesture*)
{
    if (auto* controller = webkitWebViewBaseViewGestureController(webViewBase))
        controller->cancelSwipe();
}

static void webkitWebViewBaseTouchSwipe(WebKitWebViewBase* webViewBase, gdouble velocityX, gdouble velocityY, GtkGesture* gesture)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->pageGrabbedTouch || !priv->isBeingDragged || priv->isLongPressed)
        return;

    double x, y;
    if (gtk_gesture_get_point(gesture, gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture)), &x, &y)) {
        auto* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
        auto* event = gtk_gesture_get_last_event(gesture, sequence);

        ViewGestureController* controller = webkitWebViewBaseViewGestureController(webViewBase);
        if (controller && controller->isSwipeGestureEnabled()) {
            int32_t eventTime = static_cast<int32_t>(gtk_event_controller_get_current_event_time(GTK_EVENT_CONTROLLER(gesture)));
            PlatformGtkScrollData scrollData = { .delta = FloatSize(), .eventTime = eventTime, .source = GDK_SOURCE_TOUCHSCREEN, .isEnd = true };
            if (controller->handleScrollWheelEvent(&scrollData))
                return;
        }

        webkitWebViewBaseSynthesizeWheelEvent(webViewBase, event, -velocityX, -velocityY, x, y, WheelEventPhase::NoPhase, WheelEventPhase::Began, true);
    }
}

static void webkitWebViewBaseConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_base_parent_class)->constructed(object);

    GtkWidget* viewWidget = GTK_WIDGET(object);
    gtk_widget_set_can_focus(viewWidget, TRUE);

    WebKitWebViewBasePrivate* priv = WEBKIT_WEB_VIEW_BASE(object)->priv;
    priv->pageClient = makeUnique<PageClientImpl>(viewWidget);
    gtk_widget_set_parent(priv->keyBindingTranslator.widget(), viewWidget);

#if ENABLE(DRAG_SUPPORT)
    priv->dropTarget = makeUnique<DropTarget>(viewWidget);
#endif

#if USE(GTK4)
    auto* controller = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect_object(controller, "scroll-begin", G_CALLBACK(webkitWebViewBaseScrollBegin), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "scroll", G_CALLBACK(webkitWebViewBaseScroll), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "scroll-end", G_CALLBACK(webkitWebViewBaseScrollEnd), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, controller);

    controller = gtk_event_controller_motion_new();
    g_signal_connect_object(controller, "enter", G_CALLBACK(webkitWebViewBaseEnter), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "motion", G_CALLBACK(webkitWebViewBaseMotion), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "leave", G_CALLBACK(webkitWebViewBaseLeave), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, controller);

    controller = gtk_event_controller_focus_new();
    g_signal_connect_object(controller, "enter", G_CALLBACK(webkitWebViewBaseFocusEnter), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "leave", G_CALLBACK(webkitWebViewBaseFocusLeave), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, controller);

    controller = gtk_event_controller_key_new();
    g_signal_connect_object(controller, "key-pressed", G_CALLBACK(webkitWebViewBaseKeyPressed), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(controller, "key-released", G_CALLBACK(webkitWebViewBaseKeyReleased), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, controller);

    controller = gtk_event_controller_legacy_new();
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(controller), GTK_PHASE_TARGET);
    g_signal_connect_object(controller, "event", G_CALLBACK(webkitWebViewBaseTouchEvent), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(controller));

    auto* gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0);
    gtk_gesture_single_set_exclusive(GTK_GESTURE_SINGLE(gesture), TRUE);
    g_signal_connect_object(gesture, "pressed", G_CALLBACK(webkitWebViewBaseButtonPressed), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(gesture, "released", G_CALLBACK(webkitWebViewBaseButtonReleased), viewWidget, G_CONNECT_SWAPPED);
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(gesture));
#endif

#if USE(GTK4)
    gesture = gtk_gesture_click_new();
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(gesture));
#else
    auto* gesture = gtk_gesture_multi_press_new(viewWidget);
    g_object_set_data_full(G_OBJECT(viewWidget), "wk-view-multi-press-gesture", gesture, g_object_unref);
#endif
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(gesture), TRUE);
    g_signal_connect_object(gesture, "pressed", G_CALLBACK(webkitWebViewBaseTouchPress), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(gesture, "released", G_CALLBACK(webkitWebViewBaseTouchRelease), viewWidget, G_CONNECT_SWAPPED);

    // Touch gestures
#if USE(GTK4)
    priv->touchGestureGroup = gtk_gesture_zoom_new();
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(priv->touchGestureGroup));
#else
    priv->touchGestureGroup = gtk_gesture_zoom_new(viewWidget);
    g_object_set_data_full(G_OBJECT(viewWidget), "wk-view-zoom-gesture", priv->touchGestureGroup, g_object_unref);
#endif
    g_signal_connect_object(priv->touchGestureGroup, "begin", G_CALLBACK(webkitWebViewBaseZoomBegin), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->touchGestureGroup, "scale-changed", G_CALLBACK(webkitWebViewBaseZoomChanged), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(priv->touchGestureGroup, "end", G_CALLBACK(webkitWebViewBaseZoomEnd), viewWidget, G_CONNECT_SWAPPED);

#if USE(GTK4)
    gesture = gtk_gesture_long_press_new();
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(gesture));
#else
    gesture = gtk_gesture_long_press_new(viewWidget);
    g_object_set_data_full(G_OBJECT(viewWidget), "wk-view-long-press-gesture", gesture, g_object_unref);
#endif
    gtk_gesture_group(gesture, priv->touchGestureGroup);
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(gesture), TRUE);
    g_signal_connect_object(gesture, "pressed", G_CALLBACK(webkitWebViewBaseTouchLongPress), viewWidget, G_CONNECT_SWAPPED);

#if USE(GTK4)
    gesture = gtk_gesture_drag_new();
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(gesture));
#else
    gesture = gtk_gesture_drag_new(viewWidget);
    g_object_set_data_full(G_OBJECT(viewWidget), "wk-view-drag-gesture", gesture, g_object_unref);
#endif
    gtk_gesture_group(gesture, priv->touchGestureGroup);
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(gesture), TRUE);
    g_signal_connect_object(gesture, "drag-begin", G_CALLBACK(webkitWebViewBaseTouchDragBegin), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(gesture, "drag-update", G_CALLBACK(webkitWebViewBaseTouchDragUpdate), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(gesture, "drag-end", G_CALLBACK(webkitWebViewBaseTouchDragEnd), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(gesture, "cancel", G_CALLBACK(webkitWebViewBaseTouchDragCancel), viewWidget, G_CONNECT_SWAPPED);

#if USE(GTK4)
    gesture = gtk_gesture_swipe_new();
    gtk_widget_add_controller(viewWidget, GTK_EVENT_CONTROLLER(gesture));
#else
    gesture = gtk_gesture_swipe_new(viewWidget);
    g_object_set_data_full(G_OBJECT(viewWidget), "wk-view-swipe-gesture", gesture, g_object_unref);
#endif
    gtk_gesture_group(gesture, priv->touchGestureGroup);
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(gesture), TRUE);
    g_signal_connect_object(gesture, "swipe", G_CALLBACK(webkitWebViewBaseTouchSwipe), viewWidget, G_CONNECT_SWAPPED);

    auto* settings = gtk_settings_get_default();
    auto callback = +[](WebKitWebViewBase* webViewBase) {
        webViewBase->priv->pageProxy->effectiveAppearanceDidChange();
    };
    g_signal_connect_object(settings, "notify::gtk-theme-name", G_CALLBACK(callback), viewWidget, G_CONNECT_SWAPPED);
    g_signal_connect_object(settings, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(callback), viewWidget, G_CONNECT_SWAPPED);
}

static void webkit_web_view_base_class_init(WebKitWebViewBaseClass* webkitWebViewBaseClass)
{
    GtkWidgetClass* widgetClass = GTK_WIDGET_CLASS(webkitWebViewBaseClass);
    widgetClass->realize = webkitWebViewBaseRealize;
    widgetClass->unrealize = webkitWebViewBaseUnrealize;
#if USE(GTK4)
    widgetClass->snapshot = webkitWebViewBaseSnapshot;
#else
    widgetClass->draw = webkitWebViewBaseDraw;
#endif
    widgetClass->size_allocate = webkitWebViewBaseSizeAllocate;
#if USE(GTK4)
    widgetClass->measure = webkitWebViewBaseMeasure;
#else
    widgetClass->get_preferred_width = webkitWebViewBaseGetPreferredWidth;
    widgetClass->get_preferred_height = webkitWebViewBaseGetPreferredHeight;
#endif
    widgetClass->map = webkitWebViewBaseMap;
    widgetClass->unmap = webkitWebViewBaseUnmap;
    widgetClass->focus = webkitWebViewBaseFocus;
#if USE(GTK4)
    widgetClass->grab_focus = webkitWebViewBaseGrabFocus;
#else
    widgetClass->focus_in_event = webkitWebViewBaseFocusInEvent;
    widgetClass->focus_out_event = webkitWebViewBaseFocusOutEvent;
    widgetClass->key_press_event = webkitWebViewBaseKeyPressEvent;
    widgetClass->key_release_event = webkitWebViewBaseKeyReleaseEvent;
    widgetClass->button_press_event = webkitWebViewBaseButtonPressEvent;
    widgetClass->button_release_event = webkitWebViewBaseButtonReleaseEvent;
    widgetClass->scroll_event = webkitWebViewBaseScrollEvent;
    widgetClass->popup_menu = webkitWebViewBasePopupMenu;
    widgetClass->motion_notify_event = webkitWebViewBaseMotionNotifyEvent;
    widgetClass->enter_notify_event = webkitWebViewBaseCrossingNotifyEvent;
    widgetClass->leave_notify_event = webkitWebViewBaseCrossingNotifyEvent;
#endif
#if ENABLE(TOUCH_EVENTS) && !USE(GTK4)
    widgetClass->touch_event = webkitWebViewBaseTouchEvent;
#endif
    widgetClass->query_tooltip = webkitWebViewBaseQueryTooltip;
#if !USE(GTK4) && ENABLE(ACCESSIBILITY)
    widgetClass->get_accessible = webkitWebViewBaseGetAccessible;
#endif
#if USE(GTK4)
    widgetClass->root = webkitWebViewBaseRoot;
    widgetClass->unroot = webkitWebViewBaseUnroot;
#else
    widgetClass->hierarchy_changed = webkitWebViewBaseHierarchyChanged;
#endif
#if USE(GTK4)
    widgetClass->css_changed = webkitWebViewBaseCssChanged;
#else
    widgetClass->style_updated = webkitWebViewBaseStyleUpdated;
#endif

    GObjectClass* gobjectClass = G_OBJECT_CLASS(webkitWebViewBaseClass);
    gobjectClass->constructed = webkitWebViewBaseConstructed;
    gobjectClass->dispose = webkitWebViewBaseDispose;

#if !USE(GTK4)
    GtkContainerClass* containerClass = GTK_CONTAINER_CLASS(webkitWebViewBaseClass);
    containerClass->add = webkitWebViewBaseContainerAdd;
    containerClass->remove = webkitWebViewBaseContainerRemove;
    containerClass->forall = webkitWebViewBaseContainerForall;
#endif

    // Before creating a WebKitWebViewBasePriv we need to be sure that WebKit is started.
    // Usually starting a context triggers webkitInitialize, but in case
    // we create a view without asking before for a default_context we get a crash.
    WebKit::webkitInitialize();

    gtk_widget_class_set_css_name(widgetClass, "webkitwebview");
}

WebKitWebViewBase* webkitWebViewBaseCreate(const API::PageConfiguration& configuration)
{
    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(g_object_new(WEBKIT_TYPE_WEB_VIEW_BASE, nullptr));
    webkitWebViewBaseCreateWebPage(webkitWebViewBase, configuration.copy());
    return webkitWebViewBase;
}

WebPageProxy* webkitWebViewBaseGetPage(WebKitWebViewBase* webkitWebViewBase)
{
    return webkitWebViewBase->priv->pageProxy.get();
}

static void deviceScaleFactorChanged(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
}

void webkitWebViewBaseCreateWebPage(WebKitWebViewBase* webkitWebViewBase, Ref<API::PageConfiguration>&& configuration)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    WebProcessPool* processPool = configuration->processPool();
    priv->pageProxy = processPool->createWebPage(*priv->pageClient, WTFMove(configuration));
    priv->acceleratedBackingStore = AcceleratedBackingStore::create(*priv->pageProxy);
    priv->pageProxy->initializeWebPage();

    // We attach this here, because changes in scale factor are passed directly to the page proxy.
    priv->pageProxy->setIntrinsicDeviceScaleFactor(gtk_widget_get_scale_factor(GTK_WIDGET(webkitWebViewBase)));
    g_signal_connect(webkitWebViewBase, "notify::scale-factor", G_CALLBACK(deviceScaleFactorChanged), nullptr);
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

void webkitWebViewBaseSetMouseIsOverScrollbar(WebKitWebViewBase* webViewBase, WebHitTestResultData::IsScrollbar isScrollbar)
{
    webViewBase->priv->mouseIsOverScrollbar = isScrollbar;
}

#if ENABLE(DRAG_SUPPORT)
void webkitWebViewBaseStartDrag(WebKitWebViewBase* webViewBase, SelectionData&& selectionData, OptionSet<DragOperation> dragOperationMask, RefPtr<ShareableBitmap>&& image, IntPoint&& dragImageHotspot)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->dragSource)
        priv->dragSource = makeUnique<DragSource>(GTK_WIDGET(webViewBase));

    priv->dragSource->begin(WTFMove(selectionData), dragOperationMask, WTFMove(image), WTFMove(dragImageHotspot));

#if !USE(GTK4)
    // A drag starting should prevent a double-click from happening. This might
    // happen if a drag is followed very quickly by another click (like in the WTR).
    priv->clickCounter.reset();
#endif
}

void webkitWebViewBaseDidPerformDragControllerAction(WebKitWebViewBase* webViewBase)
{
    webViewBase->priv->dropTarget->didPerformAction();
}
#endif // ENABLE(DRAG_SUPPORT)

void webkitWebViewBaseForwardNextKeyEvent(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->shouldForwardNextKeyEvent = TRUE;
}

void webkitWebViewBaseForwardNextWheelEvent(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->shouldForwardNextWheelEvent = true;
}

void webkitWebViewBaseEnterFullScreen(WebKitWebViewBase* webkitWebViewBase)
{
#if ENABLE(FULLSCREEN_API)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    ASSERT(!priv->fullScreenModeActive);

    WebFullScreenManagerProxy* fullScreenManagerProxy = priv->pageProxy->fullScreenManager();
    fullScreenManagerProxy->willEnterFullScreen();

    GtkWidget* topLevelWindow = gtk_widget_get_toplevel(GTK_WIDGET(webkitWebViewBase));
    if (gtk_widget_is_toplevel(topLevelWindow))
        gtk_window_fullscreen(GTK_WINDOW(topLevelWindow));
    fullScreenManagerProxy->didEnterFullScreen();
    priv->fullScreenModeActive = true;
    priv->sleepDisabler = PAL::SleepDisabler::create(String::fromUTF8(_("Website running in fullscreen mode")), PAL::SleepDisabler::Type::Display);
#endif
}

void webkitWebViewBaseExitFullScreen(WebKitWebViewBase* webkitWebViewBase)
{
#if ENABLE(FULLSCREEN_API)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    ASSERT(priv->fullScreenModeActive);

    WebFullScreenManagerProxy* fullScreenManagerProxy = priv->pageProxy->fullScreenManager();
    fullScreenManagerProxy->willExitFullScreen();

    GtkWidget* topLevelWindow = gtk_widget_get_toplevel(GTK_WIDGET(webkitWebViewBase));
    if (gtk_widget_is_toplevel(topLevelWindow))
        gtk_window_unfullscreen(GTK_WINDOW(topLevelWindow));
    fullScreenManagerProxy->didExitFullScreen();
    priv->fullScreenModeActive = false;
    priv->sleepDisabler = nullptr;
#endif
}

bool webkitWebViewBaseIsFullScreen(WebKitWebViewBase* webkitWebViewBase)
{
#if ENABLE(FULLSCREEN_API)
    return webkitWebViewBase->priv->fullScreenModeActive;
#else
    return false;
#endif
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
    g_signal_connect(contextMenuProxy->gtkWidget(), WebContextMenuProxyGtk::widgetDismissedSignal, G_CALLBACK(+[](GtkWidget* widget, WebKitWebViewBase* webViewBase) {
        if (webViewBase->priv->activeContextMenuProxy && webViewBase->priv->activeContextMenuProxy->gtkWidget() == widget)
            webViewBase->priv->activeContextMenuProxy = nullptr;
    }), webkitWebViewBase);
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
    if (!priv->shouldNotifyFocusEvents)
        return;
    if ((focused && priv->activityState & ActivityState::IsFocused) || (!focused && !(priv->activityState & ActivityState::IsFocused)))
        return;

    OptionSet<ActivityState::Flag> flagsToUpdate { ActivityState::IsFocused };
    if (focused) {
        priv->activityState.add(ActivityState::IsFocused);

        // If the view has received the focus and the window is not active
        // mark the current window as active now. This can happen if the
        // toplevel window is a GTK_WINDOW_POPUP and the focus has been
        // set programatically like WebKitTestRunner does, because POPUP
        // can't be focused.
        if (!(priv->activityState & ActivityState::WindowIsActive)) {
            priv->activityState.add(ActivityState::WindowIsActive);
            flagsToUpdate.add(ActivityState::WindowIsActive);
        }
    } else
        priv->activityState.remove(ActivityState::IsFocused);

    webkitWebViewBaseScheduleUpdateActivityState(webViewBase, flagsToUpdate);
}

void webkitWebViewBaseSetEditable(WebKitWebViewBase* webViewBase, bool editable)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->pageProxy->setEditable(editable);
}

IntSize webkitWebViewBaseGetViewSize(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    int width = priv->viewSize.width();
    int height = priv->viewSize.height();

    // First try the widget's own size. If it's already allocated,
    // everything is fine and we'll just use that.
    if (width > 0 || height > 0)
        return IntSize(width, height);

    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(webViewBase));

    // If it's not allocated, then its size will be 0. This can be a problem
    // if the web view is loaded in background and the container doesn't
    // allocate non-visible children: e.g. GtkNotebook in GTK3 does allocate
    // them, but GtkStack, and so GtkNotebook in GTK4 and HdyTabView don't.
    // See https://gitlab.gnome.org/GNOME/epiphany/-/issues/1532
    // In that case we go up through the hierarchy and try to find a parent
    // with non-0 size.
    while (parent) {
#if USE(GTK4)
        width = gtk_widget_get_width(parent);
        height = gtk_widget_get_height(parent);

        if (width > 0 || height > 0)
#else
        width = gtk_widget_get_allocated_width(parent);
        height = gtk_widget_get_allocated_height(parent);

        // The default widget size in GTK3 is 1x1, not 0x0.
        if (width > 1 || height > 1)
#endif
            return IntSize(width, height);

        parent = gtk_widget_get_parent(parent);
    }

    // If there was no such a parent, it's likely the widget widget isn't
    // in a window, or the whole window isn't mapped. No point in trying
    // in this case.

    return IntSize();
}

bool webkitWebViewBaseIsInWindowActive(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->activityState.contains(ActivityState::WindowIsActive);
}

bool webkitWebViewBaseIsFocused(WebKitWebViewBase* webViewBase)
{
#if ENABLE(DEVELOPER_MODE)
    // Xvfb doesn't support toplevel focus, so the view is never focused. We consider it to tbe focused when
    // its window is marked as active.
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11) {
        if (!g_strcmp0(g_getenv("UNDER_XVFB"), "yes") && webViewBase->priv->activityState.contains(ActivityState::WindowIsActive))
            return true;
    }
#endif
    return webViewBase->priv->activityState.contains(ActivityState::IsFocused);
}

bool webkitWebViewBaseIsVisible(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->activityState.contains(ActivityState::IsVisible);
}

bool webkitWebViewBaseIsInWindow(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->activityState.contains(ActivityState::IsInWindow);
}

void webkitWebViewBaseSetInputMethodState(WebKitWebViewBase* webkitWebViewBase, std::optional<InputMethodState>&& state)
{
    webkitWebViewBase->priv->inputMethodFilter.setState(WTFMove(state));
}

void webkitWebViewBaseUpdateTextInputState(WebKitWebViewBase* webkitWebViewBase)
{
    const auto& editorState = webkitWebViewBase->priv->pageProxy->editorState();
    if (editorState.hasPostLayoutAndVisualData()) {
        webkitWebViewBase->priv->inputMethodFilter.notifyCursorRect(editorState.visualData->caretRectAtStart);
        webkitWebViewBase->priv->inputMethodFilter.notifySurrounding(editorState.postLayoutData->surroundingContext, editorState.postLayoutData->surroundingContextCursorPosition,
            editorState.postLayoutData->surroundingContextSelectionPosition);
    }
}

void webkitWebViewBaseSetContentsSize(WebKitWebViewBase* webkitWebViewBase, const IntSize& contentsSize)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (priv->contentsSize == contentsSize)
        return;
    priv->contentsSize = contentsSize;
}

void webkitWebViewBaseResetClickCounter(WebKitWebViewBase* webkitWebViewBase)
{
#if !USE(GTK4)
    webkitWebViewBase->priv->clickCounter.reset();
#endif
}

void webkitWebViewBaseEnterAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase, const LayerTreeContext& layerTreeContext)
{
    ASSERT(webkitWebViewBase->priv->acceleratedBackingStore);
    webkitWebViewBase->priv->acceleratedBackingStore->update(layerTreeContext);
}

void webkitWebViewBaseUpdateAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase, const LayerTreeContext& layerTreeContext)
{
    ASSERT(webkitWebViewBase->priv->acceleratedBackingStore);
    webkitWebViewBase->priv->acceleratedBackingStore->update(layerTreeContext);
}

void webkitWebViewBaseExitAcceleratedCompositingMode(WebKitWebViewBase* webkitWebViewBase)
{
    ASSERT(webkitWebViewBase->priv->acceleratedBackingStore);
    webkitWebViewBase->priv->acceleratedBackingStore->update(LayerTreeContext());
}

bool webkitWebViewBaseMakeGLContextCurrent(WebKitWebViewBase* webkitWebViewBase)
{
    ASSERT(webkitWebViewBase->priv->acceleratedBackingStore);
    return webkitWebViewBase->priv->acceleratedBackingStore->makeContextCurrent();
}

void webkitWebViewBaseWillSwapWebProcess(WebKitWebViewBase* webkitWebViewBase)
{
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (priv->viewGestureController)
        priv->viewGestureController->disconnectFromProcess();
}

void webkitWebViewBaseDidExitWebProcess(WebKitWebViewBase* webkitWebViewBase)
{
    webkitWebViewBase->priv->viewGestureController = nullptr;
}

void webkitWebViewBaseDidRelaunchWebProcess(WebKitWebViewBase* webkitWebViewBase)
{
    // Queue a resize to ensure the new DrawingAreaProxy is resized.
    gtk_widget_queue_resize_no_redraw(GTK_WIDGET(webkitWebViewBase));

    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    if (priv->acceleratedBackingStore) {
        auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(priv->pageProxy->drawingArea());
        priv->acceleratedBackingStore->update(drawingArea->layerTreeContext());
    }
    if (priv->viewGestureController)
        priv->viewGestureController->connectToProcess();
    else {
        priv->viewGestureController = makeUnique<WebKit::ViewGestureController>(*priv->pageProxy);
        priv->viewGestureController->setSwipeGestureEnabled(priv->isBackForwardNavigationGestureEnabled);
    }
}

void webkitWebViewBasePageClosed(WebKitWebViewBase* webkitWebViewBase)
{
    if (webkitWebViewBase->priv->acceleratedBackingStore)
        webkitWebViewBase->priv->acceleratedBackingStore->update({ });
}

RefPtr<WebKit::ViewSnapshot> webkitWebViewBaseTakeViewSnapshot(WebKitWebViewBase* webkitWebViewBase, std::optional<IntRect>&& clipRect)
{
    WebPageProxy* page = webkitWebViewBase->priv->pageProxy.get();

    IntSize size = clipRect ? clipRect->size() : page->viewSize();
    float deviceScale = page->deviceScaleFactor();
    size.scale(deviceScale);

#if !USE(GTK4)
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_RGB24, size.width(), size.height()));
    cairo_surface_set_device_scale(surface.get(), deviceScale, deviceScale);

    RefPtr<cairo_t> cr = adoptRef(cairo_create(surface.get()));
    if (clipRect) {
        cairo_translate(cr.get(), -clipRect->x(), -clipRect->y());
        cairo_rectangle(cr.get(), clipRect->x(), clipRect->y(), clipRect->width(), clipRect->height());
        cairo_clip(cr.get());
    }
    webkitWebViewBaseDraw(GTK_WIDGET(webkitWebViewBase), cr.get());

    return ViewSnapshot::create(WTFMove(surface));
#else
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    auto* renderer = gtk_native_get_renderer(GTK_NATIVE(priv->toplevelOnScreenWindow));

    GRefPtr<GtkSnapshot> snapshot = adoptGRef(gtk_snapshot_new());

    if (clipRect) {
        graphene_point_t point = { -static_cast<float>(clipRect->x()), -static_cast<float>(clipRect->y()) };
        graphene_rect_t rect = {
            { static_cast<float>(clipRect->x()), static_cast<float>(clipRect->y()) },
            { static_cast<float>(clipRect->width()), static_cast<float>(clipRect->height()) }
        };

        gtk_snapshot_translate(snapshot.get(), &point);
        gtk_snapshot_push_clip(snapshot.get(), &rect);
    }

    gtk_snapshot_scale(snapshot.get(), deviceScale, deviceScale);
    webkitWebViewBaseSnapshot(GTK_WIDGET(webkitWebViewBase), snapshot.get());

    if (clipRect)
        gtk_snapshot_pop(snapshot.get());

    GRefPtr<GskRenderNode> renderNode = adoptGRef(gtk_snapshot_to_node(snapshot.get()));

    graphene_rect_t viewport = { 0, 0, static_cast<float>(size.width()), static_cast<float>(size.height()) };
    GdkTexture* texture = gsk_renderer_render_texture(renderer, renderNode.get(), &viewport);

    return ViewSnapshot::create(WTFMove(texture));
#endif
}

void webkitWebViewBaseDidStartProvisionalLoadForMainFrame(WebKitWebViewBase* webkitWebViewBase)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        controller->didStartProvisionalLoadForMainFrame();
}

void webkitWebViewBaseDidFirstVisuallyNonEmptyLayoutForMainFrame(WebKitWebViewBase* webkitWebViewBase)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        controller->didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void webkitWebViewBaseDidFinishNavigation(WebKitWebViewBase* webkitWebViewBase, API::Navigation* navigation)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        controller->didFinishNavigation(navigation);
}

void webkitWebViewBaseDidFailNavigation(WebKitWebViewBase* webkitWebViewBase, API::Navigation* navigation)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        controller->didFailNavigation(navigation);
}

void webkitWebViewBaseDidSameDocumentNavigationForMainFrame(WebKitWebViewBase* webkitWebViewBase, SameDocumentNavigationType type)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        controller->didSameDocumentNavigationForMainFrame(type);
}

void webkitWebViewBaseDidRestoreScrollPosition(WebKitWebViewBase* webkitWebViewBase)
{
    ViewGestureController* controller = webkitWebViewBaseViewGestureController(webkitWebViewBase);
    if (controller && controller->isSwipeGestureEnabled())
        webkitWebViewBase->priv->viewGestureController->didRestoreScrollPosition();
}

#if GTK_CHECK_VERSION(3, 24, 0)
static void emojiChooserEmojiPicked(WebKitWebViewBase* webkitWebViewBase, const char* text)
{
    webkitWebViewBaseCompleteEmojiChooserRequest(webkitWebViewBase, String::fromUTF8(text));
}

static void emojiChooserClosed(WebKitWebViewBase* webkitWebViewBase)
{
    // The emoji chooser first closes the popover and then emits emoji-picked signal, so complete
    // the request if the emoji isn't picked before the next run loop iteration.
    RunLoop::main().dispatch([webViewBase = GRefPtr<WebKitWebViewBase>(webkitWebViewBase)] {
        webkitWebViewBaseCompleteEmojiChooserRequest(webViewBase.get(), emptyString());
    });
    webkitWebViewBase->priv->releaseEmojiChooserTimer.startOneShot(2_min);
}
#endif

void webkitWebViewBaseShowEmojiChooser(WebKitWebViewBase* webkitWebViewBase, const IntRect& caretRect, CompletionHandler<void(String)>&& completionHandler)
{
#if GTK_CHECK_VERSION(3, 24, 0)
    WebKitWebViewBasePrivate* priv = webkitWebViewBase->priv;
    priv->releaseEmojiChooserTimer.stop();

    if (!priv->emojiChooser) {
#if USE(GTK4)
        priv->emojiChooser = gtk_emoji_chooser_new();
        gtk_widget_set_parent(priv->emojiChooser, GTK_WIDGET(webkitWebViewBase));
#else
        priv->emojiChooser = webkitEmojiChooserNew();
        gtk_popover_set_relative_to(GTK_POPOVER(priv->emojiChooser), GTK_WIDGET(webkitWebViewBase));
#endif
        g_signal_connect_swapped(priv->emojiChooser, "emoji-picked", G_CALLBACK(emojiChooserEmojiPicked), webkitWebViewBase);
        g_signal_connect_swapped(priv->emojiChooser, "closed", G_CALLBACK(emojiChooserClosed), webkitWebViewBase);
    }

    priv->emojiChooserCompletionHandler = WTFMove(completionHandler);

    GdkRectangle gdkCaretRect = caretRect;
    gtk_popover_set_pointing_to(GTK_POPOVER(priv->emojiChooser), &gdkCaretRect);
    gtk_popover_popup(GTK_POPOVER(priv->emojiChooser));
#else
    UNUSED_PARAM(webkitWebViewBase);
    UNUSED_PARAM(caretRect);
    completionHandler(emptyString());
#endif
}

#if USE(WPE_RENDERER)
int webkitWebViewBaseRenderHostFileDescriptor(WebKitWebViewBase* webkitWebViewBase)
{
    if (!webkitWebViewBase->priv->acceleratedBackingStore)
        return -1;
    return webkitWebViewBase->priv->acceleratedBackingStore->renderHostFileDescriptor();
}
#endif

void webkitWebViewBaseRequestPointerLock(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    ASSERT(!priv->pointerLockManager);
    if (!priv->lastMotionEvent)
        priv->lastMotionEvent = MotionEvent(GTK_WIDGET(webViewBase), nullptr);
    priv->pointerLockManager = PointerLockManager::create(*priv->pageProxy, priv->lastMotionEvent->position, priv->lastMotionEvent->globalPosition,
        priv->lastMotionEvent->button, priv->lastMotionEvent->buttons, priv->lastMotionEvent->modifiers);
    if (priv->pointerLockManager->lock()) {
        priv->pageProxy->didAllowPointerLock();
        return;
    }

    priv->pointerLockManager = nullptr;
    priv->pageProxy->didDenyPointerLock();
}

void webkitWebViewBaseDidLosePointerLock(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (!priv->pointerLockManager)
        return;

    priv->pointerLockManager->unlock();
    priv->pointerLockManager = nullptr;
}

void webkitWebViewBaseSetInputMethodContext(WebKitWebViewBase* webViewBase, WebKitInputMethodContext* context)
{
    webViewBase->priv->inputMethodFilter.setContext(context);
}

WebKitInputMethodContext* webkitWebViewBaseGetInputMethodContext(WebKitWebViewBase* webViewBase)
{
    return webViewBase->priv->inputMethodFilter.context();
}

void webkitWebViewBaseSynthesizeCompositionKeyPress(WebKitWebViewBase* webViewBase, const String& text, std::optional<Vector<CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
    webViewBase->priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(text, WTFMove(underlines), WTFMove(selectionRange)));
}

static inline OptionSet<WebEventModifier> toWebKitModifiers(unsigned modifiers)
{
    OptionSet<WebEventModifier> webEventModifiers;
    if (modifiers & GDK_CONTROL_MASK)
        webEventModifiers.add(WebEventModifier::ControlKey);
    if (modifiers & GDK_SHIFT_MASK)
        webEventModifiers.add(WebEventModifier::ShiftKey);
    if (modifiers & GDK_MOD1_MASK)
        webEventModifiers.add(WebEventModifier::AltKey);
    if (modifiers & GDK_META_MASK)
        webEventModifiers.add(WebEventModifier::MetaKey);
    if (PlatformKeyboardEvent::modifiersContainCapsLock(modifiers))
        webEventModifiers.add(WebEventModifier::CapsLockKey);
    return webEventModifiers;
}

static inline PointerID primaryPointerForType(const String& pointerType)
{
    if (pointerType == mousePointerEventType())
        return mousePointerID;

    if (pointerType == penPointerEventType())
        return 2;

    return mousePointerID;
}

void webkitWebViewBaseSynthesizeMouseEvent(WebKitWebViewBase* webViewBase, MouseEventType type, unsigned button, unsigned short buttons, int x, int y, unsigned modifiers, int clickCount, const String& pointerType, PlatformMouseEvent::IsTouch isTouchEvent)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

    if (priv->pointerLockManager) {
        priv->pointerLockManager->didReceiveMotionEvent(FloatPoint(x, y));
        return;
    }

    WebMouseEventButton webEventButton = WebMouseEventButton::NoButton;
    switch (button) {
    case 0:
        webEventButton = WebMouseEventButton::NoButton;
        break;
    case 1:
        webEventButton = WebMouseEventButton::LeftButton;
        break;
    case 2:
        webEventButton = WebMouseEventButton::MiddleButton;
        break;
    case 3:
        webEventButton = WebMouseEventButton::RightButton;
        break;
    }

    unsigned short webEventButtons = 0;
    if (buttons & GDK_BUTTON1_MASK)
        webEventButtons |= 1;
    if (buttons & GDK_BUTTON2_MASK)
        webEventButtons |= 4;
    if (buttons & GDK_BUTTON3_MASK)
        webEventButtons |= 2;

    std::optional<FloatSize> movementDelta;
    WebEvent::Type webEventType;
    switch (type) {
    case MouseEventType::Press:
        webEventType = WebEvent::MouseDown;
        priv->inputMethodFilter.cancelComposition();
#if !USE(GTK4)
        if (webEventButton == WebMouseEventButton::RightButton) {
            GUniquePtr<GdkEvent> event(gdk_event_new(GDK_BUTTON_PRESS));
            event->button.window = gtk_widget_get_window(GTK_WIDGET(webViewBase));
            g_object_ref(event->button.window);
            event->button.time = GDK_CURRENT_TIME;
            event->button.x = x;
            event->button.y = y;
            event->button.axes = 0;
            event->button.state = modifiers;
            event->button.button = button;
            event->button.device = gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(GTK_WIDGET(webViewBase))));
            int xRoot, yRoot;
            gdk_window_get_root_coords(event->button.window, x, y, &xRoot, &yRoot);
            event->button.x_root = xRoot;
            event->button.y_root = yRoot;
            priv->contextMenuEvent = WTFMove(event);
        }
#endif
        if (!gtk_widget_has_focus(GTK_WIDGET(webViewBase)) && gtk_widget_is_focus(GTK_WIDGET(webViewBase)))
            gtk_widget_grab_focus(GTK_WIDGET(webViewBase));
        break;
    case MouseEventType::Release:
        webEventType = WebEvent::MouseUp;
        if (!gtk_widget_has_focus(GTK_WIDGET(webViewBase)) && gtk_widget_is_focus(GTK_WIDGET(webViewBase)))
            gtk_widget_grab_focus(GTK_WIDGET(webViewBase));
        break;
    case MouseEventType::Motion:
        webEventType = WebEvent::MouseMove;
        if (buttons & GDK_BUTTON1_MASK)
            webEventButton = WebMouseEventButton::LeftButton;
        else if (buttons & GDK_BUTTON2_MASK)
            webEventButton = WebMouseEventButton::MiddleButton;
        else if (buttons & GDK_BUTTON3_MASK)
            webEventButton = WebMouseEventButton::RightButton;

        if (priv->lastMotionEvent)
            movementDelta = FloatPoint(x, y) - priv->lastMotionEvent->globalPosition;
        priv->lastMotionEvent = MotionEvent(FloatPoint(x, y), widgetRootCoords(GTK_WIDGET(webViewBase), x, y), webEventButton, webEventButtons, toWebKitModifiers(modifiers));
        break;
    }

    priv->pageProxy->handleMouseEvent(NativeWebMouseEvent(webEventType, webEventButton, webEventButtons, { x, y },
        widgetRootCoords(GTK_WIDGET(webViewBase), x, y), clickCount, toWebKitModifiers(modifiers), movementDelta,
        primaryPointerForType(pointerType), pointerType.isNull() ? mousePointerEventType() : pointerType, isTouchEvent));
}

void webkitWebViewBaseSynthesizeKeyEvent(WebKitWebViewBase* webViewBase, KeyEventType type, unsigned keyval, unsigned modifiers, ShouldTranslateKeyboardState shouldTranslate)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

    if (type != KeyEventType::Release) {
        if (auto* popupMenu = priv->pageProxy->activePopupMenu()) {
            auto* gtkPopupMenu = static_cast<WebPopupMenuProxyGtk*>(popupMenu);
            if (gtkPopupMenu->handleKeyPress(keyval, GDK_CURRENT_TIME))
                return;

            if (keyval == GDK_KEY_Return) {
                gtkPopupMenu->activateSelectedItem();
                return;
            }
        }

#if ENABLE(FULLSCREEN_API)
        if (priv->fullScreenModeActive) {
            switch (keyval) {
            case GDK_KEY_Escape:
            case GDK_KEY_f:
            case GDK_KEY_F:
                priv->pageProxy->fullScreenManager()->requestExitFullScreen();
                return;
            default:
                break;
            }
        }
#endif

#if !USE(GTK4)
        if (priv->activeContextMenuProxy && keyval == GDK_KEY_Escape) {
            gtk_menu_shell_deactivate(GTK_MENU_SHELL(priv->activeContextMenuProxy->gtkWidget()));
            return;
        }

        if (keyval == GDK_KEY_Menu) {
            GUniquePtr<GdkEvent> event(gdk_event_new(GDK_KEY_PRESS));
            event->key.window = gtk_widget_get_window(GTK_WIDGET(webViewBase));
            g_object_ref(event->key.window);
            event->key.time = GDK_CURRENT_TIME;
            event->key.keyval = keyval;
            event->key.state = modifiers;
            gdk_event_set_device(event.get(), gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(GTK_WIDGET(webViewBase)))));
            priv->contextMenuEvent = WTFMove(event);
            priv->pageProxy->handleContextMenuKeyEvent();
            return;
        }
#endif
    }

    auto keycode = widgetKeyvalToKeycode(GTK_WIDGET(webViewBase), keyval);
    if (modifiers && shouldTranslate == ShouldTranslateKeyboardState::Yes) {
        auto* display = gtk_widget_get_display(GTK_WIDGET(webViewBase));
#if USE(GTK4)
        gdk_display_translate_key(display, keycode, static_cast<GdkModifierType>(modifiers), 0, &keyval, nullptr, nullptr, nullptr);
#else
        gdk_keymap_translate_keyboard_state(gdk_keymap_get_for_display(display), keycode, static_cast<GdkModifierType>(modifiers), 0, &keyval, nullptr, nullptr, nullptr);
#endif
    }
    auto webEventModifiers = toWebKitModifiers(modifiers);

    if (type != KeyEventType::Release) {
        // Modifier masks are set different in X than other platforms. This code makes WebKitGTK
        // to behave similar to other platforms and other browsers under X (see http://crbug.com/127142#c8).
        switch (keyval) {
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            webEventModifiers.add(WebEventModifier::ControlKey);
            break;
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
            webEventModifiers.add(WebEventModifier::ShiftKey);
            break;
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
            webEventModifiers.add(WebEventModifier::AltKey);
            break;
        case GDK_KEY_Meta_L:
        case GDK_KEY_Meta_R:
            webEventModifiers.add(WebEventModifier::MetaKey);
            break;
        case GDK_KEY_Caps_Lock:
            webEventModifiers.add(WebEventModifier::CapsLockKey);
            break;
        }

        auto filterResult = priv->inputMethodFilter.filterKeyEvent(GDK_KEY_PRESS, keyval, keycode, modifiers);
        if (!filterResult.handled) {
            priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(
                WebEvent::KeyDown,
                filterResult.keyText.isNull() ? PlatformKeyboardEvent::singleCharacterString(keyval) : filterResult.keyText,
                PlatformKeyboardEvent::keyValueForGdkKeyCode(keyval),
                PlatformKeyboardEvent::keyCodeForHardwareKeyCode(keycode),
                PlatformKeyboardEvent::keyIdentifierForGdkKeyCode(keyval),
                PlatformKeyboardEvent::windowsKeyCodeForGdkKeyCode(keyval),
                static_cast<int>(keyval),
                priv->keyBindingTranslator.commandsForKeyval(keyval, modifiers),
                keyval >= GDK_KEY_KP_Space && keyval <= GDK_KEY_KP_9,
                webEventModifiers));
        }
    }

    if (type != KeyEventType::Press) {
        if (!priv->inputMethodFilter.filterKeyEvent(GDK_KEY_RELEASE, keyval, keycode, modifiers).handled) {
            priv->pageProxy->handleKeyboardEvent(NativeWebKeyboardEvent(
                WebEvent::KeyUp,
                PlatformKeyboardEvent::singleCharacterString(keyval),
                PlatformKeyboardEvent::keyValueForGdkKeyCode(keyval),
                PlatformKeyboardEvent::keyCodeForHardwareKeyCode(keycode),
                PlatformKeyboardEvent::keyIdentifierForGdkKeyCode(keyval),
                PlatformKeyboardEvent::windowsKeyCodeForGdkKeyCode(keyval),
                static_cast<int>(keyval),
                { },
                keyval >= GDK_KEY_KP_Space && keyval <= GDK_KEY_KP_9,
                webEventModifiers));
        }
    }
}

static inline WebWheelEvent::Phase toWebKitWheelEventPhase(WheelEventPhase phase)
{
    switch (phase) {
    case WheelEventPhase::NoPhase:
        return WebWheelEvent::Phase::PhaseNone;
    case WheelEventPhase::Began:
        return WebWheelEvent::Phase::PhaseBegan;
    case WheelEventPhase::Changed:
        return WebWheelEvent::Phase::PhaseChanged;
    case WheelEventPhase::Ended:
        return WebWheelEvent::Phase::PhaseEnded;
    case WheelEventPhase::Cancelled:
        return WebWheelEvent::Phase::PhaseCancelled;
    case WheelEventPhase::MayBegin:
        return WebWheelEvent::Phase::PhaseMayBegin;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void webkitWebViewBaseSynthesizeWheelEvent(WebKitWebViewBase* webViewBase, double deltaX, double deltaY, int x, int y, WheelEventPhase phase, WheelEventPhase momentumPhase, bool hasPreciseDeltas)
{
    webkitWebViewBaseSynthesizeWheelEvent(webViewBase, nullptr, deltaX, deltaY, x, y, phase, momentumPhase, hasPreciseDeltas);
}

void webkitWebViewBaseSynthesizeWheelEvent(WebKitWebViewBase* webViewBase, const GdkEvent* event, double deltaX, double deltaY, int x, int y, WheelEventPhase phase, WheelEventPhase momentumPhase, bool hasPreciseDeltas)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->dialog)
        return;

    FloatSize wheelTicks(deltaX, deltaY);
    FloatSize delta(wheelTicks);
    if (!hasPreciseDeltas)
        delta.scale(static_cast<float>(Scrollbar::pixelsPerLineStep()));

    priv->pageProxy->handleWheelEvent(NativeWebWheelEvent(const_cast<GdkEvent*>(event), { x, y }, widgetRootCoords(GTK_WIDGET(webViewBase), x, y),
        delta, wheelTicks, toWebKitWheelEventPhase(phase), toWebKitWheelEventPhase(momentumPhase), true));
}

void webkitWebViewBaseMakeBlank(WebKitWebViewBase* webViewBase, bool makeBlank)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    if (priv->isBlank == makeBlank)
        return;

    priv->isBlank = makeBlank;
    gtk_widget_queue_draw(GTK_WIDGET(webViewBase));
}

void webkitWebViewBasePageGrabbedTouch(WebKitWebViewBase* webViewBase)
{
    WebKitWebViewBasePrivate* priv = webViewBase->priv;
    priv->pageGrabbedTouch = true;
    gtk_gesture_set_state(priv->touchGestureGroup, GTK_EVENT_SEQUENCE_DENIED);
}

void webkitWebViewBaseSetShouldNotifyFocusEvents(WebKitWebViewBase* webViewBase, bool shouldNotifyFocusEvents)
{
    webViewBase->priv->shouldNotifyFocusEvents = shouldNotifyFocusEvents;
}
