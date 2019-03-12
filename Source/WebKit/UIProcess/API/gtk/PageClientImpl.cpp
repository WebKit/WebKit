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
#include "PageClientImpl.h"

#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "NativeWebKeyboardEvent.h"
#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "ViewSnapshotStore.h"
#include "WebColorPickerGtk.h"
#include "WebContextMenuProxyGtk.h"
#include "WebEventFactory.h"
#include "WebKitColorChooser.h"
#include "WebKitPopupMenu.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/CairoUtilities.h>
#include <WebCore/Cursor.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/EventNames.h>
#include <WebCore/GtkUtilities.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/RefPtrCairo.h>
#include <wtf/Compiler.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(GtkWidget* viewWidget)
    : m_viewWidget(viewWidget)
{
}

// PageClient's pure virtual functions
std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return std::make_unique<DrawingAreaProxyCoordinatedGraphics>(*webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget)), process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
    WebPageProxy* pageProxy = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    ASSERT(pageProxy);

    // During the gesture, the page may be displayed with an offset.
    // To avoid visual glitches, redraw the whole page.
    if (pageProxy->isShowingNavigationGestureSnapshot()) {
        gtk_widget_queue_draw(m_viewWidget);
        return;
    }

    gtk_widget_queue_draw_region(m_viewWidget, toCairoRegion(region).get());
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, bool)
{
    notImplemented();
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget))->drawingArea());
    return drawingArea ? drawingArea->size() : IntSize();
}

