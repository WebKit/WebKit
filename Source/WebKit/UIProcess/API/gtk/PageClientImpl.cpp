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
#include "WebDataListSuggestionsDropdownGtk.h"
#include "WebEventFactory.h"
#include "WebKitColorChooser.h"
#include "WebKitPopupMenu.h"
#include "WebKitWebViewBaseInternal.h"
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
#include <WebCore/ValidationBubble.h>
#include <wtf/Compiler.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
#include "WebDateTimePickerGtk.h"
#endif

namespace WebKit {
using namespace WebCore;

PageClientImpl::PageClientImpl(GtkWidget* viewWidget)
    : m_viewWidget(viewWidget)
{
}

// PageClient's pure virtual functions
std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return makeUnique<DrawingAreaProxyCoordinatedGraphics>(*webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget)), process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region& region)
{
#if USE(GTK4)
    gtk_widget_queue_draw(m_viewWidget);
#else
    WebPageProxy* pageProxy = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    ASSERT(pageProxy);

    // During the gesture, the page may be displayed with an offset.
    // To avoid visual glitches, redraw the whole page.
    if (pageProxy->isShowingNavigationGestureSnapshot()) {
        gtk_widget_queue_draw(m_viewWidget);
        return;
    }

    gtk_widget_queue_draw_region(m_viewWidget, toCairoRegion(region).get());
#endif
}

void PageClientImpl::requestScroll(const WebCore::FloatPoint&, const WebCore::IntPoint&, WebCore::ScrollIsAnimated)
{
    notImplemented();
}

void PageClientImpl::requestScrollToRect(const WebCore::FloatRect&, const WebCore::FloatPoint&)
{
    notImplemented();
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    return { };
}

