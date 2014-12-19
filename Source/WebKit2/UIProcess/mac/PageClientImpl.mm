/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#import "AttributedString.h"
#import "ColorSpaceData.h"
#import "DataReference.h"
#import "DictionaryPopupInfo.h"
#import "DownloadProxy.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebWheelEvent.h"
#import "NavigationState.h"
#import "StringUtilities.h"
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
#import "WindowServerConnection.h"
#import "_WKDownloadInternal.h"
#import "_WKThumbnailView.h"
#import <WebCore/AlternativeTextUIController.h>
#import <WebCore/BitmapImage.h>
#import <WebCore/Cursor.h>
#import <WebCore/FloatRect.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/Image.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LookupSPI.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextUndoInsertionMarkupMac.h>
#import <WebKitSystemInterface.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if USE(DICTATION_ALTERNATIVES)
#import <AppKit/NSTextAlternatives.h>
#endif

@interface NSApplication (WebNSApplicationDetails)
- (NSCursor *)_cursorRectCursor;
@end

#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
@interface NSWindow (WebNSWindowDetails)
- (BOOL)_hostsLayersInWindowServer;
@end
#endif

SOFT_LINK_CONSTANT_MAY_FAIL(Lookup, LUTermOptionDisableSearchTermIndicator, NSString *)

using namespace WebCore;
using namespace WebKit;

@interface WKEditCommandObjC : NSObject
{
    RefPtr<WebEditCommandProxy> m_command;
}
- (id)initWithWebEditCommandProxy:(PassRefPtr<WebEditCommandProxy>)command;
- (WebEditCommandProxy*)command;
@end

@interface WKEditorUndoTargetObjC : NSObject
- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;
@end

@implementation WKEditCommandObjC

- (id)initWithWebEditCommandProxy:(PassRefPtr<WebEditCommandProxy>)command
{
    self = [super init];
    if (!self)
        return nil;

    m_command = command;
    return self;
}

- (WebEditCommandProxy*)command
{
    return m_command.get();
}

@end

@implementation WKEditorUndoTargetObjC

- (void)undoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->unapply();
}

- (void)redoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->reapply();
}

@end

namespace WebKit {

PageClientImpl::PageClientImpl(WKView* wkView, WKWebView *webView)
    : m_wkView(wkView)
    , m_webView(webView)
    , m_undoTarget(adoptNS([[WKEditorUndoTargetObjC alloc] init]))
#if USE(DICTATION_ALTERNATIVES)
    , m_alternativeTextUIController(adoptPtr(new AlternativeTextUIController))
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
    return [m_wkView _createDrawingAreaProxy];
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

void PageClientImpl::requestScroll(const FloatPoint& scrollPosition, bool isProgrammaticScroll)
{
    ASSERT_NOT_REACHED();
}

IntSize PageClientImpl::viewSize()
{
    return IntSize([m_wkView bounds].size);
}

NSView *PageClientImpl::activeView() const
{
#if WK_API_ENABLED
    return m_wkView._thumbnailView ? (NSView *)m_wkView._thumbnailView : (NSView *)m_wkView;
#else
    return m_wkView;
#endif
}

NSWindow *PageClientImpl::activeWindow() const
{
#if WK_API_ENABLED
    if (m_wkView._thumbnailView)
        return m_wkView._thumbnailView.window;
#endif
    if (m_wkView._targetWindowForMovePreparation)
        return m_wkView._targetWindowForMovePreparation;
    return m_wkView.window;
}

bool PageClientImpl::isViewWindowActive()
{
    NSWindow *activeViewWindow = activeWindow();
    return activeViewWindow.isKeyWindow || [NSApp keyWindow] == activeViewWindow;
}

bool PageClientImpl::isViewFocused()
{
    return [m_wkView _isFocused];
}

void PageClientImpl::makeFirstResponder()
{
     [[m_wkView window] makeFirstResponder:m_wkView];
}
    
bool PageClientImpl::isViewVisible()
{
    NSView *activeView = this->activeView();
    NSWindow *activeViewWindow = activeWindow();

    if (!activeViewWindow)
        return false;

    if (!activeViewWindow.isVisible)
        return false;

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1080
    // Mountain Lion and previous do not support occlusion notifications, and as such will
    // continue to report as "visible" when not on the active space.
    if (!activeViewWindow.isOnActiveSpace)
        return false;
#endif

    if (activeView.isHiddenOrHasHiddenAncestor)
        return false;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    if ([m_wkView windowOcclusionDetectionEnabled] && (activeViewWindow.occlusionState & NSWindowOcclusionStateVisible) != NSWindowOcclusionStateVisible)
        return false;
#endif

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
    return WindowServerConnection::shared().applicationWindowModificationsHaveStopped() || !isViewVisible();
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
    return [m_wkView _colorSpace];
}

void PageClientImpl::processDidExit()
{
    [m_wkView _processDidExit];
}

void PageClientImpl::pageClosed()
{
    [m_wkView _pageClosed];
#if USE(DICTATION_ALTERNATIVES)
    m_alternativeTextUIController->clear();
#endif
}

void PageClientImpl::didRelaunchProcess()
{
    [m_wkView _didRelaunchProcess];
}

void PageClientImpl::preferencesDidChange()
{
    [m_wkView _preferencesDidChange];
}

void PageClientImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    [m_wkView _toolTipChangedFrom:nsStringFromWebCoreString(oldToolTip) to:nsStringFromWebCoreString(newToolTip)];
}

