/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#import "DrawingAreaProxy.h"
#import "Logging.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "NavigationState.h"
#import "PlatformWritingToolsUtilities.h"
#import "RemoteLayerTreeNode.h"
#import "TextExtractionFilter.h"
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
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebCore/ValidationBubble.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/WebMediaSessionManager.h>
#endif

#if HAVE(DIGITAL_CREDENTIALS_UI)
#import <WebCore/DigitalCredentialsRequestData.h>
#import <WebCore/DigitalCredentialsResponseData.h>
#import <WebCore/ExceptionData.h>
#endif

#import <pal/cocoa/WritingToolsUISoftLink.h>

@interface NSApplication (WebNSApplicationDetails)
- (NSCursor *)_cursorRectCursor;
@end

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

Ref<DrawingAreaProxy> PageClientImpl::createDrawingAreaProxy(WebProcessProxy& webProcessProxy)
{
    return checkedImpl()->createDrawingAreaProxy(webProcessProxy);
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
    return IntSize([m_view.get() bounds].size);
}

NSView *PageClientImpl::activeView() const
{
    CheckedPtr impl = m_impl.get();
    return (impl && impl->thumbnailView()) ? impl->thumbnailView().unsafeGet() : m_view.getAutoreleased();
}

NSWindow *PageClientImpl::activeWindow() const
{
    CheckedPtr impl = m_impl.get();
    if (impl && impl->thumbnailView())
        return [impl->thumbnailView() window];
    if (impl && impl->targetWindowForMovePreparation())
        return impl->targetWindowForMovePreparation();
    return [m_view.get() window];
}

bool PageClientImpl::isViewWindowActive()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    RetainPtr activeViewWindow = activeWindow();
    return activeViewWindow.get().isKeyWindow || (activeViewWindow && [NSApp keyWindow] == activeViewWindow.get());
}

bool PageClientImpl::isViewFocused()
{
    // FIXME: This is called from the WebPageProxy constructor before we have a WebViewImpl.
    // Once WebViewImpl and PageClient merge, this won't be a problem.
    if (CheckedPtr impl = m_impl.get())
        return impl->isFocused();

    return NO;
}

void PageClientImpl::assistiveTechnologyMakeFirstResponder()
{
    [retainPtr([m_view.get() window]) makeFirstResponder:m_view.get().get()];
}
    
void PageClientImpl::makeFirstResponder()
{
    if (m_shouldSuppressFirstResponderChanges)
        return;

    [retainPtr([m_view.get() window]) makeFirstResponder:m_view.get().get()];
}
    
bool PageClientImpl::isViewVisible(NSView *view, NSWindow *viewWindow)
{
    auto windowIsOccluded = [&]()->bool {
        return m_impl && m_impl->windowOcclusionDetectionEnabled() && (viewWindow.occlusionState & NSWindowOcclusionStateVisible) != NSWindowOcclusionStateVisible;
    };

    RELEASE_LOG(ActivityState, "PageClientImpl %p isViewVisible(): viewWindow %p, window visible %d, view hidden %d, window occluded %d", this, viewWindow, viewWindow.isVisible, view.isHiddenOrHasHiddenAncestor, windowIsOccluded());

    if (!viewWindow)
        return false;

    if (!viewWindow.isVisible)
        return false;

    if (view.isHiddenOrHasHiddenAncestor)
        return false;

    if (windowIsOccluded())
        return false;

    return true;
}

bool PageClientImpl::isActiveViewVisible()
{
    RetainPtr activeView = this->activeView();
    RetainPtr activeViewWindow = activeWindow();

    return isViewVisible(activeView.get(), activeViewWindow.get());
}

bool PageClientImpl::isMainViewVisible()
{
    RetainPtr mainView = m_view.get();
    RetainPtr mainViewWindow = [mainView window];

    return isViewVisible(mainView.get(), mainViewWindow.get());
}

bool PageClientImpl::isViewVisibleOrOccluded()
{
    return RetainPtr { activeWindow() }.get().isVisible;
}

bool PageClientImpl::isViewInWindow()
{
    return activeWindow();
}