bool PageClientImpl::isViewWindowActive()
{
    return webkitWebViewBaseIsInWindowActive(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

bool PageClientImpl::isViewFocused()
{
    return webkitWebViewBaseIsFocused(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

bool PageClientImpl::isViewVisible()
{
    return webkitWebViewBaseIsVisible(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

bool PageClientImpl::isViewInWindow()
{
    return webkitWebViewBaseIsInWindow(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::PageClientImpl::processDidExit()
{
    notImplemented();
}

void PageClientImpl::didRelaunchProcess()
{
    webkitWebViewBaseDidRelaunchWebProcess(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::toolTipChanged(const String&, const String& newToolTip)
{
    webkitWebViewBaseSetTooltipText(WEBKIT_WEB_VIEW_BASE(m_viewWidget), newToolTip.utf8().data());
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    if (!gtk_widget_get_realized(m_viewWidget))
        return;

    // [GTK] Widget::setCursor() gets called frequently
    // http://bugs.webkit.org/show_bug.cgi?id=16388
    // Setting the cursor may be an expensive operation in some backends,
    // so don't re-set the cursor if it's already set to the target value.
    GdkWindow* window = gtk_widget_get_window(m_viewWidget);
    GdkCursor* currentCursor = gdk_window_get_cursor(window);
    GdkCursor* newCursor = cursor.platformCursor().get();
    if (currentCursor != newCursor)
        gdk_window_set_cursor(window, newCursor);
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool /* hiddenUntilMouseMoves */)
{
    notImplemented();
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
    notImplemented();
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    m_undoController.registerEditCommand(WTFMove(command), undoOrRedo);
}

void PageClientImpl::clearAllEditCommands()
{
    m_undoController.clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return m_undoController.canUndoRedo(undoOrRedo);
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    m_undoController.executeUndoRedo(undoOrRedo);
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& viewRect)
{
    notImplemented();
    return viewRect;
}

IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    IntPoint widgetPositionOnScreen = convertWidgetPointToScreenPoint(m_viewWidget, IntPoint());
    IntPoint result(point);
    result.move(-widgetPositionOnScreen.x(), -widgetPositionOnScreen.y());
    return result;
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    return IntRect(convertWidgetPointToScreenPoint(m_viewWidget, rect.location()), rect.size());
}

WebCore::IntPoint PageClientImpl::accessibilityScreenToRootView(const WebCore::IntPoint& point)
{
    return screenToRootView(point);
}

WebCore::IntRect PageClientImpl::rootViewToAccessibilityScreen(const WebCore::IntRect& rect)    
{
    return rootViewToScreen(rect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool wasEventHandled)
{
    if (wasEventHandled)
        return;
    if (event.isFakeEventForComposition())
        return;

    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(m_viewWidget);
    webkitWebViewBaseForwardNextKeyEvent(webkitWebViewBase);
    gtk_main_do_event(event.nativeEvent());
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        return WebKitPopupMenu::create(m_viewWidget, page);
    return WebPopupMenuProxyGtk::create(m_viewWidget, page);
}

Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyGtk::create(m_viewWidget, page, WTFMove(context), userData);
}

RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy* page, const WebCore::Color& color, const WebCore::IntRect& rect, Vector<WebCore::Color>&&)
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        return WebKitColorChooser::create(*page, color, rect);
    return WebColorPickerGtk::create(*page, color, rect);
}

void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    webkitWebViewBaseEnterAcceleratedCompositingMode(WEBKIT_WEB_VIEW_BASE(m_viewWidget), layerTreeContext);
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    webkitWebViewBaseExitAcceleratedCompositingMode(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    webkitWebViewBaseUpdateAcceleratedCompositingMode(WEBKIT_WEB_VIEW_BASE(m_viewWidget), layerTreeContext);
}

void PageClientImpl::pageClosed()
{
    webkitWebViewBasePageClosed(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::preferencesDidChange()
{
    notImplemented();
}

void PageClientImpl::selectionDidChange()
{
    webkitWebViewBaseUpdateTextInputState(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewSelectionDidChange(WEBKIT_WEB_VIEW(m_viewWidget));
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot()
{
    return webkitWebViewBaseTakeViewSnapshot(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
    webkitWebViewBaseSetContentsSize(WEBKIT_WEB_VIEW_BASE(m_viewWidget), size);
}

#if ENABLE(DRAG_SUPPORT)
void PageClientImpl::startDrag(Ref<SelectionData>&& selection, DragOperation dragOperation, RefPtr<ShareableBitmap>&& dragImage)
{
    WebKitWebViewBase* webView = WEBKIT_WEB_VIEW_BASE(m_viewWidget);
    webkitWebViewBaseDragAndDropHandler(webView).startDrag(WTFMove(selection), dragOperation, WTFMove(dragImage));

    // A drag starting should prevent a double-click from happening. This might
    // happen if a drag is followed very quickly by another click (like in the WTR).
    webkitWebViewBaseResetClickCounter(webView);
}
#endif

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewHandleDownloadRequest(WEBKIT_WEB_VIEW(m_viewWidget), download);
}

void PageClientImpl::didCommitLoadForMainFrame(const String& /* mimeType */, bool /* useCustomContentProvider */ )
{
    webkitWebViewBaseResetClickCounter(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

#if ENABLE(FULLSCREEN_API)
WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

void PageClientImpl::closeFullScreenManager()
{
    notImplemented();
}

bool PageClientImpl::isFullScreen()
{
    return webkitWebViewBaseIsFullScreen(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::enterFullScreen()
{
    if (!m_viewWidget)
        return;

    if (isFullScreen())
        return;

    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewEnterFullScreen(WEBKIT_WEB_VIEW(m_viewWidget));
    else
        webkitWebViewBaseEnterFullScreen(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::exitFullScreen()
{
    if (!m_viewWidget)
        return;

    if (!isFullScreen())
        return;

    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewExitFullScreen(WEBKIT_WEB_VIEW(m_viewWidget));
    else
        webkitWebViewBaseExitFullScreen(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::beganEnterFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}

void PageClientImpl::beganExitFullScreen(const IntRect& /* initialFrame */, const IntRect& /* finalFrame */)
{
    notImplemented();
}

#endif // ENABLE(FULLSCREEN_API)

#if ENABLE(TOUCH_EVENTS)
void PageClientImpl::doneWithTouchEvent(const NativeWebTouchEvent& event, bool wasEventHandled)
{
    const GdkEvent* touchEvent = event.nativeEvent();

#if HAVE(GTK_GESTURES)
    GestureController& gestureController = webkitWebViewBaseGestureController(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    if (wasEventHandled) {
        gestureController.reset();
        return;
    }
    wasEventHandled = gestureController.handleEvent(const_cast<GdkEvent*>(event.nativeEvent()));
#endif

    if (wasEventHandled)
        return;

    // Emulate pointer events if unhandled.
    if (!touchEvent->touch.emulating_pointer)
        return;

    GUniquePtr<GdkEvent> pointerEvent;

    if (touchEvent->type == GDK_TOUCH_UPDATE) {
        pointerEvent.reset(gdk_event_new(GDK_MOTION_NOTIFY));
        pointerEvent->motion.time = touchEvent->touch.time;
        pointerEvent->motion.x = touchEvent->touch.x;
        pointerEvent->motion.y = touchEvent->touch.y;
        pointerEvent->motion.x_root = touchEvent->touch.x_root;
        pointerEvent->motion.y_root = touchEvent->touch.y_root;
        pointerEvent->motion.state = touchEvent->touch.state | GDK_BUTTON1_MASK;
    } else {
        switch (touchEvent->type) {
        case GDK_TOUCH_CANCEL:
            FALLTHROUGH;
        case GDK_TOUCH_END:
            pointerEvent.reset(gdk_event_new(GDK_BUTTON_RELEASE));
            pointerEvent->button.state = touchEvent->touch.state | GDK_BUTTON1_MASK;
            break;
        case GDK_TOUCH_BEGIN:
            pointerEvent.reset(gdk_event_new(GDK_BUTTON_PRESS));
            break;
        default:
            ASSERT_NOT_REACHED();
        }

        pointerEvent->button.button = 1;
        pointerEvent->button.time = touchEvent->touch.time;
        pointerEvent->button.x = touchEvent->touch.x;
        pointerEvent->button.y = touchEvent->touch.y;
        pointerEvent->button.x_root = touchEvent->touch.x_root;
        pointerEvent->button.y_root = touchEvent->touch.y_root;
    }

    gdk_event_set_device(pointerEvent.get(), gdk_event_get_device(touchEvent));
    gdk_event_set_source_device(pointerEvent.get(), gdk_event_get_source_device(touchEvent));
    pointerEvent->any.window = GDK_WINDOW(g_object_ref(touchEvent->any.window));
    pointerEvent->any.send_event = TRUE;

    gtk_widget_event(m_viewWidget, pointerEvent.get());
}
#endif // ENABLE(TOUCH_EVENTS)

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    ViewGestureController& controller = webkitWebViewBaseViewGestureController(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    if (controller.isSwipeGestureEnabled()) {
        controller.wheelEventWasNotHandledByWebCore(&event.nativeEvent()->scroll);
        return;
    }

    webkitWebViewBaseForwardNextWheelEvent(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    gtk_main_do_event(event.nativeEvent());
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, const IPC::DataReference&)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
}

void PageClientImpl::navigationGestureWillEnd(bool, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd(bool, WebBackForwardListItem&)
{
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem&)
{
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    gtk_widget_queue_draw(m_viewWidget);
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewWillStartLoad(WEBKIT_WEB_VIEW(m_viewWidget));

    webkitWebViewBaseDidStartProvisionalLoadForMainFrame(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    webkitWebViewBaseDidFirstVisuallyNonEmptyLayoutForMainFrame(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didFinishLoadForMainFrame()
{
    webkitWebViewBaseDidFinishLoadForMainFrame(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didFailLoadForMainFrame()
{
    webkitWebViewBaseDidFailLoadForMainFrame(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    webkitWebViewBaseDidSameDocumentNavigationForMainFrame(WEBKIT_WEB_VIEW_BASE(m_viewWidget), type);
}

void PageClientImpl::didRestoreScrollPosition()
{
    webkitWebViewBaseDidRestoreScrollPosition(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::didChangeBackgroundColor()
{
}

void PageClientImpl::refView()
{
    g_object_ref(m_viewWidget);
}

void PageClientImpl::derefView()
{
    g_object_unref(m_viewWidget);
}

#if ENABLE(VIDEO) && USE(GSTREAMER)
bool PageClientImpl::decidePolicyForInstallMissingMediaPluginsPermissionRequest(InstallMissingMediaPluginsPermissionRequest& request)
{
    if (!WEBKIT_IS_WEB_VIEW(m_viewWidget))
        return false;

    webkitWebViewRequestInstallMissingMediaPlugins(WEBKIT_WEB_VIEW(m_viewWidget), request);
    return true;
}
#endif

void PageClientImpl::requestDOMPasteAccess(const IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

} // namespace WebKit
