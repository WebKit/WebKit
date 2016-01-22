/*
 * Copyright (C) 2010, 2011, 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PageClientImpl.h"

#if PLATFORM(MAC)

#import "APIHitTestResult.h"
#import "ColorSpaceData.h"
#import "DataReference.h"
#import "DownloadProxy.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "NavigationState.h"
#import "StringUtilities.h"
#import "ViewGestureController.h"
#import "ViewSnapshotStore.h"
#import "WKAPICast.h"
#import "WKFullScreenWindowController.h"
#import "WKStringCF.h"
#import "WKViewInternal.h"
#import "WKWebViewInternal.h"
#import "WebColorPickerMac.h"
#import "WebContextMenuProxyMac.h"
#import "WebEditCommandProxy.h"
#import "WebPopupMenuProxyMac.h"
#import "WebViewImpl.h"
#import "WindowServerConnection.h"
#import "_WKDownloadInternal.h"
#import "_WKHitTestResultInternal.h"
#import "_WKThumbnailView.h"
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/Cursor.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/Image.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIndicatorWindow.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebKitSystemInterface.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if USE(DICTATION_ALTERNATIVES)
#import <AppKit/NSTextAlternatives.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include <WebCore/WebMediaSessionManager.h>
#endif

@interface NSApplication (WebNSApplicationDetails)
- (NSCursor *)_cursorRectCursor;
@end

#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
@interface NSWindow (WebNSWindowDetails)
- (BOOL)_hostsLayersInWindowServer;
@end
#endif

using namespace WebCore;
using namespace WebKit;

namespace WebKit {

PageClientImpl::PageClientImpl(NSView* view, WKWebView *webView)
    : m_view(view)
    , m_webView(webView)
#if USE(DICTATION_ALTERNATIVES)
    , m_alternativeTextUIController(std::make_unique<AlternativeTextUIController>())
#endif
{
#if !WK_API_ENABLED
    ASSERT_UNUSED(m_webView, !m_webView);
#endif
}

PageClientImpl::~PageClientImpl()
{
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy()
{
    return m_impl->createDrawingAreaProxy();
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::IntRect& rect)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::displayView()
{
    ASSERT_NOT_REACHED();
}

bool PageClientImpl::canScrollView()
{
    return false;
}

void PageClientImpl::scrollView(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, bool isProgrammaticScroll)
{
    ASSERT_NOT_REACHED();
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([m_view bounds].size);
}

NSView *PageClientImpl::activeView() const
{
#if WK_API_ENABLED
    return (m_impl && m_impl->thumbnailView()) ? (NSView *)m_impl->thumbnailView() : m_view;
#else
    return m_view;
#endif
}

NSWindow *PageClientImpl::activeWindow() const
{
#if WK_API_ENABLED
    if (m_impl && m_impl->thumbnailView())
        return m_impl->thumbnailView().window;
#endif
    if (m_impl && m_impl->targetWindowForMovePreparation())
        return m_impl->targetWindowForMovePreparation();
    return m_view.window;
}

bool PageClientImpl::isViewWindowActive()
{
    NSWindow *activeViewWindow = activeWindow();
    return activeViewWindow.isKeyWindow || [NSApp keyWindow] == activeViewWindow;
}

bool PageClientImpl::isViewFocused()
{
    // FIXME: This is called from the WebPageProxy constructor before we have a WebViewImpl.
    // Once WebViewImpl and PageClient merge, this won't be a problem.
    if (!m_impl)
        return NO;

    return m_impl->isFocused();
}

void PageClientImpl::makeFirstResponder()
{
     [[m_view window] makeFirstResponder:m_view];
}
    
bool PageClientImpl::isViewVisible()
{
    NSView *activeView = this->activeView();
    NSWindow *activeViewWindow = activeWindow();

    if (!activeViewWindow)
        return false;

    if (!activeViewWindow.isVisible)
        return false;

    if (activeView.isHiddenOrHasHiddenAncestor)
        return false;

    if (m_impl->windowOcclusionDetectionEnabled() && (activeViewWindow.occlusionState & NSWindowOcclusionStateVisible) != NSWindowOcclusionStateVisible)
        return false;

    return true;
}

bool PageClientImpl::isViewVisibleOrOccluded()
{
    return activeWindow().isVisible;
}

bool PageClientImpl::isViewInWindow()
{
    return activeWindow();
}

bool PageClientImpl::isVisuallyIdle()
{
    return WindowServerConnection::singleton().applicationWindowModificationsHaveStopped() || !isViewVisible();
}

LayerHostingMode PageClientImpl::viewLayerHostingMode()
{
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    if ([activeWindow() _hostsLayersInWindowServer])
        return LayerHostingMode::OutOfProcess;
#endif
    return LayerHostingMode::InProcess;
}

void PageClientImpl::viewWillMoveToAnotherWindow()
{
    clearAllEditCommands();
}

ColorSpaceData PageClientImpl::colorSpace()
{
    return m_impl->colorSpace();
}

void PageClientImpl::processDidExit()
{
    m_impl->processDidExit();
}

void PageClientImpl::pageClosed()
{
    m_impl->pageClosed();
#if USE(DICTATION_ALTERNATIVES)
    m_alternativeTextUIController->clear();
#endif
}

void PageClientImpl::didRelaunchProcess()
{
    m_impl->didRelaunchProcess();
}

void PageClientImpl::preferencesDidChange()
{
    m_impl->preferencesDidChange();
}

void PageClientImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    m_impl->toolTipChanged(oldToolTip, newToolTip);
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
    m_impl->updateSupportsArbitraryLayoutModes();
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
}

void PageClientImpl::handleDownloadRequest(DownloadProxy* download)
{
    ASSERT_ARG(download, download);
#if WK_API_ENABLED
    ASSERT([download->wrapper() isKindOfClass:[_WKDownload class]]);
    [static_cast<_WKDownload *>(download->wrapper()) setOriginatingWebView:m_webView];
#endif
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize& newSize)
{
    m_impl->didChangeContentSize(newSize);
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    // FIXME: Would be nice to share this code with WebKit1's WebChromeClient.

    if ([NSApp _cursorRectCursor])
        return;

    if (!m_view)
        return;

    NSWindow *window = [m_view window];
    if (!window)
        return;

    if ([window windowNumber] != [NSWindow windowNumberAtPoint:[NSEvent mouseLocation] belowWindowWithWindowNumber:0])
        return;

    NSCursor *platformCursor = cursor.platformCursor();
    if ([NSCursor currentCursor] == platformCursor)
        return;

    [platformCursor set];
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    [NSCursor setHiddenUntilMouseMoves:hiddenUntilMouseMoves];
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
}

void PageClientImpl::registerEditCommand(PassRefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    m_impl->registerEditCommand(prpCommand, undoOrRedo);
}

#if USE(INSERTION_UNDO_GROUPING)
void PageClientImpl::registerInsertionUndoGrouping()
{
    registerInsertionUndoGroupingWithUndoManager([m_view undoManager]);
}
#endif

void PageClientImpl::clearAllEditCommands()
{
    m_impl->clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_view undoManager] canUndo] : [[m_view undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_view undoManager] undo] : [[m_view undoManager] redo];
}

void PageClientImpl::setDragImage(const IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag)
{
    RetainPtr<CGImageRef> dragCGImage = dragImage->makeCGImage();
    RetainPtr<NSImage> dragNSImage = adoptNS([[NSImage alloc] initWithCGImage:dragCGImage.get() size:dragImage->size()]);
    IntSize size([dragNSImage size]);
    size.scale(1.0 / m_impl->page().deviceScaleFactor());
    [dragNSImage setSize:size];

    m_impl->dragImageForView(m_view, dragNSImage.get(), clientPosition, isLinkDrag);
}

void PageClientImpl::setPromisedDataForImage(const String& pasteboardName, PassRefPtr<SharedBuffer> imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL, PassRefPtr<SharedBuffer> archiveBuffer)
{
    RefPtr<Image> image = BitmapImage::create();
    image->setData(imageBuffer.get(), true);
    m_impl->setPromisedDataForImage(image.get(), filename, extension, title, url, visibleURL, archiveBuffer.get(), pasteboardName);
}

#if ENABLE(ATTACHMENT_ELEMENT)
void PageClientImpl::setPromisedDataForAttachment(const String& pasteboardName, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL)
{
    m_impl->setPromisedDataForAttachment(filename, extension, title, url, visibleURL, pasteboardName);
}
#endif

void PageClientImpl::updateSecureInputState()
{
    m_impl->updateSecureInputState();
}

void PageClientImpl::resetSecureInputState()
{
    m_impl->resetSecureInputState();
}

void PageClientImpl::notifyInputContextAboutDiscardedComposition()
{
    m_impl->notifyInputContextAboutDiscardedComposition();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    return toDeviceSpace(rect, [m_view window]);
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    return toUserSpace(rect, [m_view window]);
}
   
IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSPoint windowCoord = [[m_view window] convertScreenToBase:point];
#pragma clang diagnostic pop
    return IntPoint([m_view convertPoint:windowCoord fromView:nil]);
}
    
IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    NSRect tempRect = rect;
    tempRect = [m_view convertRect:tempRect toView:nil];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    tempRect.origin = [[m_view window] convertBaseToScreen:tempRect.origin];
#pragma clang diagnostic pop
    return enclosingIntRect(tempRect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool eventWasHandled)
{
    m_impl->doneWithKeyEvent(event.nativeEvent(), eventWasHandled);
}

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyMac::create(m_view, page);
}

#if ENABLE(CONTEXT_MENUS)
std::unique_ptr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, const ContextMenuContextData& context, const UserData& userData)
{
    return std::make_unique<WebContextMenuProxyMac>(m_view, page, context, userData);
}
#endif

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy* page, const WebCore::Color& initialColor,  const WebCore::IntRect& rect)
{
    return WebColorPickerMac::create(page, initialColor, rect, m_view);
}
#endif

void PageClientImpl::setTextIndicator(Ref<TextIndicator> textIndicator, WebCore::TextIndicatorWindowLifetime lifetime)
{
    m_impl->setTextIndicator(textIndicator.get(), lifetime);
}

void PageClientImpl::clearTextIndicator(WebCore::TextIndicatorWindowDismissalAnimation dismissalAnimation)
{
    m_impl->clearTextIndicatorWithAnimation(dismissalAnimation);
}

void PageClientImpl::setTextIndicatorAnimationProgress(float progress)
{
    m_impl->setTextIndicatorAnimationProgress(progress);
}

void PageClientImpl::accessibilityWebProcessTokenReceived(const IPC::DataReference& data)
{
    m_impl->setAccessibilityWebProcessToken([NSData dataWithBytes:data.data() length:data.size()]);
}
    
void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = WKMakeRenderLayer(layerTreeContext.contextID);
    m_impl->setAcceleratedCompositingRootLayer(renderLayer);
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    m_impl->setAcceleratedCompositingRootLayer(nil);
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = WKMakeRenderLayer(layerTreeContext.contextID);
    m_impl->setAcceleratedCompositingRootLayer(renderLayer);
}

void PageClientImpl::willEnterAcceleratedCompositingMode()
{
}

void PageClientImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    m_impl->setAcceleratedCompositingRootLayer(rootLayer);
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    return m_impl->acceleratedCompositingRootLayer();
}

PassRefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot()
{
    return m_impl->takeViewSnapshot();
}

void PageClientImpl::selectionDidChange()
{
    m_impl->selectionDidChange();
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->wheelEventWasNotHandledByWebCore(event.nativeEvent());
}

#if ENABLE(MAC_GESTURE_EVENTS)
void PageClientImpl::gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent& event)
{
    m_impl->gestureEventWasNotHandledByWebCore(event.nativeEvent());
}
#endif

void PageClientImpl::pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus)
{
    m_impl->pluginFocusOrWindowFocusChanged(pluginHasFocusAndWindowHasFocus, pluginComplexTextInputIdentifier);
}

void PageClientImpl::setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, PluginComplexTextInputState pluginComplexTextInputState)
{
    m_impl->setPluginComplexTextInputStateAndIdentifier(pluginComplexTextInputState, pluginComplexTextInputIdentifier);
}

void PageClientImpl::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    m_impl->prepareForDictionaryLookup();

    DictionaryLookup::showPopup(dictionaryPopupInfo, m_view, [this](TextIndicator& textIndicator) {
        m_impl->setTextIndicator(textIndicator, TextIndicatorWindowLifetime::Permanent);
    });
}

void PageClientImpl::dismissContentRelativeChildWindows(bool withAnimation)
{
    m_impl->dismissContentRelativeChildWindowsWithAnimation(withAnimation);
}

void PageClientImpl::showCorrectionPanel(AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
#if USE(AUTOCORRECTION_PANEL)
    if (!isViewVisible() || !isViewInWindow())
        return;
    m_correctionPanel.show(m_view, *m_impl, type, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
#endif
}

void PageClientImpl::dismissCorrectionPanel(ReasonForDismissingAlternativeText reason)
{
#if USE(AUTOCORRECTION_PANEL)
    m_correctionPanel.dismiss(reason);
#endif
}

String PageClientImpl::dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText reason)
{
#if USE(AUTOCORRECTION_PANEL)
    return m_correctionPanel.dismiss(reason);
#else
    return String();
#endif
}

void PageClientImpl::recordAutocorrectionResponse(AutocorrectionResponseType responseType, const String& replacedString, const String& replacementString)
{
    NSCorrectionResponse response = responseType == AutocorrectionReverted ? NSCorrectionResponseReverted : NSCorrectionResponseEdited;
    CorrectionPanel::recordAutocorrectionResponse(m_view, m_impl->spellCheckerDocumentTag(), response, replacedString, replacementString);
}

void PageClientImpl::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    // Now re-create a tracking area with the appropriate options given the new scrollbar style
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    if (newStyle == ScrollbarStyle::AlwaysVisible)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;

    RetainPtr<NSTrackingArea> trackingArea = adoptNS([[NSTrackingArea alloc] initWithRect:[m_view frame] options:options owner:m_view userInfo:nil]);
    m_impl->setPrimaryTrackingArea(trackingArea.get());
}

void PageClientImpl::intrinsicContentSizeDidChange(const IntSize& intrinsicContentSize)
{
    m_impl->setIntrinsicContentSize(intrinsicContentSize);
}

bool PageClientImpl::executeSavedCommandBySelector(const String& selectorString)
{
    return m_impl->executeSavedCommandBySelector(NSSelectorFromString(selectorString));
}

#if USE(DICTATION_ALTERNATIVES)
uint64_t PageClientImpl::addDictationAlternatives(const RetainPtr<NSTextAlternatives>& alternatives)
{
    return m_alternativeTextUIController->addAlternatives(alternatives);
}

void PageClientImpl::removeDictationAlternatives(uint64_t dictationContext)
{
    m_alternativeTextUIController->removeAlternatives(dictationContext);
}

void PageClientImpl::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext)
{
    if (!isViewVisible() || !isViewInWindow())
        return;
    m_alternativeTextUIController->showAlternatives(m_view, boundingBoxOfDictatedText, dictationContext, ^(NSString *acceptedAlternative) {
        m_impl->handleAcceptedAlternativeText(acceptedAlternative);
    });
}

Vector<String> PageClientImpl::dictationAlternatives(uint64_t dictationContext)
{
    return m_alternativeTextUIController->alternativesForContext(dictationContext);
}
#endif

#if ENABLE(FULLSCREEN_API)

WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    return *this;
}

// WebFullScreenManagerProxyClient

void PageClientImpl::closeFullScreenManager()
{
    m_impl->closeFullScreenWindowController();
}

bool PageClientImpl::isFullScreen()
{
    if (!m_impl->hasFullScreenWindowController())
        return false;

    return m_impl->fullScreenWindowController().isFullScreen;
}

void PageClientImpl::enterFullScreen()
{
    [m_impl->fullScreenWindowController() enterFullScreen:nil];
}

void PageClientImpl::exitFullScreen()
{
    [m_impl->fullScreenWindowController() exitFullScreen];
}

void PageClientImpl::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [m_impl->fullScreenWindowController() beganEnterFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
    m_impl->updateSupportsArbitraryLayoutModes();
}

void PageClientImpl::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [m_impl->fullScreenWindowController() beganExitFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
    m_impl->updateSupportsArbitraryLayoutModes();
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::navigationGestureDidBegin()
{
    dismissContentRelativeChildWindows();

#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidBegin();
#endif
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).navigationGestureWillEnd(willNavigate, item);
#else
    UNUSED_PARAM(willNavigate);
    UNUSED_PARAM(item);
#endif
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).navigationGestureDidEnd(willNavigate, item);
#else
    UNUSED_PARAM(willNavigate);
    UNUSED_PARAM(item);
#endif
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).willRecordNavigationSnapshot(item);
#else
    UNUSED_PARAM(item);
#endif
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).navigationGestureSnapshotWasRemoved();
#endif
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void PageClientImpl::didFinishLoadForMainFrame()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFinishLoadForMainFrame();
}

void PageClientImpl::didFailLoadForMainFrame()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFailLoadForMainFrame();
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didSameDocumentNavigationForMainFrame(type);
}

void PageClientImpl::removeNavigationGestureSnapshot()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->removeSwipeSnapshot();
}

void PageClientImpl::didChangeBackgroundColor()
{
    notImplemented();
}

CGRect PageClientImpl::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    CALayer *windowContentLayer = static_cast<NSView *>(m_view.window.contentView).layer;
    ASSERT(windowContentLayer);

    return [windowContentLayer convertRect:layer.bounds fromLayer:layer];
}

void PageClientImpl::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, API::Object* userData)
{
    m_impl->didPerformImmediateActionHitTest(result, contentPreventsDefault, userData);
}

void* PageClientImpl::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return m_impl->immediateActionAnimationControllerForHitTestResult(hitTestResult.get(), type, userData.get());
}

void PageClientImpl::showPlatformContextMenu(NSMenu *menu, IntPoint location)
{
    [menu popUpMenuPositioningItem:nil atLocation:location inView:m_view];
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
WebCore::WebMediaSessionManager& PageClientImpl::mediaSessionManager()
{
    return WebMediaSessionManager::shared();
}
#endif

void PageClientImpl::refView()
{
    CFRetain(m_view);
}

void PageClientImpl::derefView()
{
    CFRelease(m_view);
}

void PageClientImpl::startWindowDrag()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    m_impl->startWindowDrag();
#endif
}

NSWindow *PageClientImpl::platformWindow()
{
    return m_impl->window();
}

#if WK_API_ENABLED
NSView *PageClientImpl::inspectorAttachmentView()
{
    return m_impl->inspectorAttachmentView();
}

_WKRemoteObjectRegistry *PageClientImpl::remoteObjectRegistry()
{
    return m_impl->remoteObjectRegistry();
}
#endif

void PageClientImpl::didRestoreScrollPosition()
{
    m_impl->didRestoreScrollPosition();
}

bool PageClientImpl::windowIsFrontWindowUnderMouse(const NativeWebMouseEvent& event)
{
    return m_impl->windowIsFrontWindowUnderMouse(event.nativeEvent());
}

} // namespace WebKit

#endif // PLATFORM(MAC)