bool PageClientImpl::isVisuallyIdle()
{
    return WindowServerConnection::singleton().applicationWindowModificationsHaveStopped() || !isActiveViewVisible();
}

void PageClientImpl::viewWillMoveToAnotherWindow()
{
    clearAllEditCommands();
}

WebCore::DestinationColorSpace PageClientImpl::colorSpace()
{
    return checkedImpl()->colorSpace();
}

void PageClientImpl::processWillSwap()
{
    checkedImpl()->processWillSwap();

#if ENABLE(TEXT_EXTRACTION_FILTER)
    [webView() _clearTextExtractionFilterCache];
#endif
}

void PageClientImpl::processDidExit()
{
    CheckedRef impl = *m_impl;
    impl->processDidExit();
    impl->setAcceleratedCompositingRootLayer(nil);
}

void PageClientImpl::pageClosed()
{
    checkedImpl()->pageClosed();
    PageClientImplCocoa::pageClosed();
}

void PageClientImpl::didRelaunchProcess()
{
    checkedImpl()->didRelaunchProcess();
}

void PageClientImpl::preferencesDidChange()
{
    checkedImpl()->preferencesDidChange();
}

void PageClientImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    checkedImpl()->toolTipChanged(oldToolTip, newToolTip);
}

void PageClientImpl::didCommitLoadForMainFrame(const String&, bool)
{
    CheckedRef impl = *m_impl;
    impl->updateSupportsArbitraryLayoutModes();
    impl->dismissContentRelativeChildWindowsWithAnimation(true);
    impl->clearPromisedDragImage();
    impl->pageDidScroll({ 0, 0 });
#if ENABLE(WRITING_TOOLS)
    impl->hideTextAnimationView();
#endif

#if ENABLE(TEXT_EXTRACTION_FILTER)
    [webView() _clearTextExtractionFilterCache];
#endif
}

void PageClientImpl::didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, std::span<const uint8_t> dataReference)
{
}