void PageClientImpl::didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider)
{
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

void PageClientImpl::setCursor(const WebCore::Cursor& cursor)
{
    // FIXME: Would be nice to share this code with WebKit1's WebChromeClient.

    if ([NSApp _cursorRectCursor])
        return;

    if (!m_wkView)
        return;

    NSWindow *window = [m_wkView window];
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
    RefPtr<WebEditCommandProxy> command = prpCommand;

    RetainPtr<WKEditCommandObjC> commandObjC = adoptNS([[WKEditCommandObjC alloc] initWithWebEditCommandProxy:command]);
    String actionName = WebEditCommandProxy::nameForEditAction(command->editAction());

    NSUndoManager *undoManager = [m_wkView undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == WebPageProxy::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

#if USE(INSERTION_UNDO_GROUPING)
void PageClientImpl::registerInsertionUndoGrouping()
{
    registerInsertionUndoGroupingWithUndoManager([m_wkView undoManager]);
}
#endif

void PageClientImpl::clearAllEditCommands()
{
    [[m_wkView undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool PageClientImpl::canUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_wkView undoManager] canUndo] : [[m_wkView undoManager] canRedo];
}

void PageClientImpl::executeUndoRedo(WebPageProxy::UndoOrRedo undoOrRedo)
{
    return (undoOrRedo == WebPageProxy::Undo) ? [[m_wkView undoManager] undo] : [[m_wkView undoManager] redo];
}

void PageClientImpl::setDragImage(const IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag)
{
    RetainPtr<CGImageRef> dragCGImage = dragImage->makeCGImage();
    RetainPtr<NSImage> dragNSImage = adoptNS([[NSImage alloc] initWithCGImage:dragCGImage.get() size:dragImage->size()]);

    [m_wkView _setDragImage:dragNSImage.get() at:clientPosition linkDrag:isLinkDrag];
}

void PageClientImpl::setPromisedData(const String& pasteboardName, PassRefPtr<SharedBuffer> imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleUrl, PassRefPtr<SharedBuffer> archiveBuffer)
{
    RefPtr<Image> image = BitmapImage::create();
    image->setData(imageBuffer.get(), true);
    [m_wkView _setPromisedData:image.get() withFileName:filename withExtension:extension withTitle:title withURL:url withVisibleURL:visibleUrl withArchive:archiveBuffer.get() forPasteboard:pasteboardName];
}

void PageClientImpl::updateSecureInputState()
{
    [m_wkView _updateSecureInputState];
}

void PageClientImpl::resetSecureInputState()
{
    [m_wkView _resetSecureInputState];
}

void PageClientImpl::notifyInputContextAboutDiscardedComposition()
{
    [m_wkView _notifyInputContextAboutDiscardedComposition];
}

FloatRect PageClientImpl::convertToDeviceSpace(const FloatRect& rect)
{
    return [m_wkView _convertToDeviceSpace:rect];
}

FloatRect PageClientImpl::convertToUserSpace(const FloatRect& rect)
{
    return [m_wkView _convertToUserSpace:rect];
}
   
IntPoint PageClientImpl::screenToRootView(const IntPoint& point)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSPoint windowCoord = [[m_wkView window] convertScreenToBase:point];
#pragma clang diagnostic pop
    return IntPoint([m_wkView convertPoint:windowCoord fromView:nil]);
}
    
IntRect PageClientImpl::rootViewToScreen(const IntRect& rect)
{
    NSRect tempRect = rect;
    tempRect = [m_wkView convertRect:tempRect toView:nil];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    tempRect.origin = [[m_wkView window] convertBaseToScreen:tempRect.origin];
#pragma clang diagnostic pop
    return enclosingIntRect(tempRect);
}

void PageClientImpl::doneWithKeyEvent(const NativeWebKeyboardEvent& event, bool eventWasHandled)
{
    [m_wkView _doneWithKeyEvent:event.nativeEvent() eventWasHandled:eventWasHandled];
}

PassRefPtr<WebPopupMenuProxy> PageClientImpl::createPopupMenuProxy(WebPageProxy* page)
{
    return WebPopupMenuProxyMac::create(m_wkView, page);
}

PassRefPtr<WebContextMenuProxy> PageClientImpl::createContextMenuProxy(WebPageProxy* page)
{
    return WebContextMenuProxyMac::create(m_wkView, page);
}

#if ENABLE(INPUT_TYPE_COLOR)
PassRefPtr<WebColorPicker> PageClientImpl::createColorPicker(WebPageProxy* page, const WebCore::Color& initialColor,  const WebCore::IntRect& rect)
{
    return WebColorPickerMac::create(page, initialColor, rect, wkView());
}
#endif

void PageClientImpl::setTextIndicator(PassRefPtr<TextIndicator> textIndicator, bool fadeOut)
{
    [m_wkView _setTextIndicator:textIndicator fadeOut:fadeOut];
}

void PageClientImpl::setTextIndicatorAnimationProgress(float progress)
{
    [m_wkView _setTextIndicatorAnimationProgress:progress];
}

void PageClientImpl::accessibilityWebProcessTokenReceived(const IPC::DataReference& data)
{
    NSData* remoteToken = [NSData dataWithBytes:data.data() length:data.size()];
    [m_wkView _setAccessibilityWebProcessToken:remoteToken];
}
    
void PageClientImpl::enterAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = WKMakeRenderLayer(layerTreeContext.contextID);
    [m_wkView _setAcceleratedCompositingModeRootLayer:renderLayer];
}

void PageClientImpl::exitAcceleratedCompositingMode()
{
    [m_wkView _setAcceleratedCompositingModeRootLayer:nil];
}

void PageClientImpl::updateAcceleratedCompositingMode(const LayerTreeContext& layerTreeContext)
{
    ASSERT(!layerTreeContext.isEmpty());

    CALayer *renderLayer = WKMakeRenderLayer(layerTreeContext.contextID);
    [m_wkView _setAcceleratedCompositingModeRootLayer:renderLayer];
}

void PageClientImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    [m_wkView _setAcceleratedCompositingModeRootLayer:rootLayer];
}

