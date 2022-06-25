/*
 * Copyright (C) 2010-2018 Apple Inc. All rights reserved.
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
#import "PageClientImplMac.h"

#if PLATFORM(MAC)

#import "APIHitTestResult.h"
#import "AppKitSPI.h"
#import "DataReference.h"
#import "DownloadProxy.h"
#import "DrawingAreaProxy.h"
#import "Logging.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "NavigationState.h"
#import "StringUtilities.h"
#import "UndoOrRedo.h"
#import "ViewGestureController.h"
#import "ViewSnapshotStore.h"
#import "WKAPICast.h"
#import "WKFullScreenWindowController.h"
#import "WKStringCF.h"
#import "WKViewInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebColorPickerMac.h"
#import "WebContextMenuProxyMac.h"
#import "WebDataListSuggestionsDropdownMac.h"
#import "WebDateTimePickerMac.h"
#import "WebEditCommandProxy.h"
#import "WebPageProxy.h"
#import "WebPopupMenuProxyMac.h"
#import "WebViewImpl.h"
#import "WindowServerConnection.h"
#import "_WKDownloadInternal.h"
#import "_WKHitTestResultInternal.h"
#import "_WKThumbnailView.h"
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/ColorMac.h>
#import <WebCore/Cursor.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragItem.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/Image.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/PromisedAttachmentInfo.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIndicatorWindow.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebCore/ValidationBubble.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/WebMediaSessionManager.h>
#endif

static NSString * const kAXLoadCompleteNotification = @"AXLoadComplete";

@interface NSApplication (WebNSApplicationDetails)
- (NSCursor *)_cursorRectCursor;
@end

#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
@interface NSWindow (WebNSWindowLayerHostingDetails)
- (BOOL)_hostsLayersInWindowServer;
@end
#endif

namespace WebKit {

using namespace WebCore;

PageClientImpl::PageClientImpl(NSView *view, WKWebView *webView)
    : PageClientImplCocoa(webView)
    , m_view(view)
{
}

PageClientImpl::~PageClientImpl() = default;

void PageClientImpl::setImpl(WebViewImpl& impl)
{
    m_impl = impl;
}

std::unique_ptr<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& process)
{
    return m_impl->createDrawingAreaProxy(process);
}

void PageClientImpl::setViewNeedsDisplay(const WebCore::Region&)
{
    ASSERT_NOT_REACHED();
}

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, const IntPoint& scrollOrigin, ScrollIsAnimated)
{
}

WebCore::FloatPoint PageClientImpl::viewScrollPosition()
{
    return { };
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([m_view bounds].size);
}

NSView *PageClientImpl::activeView() const
{
    return (m_impl && m_impl->thumbnailView()) ? (NSView *)m_impl->thumbnailView() : m_view;
}

NSWindow *PageClientImpl::activeWindow() const
{
    if (m_impl && m_impl->thumbnailView())
        return m_impl->thumbnailView().window;
    if (m_impl && m_impl->targetWindowForMovePreparation())
        return m_impl->targetWindowForMovePreparation();
    return m_view.window;
}

bool PageClientImpl::isViewWindowActive()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    NSWindow *activeViewWindow = activeWindow();
    return activeViewWindow.isKeyWindow || (activeViewWindow && [NSApp keyWindow] == activeViewWindow);
}

bool PageClientImpl::isViewFocused()
{
    // FIXME: This is called from the WebPageProxy constructor before we have a WebViewImpl.
    // Once WebViewImpl and PageClient merge, this won't be a problem.
    if (!m_impl)
        return NO;

    return m_impl->isFocused();
}

void PageClientImpl::assistiveTechnologyMakeFirstResponder()
{
    [[m_view window] makeFirstResponder:m_view];
}
    
void PageClientImpl::makeFirstResponder()
{
    if (m_shouldSuppressFirstResponderChanges)
        return;

    [[m_view window] makeFirstResponder:m_view];
}
    
bool PageClientImpl::isViewVisible()
{
    NSView *activeView = this->activeView();
    NSWindow *activeViewWindow = activeWindow();

    auto windowIsOccluded = [&]()->bool {
        return m_impl && m_impl->windowOcclusionDetectionEnabled() && (activeViewWindow.occlusionState & NSWindowOcclusionStateVisible) != NSWindowOcclusionStateVisible;
    };

    LOG_WITH_STREAM(ActivityState, stream << "PageClientImpl " << this << " isViewVisible(): activeViewWindow " << activeViewWindow
        << " (window visible " << activeViewWindow.isVisible << ", view hidden " << activeView.isHiddenOrHasHiddenAncestor << ", window occluded " << windowIsOccluded() << ")");

    if (!activeViewWindow)
        return false;

    if (!activeViewWindow.isVisible)
        return false;

    if (activeView.isHiddenOrHasHiddenAncestor)
        return false;

    if (windowIsOccluded())
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

WebCore::DestinationColorSpace PageClientImpl::colorSpace()
{
    return m_impl->colorSpace();
}

void PageClientImpl::processWillSwap()
{
    m_impl->processWillSwap();
}

void PageClientImpl::processDidExit()
{
    m_impl->processDidExit();
    m_impl->setAcceleratedCompositingRootLayer(nil);
}

void PageClientImpl::pageClosed()
{
    m_impl->pageClosed();
    PageClientImplCocoa::pageClosed();
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

void PageClientImpl::didCommitLoadForMainFrame(const String&, bool)
{
    m_impl->updateSupportsArbitraryLayoutModes();
    m_impl->dismissContentRelativeChildWindowsWithAnimation(true);
    m_impl->clearPromisedDragImage();
    m_impl->pageDidScroll({0, 0});
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference& dataReference)
{
}

void PageClientImpl::handleDownloadRequest(DownloadProxy&)
{
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize& newSize)
{
    m_impl->didChangeContentSize(newSize);
}

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    // FIXME: Would be nice to share this code with WebKit1's WebChromeClient.

    // The Web process may have asked to change the cursor when the view was in an active window, but
    // if it is no longer in a window or the window is not active, then the cursor should not change.
    if (!isViewWindowActive())
        return;

    if ([NSApp _cursorRectCursor])
        return;

    if (!m_view)
        return;

    NSWindow *window = [m_view window];
    if (!window)
        return;

    auto mouseLocationInScreen = NSEvent.mouseLocation;
    if (window.windowNumber != [NSWindow windowNumberAtPoint:mouseLocationInScreen belowWindowWithWindowNumber:0])
        return;

    NSCursor *platformCursor = cursor.platformCursor();
    if ([NSCursor currentCursor] == platformCursor)
        return;

    if (m_impl->imageAnalysisOverlayViewHasCursorAtPoint([m_view convertPoint:mouseLocationInScreen fromView:nil]))
        return;

    [platformCursor set];

    if (cursor.type() == WebCore::Cursor::Type::None) {
        if ([NSCursor respondsToSelector:@selector(hideUntilChanged)])
            [NSCursor hideUntilChanged];
    }
}

void PageClientImpl::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    [NSCursor setHiddenUntilMouseMoves:hiddenUntilMouseMoves];
}

void PageClientImpl::didChangeViewportProperties(const WebCore::ViewportAttributes&)
{
}

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    m_impl->registerEditCommand(WTFMove(command), undoOrRedo);
}

void PageClientImpl::registerInsertionUndoGrouping()
{
    registerInsertionUndoGroupingWithUndoManager([m_view undoManager]);
}

#if ENABLE(UI_PROCESS_PDF_HUD)

void PageClientImpl::createPDFHUD(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    m_impl->createPDFHUD(identifier, rect);
}

void PageClientImpl::updatePDFHUDLocation(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    m_impl->updatePDFHUDLocation(identifier, rect);
}

void PageClientImpl::removePDFHUD(PDFPluginIdentifier identifier)
{
    m_impl->removePDFHUD(identifier);
}

void PageClientImpl::removeAllPDFHUDs()
{
    m_impl->removeAllPDFHUDs();
}

#endif // ENABLE(UI_PROCESS_PDF_HUD)

void PageClientImpl::clearAllEditCommands()
{
    m_impl->clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[m_view undoManager] canUndo] : [[m_view undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == UndoOrRedo::Undo) ? [[m_view undoManager] undo] : [[m_view undoManager] redo];
}

void PageClientImpl::startDrag(const WebCore::DragItem& item, const ShareableBitmap::Handle& image)
{
    m_impl->startDrag(item, image);
}

void PageClientImpl::setPromisedDataForImage(const String& pasteboardName, Ref<FragmentedSharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL, RefPtr<FragmentedSharedBuffer>&& archiveBuffer, const String& originIdentifier)
{
    auto image = BitmapImage::create();
    image->setData(WTFMove(imageBuffer), true);
    m_impl->setPromisedDataForImage(image.ptr(), filename, extension, title, url, visibleURL, archiveBuffer.get(), pasteboardName, originIdentifier);
}

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

void PageClientImpl::pinnedStateWillChange()
{
    [m_webView willChangeValueForKey:@"_pinnedState"];
}

void PageClientImpl::pinnedStateDidChange()
{
    [m_webView didChangeValueForKey:@"_pinnedState"];
}
    
IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    NSPoint windowCoord = [m_view.window convertPointFromScreen:point];
    return IntPoint([m_view convertPoint:windowCoord fromView:nil]);
}
    
IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    NSRect tempRect = rect;
    tempRect = [m_view convertRect:tempRect toView:nil];
    tempRect.origin = [m_view.window convertPointToScreen:tempRect.origin];
    return enclosingIntRect(tempRect);
}

IntRect PageClientImpl::rootViewToWindow(const WebCore::IntRect& rect)
{
    NSRect tempRect = rect;
    tempRect = [m_view convertRect:tempRect toView:nil];
    return enclosingIntRect(tempRect);
}

IntPoint PageClientImpl::accessibilityScreenToRootView(const IntPoint& point)
{
    return screenToRootView(point);
}

IntRect PageClientImpl::rootViewToAccessibilityScreen(const IntRect& rect)
{
    return rootViewToScreen(rect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool eventWasHandled)
{
    m_impl->doneWithKeyEvent(event.nativeEvent(), eventWasHandled);
}

#if ENABLE(IMAGE_ANALYSIS)

void PageClientImpl::requestTextRecognition(const URL& imageURL, const ShareableBitmap::Handle& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completion)
{
    m_impl->requestTextRecognition(imageURL, imageData, sourceLanguageIdentifier, targetLanguageIdentifier, WTFMove(completion));
}

void PageClientImpl::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    m_impl->computeHasVisualSearchResults(imageURL, imageBitmap, WTFMove(completion));
}

#endif

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyMac::create(m_view, page);
}

#if ENABLE(CONTEXT_MENUS)

Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyMac::create(m_view, page, WTFMove(context), userData);
}

void PageClientImpl::didShowContextMenu()
{
    [m_webView _didShowContextMenu];
}

void PageClientImpl::didDismissContextMenu()
{
    [m_webView _didDismissContextMenu];
}

#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(INPUT_TYPE_COLOR)
RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy* page, const WebCore::Color& initialColor, const WebCore::IntRect& rect, Vector<WebCore::Color>&& suggestions)
{
    return WebColorPickerMac::create(page, initialColor, rect, WTFMove(suggestions), m_view);
}
#endif

#if ENABLE(DATALIST_ELEMENT)
RefPtr<WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebPageProxy& page)
{
    return WebDataListSuggestionsDropdownMac::create(page, m_view);
}
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy& page)
{
    return WebDateTimePickerMac::create(page, m_view);
}
#endif

Ref<ValidationBubble> PageClientImpl::createValidationBubble(const String& message, const ValidationBubble::Settings& settings)
{
    return ValidationBubble::create(m_view, message, settings);
}

void PageClientImpl::showSafeBrowsingWarning(const SafeBrowsingWarning& warning, CompletionHandler<void(std::variant<WebKit::ContinueUnsafeLoad, URL>&&)>&& completionHandler)
{
    if (!m_impl)
        return completionHandler(ContinueUnsafeLoad::Yes);
    m_impl->showSafeBrowsingWarning(warning, WTFMove(completionHandler));
}

bool PageClientImpl::hasSafeBrowsingWarning() const
{
    if (!m_impl)
        return false;
    return !!m_impl->safeBrowsingWarning();
}

void PageClientImpl::clearSafeBrowsingWarning()
{
    m_impl->clearSafeBrowsingWarning();
}

void PageClientImpl::clearSafeBrowsingWarningIfForMainFrameNavigation()
{
    m_impl->clearSafeBrowsingWarningIfForMainFrameNavigation();
}

void PageClientImpl::setTextIndicator(Ref<TextIndicator> textIndicator, WebCore::TextIndicatorLifetime lifetime)
{
    m_impl->setTextIndicator(textIndicator.get(), lifetime);
}

void PageClientImpl::clearTextIndicator(WebCore::TextIndicatorDismissalAnimation dismissalAnimation)
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

    CALayer *renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID];
    m_impl->enterAcceleratedCompositingWithRootLayer(renderLayer);
}

void PageClientImpl::didFirstLayerFlush(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID];
    m_impl->setAcceleratedCompositingRootLayer(renderLayer);
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    m_impl->setAcceleratedCompositingRootLayer(nil);
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID];
    m_impl->setAcceleratedCompositingRootLayer(renderLayer);
}

void PageClientImpl::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    m_impl->setAcceleratedCompositingRootLayer(rootNode ? rootNode->layer() : nil);
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    return m_impl->acceleratedCompositingRootLayer();
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&&)
{
    return m_impl->takeViewSnapshot();
}

void PageClientImpl::selectionDidChange()
{
    m_impl->selectionDidChange();
}

bool PageClientImpl::showShareSheet(const ShareDataWithParsedURL& shareData, WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    m_impl->showShareSheet(shareData, WTFMove(completionHandler), m_webView.get().get());
    return true;
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

void PageClientImpl::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    m_impl->prepareForDictionaryLookup();

    DictionaryLookup::showPopup(dictionaryPopupInfo, m_view, [this](TextIndicator& textIndicator) {
        m_impl->setTextIndicator(textIndicator, WebCore::TextIndicatorLifetime::Permanent);
    }, nullptr, [this]() {
        m_impl->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::None);
    });
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

static inline NSCorrectionResponse toCorrectionResponse(AutocorrectionResponse response)
{
    switch (response) {
    case WebCore::AutocorrectionResponse::Reverted:
        return NSCorrectionResponseReverted;
    case WebCore::AutocorrectionResponse::Edited:
        return NSCorrectionResponseEdited;
    case WebCore::AutocorrectionResponse::Accepted:
        return NSCorrectionResponseAccepted;
    }

    ASSERT_NOT_REACHED();
    return NSCorrectionResponseAccepted;
}

void PageClientImpl::recordAutocorrectionResponse(AutocorrectionResponse response, const String& replacedString, const String& replacementString)
{
    CorrectionPanel::recordAutocorrectionResponse(*m_impl, m_impl->spellCheckerDocumentTag(), toCorrectionResponse(response), replacedString, replacementString);
}

void PageClientImpl::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    // Now re-create a tracking area with the appropriate options given the new scrollbar style
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    if (newStyle == ScrollbarStyle::AlwaysVisible)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;

    m_impl->updatePrimaryTrackingAreaOptions(options);
}

void PageClientImpl::intrinsicContentSizeDidChange(const IntSize& intrinsicContentSize)
{
    m_impl->setIntrinsicContentSize(intrinsicContentSize);
}

bool PageClientImpl::executeSavedCommandBySelector(const String& selectorString)
{
    return m_impl->executeSavedCommandBySelector(NSSelectorFromString(selectorString));
}

void PageClientImpl::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext dictationContext)
{
    if (!isViewVisible() || !isViewInWindow())
        return;
    m_alternativeTextUIController->showAlternatives(m_view, boundingBoxOfDictatedText, dictationContext, ^(NSString *acceptedAlternative) {
        m_impl->handleAcceptedAlternativeText(acceptedAlternative);
    });
}

void PageClientImpl::setEditableElementIsFocused(bool editableElementIsFocused)
{
    m_impl->setEditableElementIsFocused(editableElementIsFocused);
}

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
    m_impl->dismissContentRelativeChildWindowsWithAnimation(true);

    if (auto webView = m_webView.get())
        NavigationState::fromWebPage(*webView->_page).navigationGestureDidBegin();
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = m_webView.get())
        NavigationState::fromWebPage(*webView->_page).navigationGestureWillEnd(willNavigate, item);
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = m_webView.get())
        NavigationState::fromWebPage(*webView->_page).navigationGestureDidEnd(willNavigate, item);
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (auto webView = m_webView.get())
        NavigationState::fromWebPage(*webView->_page).willRecordNavigationSnapshot(item);
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    if (auto webView = m_webView.get())
        NavigationState::fromWebPage(*webView->_page).navigationGestureSnapshotWasRemoved();
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didStartProvisionalLoadForMainFrame();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void PageClientImpl::didFinishNavigation(API::Navigation* navigation)
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFinishNavigation(navigation);

    NSAccessibilityPostNotification(NSAccessibilityUnignoredAncestor(m_view), kAXLoadCompleteNotification);
}

void PageClientImpl::didFailNavigation(API::Navigation* navigation)
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didFailNavigation(navigation);

    NSAccessibilityPostNotification(NSAccessibilityUnignoredAncestor(m_view), kAXLoadCompleteNotification);
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    if (auto gestureController = m_impl->gestureController())
        gestureController->didSameDocumentNavigationForMainFrame(type);
}

void PageClientImpl::handleControlledElementIDResponse(const String& identifier)
{
    [m_webView _handleControlledElementIDResponse:nsStringFromWebCoreString(identifier)];
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

NSObject *PageClientImpl::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return m_impl->immediateActionAnimationControllerForHitTestResult(hitTestResult.get(), type, userData.get());
}

void PageClientImpl::didHandleAcceptedCandidate()
{
    m_impl->didHandleAcceptedCandidate();
}

void PageClientImpl::videoControlsManagerDidChange()
{
    m_impl->videoControlsManagerDidChange();
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
    CFRetain((__bridge CFTypeRef)m_view);
}

void PageClientImpl::derefView()
{
    CFRelease((__bridge CFTypeRef)m_view);
}

void PageClientImpl::startWindowDrag()
{
    m_impl->startWindowDrag();
}

NSWindow *PageClientImpl::platformWindow()
{
    return m_impl->window();
}

#if ENABLE(DRAG_SUPPORT)

void PageClientImpl::didPerformDragOperation(bool handled)
{
    m_impl->didPerformDragOperation(handled);
}

#endif

NSView *PageClientImpl::inspectorAttachmentView()
{
    return m_impl->inspectorAttachmentView();
}

_WKRemoteObjectRegistry *PageClientImpl::remoteObjectRegistry()
{
    return m_impl->remoteObjectRegistry();
}

void PageClientImpl::pageDidScroll(const WebCore::IntPoint& scrollPosition)
{
    m_impl->pageDidScroll(scrollPosition);
}

void PageClientImpl::didRestoreScrollPosition()
{
    m_impl->didRestoreScrollPosition();
}

void PageClientImpl::requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin)
{
    // FIXME: Add additional logic to avoid Note Pip.
    m_impl->scrollToRect(targetRect, origin);
}

bool PageClientImpl::windowIsFrontWindowUnderMouse(const NativeWebMouseEvent& event)
{
    return m_impl->windowIsFrontWindowUnderMouse(event.nativeEvent());
}

WebCore::UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    if (!m_view)
        return WebCore::UserInterfaceLayoutDirection::LTR;
    return (m_view.userInterfaceLayoutDirection == NSUserInterfaceLayoutDirectionLeftToRight) ? WebCore::UserInterfaceLayoutDirection::LTR : WebCore::UserInterfaceLayoutDirection::RTL;
}

bool PageClientImpl::effectiveAppearanceIsDark() const
{
    return m_impl->effectiveAppearanceIsDark();
}

bool PageClientImpl::effectiveUserInterfaceLevelIsElevated() const
{
    return m_impl->effectiveUserInterfaceLevelIsElevated();
}

void PageClientImpl::takeFocus(WebCore::FocusDirection direction)
{
    m_impl->takeFocus(direction);
}

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completion)
{
    m_impl->requestDOMPasteAccess(pasteAccessCategory, elementRect, originIdentifier, WTFMove(completion));
}


void PageClientImpl::makeViewBlank(bool makeBlank)
{
    m_impl->acceleratedCompositingRootLayer().opacity = makeBlank ? 0 : 1;
}

#if HAVE(APP_ACCENT_COLORS)
WebCore::Color PageClientImpl::accentColor()
{
    return WebCore::colorFromCocoaColor([NSApp _effectiveAccentColor]);
}
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

bool PageClientImpl::canHandleContextMenuTranslation() const
{
    return m_impl->canHandleContextMenuTranslation();
}

void PageClientImpl::handleContextMenuTranslation(const TranslationContextMenuInfo& info)
{
    m_impl->handleContextMenuTranslation(info);
}

#endif // HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

#if ENABLE(DATA_DETECTION)

void PageClientImpl::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    m_impl->handleClickForDataDetectionResult(info, clickLocation);
}

#endif

void PageClientImpl::beginTextRecognitionForVideoInElementFullscreen(const ShareableBitmap::Handle& bitmapHandle, FloatRect bounds)
{
    m_impl->beginTextRecognitionForVideoInElementFullscreen(bitmapHandle, bounds);
}

void PageClientImpl::cancelTextRecognitionForVideoInElementFullscreen()
{
    m_impl->cancelTextRecognitionForVideoInElementFullscreen();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