void PageClientImpl::didChangeContentSize(const WebCore::IntSize& newSize)
{
    checkedImpl()->didChangeContentSize(newSize);
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

    RetainPtr view = m_view.get();
    if (!view)
        return;

    RetainPtr window = [view window];
    if (!window)
        return;

    auto mouseLocationInScreen = NSEvent.mouseLocation;
    if (window.get().windowNumber != [NSWindow windowNumberAtPoint:mouseLocationInScreen belowWindowWithWindowNumber:0])
        return;

    RetainPtr platformCursor = cursor.platformCursor();
    if ([NSCursor currentCursor] == platformCursor.get())
        return;

    if (checkedImpl()->imageAnalysisOverlayViewHasCursorAtPoint([view convertPoint:mouseLocationInScreen fromView:nil]))
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

void PageClientImpl::registerEditCommand(Ref<WebEditCommandProxy>&& command, UndoOrRedo undoOrRedo)
{
    checkedImpl()->registerEditCommand(WTFMove(command), undoOrRedo);
}

void PageClientImpl::registerInsertionUndoGrouping()
{
    registerInsertionUndoGroupingWithUndoManager(retainPtr([m_view.get() undoManager]).get());
}

void PageClientImpl::createPDFHUD(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID, const WebCore::IntRect& rect)
{
    checkedImpl()->createPDFHUD(identifier, frameID, rect);
}

void PageClientImpl::updatePDFHUDLocation(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    checkedImpl()->updatePDFHUDLocation(identifier, rect);
}

void PageClientImpl::removePDFHUD(PDFPluginIdentifier identifier)
{
    checkedImpl()->removePDFHUD(identifier);
}

void PageClientImpl::removeAllPDFHUDs()
{
    checkedImpl()->removeAllPDFHUDs();
}

void PageClientImpl::clearAllEditCommands()
{
    checkedImpl()->clearAllEditCommands();
}

bool PageClientImpl::canUndoRedo(UndoOrRedo undoOrRedo)
{
    RetainPtr undoManager = [m_view.get() undoManager];
    return undoOrRedo == UndoOrRedo::Undo ? [undoManager canUndo] : [undoManager canRedo];
}

void PageClientImpl::executeUndoRedo(UndoOrRedo undoOrRedo)
{
    RetainPtr undoManager = [m_view.get() undoManager];
    return undoOrRedo == UndoOrRedo::Undo ? [undoManager undo] : [undoManager redo];
}

void PageClientImpl::startDrag(const WebCore::DragItem& item, ShareableBitmap::Handle&& image, const std::optional<WebCore::NodeIdentifier>& nodeID)
{
    UNUSED_PARAM(nodeID);
    checkedImpl()->startDrag(item, WTFMove(image));
}

void PageClientImpl::setPromisedDataForImage(const String& pasteboardName, Ref<FragmentedSharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL, RefPtr<FragmentedSharedBuffer>&& archiveBuffer, const String& originIdentifier)
{
    auto image = BitmapImage::create();
    image->setData(WTFMove(imageBuffer), true);
    checkedImpl()->setPromisedDataForImage(image.get(), filename.createNSString().get(), extension.createNSString().get(), title.createNSString().get(), url.createNSString().get(), visibleURL.createNSString().get(), archiveBuffer.get(), pasteboardName.createNSString().get(), originIdentifier.createNSString().get());
}

void PageClientImpl::updateSecureInputState()
{
    checkedImpl()->updateSecureInputState();
}

void PageClientImpl::resetSecureInputState()
{
    checkedImpl()->resetSecureInputState();
}

void PageClientImpl::notifyInputContextAboutDiscardedComposition()
{
    checkedImpl()->notifyInputContextAboutDiscardedComposition();
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    return toDeviceSpace(rect, retainPtr([m_view.get() window]).get());
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    return toUserSpace(rect, retainPtr([m_view.get() window]).get());
}

void PageClientImpl::pinnedStateWillChange()
{
    [webView() willChangeValueForKey:@"_pinnedState"];
}

void PageClientImpl::pinnedStateDidChange()
{
    [webView() didChangeValueForKey:@"_pinnedState"];
}

void PageClientImpl::drawPageBorderForPrinting(WebCore::FloatSize&& size)
{
    [webView() drawPageBorderWithSize:size];
}
    
IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
    RetainPtr view = m_view.get();
    NSPoint windowCoord = [retainPtr([view window]) convertPointFromScreen:point];
    return IntPoint([view convertPoint:windowCoord fromView:nil]);
}

IntPoint PageClientImpl::rootViewToScreen(const IntPoint& point)
{
    RetainPtr view = m_view.get();
    return IntPoint([retainPtr([view window]) convertPointToScreen:[view convertPoint:point toView:nil]]);
}

IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    NSRect tempRect = rect;
    RetainPtr view = m_view.get();
    tempRect = [view convertRect:tempRect toView:nil];
    tempRect.origin = [retainPtr([view window]) convertPointToScreen:tempRect.origin];
    return enclosingIntRect(tempRect);
}

IntRect PageClientImpl::rootViewToWindow(const WebCore::IntRect& rect)
{
    NSRect tempRect = rect;
    tempRect = [m_view.get() convertRect:tempRect toView:nil];
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
    checkedImpl()->doneWithKeyEvent(RetainPtr { event.nativeEvent() }.get(), eventWasHandled);
}

#if ENABLE(IMAGE_ANALYSIS)

void PageClientImpl::requestTextRecognition(const URL& imageURL, ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(TextRecognitionResult&&)>&& completion)
{
    checkedImpl()->requestTextRecognition(imageURL, WTFMove(imageData), sourceLanguageIdentifier, targetLanguageIdentifier, WTFMove(completion));
}

void PageClientImpl::computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&& completion)
{
    checkedImpl()->computeHasVisualSearchResults(imageURL, imageBitmap, WTFMove(completion));
}

#endif

RefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy& page)
{
    return WebPopupMenuProxyMac::create(m_view.get().get(), page.checkedPopupMenuClient().get());
}

#if ENABLE(CONTEXT_MENUS)

Ref<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy& page, FrameInfoData&& frameInfo, ContextMenuContextData&& context, const UserData& userData)
{
    return WebContextMenuProxyMac::create(m_view.get().get(), page, WTFMove(frameInfo), WTFMove(context), userData);
}