CALayer *PageClientImpl::acceleratedCompositingRootLayer() const
{
    return m_wkView._acceleratedCompositingModeRootLayer;
}

PassRefPtr<ViewSnapshot> PageClientImpl::takeViewSnapshot()
{
    return [m_wkView _takeViewSnapshot];
}

void PageClientImpl::wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent& event)
{
    [m_wkView _wheelEventWasNotHandledByWebCore:event.nativeEvent()];
}

void PageClientImpl::pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus)
{
    [m_wkView _pluginFocusOrWindowFocusChanged:pluginHasFocusAndWindowHasFocus pluginComplexTextInputIdentifier:pluginComplexTextInputIdentifier];
}

void PageClientImpl::setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, PluginComplexTextInputState pluginComplexTextInputState)
{
    [m_wkView _setPluginComplexTextInputState:pluginComplexTextInputState pluginComplexTextInputIdentifier:pluginComplexTextInputIdentifier];
}

void PageClientImpl::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    if (!getLULookupDefinitionModuleClass())
        return;

    NSPoint textBaselineOrigin = dictionaryPopupInfo.origin;

    // Convert to screen coordinates.
    textBaselineOrigin = [m_wkView convertPoint:textBaselineOrigin toView:nil];
    textBaselineOrigin = [m_wkView.window convertRectToScreen:NSMakeRect(textBaselineOrigin.x, textBaselineOrigin.y, 0, 0)].origin;

    RetainPtr<NSMutableDictionary> mutableOptions = adoptNS([(NSDictionary *)dictionaryPopupInfo.options.get() mutableCopy]);

    if (canLoadLUTermOptionDisableSearchTermIndicator() && dictionaryPopupInfo.textIndicator.contentImage) {
        // Run the animations serially because attaching another subwindow breaks the bounce animation.
        // We could consider making the bounce NSAnimationNonblockingThreaded instead, which seems
        // to work, but need to consider all of the implications.
        [m_wkView _setTextIndicator:TextIndicator::create(dictionaryPopupInfo.textIndicator) fadeOut:NO];
        [mutableOptions setObject:@YES forKey:getLUTermOptionDisableSearchTermIndicator()];
        [getLULookupDefinitionModuleClass() showDefinitionForTerm:dictionaryPopupInfo.attributedString.string.get() atLocation:textBaselineOrigin options:mutableOptions.get()];
    } else
        [getLULookupDefinitionModuleClass() showDefinitionForTerm:dictionaryPopupInfo.attributedString.string.get() atLocation:textBaselineOrigin options:mutableOptions.get()];
}