WebCore::IntSize PageClientImpl::viewSize()
{
    return webkitWebViewBaseGetViewSize(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
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

void PageClientImpl::PageClientImpl::processWillSwap()
{
    webkitWebViewBaseWillSwapWebProcess(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}

void PageClientImpl::PageClientImpl::processDidExit()
{
    webkitWebViewBaseDidExitWebProcess(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
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
#if USE(GTK4)
    GdkCursor* currentCursor = gtk_widget_get_cursor(m_viewWidget);
    GdkCursor* newCursor = cursor.platformCursor().get();
    if (currentCursor != newCursor)
        gtk_widget_set_cursor(m_viewWidget, newCursor);
#else
    GdkWindow* window = gtk_widget_get_window(m_viewWidget);
    GdkCursor* currentCursor = gdk_window_get_cursor(window);
    GdkCursor* newCursor = cursor.platformCursor().get();
    if (currentCursor != newCursor)
        gdk_window_set_cursor(window, newCursor);
#endif
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    if (!hiddenUntilMouseMoves)
        return;

    setCursor(WebCore::noneCursor());

    // There's no need to set a timer to restore the cursor by hand. It will
    // be automatically restored when the mouse moves.
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
    if (wasEventHandled || event.type() != WebEvent::Type::KeyDown || !event.nativeEvent())
        return;

    // Always consider arrow keys as handled, otherwise the GtkWindow key bindings will move the focus.
    guint keyval;
    gdk_event_get_keyval(event.nativeEvent(), &keyval);
    switch (keyval) {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
        return;
    default:
        break;
    }

    WebKitWebViewBase* webkitWebViewBase = WEBKIT_WEB_VIEW_BASE(m_viewWidget);
    webkitWebViewBaseForwardNextKeyEvent(webkitWebViewBase);
#if USE(GTK4)
    gdk_display_put_event(gtk_widget_get_display(m_viewWidget), event.nativeEvent());
#else
    gtk_main_do_event(event.nativeEvent());
#endif
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

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy& page)
{
    return WebDateTimePickerGtk::create(page);
}
#endif

#if ENABLE(DATALIST_ELEMENT)
RefPtr<WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebPageProxy& page)
{
    return WebDataListSuggestionsDropdownGtk::create(m_viewWidget, page);
}
#endif

Ref<ValidationBubble> PageClientImpl::createValidationBubble(const String& message, const ValidationBubble::Settings& settings)
{
    return ValidationBubble::create(m_viewWidget, message, settings, [](GtkWidget* webView, bool shouldNotifyFocusEvents) {
        webkitWebViewBaseSetShouldNotifyFocusEvents(WEBKIT_WEB_VIEW_BASE(webView), shouldNotifyFocusEvents);
    });
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

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&& clipRect)
{
    return webkitWebViewBaseTakeViewSnapshot(WEBKIT_WEB_VIEW_BASE(m_viewWidget), WTFMove(clipRect));
}

void PageClientImpl::didChangeContentSize(const IntSize& size)
{
    webkitWebViewBaseSetContentsSize(WEBKIT_WEB_VIEW_BASE(m_viewWidget), size);
}

#if ENABLE(DRAG_SUPPORT)
void PageClientImpl::startDrag(SelectionData&& selection, OptionSet<DragOperation> dragOperationMask, RefPtr<ShareableBitmap>&& dragImage, IntPoint&& dragImageHotspot)
{
    webkitWebViewBaseStartDrag(WEBKIT_WEB_VIEW_BASE(m_viewWidget), WTFMove(selection), dragOperationMask, WTFMove(dragImage), WTFMove(dragImageHotspot));
}

void PageClientImpl::didPerformDragControllerAction()
{
    webkitWebViewBaseDidPerformDragControllerAction(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}
#endif

void PageClientImpl::handleDownloadRequest(DownloadProxy& download)
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewHandleDownloadRequest(WEBKIT_WEB_VIEW(m_viewWidget), &download);
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
    if (wasEventHandled)
        webkitWebViewBasePageGrabbedTouch(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
}
#endif // ENABLE(TOUCH_EVENTS)

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    if (!event.nativeEvent())
        return;

    ViewGestureController* controller = webkitWebViewBaseViewGestureController(WEBKIT_WEB_VIEW_BASE(m_viewWidget));
    if (controller && controller->isSwipeGestureEnabled()) {
        FloatSize delta(-event.wheelTicks());

        int32_t eventTime = static_cast<int32_t>(gdk_event_get_time(event.nativeEvent()));

        GdkDevice* device = gdk_event_get_source_device(event.nativeEvent());
        GdkInputSource source = gdk_device_get_source(device);

        bool isEnd = event.phase() == WebWheelEvent::Phase::PhaseEnded;

        PlatformGtkScrollData scrollData = { .delta = delta, .eventTime = eventTime, .source = source, .isEnd = isEnd };
        controller->wheelEventWasNotHandledByWebCore(&scrollData);
        return;
    }

    // Wheel events can have either scroll events or touch events attached to them.
    // We only want to propagate scroll events; touch events are controlled via their
    // event sequences and if we're scrolling with touch events, that sequence is
    // already claimed and there's no point in propagating it.

    if (gdk_event_get_event_type(event.nativeEvent()) != GDK_SCROLL)
        return;

    webkitWebViewBaseForwardNextWheelEvent(WEBKIT_WEB_VIEW_BASE(m_viewWidget));

#if USE(GTK4)
    gdk_display_put_event(gtk_widget_get_display(m_viewWidget), event.nativeEvent());
#else
    gtk_main_do_event(event.nativeEvent());
#endif
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String&, const IPC::DataReference&)
{
}

void PageClientImpl::navigationGestureDidBegin()
{
    webkitWebViewBaseSynthesizeWheelEvent(WEBKIT_WEB_VIEW_BASE(m_viewWidget), 0, 0, 0, 0, WheelEventPhase::Began, WheelEventPhase::NoPhase, true);
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

void PageClientImpl::didFinishNavigation(API::Navigation* navigation)
{
    webkitWebViewBaseDidFinishNavigation(WEBKIT_WEB_VIEW_BASE(m_viewWidget), navigation);
}

void PageClientImpl::didFailNavigation(API::Navigation* navigation)
{
    webkitWebViewBaseDidFailNavigation(WEBKIT_WEB_VIEW_BASE(m_viewWidget), navigation);
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

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, const IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completionHandler)
{
    completionHandler(WebCore::DOMPasteAccessResponse::DeniedForGesture);
}

UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    GtkTextDirection direction = gtk_widget_get_direction(m_viewWidget);
    if (direction == GTK_TEXT_DIR_RTL)
        return UserInterfaceLayoutDirection::RTL;

    return UserInterfaceLayoutDirection::LTR;
}

bool PageClientImpl::effectiveAppearanceIsDark() const
{
    auto* settings = gtk_widget_get_settings(m_viewWidget);
    gboolean preferDarkTheme;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &preferDarkTheme, nullptr);
    if (preferDarkTheme)
        return true;

    if (auto* themeNameEnv = g_getenv("GTK_THEME"))
        return g_str_has_suffix(themeNameEnv, "-dark") || g_str_has_suffix(themeNameEnv, "-Dark") || g_str_has_suffix(themeNameEnv, ":dark");

    GUniqueOutPtr<char> themeName;
    g_object_get(settings, "gtk-theme-name", &themeName.outPtr(), nullptr);
    if (g_str_has_suffix(themeName.get(), "-dark") || (g_str_has_suffix(themeName.get(), "-Dark")))
        return true;

    return false;
}

#if USE(WPE_RENDERER)
UnixFileDescriptor PageClientImpl::hostFileDescriptor()
{
    return { webkitWebViewBaseRenderHostFileDescriptor(WEBKIT_WEB_VIEW_BASE(m_viewWidget)), UnixFileDescriptor::Adopt };
}
#endif

void PageClientImpl::didChangeWebPageID() const
{
    if (WEBKIT_IS_WEB_VIEW(m_viewWidget))
        webkitWebViewDidChangePageID(WEBKIT_WEB_VIEW(m_viewWidget));
}

void PageClientImpl::makeViewBlank(bool makeBlank)
{
    webkitWebViewBaseMakeBlank(WEBKIT_WEB_VIEW_BASE(m_viewWidget), makeBlank);
}

WebCore::Color PageClientImpl::accentColor()
{
    auto* context = gtk_widget_get_style_context(m_viewWidget);
    GdkRGBA accentColor;

    // libadwaita
    if (gtk_style_context_lookup_color(context, "accent_bg_color", &accentColor))
        return WebCore::Color(accentColor);

    // elementary OS 6.x
    if (gtk_style_context_lookup_color(context, "accent_color", &accentColor))
        return WebCore::Color(accentColor);

    // elementary OS 5.x
    if (gtk_style_context_lookup_color(context, "accentColor", &accentColor))
        return WebCore::Color(accentColor);

    // Legacy
    if (gtk_style_context_lookup_color(context, "theme_selected_bg_color", &accentColor))
        return WebCore::Color(accentColor);

    return SRGBA<uint8_t> { 52, 132, 228 };
}

WebKitWebResourceLoadManager* PageClientImpl::webResourceLoadManager()
{
    return WEBKIT_IS_WEB_VIEW(m_viewWidget) ? webkitWebViewGetWebResourceLoadManager(WEBKIT_WEB_VIEW(m_viewWidget)) : nullptr;
}

} // namespace WebKit