void PageClientImpl::didShowContextMenu()
{
    [webView() _didShowContextMenu];
}

void PageClientImpl::didDismissContextMenu()
{
    [webView() _didDismissContextMenu];
}

#endif // ENABLE(CONTEXT_MENUS)

RefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy& page, const WebCore::Color& initialColor, const WebCore::IntRect& rect, ColorControlSupportsAlpha supportsAlpha, Vector<WebCore::Color>&& suggestions)
{
    return WebColorPickerMac::create(&page.checkedColorPickerClient().get(), initialColor, rect, supportsAlpha, WTFMove(suggestions), m_view.get().get());
}

RefPtr<WebDataListSuggestionsDropdown> PageClientImpl::createDataListSuggestionsDropdown(WebPageProxy& page)
{
    return WebDataListSuggestionsDropdownMac::create(page, m_view.get().get());
}

RefPtr<WebDateTimePicker> PageClientImpl::createDateTimePicker(WebPageProxy& page)
{
    return WebDateTimePickerMac::create(page, m_view.get().get());
}

Ref<ValidationBubble> PageClientImpl::createValidationBubble(String&& message, const ValidationBubble::Settings& settings)
{
    return ValidationBubble::create(m_view.get().get(), WTFMove(message), settings);
}

void PageClientImpl::showBrowsingWarning(const BrowsingWarning& warning, CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&& completionHandler)
{
    if (CheckedPtr impl = m_impl.get())
        return impl->showWarningView(warning, WTFMove(completionHandler));
    completionHandler(ContinueUnsafeLoad::Yes);
}

bool PageClientImpl::hasBrowsingWarning() const
{
    if (!m_impl)
        return false;
    return !!m_impl->warningView();
}

void PageClientImpl::clearBrowsingWarning()
{
    checkedImpl()->clearWarningView();
}

void PageClientImpl::clearBrowsingWarningIfForMainFrameNavigation()
{
    checkedImpl()->clearWarningViewIfForMainFrameNavigation();
}

CALayer* PageClientImpl::textIndicatorInstallationLayer()
{
    return checkedImpl()->textIndicatorInstallationLayer();
}

void PageClientImpl::accessibilityWebProcessTokenReceived(std::span<const uint8_t> data, pid_t pid)
{
    checkedImpl()->setAccessibilityWebProcessToken(toNSData(data).get(), pid);
}
    
void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    RetainPtr renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID shouldPreserveFlip:NO];
    checkedImpl()->enterAcceleratedCompositingWithRootLayer(renderLayer.get());
}

void PageClientImpl::didFirstLayerFlush(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    RetainPtr renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID shouldPreserveFlip:NO];
    checkedImpl()->setAcceleratedCompositingRootLayer(renderLayer.get());
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    checkedImpl()->setAcceleratedCompositingRootLayer(nil);
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    RetainPtr renderLayer = [CALayer _web_renderLayerWithContextID:layerTreeContext.contextID shouldPreserveFlip:NO];
    checkedImpl()->setAcceleratedCompositingRootLayer(renderLayer.get());
}

void PageClientImpl::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    checkedImpl()->setAcceleratedCompositingRootLayer(rootNode ? rootNode->protectedLayer().get() : nil);
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    return m_impl->acceleratedCompositingRootLayer();
}

CALayer *PageClientImpl::headerBannerLayer() const
{
    return m_impl->headerBannerLayer();
}

CALayer *PageClientImpl::footerBannerLayer() const
{
    return m_impl->footerBannerLayer();
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&&)
{
    return checkedImpl()->takeViewSnapshot();
}

RefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot(std::optional<WebCore::IntRect>&&, ForceSoftwareCapturingViewportSnapshot forceSoftwareCapturing)
{
    return checkedImpl()->takeViewSnapshot(forceSoftwareCapturing);
}

void PageClientImpl::selectionDidChange()
{
    checkedImpl()->selectionDidChange();
}

bool PageClientImpl::showShareSheet(ShareDataWithParsedURL&& shareData, WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    checkedImpl()->showShareSheet(WTFMove(shareData), WTFMove(completionHandler), webView().get());
    return true;
}