void PageClientImpl::dismissContentRelativeChildWindows()
{
    [m_wkView _dismissContentRelativeChildWindows];
}

void PageClientImpl::showCorrectionPanel(AlternativeTextType type, const FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings)
{
#if USE(AUTOCORRECTION_PANEL)
    if (!isViewVisible() || !isViewInWindow())
        return;
    m_correctionPanel.show(m_wkView, type, boundingBoxOfReplacedString, replacedString, replacementString, alternativeReplacementStrings);
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
    CorrectionPanel::recordAutocorrectionResponse(m_wkView, response, replacedString, replacementString);
}

void PageClientImpl::recommendedScrollbarStyleDidChange(int32_t newStyle)
{
    // Now re-create a tracking area with the appropriate options given the new scrollbar style
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect;
    if (newStyle == NSScrollerStyleLegacy)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;

    RetainPtr<NSTrackingArea> trackingArea = adoptNS([[NSTrackingArea alloc] initWithRect:[m_wkView frame] options:options owner:m_wkView userInfo:nil]);
    [m_wkView _setPrimaryTrackingArea:trackingArea.get()];
}

void PageClientImpl::intrinsicContentSizeDidChange(const IntSize& intrinsicContentSize)
{
    [m_wkView _setIntrinsicContentSize:intrinsicContentSize];
}

bool PageClientImpl::executeSavedCommandBySelector(const String& selectorString)
{
    return [m_wkView _executeSavedCommandBySelector:NSSelectorFromString(selectorString)];
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
    m_alternativeTextUIController->showAlternatives(m_wkView, boundingBoxOfDictatedText, dictationContext, ^(NSString* acceptedAlternative){
        [m_wkView handleAcceptedAlternativeText:acceptedAlternative];
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
    [m_wkView _closeFullScreenWindowController];
}

bool PageClientImpl::isFullScreen()
{
    if (!m_wkView._hasFullScreenWindowController)
        return false;

    return m_wkView._fullScreenWindowController.isFullScreen;
}

void PageClientImpl::enterFullScreen()
{
    [m_wkView._fullScreenWindowController enterFullScreen:nil];
}

void PageClientImpl::exitFullScreen()
{
    [m_wkView._fullScreenWindowController exitFullScreen];
}

void PageClientImpl::beganEnterFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [m_wkView._fullScreenWindowController beganEnterFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
}

void PageClientImpl::beganExitFullScreen(const IntRect& initialFrame, const IntRect& finalFrame)
{
    [m_wkView._fullScreenWindowController beganExitFullScreenWithInitialFrame:initialFrame finalFrame:finalFrame];
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

void PageClientImpl::willRecordNavigationSnapshot(WebBackForwardListItem& item)
{
#if WK_API_ENABLED
    if (m_webView)
        NavigationState::fromWebPage(*m_webView->_page).willRecordNavigationSnapshot(item);
#else
    UNUSED_PARAM(item);
#endif
}

void PageClientImpl::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    [m_wkView _didFirstVisuallyNonEmptyLayoutForMainFrame];
}

void PageClientImpl::didFinishLoadForMainFrame()
{
    [m_wkView _didFinishLoadForMainFrame];
}

void PageClientImpl::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    [m_wkView _didSameDocumentNavigationForMainFrame:type];
}

void PageClientImpl::removeNavigationGestureSnapshot()
{
    [m_wkView _removeNavigationGestureSnapshot];
}

CGRect PageClientImpl::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    CALayer *windowContentLayer = static_cast<NSView *>(m_wkView.window.contentView).layer;
    ASSERT(windowContentLayer);

    return [windowContentLayer convertRect:layer.bounds fromLayer:layer];
}

void PageClientImpl::didPerformActionMenuHitTest(const ActionMenuHitTestResult& result, bool forImmediateAction, API::Object* userData)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [m_wkView _didPerformActionMenuHitTest:result forImmediateAction:forImmediateAction userData:userData];
#endif
}

void PageClientImpl::showPlatformContextMenu(NSMenu *menu, IntPoint location)
{
    [menu popUpMenuPositioningItem:nil atLocation:location inView:m_wkView];
}


} // namespace WebKit

#endif // PLATFORM(MAC)