#if HAVE(DIGITAL_CREDENTIALS_UI)
void PageClientImpl::showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData& requestData, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    m_impl->showDigitalCredentialsPicker(requestData, WTFMove(completionHandler), webView().get());
}

void PageClientImpl::dismissDigitalCredentialsPicker(WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    m_impl->dismissDigitalCredentialsPicker(WTFMove(completionHandler), webView().get());
}
#endif

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->wheelEventWasNotHandledByWebCore(RetainPtr { event.nativeEvent() }.get());
}

#if ENABLE(MAC_GESTURE_EVENTS)
void PageClientImpl::gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent& event)
{
    m_impl->gestureEventWasNotHandledByWebCore(event.nativeEvent());
}
#endif

void PageClientImpl::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    checkedImpl()->prepareForDictionaryLookup();
}

void PageClientImpl::showCorrectionPanel(AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
#if USE(AUTOCORRECTION_PANEL)
    if (!isActiveViewVisible() || !isViewInWindow())
        return;
    m_correctionPanel.show(m_view.get().get(), *checkedImpl(), type, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
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
    CheckedRef impl = *m_impl;
    CorrectionPanel::recordAutocorrectionResponse(impl.get(), impl->spellCheckerDocumentTag(), toCorrectionResponse(response), replacedString, replacementString);
}

void PageClientImpl::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    // Now re-create a tracking area with the appropriate options given the new scrollbar style
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    if (newStyle == ScrollbarStyle::AlwaysVisible)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;

    checkedImpl()->updatePrimaryTrackingAreaOptions(options);
}

void PageClientImpl::intrinsicContentSizeDidChange(const IntSize& intrinsicContentSize)
{
    checkedImpl()->setIntrinsicContentSize(intrinsicContentSize);
}

bool PageClientImpl::executeSavedCommandBySelector(const String& selectorString)
{
    return checkedImpl()->executeSavedCommandBySelector(NSSelectorFromString(selectorString.createNSString().get()));
}

void PageClientImpl::showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext dictationContext)
{
    if (!isActiveViewVisible() || !isViewInWindow())
        return;
    m_alternativeTextUIController->showAlternatives(m_view.get().get(), boundingBoxOfDictatedText, dictationContext, ^(NSString *acceptedAlternative) {
        checkedImpl()->handleAcceptedAlternativeText(acceptedAlternative);
    });
}

void PageClientImpl::setEditableElementIsFocused(bool editableElementIsFocused)
{
    checkedImpl()->setEditableElementIsFocused(editableElementIsFocused);
}

void PageClientImpl::scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID)
{
    checkedImpl()->suppressContentRelativeChildViews(WebViewImpl::ContentRelativeChildViewsSuppressionType::TemporarilyRemove);
}

void PageClientImpl::willBeginViewGesture()
{
    checkedImpl()->suppressContentRelativeChildViews(WebViewImpl::ContentRelativeChildViewsSuppressionType::Remove);
}

void PageClientImpl::didEndViewGesture()
{
    checkedImpl()->suppressContentRelativeChildViews(WebViewImpl::ContentRelativeChildViewsSuppressionType::Restore);
}

#if ENABLE(FULLSCREEN_API)

WebFullScreenManagerProxyClient& PageClientImpl::fullScreenManagerProxyClient()
{
    if (m_fullscreenClientForTesting)
        return *m_fullscreenClientForTesting;
    return *this;
}

// WebFullScreenManagerProxyClient

void PageClientImpl::closeFullScreenManager()
{
    checkedImpl()->closeFullScreenWindowController();
}

bool PageClientImpl::isFullScreen()
{
    CheckedRef impl = *m_impl;
    if (!impl->hasFullScreenWindowController())
        return false;

    return impl->protectedFullScreenWindowController().get().isFullScreen;
}

void PageClientImpl::enterFullScreen(FloatSize, CompletionHandler<void(bool)>&& completionHandler)
{
    CheckedRef impl = *m_impl;
    if (RetainPtr fullScreenWindowController = impl->fullScreenWindowController())
        [fullScreenWindowController enterFullScreen:WTFMove(completionHandler)];
    else
        return completionHandler(false);
}

void PageClientImpl::exitFullScreen(CompletionHandler<void()>&& completionHandler)
{
    CheckedRef impl = *m_impl;
    if (RetainPtr fullScreenWindowController = impl->fullScreenWindowController())
        [fullScreenWindowController exitFullScreen:WTFMove(completionHandler)];
    else
        return completionHandler();
}

void PageClientImpl::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame, CompletionHandler<void(bool)>&& completionHandler)
{
    CheckedRef impl = *m_impl;
    if (RetainPtr fullScreenWindowController = impl->fullScreenWindowController())
        [fullScreenWindowController beganEnterFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame completionHandler:WTFMove(completionHandler)];
    else
        completionHandler(false);

    impl->updateSupportsArbitraryLayoutModes();
}

void PageClientImpl::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame, CompletionHandler<void()>&& completionHandler)
{
    CheckedRef impl = *m_impl;
    if (RetainPtr fullScreenWindowController = impl->fullScreenWindowController()) {
        [fullScreenWindowController beganExitFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame completionHandler:WTFMove(completionHandler)];
        impl->updateSupportsArbitraryLayoutModes();
    } else
        return completionHandler();
}

#endif // ENABLE(FULLSCREEN_API)

void PageClientImpl::navigationGestureDidBegin()
{
    checkedImpl()->dismissContentRelativeChildWindowsWithAnimation(true);

    if (auto webView = this->webView()) {
        if (RefPtr navigationState = NavigationState::fromWebPage(Ref { *webView->_page }))
            navigationState->navigationGestureDidBegin();
    }
}

void PageClientImpl::navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (RefPtr navigationState = NavigationState::fromWebPage(Ref { *webView->_page }))
            navigationState->navigationGestureWillEnd(willNavigate, item);
    }
}

void PageClientImpl::navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (RefPtr navigationState = NavigationState::fromWebPage(Ref { *webView->_page }))
            navigationState->navigationGestureDidEnd(willNavigate, item);
    }
}

void PageClientImpl::navigationGestureDidEnd()
{
}

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
    if (auto webView = this->webView()) {
        if (RefPtr navigationState = NavigationState::fromWebPage(Ref { *webView->_page }))
            navigationState->willRecordNavigationSnapshot(item);
    }
}

void PageClientImpl::didRemoveNavigationGestureSnapshot()
{
    if (auto webView = this->webView()) {
        if (RefPtr navigationState = NavigationState::fromWebPage(Ref { *webView->_page }))
            navigationState->navigationGestureSnapshotWasRemoved();
    }
}

void PageClientImpl::didStartProvisionalLoadForMainFrame()
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->didStartProvisionalLoadForMainFrame();
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->didFirstVisuallyNonEmptyLayoutForMainFrame();
}

void PageClientImpl::didFinishNavigation(API::Navigation* navigation)
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->didFinishNavigation(navigation);

    NSAccessibilityPostNotification(RetainPtr { NSAccessibilityUnignoredAncestor(m_view.get().get()) }.get(), @"AXLoadComplete");
}

void PageClientImpl::didFailNavigation(API::Navigation* navigation)
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->didFailNavigation(navigation);

    NSAccessibilityPostNotification(RetainPtr { NSAccessibilityUnignoredAncestor(m_view.get().get()) }.get(), @"AXLoadComplete");
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    if (RefPtr gestureController = m_impl->gestureController())
        gestureController->didSameDocumentNavigationForMainFrame(type);
}

void PageClientImpl::handleControlledElementIDResponse(const String& identifier)
{
    [webView() _handleControlledElementIDResponse:identifier.createNSString().get()];
}

void PageClientImpl::didChangeBackgroundColor()
{
    notImplemented();
}

CGRect PageClientImpl::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    RetainPtr<CALayer> windowContentLayer = static_cast<NSView *>([m_view.get() window].contentView).layer;
    ASSERT(windowContentLayer);

    return [windowContentLayer convertRect:layer.bounds fromLayer:layer];
}

void PageClientImpl::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, API::Object* userData)
{
    checkedImpl()->didPerformImmediateActionHitTest(result, contentPreventsDefault, userData);
}

NSObject *PageClientImpl::immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult> hitTestResult, uint64_t type, RefPtr<API::Object> userData)
{
    return checkedImpl()->immediateActionAnimationControllerForHitTestResult(hitTestResult.get(), type, userData.get());
}

void PageClientImpl::videoControlsManagerDidChange()
{
    PageClientImplCocoa::videoControlsManagerDidChange();
    checkedImpl()->videoControlsManagerDidChange();
}

void PageClientImpl::showPlatformContextMenu(NSMenu *menu, IntPoint location)
{
    [menu popUpMenuPositioningItem:nil atLocation:location inView:m_view.get().get()];
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
WebCore::WebMediaSessionManager& PageClientImpl::mediaSessionManager()
{
    return WebMediaSessionManager::singleton();
}
#endif

void PageClientImpl::refView()
{
    if (RetainPtr view = m_view.get())
        CFRetain((__bridge CFTypeRef)view.get());
}

void PageClientImpl::derefView()
{
    if (RetainPtr view = m_view.get())
        CFRelease((__bridge CFTypeRef)view.get());
}

void PageClientImpl::startWindowDrag()
{
    checkedImpl()->startWindowDrag();
}

#if ENABLE(DRAG_SUPPORT)

void PageClientImpl::didPerformDragOperation(bool handled)
{
    checkedImpl()->didPerformDragOperation(handled);
}

#endif

RetainPtr<NSView> PageClientImpl::inspectorAttachmentView()
{
    return checkedImpl()->inspectorAttachmentView();
}

_WKRemoteObjectRegistry *PageClientImpl::remoteObjectRegistry()
{
    return checkedImpl()->remoteObjectRegistry();
}

void PageClientImpl::pageDidScroll(const WebCore::IntPoint& scrollPosition)
{
    checkedImpl()->pageDidScroll(scrollPosition);
}

void PageClientImpl::didRestoreScrollPosition()
{
    checkedImpl()->didRestoreScrollPosition();
}

void PageClientImpl::requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin)
{
    // FIXME: Add additional logic to avoid Note Pip.
    checkedImpl()->scrollToRect(targetRect, origin);
}

bool PageClientImpl::windowIsFrontWindowUnderMouse(const NativeWebMouseEvent& event)
{
    return checkedImpl()->windowIsFrontWindowUnderMouse(RetainPtr { event.nativeEvent() }.get());
}

std::optional<float> PageClientImpl::computeAutomaticTopObscuredInset()
{
    RetainPtr view = m_view.get();
    RetainPtr window = [view window];
    if (([window styleMask] & NSWindowStyleMaskFullSizeContentView) && ![window titlebarAppearsTransparent] && ![view enclosingScrollView]) {
        [window updateConstraintsIfNeeded];
        NSRect contentLayoutRectInWebViewCoordinates = [view convertRect:[window contentLayoutRect] fromView:nil];
        return std::max<float>(contentLayoutRectInWebViewCoordinates.origin.y, 0);
    }

    return std::nullopt;
}

WebCore::UserInterfaceLayoutDirection PageClientImpl::userInterfaceLayoutDirection()
{
    RetainPtr view = m_view.get();
    if (!view)
        return WebCore::UserInterfaceLayoutDirection::LTR;
    return ([view userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionLeftToRight) ? WebCore::UserInterfaceLayoutDirection::LTR : WebCore::UserInterfaceLayoutDirection::RTL;
}

bool PageClientImpl::effectiveAppearanceIsDark() const
{
    return checkedImpl()->effectiveAppearanceIsDark();
}

bool PageClientImpl::effectiveUserInterfaceLevelIsElevated() const
{
    return checkedImpl()->effectiveUserInterfaceLevelIsElevated();
}

bool PageClientImpl::useFormSemanticContext() const
{
    return checkedImpl()->useFormSemanticContext();
}

void PageClientImpl::takeFocus(WebCore::FocusDirection direction)
{
    checkedImpl()->takeFocus(direction);
}

void PageClientImpl::performSwitchHapticFeedback()
{
    [[NSHapticFeedbackManager defaultPerformer] performFeedbackPattern:NSHapticFeedbackPatternLevelChange performanceTime:NSHapticFeedbackPerformanceTimeDefault];
}

void PageClientImpl::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, WebCore::DOMPasteRequiresInteraction requiresInteraction, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&& completion)
{
    checkedImpl()->requestDOMPasteAccess(pasteAccessCategory, requiresInteraction, elementRect, originIdentifier, WTFMove(completion));
}

void PageClientImpl::makeViewBlank(bool makeBlank)
{
    RetainPtr { m_impl->acceleratedCompositingRootLayer() }.get().opacity = makeBlank ? 0 : 1;
}

#if HAVE(APP_ACCENT_COLORS)
WebCore::Color PageClientImpl::accentColor()
{
    return WebCore::colorFromCocoaColor([NSApp _effectiveAccentColor]);
}

bool PageClientImpl::appUsesCustomAccentColor()
{
    static BOOL usesCustomAppAccentColor = [] {
        RetainPtr bundleForAccentColor = [NSBundle mainBundle];
        RetainPtr info = [bundleForAccentColor infoDictionary];
        RetainPtr<NSString> accentColorName = info.get()[@"NSAccentColorName"];
        BOOL usesCustomAppAccentColor = NO;
        if ([accentColorName length])
            usesCustomAppAccentColor = !![NSColor colorNamed:accentColorName.get() bundle:bundleForAccentColor.get()];

        if (!usesCustomAppAccentColor && [(accentColorName = info.get()[@"NSAppAccentColorName"]) length])
            usesCustomAppAccentColor = !![NSColor colorNamed:accentColorName.get() bundle:bundleForAccentColor.get()];
        return usesCustomAppAccentColor;
    }();

    return usesCustomAppAccentColor;
}
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

bool PageClientImpl::canHandleContextMenuTranslation() const
{
    return checkedImpl()->canHandleContextMenuTranslation();
}

void PageClientImpl::handleContextMenuTranslation(const TranslationContextMenuInfo& info)
{
    checkedImpl()->handleContextMenuTranslation(info);
}

#endif // HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)

#if ENABLE(WRITING_TOOLS) && ENABLE(CONTEXT_MENUS)

bool PageClientImpl::canHandleContextMenuWritingTools() const
{
    return checkedImpl()->canHandleContextMenuWritingTools();
}

void PageClientImpl::handleContextMenuWritingTools(WebCore::WritingTools::RequestedTool tool, WebCore::IntRect selectionRect)
{
    RetainPtr webView = this->webView();
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[PAL::getWTWritingToolsClassSingleton() sharedInstance] showTool:WebKit::convertToPlatformRequestedTool(tool) forSelectionRect:selectionRect ofView:m_view.get().get() forDelegate:webView.get()];
ALLOW_DEPRECATED_DECLARATIONS_END
}

#endif

#if ENABLE(DATA_DETECTION)

void PageClientImpl::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    checkedImpl()->handleClickForDataDetectionResult(info, clickLocation);
}

#endif

void PageClientImpl::beginTextRecognitionForVideoInElementFullscreen(ShareableBitmap::Handle&& bitmapHandle, FloatRect bounds)
{
    checkedImpl()->beginTextRecognitionForVideoInElementFullscreen(WTFMove(bitmapHandle), bounds);
}

void PageClientImpl::cancelTextRecognitionForVideoInElementFullscreen()
{
    checkedImpl()->cancelTextRecognitionForVideoInElementFullscreen();
}

void PageClientImpl::didChangeLocalInspectorAttachment()
{
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    m_impl->updateScrollPocket();
#endif
}

void PageClientImpl::showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier identifier, const WebCore::ResolvedCaptionDisplaySettingsOptions& options, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&& completionHandler)
{
    checkedImpl()->showCaptionDisplaySettings(identifier, options, WTFMove(completionHandler));
}

RetainPtr<NSView> PageClient::protectedViewForPresentingRevealPopover() const
{
    return viewForPresentingRevealPopover();
}

} // namespace WebKit

#endif // PLATFORM(MAC)
