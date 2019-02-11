/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#import "WebPage.h"

#if PLATFORM(IOS_FAMILY)

#import "AccessibilityIOS.h"
#import "DataReference.h"
#import "DrawingArea.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "GestureTypes.h"
#import "InteractionInformationAtPosition.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "SandboxUtilities.h"
#import "UIKitSPI.h"
#import "UserData.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import "WebAutocorrectionContext.h"
#import "WebChromeClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebFrame.h"
#import "WebImage.h"
#import "WebPageProxyMessages.h"
#import "WebPreviewLoaderClient.h"
#import "WebProcess.h"
#import <CoreText/CTFont.h>
#import <WebCore/Autofill.h>
#import <WebCore/AutofillElements.h>
#import <WebCore/Chrome.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DiagnosticLoggingClient.h>
#import <WebCore/DiagnosticLoggingKeys.h>
#import <WebCore/DragController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/Element.h>
#import <WebCore/ElementAncestorIterator.h>
#import <WebCore/EventHandler.h>
#import <WebCore/File.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/FrameView.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HTMLAreaElement.h>
#import <WebCore/HTMLAttachmentElement.h>
#import <WebCore/HTMLBodyElement.h>
#import <WebCore/HTMLElement.h>
#import <WebCore/HTMLElementTypeHelpers.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLLabelElement.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLSummaryElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HTMLTextFormControlElement.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/InputMode.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LibWebRTCProvider.h>
#import <WebCore/MediaSessionManagerIOS.h>
#import <WebCore/Node.h>
#import <WebCore/NodeList.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/RenderBlock.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderThemeIOS.h>
#import <WebCore/RenderView.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/Settings.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WKContentObservation.h>
#import <WebCore/WebEvent.h>
#import <wtf/MathExtras.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/SetForScope.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

const int blockSelectionStartWidth = 100;
const int blockSelectionStartHeight = 100;

void WebPage::platformInitialize()
{
    platformInitializeAccessibility();
}

void WebPage::platformDetach()
{
    [m_mockAccessibilityElement setWebPage:nullptr];
}
    
void WebPage::platformInitializeAccessibility()
{
    m_mockAccessibilityElement = adoptNS([[WKAccessibilityWebPageObject alloc] init]);
    [m_mockAccessibilityElement setWebPage:this];
    
    NSData *remoteToken = newAccessibilityRemoteToken([NSUUID UUID]);
    IPC::DataReference dataToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
}

static void computeEditableRootHasContentAndPlainText(const VisibleSelection& selection, EditorState::PostLayoutData& data)
{
    data.hasContent = false;
    data.hasPlainText = false;
    if (!selection.isContentEditable())
        return;

    if (data.selectedTextLength || data.characterAfterSelection || data.characterBeforeSelection || data.twoCharacterBeforeSelection) {
        // If any of these variables have been previously set, the editable root must have plain text content, so we can bail from the remainder of the check.
        data.hasContent = true;
        data.hasPlainText = true;
        return;
    }

    auto* root = selection.rootEditableElement();
    if (!root)
        return;

    auto startInEditableRoot = firstPositionInNode(root);
    data.hasContent = root->hasChildNodes() && !isEndOfEditableOrNonEditableContent(startInEditableRoot);
    data.hasPlainText = data.hasContent && hasAnyPlainText(Range::create(root->document(), VisiblePosition { startInEditableRoot }, VisiblePosition { lastPositionInNode(root) }));
}

void WebPage::platformEditorState(Frame& frame, EditorState& result, IncludePostLayoutDataHint shouldIncludePostLayoutData) const
{
    if (frame.editor().hasComposition()) {
        RefPtr<Range> compositionRange = frame.editor().compositionRange();
        Vector<WebCore::SelectionRect> compositionRects;
        if (compositionRange) {
            compositionRange->collectSelectionRects(compositionRects);
            if (compositionRects.size())
                result.firstMarkedRect = compositionRects[0].rect();
            if (compositionRects.size() > 1)
                result.lastMarkedRect = compositionRects.last().rect();
            else
                result.lastMarkedRect = result.firstMarkedRect;
            result.markedText = plainTextReplacingNoBreakSpace(compositionRange.get());
        }
    }

    // We only set the remaining EditorState entries if layout is done as a performance optimization
    // to avoid the need to force a synchronous layout here to compute these entries. If we
    // have a composition or are using a hardware keyboard then we send the full editor state
    // immediately so that the UIProcess can update UI, including the position of the caret.
    bool needsLayout = !frame.view() || frame.view()->needsLayout();
    bool requiresPostLayoutData = frame.editor().hasComposition();
#if !PLATFORM(IOSMAC)
    requiresPostLayoutData |= [UIKeyboard isInHardwareKeyboardMode];
#endif
    if (shouldIncludePostLayoutData == IncludePostLayoutDataHint::No && needsLayout && !requiresPostLayoutData) {
        result.isMissingPostLayoutData = true;
        return;
    }

    auto& postLayoutData = result.postLayoutData();
    FrameView* view = frame.view();
    const VisibleSelection& selection = frame.selection().selection();
    postLayoutData.isStableStateUpdate = m_isInStableState;
    bool startNodeIsInsideFixedPosition = false;
    bool endNodeIsInsideFixedPosition = false;
    if (selection.isCaret()) {
        postLayoutData.caretRectAtStart = view->contentsToRootView(frame.selection().absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        endNodeIsInsideFixedPosition = startNodeIsInsideFixedPosition;
        postLayoutData.caretRectAtEnd = postLayoutData.caretRectAtStart;
        // FIXME: The following check should take into account writing direction.
        postLayoutData.isReplaceAllowed = result.isContentEditable && atBoundaryOfGranularity(selection.start(), WordGranularity, DirectionForward);
        postLayoutData.wordAtSelection = plainTextReplacingNoBreakSpace(wordRangeFromPosition(selection.start()).get());
        if (selection.isContentEditable())
            charactersAroundPosition(selection.start(), postLayoutData.characterAfterSelection, postLayoutData.characterBeforeSelection, postLayoutData.twoCharacterBeforeSelection);
    } else if (selection.isRange()) {
        postLayoutData.caretRectAtStart = view->contentsToRootView(VisiblePosition(selection.start()).absoluteCaretBounds(&startNodeIsInsideFixedPosition));
        postLayoutData.caretRectAtEnd = view->contentsToRootView(VisiblePosition(selection.end()).absoluteCaretBounds(&endNodeIsInsideFixedPosition));
        RefPtr<Range> selectedRange = selection.toNormalizedRange();
        String selectedText;
        if (selectedRange) {
            selectedRange->collectSelectionRects(postLayoutData.selectionRects);
            convertSelectionRectsToRootView(view, postLayoutData.selectionRects);
            selectedText = plainTextReplacingNoBreakSpace(selectedRange.get(), TextIteratorDefaultBehavior, true);
            postLayoutData.selectedTextLength = selectedText.length();
            const int maxSelectedTextLength = 200;
            if (selectedText.length() <= maxSelectedTextLength)
                postLayoutData.wordAtSelection = selectedText;
        }
        // FIXME: We should disallow replace when the string contains only CJ characters.
        postLayoutData.isReplaceAllowed = result.isContentEditable && !result.isInPasswordField && !selectedText.isAllSpecialCharacters<isHTMLSpace>();
    }
    postLayoutData.atStartOfSentence = frame.selection().selectionAtSentenceStart();
    postLayoutData.insideFixedPosition = startNodeIsInsideFixedPosition || endNodeIsInsideFixedPosition;
    if (!selection.isNone()) {
        if (m_focusedElement && m_focusedElement->renderer()) {
            postLayoutData.focusedElementRect = view->contentsToRootView(m_focusedElement->renderer()->absoluteBoundingBoxRect());
            postLayoutData.caretColor = m_focusedElement->renderer()->style().caretColor();
            postLayoutData.elementIsTransparentOrFullyClipped = m_focusedElement->renderer()->isTransparentOrFullyClippedRespectingParentFrames();
        }
        computeEditableRootHasContentAndPlainText(selection, postLayoutData);
    }
}

FloatSize WebPage::screenSize() const
{
    return m_screenSize;
}

FloatSize WebPage::availableScreenSize() const
{
    return m_availableScreenSize;
}

FloatSize WebPage::overrideScreenSize() const
{
    return m_overrideScreenSize;
}

void WebPage::didReceiveMobileDocType(bool isMobileDoctype)
{
    resetViewportDefaultConfiguration(m_mainFrame.get(), isMobileDoctype);
}

void WebPage::savePageState(HistoryItem& historyItem)
{
    historyItem.setScaleIsInitial(!m_userHasChangedPageScaleFactor);
    historyItem.setMinimumLayoutSizeInScrollViewCoordinates(m_viewportConfiguration.minimumLayoutSize());
    historyItem.setContentSize(m_viewportConfiguration.contentsSize());
}

static double scaleAfterViewportWidthChange(double currentScale, bool userHasChangedPageScaleFactor, const ViewportConfiguration& viewportConfiguration, float unobscuredWidthInScrollViewCoordinates, const IntSize& newContentSize, const IntSize& oldContentSize, float visibleHorizontalFraction)
{
    double scale;
    if (!userHasChangedPageScaleFactor)
        scale = viewportConfiguration.initialScale();
    else
        scale = std::max(std::min(currentScale, viewportConfiguration.maximumScale()), viewportConfiguration.minimumScale());

    LOG(VisibleRects, "scaleAfterViewportWidthChange getting scale %.2f", scale);

    if (userHasChangedPageScaleFactor) {
        // When the content size changes, we keep the same relative horizontal content width in view, otherwise we would
        // end up zoomed too far in landscape->portrait, and too close in portrait->landscape.
        double widthToKeepInView = visibleHorizontalFraction * newContentSize.width();
        double newScale = unobscuredWidthInScrollViewCoordinates / widthToKeepInView;
        scale = std::max(std::min(newScale, viewportConfiguration.maximumScale()), viewportConfiguration.minimumScale());
    }
    return scale;
}

static FloatPoint relativeCenterAfterContentSizeChange(const FloatRect& originalContentRect, IntSize oldContentSize, IntSize newContentSize)
{
    // If the content size has changed, keep the same relative position.
    FloatPoint oldContentCenter = originalContentRect.center();
    float relativeHorizontalPosition = oldContentCenter.x() / oldContentSize.width();
    float relativeVerticalPosition =  oldContentCenter.y() / oldContentSize.height();
    return FloatPoint(relativeHorizontalPosition * newContentSize.width(), relativeVerticalPosition * newContentSize.height());
}

static inline FloatRect adjustExposedRectForNewScale(const FloatRect& exposedRect, double exposedRectScale, double newScale)
{
    if (exposedRectScale == newScale)
        return exposedRect;

    float horizontalChange = exposedRect.width() * exposedRectScale / newScale - exposedRect.width();
    float verticalChange = exposedRect.height() * exposedRectScale / newScale - exposedRect.height();

    auto adjustedRect = exposedRect;
    adjustedRect.inflate({ horizontalChange / 2, verticalChange / 2 });
    return adjustedRect;
}

void WebPage::restorePageState(const HistoryItem& historyItem)
{
    // When a HistoryItem is cleared, its scale factor and scroll point are set to zero. We should not try to restore the other
    // parameters in those conditions.
    if (!historyItem.pageScaleFactor()) {
        send(Messages::WebPageProxy::CouldNotRestorePageState());
        return;
    }

    // We can restore the exposed rect and scale, but we cannot touch the scroll position since the obscured insets
    // may be changing in the UIProcess. The UIProcess can update the position from the information we send and will then
    // scroll to the correct position through a regular VisibleContentRectUpdate.

    m_userHasChangedPageScaleFactor = !historyItem.scaleIsInitial();

    FrameView& frameView = *m_page->mainFrame().view();

    FloatSize currentMinimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.minimumLayoutSize();
    if (historyItem.minimumLayoutSizeInScrollViewCoordinates() == currentMinimumLayoutSizeInScrollViewCoordinates) {
        float boundedScale = std::min<float>(m_viewportConfiguration.maximumScale(), std::max<float>(m_viewportConfiguration.minimumScale(), historyItem.pageScaleFactor()));
        scalePage(boundedScale, IntPoint());

        Optional<FloatPoint> scrollPosition;
        if (historyItem.shouldRestoreScrollPosition()) {
            m_drawingArea->setExposedContentRect(historyItem.exposedContentRect());
            m_hasRestoredExposedContentRectAfterDidCommitLoad = true;
            scrollPosition = FloatPoint(historyItem.scrollPosition());
        }
        send(Messages::WebPageProxy::RestorePageState(scrollPosition, frameView.scrollOrigin(), historyItem.obscuredInsets(), boundedScale));
    } else {
        IntSize oldContentSize = historyItem.contentSize();
        IntSize newContentSize = frameView.contentsSize();
        double visibleHorizontalFraction = static_cast<float>(historyItem.unobscuredContentRect().width()) / oldContentSize.width();

        double newScale = scaleAfterViewportWidthChange(historyItem.pageScaleFactor(), !historyItem.scaleIsInitial(), m_viewportConfiguration, currentMinimumLayoutSizeInScrollViewCoordinates.width(), newContentSize, oldContentSize, visibleHorizontalFraction);

        Optional<FloatPoint> newCenter;
        if (historyItem.shouldRestoreScrollPosition()) {
            if (!oldContentSize.isEmpty() && !newContentSize.isEmpty() && newContentSize != oldContentSize)
                newCenter = relativeCenterAfterContentSizeChange(historyItem.unobscuredContentRect(), oldContentSize, newContentSize);
            else
                newCenter = FloatRect(historyItem.unobscuredContentRect()).center();
        }

        scalePage(newScale, IntPoint());
        send(Messages::WebPageProxy::RestorePageCenterAndScale(newCenter, newScale));
    }
}

double WebPage::minimumPageScaleFactor() const
{
    if (!m_viewportConfiguration.allowsUserScaling())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.minimumScale();
}

double WebPage::maximumPageScaleFactor() const
{
    if (!m_viewportConfiguration.allowsUserScaling())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.maximumScale();
}

double WebPage::maximumPageScaleFactorIgnoringAlwaysScalable() const
{
    if (!m_viewportConfiguration.allowsUserScalingIgnoringAlwaysScalable())
        return m_page->pageScaleFactor();
    return m_viewportConfiguration.maximumScaleIgnoringAlwaysScalable();
}

bool WebPage::allowsUserScaling() const
{
    return m_viewportConfiguration.allowsUserScaling();
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    auto* platformEvent = event->underlyingPlatformEvent();
    if (!platformEvent)
        return false;

    // FIXME: Interpret the event immediately upon receiving it in UI process, without sending to WebProcess first.
    bool eventWasHandled = false;
    bool sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(editorState(), platformEvent->type() == PlatformKeyboardEvent::Char),
        Messages::WebPageProxy::InterpretKeyEvent::Reply(eventWasHandled), m_pageID);
    return sendResult && eventWasHandled;
}

bool WebPage::parentProcessHasServiceWorkerEntitlement() const
{
    static bool hasEntitlement = WTF::hasEntitlement(WebProcess::singleton().parentProcessConnection()->xpcConnection(), "com.apple.developer.WebKit.ServiceWorkers");
    return hasEntitlement;
}

void WebPage::sendComplexTextInputToPlugin(uint64_t, const String&)
{
    notImplemented();
}

bool WebPage::performNonEditingBehaviorForSelector(const String&, WebCore::KeyboardEvent*)
{
    notImplemented();
    return false;
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&)
{
    notImplemented();
    return false;
}

void WebPage::getSelectionContext(CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isRange()) {
        send(Messages::WebPageProxy::SelectionContextCallback(String(), String(), String(), callbackID));
        return;
    }
    const int selectionExtendedContextLength = 350;
    
    String selectedText = plainTextReplacingNoBreakSpace(frame.selection().selection().toNormalizedRange().get());
    String textBefore = plainTextReplacingNoBreakSpace(rangeExpandedByCharactersInDirectionAtWordBoundary(frame.selection().selection().start(), selectionExtendedContextLength, DirectionBackward).get(), TextIteratorDefaultBehavior, true);
    String textAfter = plainTextReplacingNoBreakSpace(rangeExpandedByCharactersInDirectionAtWordBoundary(frame.selection().selection().end(), selectionExtendedContextLength, DirectionForward).get(), TextIteratorDefaultBehavior, true);

    send(Messages::WebPageProxy::SelectionContextCallback(selectedText, textBefore, textAfter, callbackID));
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
    if (!m_page)
        return nil;
    
    if (auto* pluginView = pluginViewForFrame(&m_page->mainFrame()))
        return pluginView->accessibilityObject();
    
    return nil;
}
    
void WebPage::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference&)
{
    NSData *elementTokenData = [NSData dataWithBytes:elementToken.data() length:elementToken.size()];
    [m_mockAccessibilityElement setRemoteTokenData:elementTokenData];
}

void WebPage::readSelectionFromPasteboard(const String&, bool&)
{
    notImplemented();
}

void WebPage::getStringSelectionForPasteboard(String&)
{
    notImplemented();
}

void WebPage::getDataSelectionForPasteboard(const String, SharedMemory::Handle&, uint64_t&)
{
    notImplemented();
}

WKAccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    notImplemented();
    return 0;
}

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest&)
{
    notImplemented();
    return false;
}

void WebPage::shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, bool&)
{
    notImplemented();
}

void WebPage::acceptsFirstMouse(int, const WebKit::WebMouseEvent&, bool&)
{
    notImplemented();
}

void WebPage::computePagesForPrintingPDFDocument(uint64_t, const PrintInfo&, Vector<IntRect>&)
{
    notImplemented();
}

void WebPage::drawPagesToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, uint32_t, uint32_t)
{
    notImplemented();
}

void WebPage::advanceToNextMisspelling(bool)
{
    notImplemented();
}

IntRect WebPage::rectForElementAtInteractionLocation()
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_lastInteractionLocation, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent);
    Node* hitNode = result.innerNode();
    if (!hitNode || !hitNode->renderer())
        return IntRect();
    return result.innerNodeFrame()->view()->contentsToRootView(hitNode->renderer()->absoluteBoundingBoxRect(true));
}

void WebPage::updateSelectionAppearance()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.editor().ignoreSelectionChanges() && (frame.editor().hasComposition() || !frame.selection().selection().isNone()))
        didChangeSelection();
}

void WebPage::handleSyntheticClick(Node* nodeRespondingToClick, const WebCore::FloatPoint& location)
{
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    Frame& mainframe = m_page->mainFrame();

    LOG_WITH_STREAM(ContentObservation, stream << "handleSyntheticClick: node(" << nodeRespondingToClick << ") " << location);
    WKStartObservingContentChanges();
    WKStartObservingDOMTimerScheduling();

    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, WebCore::NoTap));
    mainframe.document()->updateStyleIfNeeded();

    WKStopObservingDOMTimerScheduling();
    WKStopObservingContentChanges();

    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = FloatPoint();

    if (m_isClosed)
        return;

    switch (WKObservedContentChange()) {
    case WKContentVisibilityChange:
        // The move event caused new contents to appear. Don't send the click event.
        LOG(ContentObservation, "handleSyntheticClick: Observed meaningful visible change -> hover.");
        return;
    case WKContentIndeterminateChange:
        // Wait for callback to completePendingSyntheticClickForContentChangeObserver() to decide whether to send the click event.
        m_pendingSyntheticClickNode = nodeRespondingToClick;
        m_pendingSyntheticClickLocation = location;
        LOG(ContentObservation, "handleSyntheticClick: Observed some change, but can't decide it yet -> wait.");
        return;
    case WKContentNoChange:
        LOG(ContentObservation, "handleSyntheticClick: No change was observed -> click.");
        completeSyntheticClick(nodeRespondingToClick, location, WebCore::OneFingerTap);
        return;
    }
    ASSERT_NOT_REACHED();
}

void WebPage::completePendingSyntheticClickForContentChangeObserver()
{
    LOG_WITH_STREAM(ContentObservation, stream << "completePendingSyntheticClickForContentChangeObserver: pending target node(" << m_pendingSyntheticClickNode << ")");
    if (!m_pendingSyntheticClickNode)
        return;
    // Only dispatch the click if the document didn't get changed by any timers started by the move event.
    if (WKObservedContentChange() == WKContentNoChange) {
        LOG(ContentObservation, "No chage was observed -> click.");
        completeSyntheticClick(m_pendingSyntheticClickNode.get(), m_pendingSyntheticClickLocation, WebCore::OneFingerTap);
    } else
        LOG(ContentObservation, "Observed meaningful visible change -> hover.");

    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = FloatPoint();
}

void WebPage::completeSyntheticClick(Node* nodeRespondingToClick, const WebCore::FloatPoint& location, SyntheticClickType syntheticClickType)
{
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    Frame& mainframe = m_page->mainFrame();

    RefPtr<Frame> oldFocusedFrame = m_page->focusController().focusedFrame();
    RefPtr<Element> oldFocusedElement = oldFocusedFrame ? oldFocusedFrame->document()->focusedElement() : nullptr;

    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    bool tapWasHandled = false;
    m_lastInteractionLocation = roundedAdjustedPoint;

    tapWasHandled |= mainframe.eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, syntheticClickType));
    if (m_isClosed)
        return;

    tapWasHandled |= mainframe.eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MouseReleased, 1, false, false, false, false, WallTime::now(), WebCore::ForceAtClick, syntheticClickType));
    if (m_isClosed)
        return;

    RefPtr<Frame> newFocusedFrame = m_page->focusController().focusedFrame();
    RefPtr<Element> newFocusedElement = newFocusedFrame ? newFocusedFrame->document()->focusedElement() : nullptr;

    // If the focus has not changed, we need to notify the client anyway, since it might be
    // necessary to start assisting the node.
    // If the node has been focused by JavaScript without user interaction, the
    // keyboard is not on screen.
    if (newFocusedElement && newFocusedElement == oldFocusedElement)
        elementDidRefocus(*newFocusedElement);

    if (!tapWasHandled || !nodeRespondingToClick || !nodeRespondingToClick->isElementNode())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(location)));
    
    send(Messages::WebPageProxy::DidCompleteSyntheticClick());
}

void WebPage::handleTap(const IntPoint& point, uint64_t lastLayerTreeTransactionId)
{
    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;
    IntPoint adjustedIntPoint = roundedIntPoint(adjustedPoint);

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(adjustedIntPoint));
#if ENABLE(DATA_DETECTION)
    else if (is<Element>(*nodeRespondingToClick) && DataDetection::shouldCancelDefaultAction(downcast<Element>(*nodeRespondingToClick))) {
        InteractionInformationRequest request(adjustedIntPoint);
        requestPositionInformation(request);
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(adjustedIntPoint));
    }
#endif
    else
        handleSyntheticClick(nodeRespondingToClick, adjustedPoint);
}

void WebPage::requestFocusedElementInformation(WebKit::CallbackID callbackID)
{
    FocusedElementInformation info;
    if (m_focusedElement)
        getFocusedElementInformation(info);

    send(Messages::WebPageProxy::FocusedElementInformationCallback(info, callbackID));
}

#if ENABLE(DATA_INTERACTION)
void WebPage::requestDragStart(const IntPoint& clientPosition, const IntPoint& globalPosition)
{
    bool didStart = m_page->mainFrame().eventHandler().tryToBeginDragAtPoint(clientPosition, globalPosition);
    send(Messages::WebPageProxy::DidHandleDragStartRequest(didStart));
}

void WebPage::requestAdditionalItemsForDragSession(const IntPoint& clientPosition, const IntPoint& globalPosition)
{
    // To augment the platform drag session with additional items, end the current drag session and begin a new drag session with the new drag item.
    // This process is opaque to the UI process, which still maintains the old drag item in its drag session. Similarly, this persistent drag session
    // is opaque to the web process, which only sees that the current drag has ended, and that a new one is beginning.
    PlatformMouseEvent event(clientPosition, globalPosition, LeftButton, PlatformEvent::MouseMoved, 0, false, false, false, false, WallTime::now(), 0, NoTap);
    m_page->dragController().dragEnded();
    m_page->mainFrame().eventHandler().dragSourceEndedAt(event, DragOperationNone, MayExtendDragSession::Yes);

    bool didHandleDrag = m_page->mainFrame().eventHandler().tryToBeginDragAtPoint(clientPosition, globalPosition);
    send(Messages::WebPageProxy::DidHandleAdditionalDragItemsRequest(didHandleDrag));
}

void WebPage::didConcludeEditDrag()
{
    Optional<TextIndicatorData> textIndicatorData;

    static auto defaultTextIndicatorOptionsForEditDrag = TextIndicatorOptionIncludeSnapshotOfAllVisibleContentWithoutSelection | TextIndicatorOptionExpandClipBeyondVisibleRect | TextIndicatorOptionPaintAllContent | TextIndicatorOptionIncludeMarginIfRangeMatchesSelection | TextIndicatorOptionPaintBackgrounds | TextIndicatorOptionComputeEstimatedBackgroundColor| TextIndicatorOptionUseSelectionRectForSizing | TextIndicatorOptionIncludeSnapshotWithSelectionHighlight;
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (auto range = frame.selection().selection().toNormalizedRange()) {
        if (auto textIndicator = TextIndicator::createWithRange(*range, defaultTextIndicatorOptionsForEditDrag, TextIndicatorPresentationTransition::None, FloatSize()))
            textIndicatorData = textIndicator->data();
    }

    send(Messages::WebPageProxy::DidConcludeEditDrag(WTFMove(textIndicatorData)));
}
#endif

void WebPage::sendTapHighlightForNodeIfNecessary(uint64_t requestID, Node* node)
{
#if ENABLE(TOUCH_EVENTS)
    if (!node)
        return;

    if (m_page->isEditable() && node == m_page->mainFrame().document()->body())
        return;

    if (is<Element>(*node)) {
        ASSERT(m_page);
        m_page->mainFrame().loader().client().prefetchDNS(downcast<Element>(*node).absoluteLinkURL().host().toString());
    }

    if (is<HTMLAreaElement>(node)) {
        node = downcast<HTMLAreaElement>(node)->imageElement();
        if (!node)
            return;
    }

    Vector<FloatQuad> quads;
    if (RenderObject *renderer = node->renderer()) {
        renderer->absoluteQuads(quads);
        Color highlightColor = renderer->style().tapHighlightColor();
        if (!node->document().frame()->isMainFrame()) {
            FrameView* view = node->document().frame()->view();
            for (size_t i = 0; i < quads.size(); ++i) {
                FloatQuad& currentQuad = quads[i];
                currentQuad.setP1(view->contentsToRootView(roundedIntPoint(currentQuad.p1())));
                currentQuad.setP2(view->contentsToRootView(roundedIntPoint(currentQuad.p2())));
                currentQuad.setP3(view->contentsToRootView(roundedIntPoint(currentQuad.p3())));
                currentQuad.setP4(view->contentsToRootView(roundedIntPoint(currentQuad.p4())));
            }
        }

        RoundedRect::Radii borderRadii;
        if (is<RenderBox>(*renderer))
            borderRadii = downcast<RenderBox>(*renderer).borderRadii();

        bool nodeHasBuiltInClickHandling = is<HTMLFormControlElement>(*node) || is<HTMLAnchorElement>(*node) || is<HTMLLabelElement>(*node) || is<HTMLSummaryElement>(*node) || node->isLink();
        send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, highlightColor, quads, roundedIntSize(borderRadii.topLeft()), roundedIntSize(borderRadii.topRight()), roundedIntSize(borderRadii.bottomLeft()), roundedIntSize(borderRadii.bottomRight()), nodeHasBuiltInClickHandling));
    }
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(node);
#endif
}

void WebPage::handleTwoFingerTapAtPoint(const WebCore::IntPoint& point, uint64_t requestID)
{
    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
    if (!nodeRespondingToClick || !nodeRespondingToClick->renderer()) {
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(adjustedPoint)));
        return;
    }
    sendTapHighlightForNodeIfNecessary(requestID, nodeRespondingToClick);
#if ENABLE(DATA_DETECTION)
    if (is<Element>(*nodeRespondingToClick) && DataDetection::shouldCancelDefaultAction(downcast<Element>(*nodeRespondingToClick))) {
        InteractionInformationRequest request(roundedIntPoint(adjustedPoint));
        requestPositionInformation(request);
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(adjustedPoint)));
    } else
#endif
        completeSyntheticClick(nodeRespondingToClick, adjustedPoint, WebCore::TwoFingerTap);
}

void WebPage::handleStylusSingleTapAtPoint(const WebCore::IntPoint& point, uint64_t requestID)
{
    SetForScope<bool> userIsInteractingChange { m_userIsInteracting, true };

    auto& frame = m_page->focusController().focusedOrMainFrame();

    auto pointInDocument = frame.view()->rootViewToContents(point);
    HitTestResult hitTest = frame.eventHandler().hitTestResultAtPoint(pointInDocument, HitTestRequest::ReadOnly | HitTestRequest::Active);

    Node* node = hitTest.innerNonSharedNode();
    if (!node)
        return;
    auto renderer = node->renderer();
    if (!renderer)
        return;

    if (renderer->isReplaced())
        return;

    VisiblePosition position = renderer->positionForPoint(hitTest.localPoint(), nullptr);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    if (position.isNull())
        return;

    auto range = Range::create(*frame.document(), position, position);
    frame.selection().setSelectedRange(range.ptr(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    auto image = frame.editor().insertEditableImage();
    frame.document()->setFocusedElement(image.get());
}

void WebPage::potentialTapAtPosition(uint64_t requestID, const WebCore::FloatPoint& position)
{
    m_potentialTapNode = m_page->mainFrame().nodeRespondingToClickEvents(position, m_potentialTapLocation, m_potentialTapSecurityOrigin.get());
    sendTapHighlightForNodeIfNecessary(requestID, m_potentialTapNode.get());
#if ENABLE(TOUCH_EVENTS)
    if (m_potentialTapNode && !m_potentialTapNode->allowsDoubleTapGesture())
        send(Messages::WebPageProxy::DisableDoubleTapGesturesDuringTapIfNecessary(requestID));
#endif
}

void WebPage::commitPotentialTap(uint64_t lastLayerTreeTransactionId)
{
    if (!m_potentialTapNode || (!m_potentialTapNode->renderer() && !is<HTMLAreaElement>(m_potentialTapNode.get()))) {
        commitPotentialTapFailed();
        return;
    }

    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(m_potentialTapLocation, adjustedPoint, m_potentialTapSecurityOrigin.get());
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad()) {
        commitPotentialTapFailed();
        return;
    }

    if (m_potentialTapNode == nodeRespondingToClick) {
#if ENABLE(DATA_DETECTION)
        if (is<Element>(*nodeRespondingToClick) && DataDetection::shouldCancelDefaultAction(downcast<Element>(*nodeRespondingToClick))) {
            InteractionInformationRequest request(roundedIntPoint(m_potentialTapLocation));
            requestPositionInformation(request);
            commitPotentialTapFailed();
        } else
#endif
            handleSyntheticClick(nodeRespondingToClick, adjustedPoint);
    } else
        commitPotentialTapFailed();

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::commitPotentialTapFailed()
{
    send(Messages::WebPageProxy::CommitPotentialTapFailed());
    send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(m_potentialTapLocation)));
}

void WebPage::cancelPotentialTap()
{
    cancelPotentialTapInFrame(*m_mainFrame);
}

void WebPage::cancelPotentialTapInFrame(WebFrame& frame)
{
    if (m_potentialTapNode) {
        Frame* potentialTapFrame = m_potentialTapNode->document().frame();
        if (potentialTapFrame && !potentialTapFrame->tree().isDescendantOf(frame.coreFrame()))
            return;
    }

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
    m_potentialTapSecurityOrigin = nullptr;
}

void WebPage::tapHighlightAtPosition(uint64_t requestID, const FloatPoint& position)
{
    Frame& mainframe = m_page->mainFrame();
    FloatPoint adjustedPoint;
    sendTapHighlightForNodeIfNecessary(requestID, mainframe.nodeRespondingToClickEvents(position, adjustedPoint));
}

void WebPage::inspectorNodeSearchMovedToPosition(const FloatPoint& position)
{
    IntPoint adjustedPoint = roundedIntPoint(position);
    Frame& mainframe = m_page->mainFrame();

    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(adjustedPoint, adjustedPoint, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, { }, 0, WebCore::NoTap));
    mainframe.document()->updateStyleIfNeeded();
}

void WebPage::inspectorNodeSearchEndedAtPosition(const FloatPoint& position)
{
    if (Node* node = m_page->mainFrame().deepestNodeAtLocation(position))
        node->inspect();
}

void WebPage::blurFocusedElement()
{
    if (!m_focusedElement)
        return;

    m_focusedElement->blur();
}

void WebPage::setFocusedElementValue(const String& value)
{
    // FIXME: should also handle the case of HTMLSelectElement.
    if (is<HTMLInputElement>(m_focusedElement.get()))
        downcast<HTMLInputElement>(*m_focusedElement).setValue(value, DispatchInputAndChangeEvent);
}

void WebPage::setFocusedElementValueAsNumber(double value)
{
    if (is<HTMLInputElement>(m_focusedElement.get()))
        downcast<HTMLInputElement>(*m_focusedElement).setValueAsNumber(value, DispatchInputAndChangeEvent);
}

void WebPage::setFocusedElementSelectedIndex(uint32_t index, bool allowMultipleSelection)
{
    if (is<HTMLSelectElement>(m_focusedElement.get()))
        downcast<HTMLSelectElement>(*m_focusedElement).optionSelectedByUser(index, true, allowMultipleSelection);
}

void WebPage::showInspectorHighlight(const WebCore::Highlight& highlight)
{
    send(Messages::WebPageProxy::ShowInspectorHighlight(highlight));
}

void WebPage::hideInspectorHighlight()
{
    send(Messages::WebPageProxy::HideInspectorHighlight());
}

void WebPage::showInspectorIndication()
{
    send(Messages::WebPageProxy::ShowInspectorIndication());
}

void WebPage::hideInspectorIndication()
{
    send(Messages::WebPageProxy::HideInspectorIndication());
}

void WebPage::enableInspectorNodeSearch()
{
    send(Messages::WebPageProxy::EnableInspectorNodeSearch());
}

void WebPage::disableInspectorNodeSearch()
{
    send(Messages::WebPageProxy::DisableInspectorNodeSearch());
}

void WebPage::setForceAlwaysUserScalable(bool userScalable)
{
    m_forceAlwaysUserScalable = userScalable;
    m_viewportConfiguration.setForceAlwaysUserScalable(userScalable);
}

static FloatQuad innerFrameQuad(const Frame& frame, const Element& focusedElement)
{
    frame.document()->updateLayoutIgnorePendingStylesheets();
    RenderElement* renderer = nullptr;
    if (focusedElement.hasTagName(HTMLNames::textareaTag) || focusedElement.hasTagName(HTMLNames::inputTag) || focusedElement.hasTagName(HTMLNames::selectTag))
        renderer = focusedElement.renderer();
    else if (auto* rootEditableElement = focusedElement.rootEditableElement())
        renderer = rootEditableElement->renderer();
    
    if (!renderer)
        return FloatQuad();

    auto& style = renderer->style();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);

    boundingBox.move(style.borderLeftWidth(), style.borderTopWidth());
    boundingBox.setWidth(boundingBox.width() - style.borderLeftWidth() - style.borderRightWidth());
    boundingBox.setHeight(boundingBox.height() - style.borderBottomWidth() - style.borderTopWidth());

    return FloatQuad(boundingBox);
}

static IntPoint constrainPoint(const IntPoint& point, const Frame& frame, const Element& focusedElement)
{
    ASSERT(&focusedElement.document() == frame.document());
    const int DEFAULT_CONSTRAIN_INSET = 2;
    IntRect innerFrame = innerFrameQuad(frame, focusedElement).enclosingBoundingBox();
    IntPoint constrainedPoint = point;

    int minX = innerFrame.x() + DEFAULT_CONSTRAIN_INSET;
    int maxX = innerFrame.maxX() - DEFAULT_CONSTRAIN_INSET;
    int minY = innerFrame.y() + DEFAULT_CONSTRAIN_INSET;
    int maxY = innerFrame.maxY() - DEFAULT_CONSTRAIN_INSET;

    if (point.x() < minX)
        constrainedPoint.setX(minX);
    else if (point.x() > maxX)
        constrainedPoint.setX(maxX);

    if (point.y() < minY)
        constrainedPoint.setY(minY);
    else if (point.y() >= maxY)
        constrainedPoint.setY(maxY);
                    
    return constrainedPoint;
}

static IntRect selectionBoxForRange(WebCore::Range* range)
{
    if (!range)
        return IntRect();
    
    IntRect boundingRect;
    Vector<SelectionRect> selectionRects;
    range->collectSelectionRects(selectionRects);
    unsigned size = selectionRects.size();
    
    for (unsigned i = 0; i < size; ++i) {
        const IntRect &coreRect = selectionRects[i].rect();
        if (!i)
            boundingRect = coreRect;
        else
            boundingRect.unite(coreRect);
    }
    return boundingRect;
}

static bool canShrinkToTextSelection(Node* node)
{
    if (node && !is<Element>(*node))
        node = node->parentElement();
    
    auto* renderer = (node) ? node->renderer() : nullptr;
    return renderer && renderer->childrenInline() && (is<RenderBlock>(*renderer) && !downcast<RenderBlock>(*renderer).inlineContinuation()) && !renderer->isTable();
}

static bool hasCustomLineHeight(Node& node)
{
    auto* renderer = node.renderer();
    return renderer && renderer->style().lineHeight().isSpecified();
}
    
RefPtr<Range> WebPage::rangeForWebSelectionAtPosition(const IntPoint& point, const VisiblePosition& position, SelectionFlags& flags)
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent);

    Node* currentNode = result.innerNode();
    if (!currentNode)
        return nullptr;
    RefPtr<Range> range;
    FloatRect boundingRectInScrollViewCoordinates;

    if (!currentNode->isTextNode() && !canShrinkToTextSelection(currentNode) && hasCustomLineHeight(*currentNode)) {
        auto* renderer = currentNode->renderer();
        if (is<RenderBlockFlow>(renderer)) {
            auto* renderText = downcast<RenderBlockFlow>(*renderer).findClosestTextAtAbsolutePoint(point);
            if (renderText && renderText->textNode())
                currentNode = renderText->textNode();
        }
    }

    if (currentNode->isTextNode()) {
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        if (!range || range->collapsed())
            range = Range::create(currentNode->document(), position, position);
        else {
            m_blockRectForTextSelection = selectionBoxForRange(range.get());
            range = wordRangeFromPosition(position);
        }

        return range;
    }

    if (!currentNode->isElementNode())
        currentNode = currentNode->parentElement();

    Node* bestChoice = currentNode;
    while (currentNode) {
        if (currentNode->renderer()) {
            boundingRectInScrollViewCoordinates = currentNode->renderer()->absoluteBoundingBoxRect(true);
            boundingRectInScrollViewCoordinates.scale(m_page->pageScaleFactor());
            if (boundingRectInScrollViewCoordinates.width() > m_blockSelectionDesiredSize.width() && boundingRectInScrollViewCoordinates.height() > m_blockSelectionDesiredSize.height())
                break;
            bestChoice = currentNode;
        }
        currentNode = currentNode->parentElement();
    }

    if (!bestChoice)
        return nullptr;

    RenderObject* renderer = bestChoice->renderer();
    if (!renderer || renderer->style().userSelect() == UserSelect::None)
        return nullptr;

    if (renderer->childrenInline() && (is<RenderBlock>(*renderer) && !downcast<RenderBlock>(*renderer).inlineContinuation()) && !renderer->isTable()) {
        range = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionBackward);
        if (range && !range->collapsed())
            return range;
    }

    // If all we could find is a block whose height is very close to the height
    // of the visible area, don't use it.
    const float adjustmentFactor = .97;
    boundingRectInScrollViewCoordinates = renderer->absoluteBoundingBoxRect(true);

    if (boundingRectInScrollViewCoordinates.height() > m_page->mainFrame().view()->exposedContentRect().height() * adjustmentFactor)
        return nullptr;

    range = Range::create(bestChoice->document());
    range->selectNodeContents(*bestChoice);
    return range->collapsed() ? nullptr : range;
}

void WebPage::selectWithGesture(const IntPoint& point, uint32_t granularity, uint32_t gestureType, uint32_t gestureState, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNull()) {
        send(Messages::WebPageProxy::GestureCallback(point, gestureType, gestureState, 0, callbackID));
        return;
    }
    RefPtr<Range> range;
    SelectionFlags flags = None;
    GestureRecognizerState wkGestureState = static_cast<GestureRecognizerState>(gestureState);
    switch (static_cast<GestureType>(gestureType)) {
    case GestureType::PhraseBoundary:
    {
        if (!frame.editor().hasComposition())
            break;
        RefPtr<Range> markedRange = frame.editor().compositionRange();
        if (position < markedRange->startPosition())
            position = markedRange->startPosition();
        if (position > markedRange->endPosition())
            position = markedRange->endPosition();
        if (wkGestureState != GestureRecognizerState::Began)
            flags = distanceBetweenPositions(markedRange->startPosition(), frame.selection().selection().start()) != distanceBetweenPositions(markedRange->startPosition(), position) ? PhraseBoundaryChanged : None;
        else
            flags = PhraseBoundaryChanged;
        range = Range::create(*frame.document(), position, position);
    }
        break;

    case GestureType::OneFingerTap:
    {
        VisiblePosition result;
        // move the position at the end of the word
        if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
            // Don't cross line boundaries.
            result = position;
        } else if (withinTextUnitOfGranularity(position, WordGranularity, DirectionForward)) {
            // The position lies within a word.
            RefPtr<Range> wordRange = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionForward);
            if (wordRange) {
                result = wordRange->startPosition();
                if (distanceBetweenPositions(position, result) > 1)
                    result = wordRange->endPosition();
            }
        } else if (atBoundaryOfGranularity(position, WordGranularity, DirectionBackward)) {
            // The position is at the end of a word.
            result = position;
        } else {
            // The position is not within a word.
            // Go to the next boundary.
            result = positionOfNextBoundaryOfGranularity(position, WordGranularity, DirectionForward);

            // If there is no such boundary we go to the end of the element.
            if (result.isNull())
                result = endOfEditableContent(position);
        }
        if (result.isNotNull())
            range = Range::create(*frame.document(), result, result);
    }
        break;

    case GestureType::Loupe:
        if (position.rootEditableElement())
            range = Range::create(*frame.document(), position, position);
        else
#if !PLATFORM(IOSMAC)
            range = wordRangeFromPosition(position);
#else
            switch (wkGestureState) {
            case GestureRecognizerState::Began:
                m_startingGestureRange = Range::create(*frame.document(), position, position);
                break;
            case GestureRecognizerState::Changed:
                if (m_startingGestureRange) {
                    if (m_startingGestureRange->startPosition() < position)
                        range = Range::create(*frame.document(), m_startingGestureRange->startPosition(), position);
                    else
                        range = Range::create(*frame.document(), position, m_startingGestureRange->startPosition());
                }
                break;
            case GestureRecognizerState::Ended:
            case GestureRecognizerState::Cancelled:
                m_startingGestureRange = nullptr;
                break;
            case GestureRecognizerState::Failed:
            case GestureRecognizerState::Possible:
                ASSERT_NOT_REACHED();
                break;
            }
#endif
        break;

    case GestureType::TapAndAHalf:
        switch (wkGestureState) {
        case GestureRecognizerState::Began:
            range = wordRangeFromPosition(position);
            m_currentWordRange = range ? RefPtr<Range>(Range::create(*frame.document(), range->startPosition(), range->endPosition())) : nullptr;
            break;
        case GestureRecognizerState::Changed:
            if (!m_currentWordRange)
                break;
            range = Range::create(*frame.document(), m_currentWordRange->startPosition(), m_currentWordRange->endPosition());
            if (position < range->startPosition())
                range->setStart(position.deepEquivalent());
            if (position > range->endPosition())
                range->setEnd(position.deepEquivalent());
            break;
        case GestureRecognizerState::Ended:
        case GestureRecognizerState::Cancelled:
            m_currentWordRange = nullptr;
            break;
        case GestureRecognizerState::Failed:
        case GestureRecognizerState::Possible:
            ASSERT_NOT_REACHED();
        }
        break;

    case GestureType::OneFingerDoubleTap:
        if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
            // Double-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines,
            // paragraphs, and documents.
            range = Range::create(*frame.document(), position, position);
         } else
            range = wordRangeFromPosition(position);
        break;

    case GestureType::TwoFingerSingleTap:
        // Single tap with two fingers selects the entire paragraph.
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        break;

    case GestureType::OneFingerTripleTap:
        if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
            // Triple-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines,
            // paragraphs, and documents.
            range = Range::create(*frame.document(), position, position);
        } else
            range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        break;

    case GestureType::MakeWebSelection:
        if (wkGestureState == GestureRecognizerState::Began) {
            m_blockSelectionDesiredSize.setWidth(blockSelectionStartWidth);
            m_blockSelectionDesiredSize.setHeight(blockSelectionStartHeight);
            m_currentBlockSelection = nullptr;
        }
        range = rangeForWebSelectionAtPosition(point, position, flags);
        break;

    default:
        break;
    }
    if (range)
        frame.selection().setSelectedRange(range.get(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    send(Messages::WebPageProxy::GestureCallback(point, gestureType, gestureState, static_cast<uint32_t>(flags), callbackID));
}

static RefPtr<Range> rangeForPointInRootViewCoordinates(Frame& frame, const IntPoint& pointInRootViewCoordinates, bool baseIsStart)
{
    VisibleSelection existingSelection = frame.selection().selection();
    VisiblePosition selectionStart = existingSelection.visibleStart();
    VisiblePosition selectionEnd = existingSelection.visibleEnd();

    auto pointInDocument = frame.view()->rootViewToContents(pointInRootViewCoordinates);

    if (baseIsStart) {
        int startY = selectionStart.absoluteCaretBounds().center().y();
        if (pointInDocument.y() < startY)
            pointInDocument.setY(startY);
    } else {
        int endY = selectionEnd.absoluteCaretBounds().center().y();
        if (pointInDocument.y() > endY)
            pointInDocument.setY(endY);
    }
    
    VisiblePosition result;
    RefPtr<Range> range;
    
    HitTestResult hitTest = frame.eventHandler().hitTestResultAtPoint(pointInDocument, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent);
    if (hitTest.targetNode())
        result = frame.eventHandler().selectionExtentRespectingEditingBoundary(frame.selection().selection(), hitTest.localPoint(), hitTest.targetNode()).deepEquivalent();
    else
        result = frame.visiblePositionForPoint(pointInDocument).deepEquivalent();
    
    if (baseIsStart) {
        if (comparePositions(result, selectionStart) <= 0)
            result = selectionStart.next();
        else if (&selectionStart.deepEquivalent().anchorNode()->treeScope() != &hitTest.targetNode()->treeScope())
            result = VisibleSelection::adjustPositionForEnd(result.deepEquivalent(), selectionStart.deepEquivalent().containerNode());
        
        if (result.isNotNull())
            range = Range::create(*frame.document(), selectionStart, result);
    } else {
        if (comparePositions(selectionEnd, result) <= 0)
            result = selectionEnd.previous();
        else if (&hitTest.targetNode()->treeScope() != &selectionEnd.deepEquivalent().anchorNode()->treeScope())
            result = VisibleSelection::adjustPositionForStart(result.deepEquivalent(), selectionEnd.deepEquivalent().containerNode());
        
        if (result.isNotNull())
            range = Range::create(*frame.document(), result.deepEquivalent(), selectionEnd);
    }
    
    return range;
}

static RefPtr<Range> rangeAtWordBoundaryForPosition(Frame* frame, const VisiblePosition& position, bool baseIsStart, SelectionDirection direction)
{
    SelectionDirection sameDirection = baseIsStart ? DirectionForward : DirectionBackward;
    SelectionDirection oppositeDirection = baseIsStart ? DirectionBackward : DirectionForward;
    VisiblePosition base = baseIsStart ? frame->selection().selection().visibleStart() : frame->selection().selection().visibleEnd();
    VisiblePosition extent = baseIsStart ? frame->selection().selection().visibleEnd() : frame->selection().selection().visibleStart();
    VisiblePosition initialExtent = position;

    if (atBoundaryOfGranularity(extent, WordGranularity, sameDirection)) {
        // This is a word boundary. Leave selection where it is.
        return nullptr;
    }

    if (atBoundaryOfGranularity(extent, WordGranularity, oppositeDirection)) {
        // This is a word boundary in the wrong direction. Nudge the selection to a character before proceeding.
        extent = baseIsStart ? extent.previous() : extent.next();
    }

    // Extend to the boundary of the word.

    VisiblePosition wordBoundary = positionOfNextBoundaryOfGranularity(extent, WordGranularity, sameDirection);
    if (wordBoundary.isNotNull()
        && atBoundaryOfGranularity(wordBoundary, WordGranularity, sameDirection)
        && initialExtent != wordBoundary) {
        extent = wordBoundary;
        return (base < extent) ? Range::create(*frame->document(), base, extent) : Range::create(*frame->document(), extent, base);
    }
    // Conversely, if the initial extent equals the current word boundary, then
    // run the rest of this function to see if the selection should extend
    // the other direction to the other word.

    // If this is where the extent was initially, then iterate in the other direction in the document until we hit the next word.
    while (extent.isNotNull()
        && !atBoundaryOfGranularity(extent, WordGranularity, sameDirection)
        && extent != base
        && !atBoundaryOfGranularity(extent, LineGranularity, sameDirection)
        && !atBoundaryOfGranularity(extent, LineGranularity, oppositeDirection)) {
        extent = baseIsStart ? extent.next() : extent.previous();
    }

    // Don't let the smart extension make the extent equal the base.
    // Expand out to word boundary.
    if (extent.isNull() || extent == base)
        extent = wordBoundary;
    if (extent.isNull())
        return nullptr;

    return (base < extent) ? Range::create(*frame->document(), base, extent) : Range::create(*frame->document(), extent, base);
}

void WebPage::clearSelection(){
    m_startingGestureRange = nullptr;
    m_currentBlockSelection = nullptr;
    m_page->focusController().focusedOrMainFrame().selection().clear();
}

void WebPage::updateSelectionWithTouches(const IntPoint& point, uint32_t touches, bool baseIsStart, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    IntPoint pointInDocument = frame.view()->rootViewToContents(point);
    VisiblePosition position = frame.visiblePositionForPoint(pointInDocument);
    if (position.isNull()) {
        send(Messages::WebPageProxy::TouchesCallback(point, touches, 0, callbackID));
        return;
    }

    RefPtr<Range> range;
    VisiblePosition result;
    SelectionFlags flags = None;

    switch (static_cast<SelectionTouch>(touches)) {
    case SelectionTouch::Started:
    case SelectionTouch::EndedNotMoving:
        break;
    
    case SelectionTouch::Ended:
        if (frame.selection().selection().isContentEditable()) {
            result = closestWordBoundaryForPosition(position);
            if (result.isNotNull())
                range = Range::create(*frame.document(), result, result);
        } else
            range = rangeForPointInRootViewCoordinates(frame, point, baseIsStart);
        break;

    case SelectionTouch::EndedMovingForward:
        range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionForward);
        break;
        
    case SelectionTouch::EndedMovingBackward:
        range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionBackward);
        break;

    case SelectionTouch::Moved:
        range = rangeForPointInRootViewCoordinates(frame, point, baseIsStart);
        break;
    }
    if (range)
        frame.selection().setSelectedRange(range.get(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    send(Messages::WebPageProxy::TouchesCallback(point, touches, flags, callbackID));
}

void WebPage::selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, uint32_t gestureType, uint32_t gestureState, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition fromPosition = frame.visiblePositionForPoint(frame.view()->rootViewToContents(from));
    VisiblePosition toPosition = frame.visiblePositionForPoint(frame.view()->rootViewToContents(to));
    RefPtr<Range> range;
    if (fromPosition.isNotNull() && toPosition.isNotNull()) {
        if (fromPosition < toPosition)
            range = Range::create(*frame.document(), fromPosition, toPosition);
        else
            range = Range::create(*frame.document(), toPosition, fromPosition);
        frame.selection().setSelectedRange(range.get(), fromPosition.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }

    // We can use the same callback for the gestures with one point.
    send(Messages::WebPageProxy::GestureCallback(from, gestureType, gestureState, 0, callbackID));
}

void WebPage::extendSelection(uint32_t granularity)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    // For the moment we handle only WordGranularity.
    if (granularity != WordGranularity || !frame.selection().isCaret())
        return;

    VisiblePosition position = frame.selection().selection().start();
    frame.selection().setSelectedRange(wordRangeFromPosition(position).get(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
}

void WebPage::selectWordBackward()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaret())
        return;

    VisiblePosition position = frame.selection().selection().start();
    VisiblePosition startPosition = positionOfNextBoundaryOfGranularity(position, WordGranularity, DirectionBackward);
    if (startPosition.isNotNull() && startPosition != position)
        frame.selection().setSelectedRange(Range::create(*frame.document(), startPosition, position).ptr(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
}

void WebPage::moveSelectionByOffset(int32_t offset, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    
    VisiblePosition startPosition = frame.selection().selection().end();
    if (startPosition.isNull())
        return;
    SelectionDirection direction = offset < 0 ? DirectionBackward : DirectionForward;
    VisiblePosition position = startPosition;
    for (int i = 0; i < abs(offset); ++i) {
        position = positionOfNextBoundaryOfGranularity(position, CharacterGranularity, direction);
        if (position.isNull())
            break;
    }
    if (position.isNotNull() && startPosition != position)
        frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}
    
void WebPage::startAutoscrollAtPosition(const WebCore::FloatPoint& positionInWindow)
{
    if (m_focusedElement && m_focusedElement->renderer())
        m_page->mainFrame().eventHandler().startSelectionAutoscroll(m_focusedElement->renderer(), positionInWindow);
    else {
        Frame& frame = m_page->focusController().focusedOrMainFrame();
        VisibleSelection selection = frame.selection().selection();
        if (selection.isRange()) {
            RefPtr<Range> range = frame.selection().toNormalizedRange();
            Node& node = range->startContainer();
            auto* renderer = node.renderer();
            if (renderer)
                m_page->mainFrame().eventHandler().startSelectionAutoscroll(renderer, positionInWindow);
        }
    }
}
    
void WebPage::cancelAutoscroll()
{
    m_page->mainFrame().eventHandler().cancelSelectionAutoscroll();
}

void WebPage::getRectsForGranularityWithSelectionOffset(uint32_t granularity, int32_t offset, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisibleSelection selection = m_storedSelectionForAccessibility.isNone() ? frame.selection().selection() : m_storedSelectionForAccessibility;
    VisiblePosition selectionStart = selection.visibleStart();

    if (selectionStart.isNull()) {
        send(Messages::WebPageProxy::SelectionRectsCallback({ }, callbackID));
        return;
    }

    auto position = visiblePositionForPositionWithOffset(selectionStart, offset);
    SelectionDirection direction = offset < 0 ? DirectionBackward : DirectionForward;

    auto range = enclosingTextUnitOfGranularity(position, static_cast<WebCore::TextGranularity>(granularity), direction);
    if (!range || range->collapsed()) {
        send(Messages::WebPageProxy::SelectionRectsCallback({ }, callbackID));
        return;
    }

    Vector<WebCore::SelectionRect> selectionRects;
    range->collectSelectionRectsWithoutUnionInteriorLines(selectionRects);
    convertSelectionRectsToRootView(frame.view(), selectionRects);
    send(Messages::WebPageProxy::SelectionRectsCallback(selectionRects, callbackID));
}

void WebPage::storeSelectionForAccessibility(bool shouldStore)
{
    if (!shouldStore)
        m_storedSelectionForAccessibility = VisibleSelection();
    else {
        Frame& frame = m_page->focusController().focusedOrMainFrame();
        m_storedSelectionForAccessibility = frame.selection().selection();
    }
}

static RefPtr<Range> rangeNearPositionMatchesText(const VisiblePosition& position, RefPtr<Range> originalRange, const String& matchText, RefPtr<Range> selectionRange)
{
    auto range = Range::create(selectionRange->ownerDocument(), selectionRange->startPosition(), position.deepEquivalent().parentAnchoredEquivalent());
    unsigned targetOffset = TextIterator::rangeLength(range.ptr(), true);
    return findClosestPlainText(*selectionRange.get(), matchText, { }, targetOffset);
}

void WebPage::getRectsAtSelectionOffsetWithText(int32_t offset, const String& text, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    uint32_t length = text.length();
    VisibleSelection selection = m_storedSelectionForAccessibility.isNone() ? frame.selection().selection() : m_storedSelectionForAccessibility;
    VisiblePosition selectionStart = selection.visibleStart();
    VisiblePosition selectionEnd = selection.visibleEnd();

    if (selectionStart.isNull() || selectionEnd.isNull()) {
        send(Messages::WebPageProxy::SelectionRectsCallback({ }, callbackID));
        return;
    }

    auto startPosition = visiblePositionForPositionWithOffset(selectionStart, offset);
    auto endPosition = visiblePositionForPositionWithOffset(startPosition, length);
    auto range = Range::create(*frame.document(), startPosition, endPosition);

    if (range->collapsed()) {
        send(Messages::WebPageProxy::SelectionRectsCallback({ }, callbackID));
        return;
    }

    String rangeText = plainTextReplacingNoBreakSpace(range.ptr(), TextIteratorDefaultBehavior, true);
    if (rangeText != text) {
        auto selectionRange = selection.toNormalizedRange();
        // Try to search for a range which is the closest to the position within the selection range that matches the passed in text.
        if (auto wordRange = rangeNearPositionMatchesText(startPosition, range.ptr(), text, selectionRange)) {
            if (!wordRange->collapsed())
                range = *wordRange;
        }
    }

    Vector<WebCore::SelectionRect> selectionRects;
    range->collectSelectionRectsWithoutUnionInteriorLines(selectionRects);
    convertSelectionRectsToRootView(frame.view(), selectionRects);
    send(Messages::WebPageProxy::SelectionRectsCallback(selectionRects, callbackID));
}

VisiblePosition WebPage::visiblePositionInFocusedNodeForPoint(const Frame& frame, const IntPoint& point, bool isInteractingWithFocusedElement)
{
    IntPoint adjustedPoint(frame.view()->rootViewToContents(point));
    IntPoint constrainedPoint = m_focusedElement && isInteractingWithFocusedElement ? constrainPoint(adjustedPoint, frame, *m_focusedElement) : adjustedPoint;
    return frame.visiblePositionForPoint(constrainedPoint);
}

void WebPage::selectPositionAtPoint(const WebCore::IntPoint& point, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);
    
    if (position.isNotNull())
        frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), position.affinity(), WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint& point, uint32_t granularity, uint32_t direction, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNotNull()) {
        position = positionOfNextBoundaryOfGranularity(position, static_cast<WebCore::TextGranularity>(granularity), static_cast<SelectionDirection>(direction));
        if (position.isNotNull())
            frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::moveSelectionAtBoundaryWithDirection(uint32_t granularity, uint32_t direction, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    
    if (!frame.selection().selection().isNone()) {
        bool isForward = (direction == DirectionForward || direction == DirectionRight);
        VisiblePosition position = (isForward) ? frame.selection().selection().visibleEnd() : frame.selection().selection().visibleStart();
        position = positionOfNextBoundaryOfGranularity(position, static_cast<WebCore::TextGranularity>(granularity), static_cast<SelectionDirection>(direction));
        if (position.isNotNull())
            frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), isForward? UPSTREAM : DOWNSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    }
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

RefPtr<Range> WebPage::rangeForGranularityAtPoint(Frame& frame, const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithFocusedElement)
{
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    RefPtr<Range> range;
    switch (static_cast<WebCore::TextGranularity>(granularity)) {
    case WordGranularity:
        range = wordRangeFromPosition(position);
        break;
    case SentenceGranularity:
        range = enclosingTextUnitOfGranularity(position, SentenceGranularity, DirectionForward);
        break;
    case ParagraphGranularity:
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        break;
    case DocumentGranularity:
        frame.selection().selectAll();
        break;
    default:
        break;
    }
    return range;
}

static inline bool rectIsTooBigForSelection(const IntRect& blockRect, const Frame& frame)
{
    const float factor = 0.97;
    return blockRect.height() > frame.view()->unobscuredContentRect().height() * factor;
}

void WebPage::selectTextWithGranularityAtPoint(const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> range = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithFocusedElement);
    if (!isInteractingWithFocusedElement) {
        m_blockSelectionDesiredSize.setWidth(blockSelectionStartWidth);
        m_blockSelectionDesiredSize.setHeight(blockSelectionStartHeight);
        m_currentBlockSelection = nullptr;
        auto* renderer = range ? range->startContainer().renderer() : nullptr;
        if (renderer && renderer->style().preserveNewline())
            m_blockRectForTextSelection = renderer->absoluteBoundingBoxRect(true);
        else {
            auto paragraphRange = enclosingTextUnitOfGranularity(visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement), ParagraphGranularity, DirectionForward);
            if (paragraphRange && !paragraphRange->collapsed())
                m_blockRectForTextSelection = selectionBoxForRange(paragraphRange.get());
        }
        
        if (rectIsTooBigForSelection(m_blockRectForTextSelection, frame))
            m_blockRectForTextSelection.setHeight(0);
    }

    if (range)
        frame.selection().setSelectedRange(range.get(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    m_initialSelection = range;
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::beginSelectionInDirection(uint32_t direction, CallbackID callbackID)
{
    m_selectionAnchor = (static_cast<SelectionDirection>(direction) == DirectionLeft) ? Start : End;
    send(Messages::WebPageProxy::UnsignedCallback(m_selectionAnchor == Start, callbackID));
}

void WebPage::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);
    RefPtr<Range> newRange = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithFocusedElement);
    
    if (position.isNull() || !m_initialSelection || !newRange) {
        send(Messages::WebPageProxy::UnsignedCallback(false, callbackID));
        return;
    }
    
    RefPtr<Range> range;
    VisiblePosition selectionStart = m_initialSelection->startPosition();
    VisiblePosition selectionEnd = m_initialSelection->endPosition();

    if (position > m_initialSelection->endPosition())
        selectionEnd = newRange->endPosition();
    else if (position < m_initialSelection->startPosition())
        selectionStart = newRange->startPosition();
    
    if (selectionStart.isNotNull() && selectionEnd.isNotNull())
        range = Range::create(*frame.document(), selectionStart, selectionEnd);
    
    if (range)
        frame.selection().setSelectedRange(range.get(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);
    
    send(Messages::WebPageProxy::UnsignedCallback(selectionStart == m_initialSelection->startPosition(), callbackID));
}

void WebPage::updateSelectionWithExtentPoint(const WebCore::IntPoint& point, bool isInteractingWithFocusedElement, CallbackID callbackID)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithFocusedElement);

    if (position.isNull()) {
        send(Messages::WebPageProxy::UnsignedCallback(false, callbackID));
        return;
    }

    RefPtr<Range> range;
    VisiblePosition selectionStart;
    VisiblePosition selectionEnd;
    
    if (m_selectionAnchor == Start) {
        selectionStart = frame.selection().selection().visibleStart();
        selectionEnd = position;

        if (position <= selectionStart) {
            selectionStart = selectionStart.previous();
            selectionEnd = frame.selection().selection().visibleEnd();
            m_selectionAnchor = End;
        }
    } else {
        selectionStart = position;
        selectionEnd = frame.selection().selection().visibleEnd();
        
        if (position >= selectionEnd) {
            selectionStart = frame.selection().selection().visibleStart();
            selectionEnd = selectionEnd.next();
            m_selectionAnchor = Start;
        }
    }
    
    if (selectionStart.isNotNull() && selectionEnd.isNotNull())
        range = Range::create(*frame.document(), selectionStart, selectionEnd);

    if (range)
        frame.selection().setSelectedRange(range.get(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes, UserTriggered);

    send(Messages::WebPageProxy::UnsignedCallback(m_selectionAnchor == Start, callbackID));
}

void WebPage::convertSelectionRectsToRootView(FrameView* view, Vector<SelectionRect>& selectionRects)
{
    for (size_t i = 0; i < selectionRects.size(); ++i) {
        SelectionRect& currentRect = selectionRects[i];
        currentRect.setRect(view->contentsToRootView(currentRect.rect()));
    }
}

void WebPage::requestDictationContext(CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition startPosition = frame.selection().selection().start();
    VisiblePosition endPosition = frame.selection().selection().end();
    const unsigned dictationContextWordCount = 5;

    String selectedText;
    if (frame.selection().isRange())
        selectedText = plainTextReplacingNoBreakSpace(frame.selection().selection().toNormalizedRange().get());

    String contextBefore;
    if (startPosition != startOfEditableContent(startPosition)) {
        VisiblePosition currentPosition = startPosition;
        VisiblePosition lastPosition = startPosition;
        for (unsigned i = 0; i < dictationContextWordCount; ++i) {
            currentPosition = startOfWord(positionOfNextBoundaryOfGranularity(lastPosition, WordGranularity, DirectionBackward));
            if (currentPosition.isNull())
                break;
            lastPosition = currentPosition;
        }
        if (lastPosition.isNotNull() && lastPosition != startPosition)
            contextBefore = plainTextReplacingNoBreakSpace(Range::create(*frame.document(), lastPosition, startPosition).ptr());
    }

    String contextAfter;
    if (endPosition != endOfEditableContent(endPosition)) {
        VisiblePosition currentPosition = endPosition;
        VisiblePosition lastPosition = endPosition;
        for (unsigned i = 0; i < dictationContextWordCount; ++i) {
            currentPosition = endOfWord(positionOfNextBoundaryOfGranularity(lastPosition, WordGranularity, DirectionForward));
            if (currentPosition.isNull())
                break;
            lastPosition = currentPosition;
        }
        if (lastPosition.isNotNull() && lastPosition != endPosition)
            contextAfter = plainTextReplacingNoBreakSpace(Range::create(*frame.document(), endPosition, lastPosition).ptr());
    }

    send(Messages::WebPageProxy::SelectionContextCallback(selectedText, contextBefore, contextAfter, callbackID));
}

void WebPage::replaceSelectedText(const String& oldText, const String& newText)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> wordRange = frame.selection().isCaret() ? wordRangeFromPosition(frame.selection().selection().start()) : frame.selection().toNormalizedRange();
    if (plainTextReplacingNoBreakSpace(wordRange.get()) != oldText)
        return;
    
    frame.editor().setIgnoreSelectionChanges(true);
    frame.selection().setSelectedRange(wordRange.get(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    frame.editor().insertText(newText, 0);
    frame.editor().setIgnoreSelectionChanges(false);
}

void WebPage::replaceDictatedText(const String& oldText, const String& newText)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone())
        return;
    
    if (frame.selection().isRange()) {
        frame.editor().deleteSelectionWithSmartDelete(false);
        return;
    }
    VisiblePosition position = frame.selection().selection().start();
    for (size_t i = 0; i < oldText.length(); ++i)
        position = position.previous();
    if (position.isNull())
        position = startOfDocument(static_cast<Node*>(frame.document()->documentElement()));
    auto range = Range::create(*frame.document(), position, frame.selection().selection().start());

    if (plainTextReplacingNoBreakSpace(range.ptr()) != oldText)
        return;

    // We don't want to notify the client that the selection has changed until we are done inserting the new text.
    frame.editor().setIgnoreSelectionChanges(true);
    frame.selection().setSelectedRange(range.ptr(), UPSTREAM, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    frame.editor().insertText(newText, 0);
    frame.editor().setIgnoreSelectionChanges(false);
}

void WebPage::requestAutocorrectionData(const String& textForAutocorrection, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaret()) {
        send(Messages::WebPageProxy::AutocorrectionDataCallback(Vector<FloatRect>(), String(), 0, 0, callbackID));
        return;
    }

    VisiblePosition position = frame.selection().selection().start();
    RefPtr<Range> range = wordRangeFromPosition(position);
    if (!range) {
        send(Messages::WebPageProxy::AutocorrectionDataCallback(Vector<FloatRect>(), String(), 0, 0, callbackID));
        return;
    }

    String textForRange = plainTextReplacingNoBreakSpace(range.get());
    const unsigned maxSearchAttempts = 5;
    for (size_t i = 0;  i < maxSearchAttempts && textForRange != textForAutocorrection; ++i)
    {
        position = range->startPosition().previous();
        if (position.isNull() || position == range->startPosition())
            break;
        range = Range::create(*frame.document(), wordRangeFromPosition(position)->startPosition(), range->endPosition());
        textForRange = plainTextReplacingNoBreakSpace(range.get());
    }

    Vector<SelectionRect> selectionRects;
    if (textForRange == textForAutocorrection)
        range->collectSelectionRects(selectionRects);

    Vector<FloatRect> rectsForText;
    rectsForText.grow(selectionRects.size());

    convertSelectionRectsToRootView(frame.view(), selectionRects);
    for (size_t i = 0; i < selectionRects.size(); i++)
        rectsForText[i] = selectionRects[i].rect();

    bool multipleFonts = false;
    CTFontRef font = nil;
    if (auto* coreFont = frame.editor().fontForSelection(multipleFonts))
        font = coreFont->getCTFont();

    CGFloat fontSize = CTFontGetSize(font);
    uint64_t fontTraits = CTFontGetSymbolicTraits(font);
    RetainPtr<NSString> fontName = adoptNS((NSString *)CTFontCopyFamilyName(font));
    send(Messages::WebPageProxy::AutocorrectionDataCallback(rectsForText, fontName.get(), fontSize, fontTraits, callbackID));
}

void WebPage::applyAutocorrection(const String& correction, const String& originalText, CallbackID callbackID)
{
    send(Messages::WebPageProxy::StringCallback(applyAutocorrectionInternal(correction, originalText) ? correction : String(), callbackID));
}

Seconds WebPage::eventThrottlingDelay() const
{
    auto behaviorOverride = m_page->eventThrottlingBehaviorOverride();
    if (behaviorOverride) {
        switch (behaviorOverride.value()) {
        case EventThrottlingBehavior::Responsive:
            return 0_s;
        case EventThrottlingBehavior::Unresponsive:
            return 1_s;
        }
    }

    if (m_isInStableState || m_estimatedLatency <= Seconds(1.0 / 60))
        return 0_s;

    return std::min(m_estimatedLatency * 2, 1_s);
}

void WebPage::syncApplyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(bool)>&& reply)
{
    reply(applyAutocorrectionInternal(correction, originalText));
}

bool WebPage::applyAutocorrectionInternal(const String& correction, const String& originalText)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaretOrRange())
        return false;

    RefPtr<Range> range;
    String textForRange;

    if (frame.selection().isCaret()) {
        VisiblePosition position = frame.selection().selection().start();
        range = wordRangeFromPosition(position);
        textForRange = plainTextReplacingNoBreakSpace(range.get());
        if (textForRange != originalText) {
            // Search for the original text before the selection caret.
            for (size_t i = 0; i < originalText.length(); ++i)
                position = position.previous();
            if (position.isNull())
                position = startOfDocument(static_cast<Node*>(frame.document()->documentElement()));
            range = Range::create(*frame.document(), position, frame.selection().selection().start());
            if (range)
                textForRange = (range) ? plainTextReplacingNoBreakSpace(range.get()) : emptyString();
            unsigned loopCount = 0;
            const unsigned maxPositionsAttempts = 10;
            while (textForRange.length() && textForRange.length() > originalText.length() && loopCount < maxPositionsAttempts) {
                position = position.next();
                if (position.isNotNull() && position >= frame.selection().selection().start())
                    range = nullptr;
                else
                    range = Range::create(*frame.document(), position, frame.selection().selection().start());
                textForRange = (range) ? plainTextReplacingNoBreakSpace(range.get()) : emptyString();
                loopCount++;
            }
        }
    } else {
        // Range selection.
        range = frame.selection().toNormalizedRange();
        if (!range)
            return false;

        textForRange = plainTextReplacingNoBreakSpace(range.get());
    }

    if (textForRange != originalText)
        return false;
    
    // Correctly determine affinity, using logic currently only present in VisiblePosition
    EAffinity affinity = DOWNSTREAM;
    if (range && range->collapsed())
        affinity = VisiblePosition(range->startPosition(), UPSTREAM).affinity();
    
    frame.selection().setSelectedRange(range.get(), affinity, WebCore::FrameSelection::ShouldCloseTyping::Yes);
    if (correction.length())
        frame.editor().insertText(correction, 0, originalText.isEmpty() ? TextEventInputKeyboard : TextEventInputAutocompletion);
    else
        frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    return true;
}

WebAutocorrectionContext WebPage::autocorrectionContext()
{
    String contextBefore;
    String markedText;
    String selectedText;
    String contextAfter;
    uint64_t location = NSNotFound;
    uint64_t length = 0;

    auto& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> range;
    VisiblePosition startPosition = frame.selection().selection().start();
    VisiblePosition endPosition = frame.selection().selection().end();
    const unsigned minContextWordCount = 3;
    const unsigned minContextLenght = 12;
    const unsigned maxContextLength = 30;

    if (frame.selection().isRange())
        selectedText = plainTextReplacingNoBreakSpace(frame.selection().selection().toNormalizedRange().get());

    if (auto compositionRange = frame.editor().compositionRange()) {
        range = Range::create(*frame.document(), compositionRange->startPosition(), startPosition);
        String markedTextBefore;
        if (range)
            markedTextBefore = plainTextReplacingNoBreakSpace(range.get());
        range = Range::create(*frame.document(), endPosition, compositionRange->endPosition());
        String markedTextAfter;
        if (range)
            markedTextAfter = plainTextReplacingNoBreakSpace(range.get());
        markedText = markedTextBefore + selectedText + markedTextAfter;
        if (!markedText.isEmpty()) {
            location = markedTextBefore.length();
            length = selectedText.length();
        }
    } else {
        if (startPosition != startOfEditableContent(startPosition)) {
            VisiblePosition currentPosition = startPosition;
            VisiblePosition previousPosition;
            unsigned totalContextLength = 0;
            for (unsigned i = 0; i < minContextWordCount; ++i) {
                if (contextBefore.length() >= minContextLenght)
                    break;
                previousPosition = startOfWord(positionOfNextBoundaryOfGranularity(currentPosition, WordGranularity, DirectionBackward));
                if (previousPosition.isNull())
                    break;
                String currentWord = plainTextReplacingNoBreakSpace(Range::create(*frame.document(), previousPosition, currentPosition).ptr());
                totalContextLength += currentWord.length();
                if (totalContextLength >= maxContextLength)
                    break;
                currentPosition = previousPosition;
            }
            if (currentPosition.isNotNull() && currentPosition != startPosition) {
                contextBefore = plainTextReplacingNoBreakSpace(Range::create(*frame.document(), currentPosition, startPosition).ptr());
                if (atBoundaryOfGranularity(currentPosition, ParagraphGranularity, DirectionBackward))
                    contextBefore = makeString("\n "_s, contextBefore);
            }
        }

        if (endPosition != endOfEditableContent(endPosition)) {
            VisiblePosition nextPosition;
            if (!atBoundaryOfGranularity(endPosition, WordGranularity, DirectionForward) && withinTextUnitOfGranularity(endPosition, WordGranularity, DirectionForward))
                nextPosition = positionOfNextBoundaryOfGranularity(endPosition, WordGranularity, DirectionForward);
            if (nextPosition.isNotNull())
                contextAfter = plainTextReplacingNoBreakSpace(Range::create(*frame.document(), endPosition, nextPosition).ptr());
        }
    }
    return { WTFMove(contextBefore), WTFMove(markedText), WTFMove(selectedText), WTFMove(contextAfter), location, length };
}

void WebPage::requestAutocorrectionContext(CallbackID callbackID)
{
    send(Messages::WebPageProxy::AutocorrectionContextCallback(autocorrectionContext(), callbackID));
}

void WebPage::autocorrectionContextSync(CompletionHandler<void(WebAutocorrectionContext&&)>&& completionHandler)
{
    completionHandler(autocorrectionContext());
}

static HTMLAnchorElement* containingLinkElement(Element* element)
{
    for (auto& currentElement : elementLineage(element)) {
        if (currentElement.isLink() && is<HTMLAnchorElement>(currentElement))
            return downcast<HTMLAnchorElement>(&currentElement);
    }
    return nullptr;
}

static inline bool isAssistableElement(Element& node)
{
    if (is<HTMLSelectElement>(node))
        return true;
    if (is<HTMLTextAreaElement>(node))
        return true;
    if (is<HTMLImageElement>(node) && downcast<HTMLImageElement>(node).hasEditableImageAttribute())
        return true;
    if (is<HTMLInputElement>(node)) {
        HTMLInputElement& inputElement = downcast<HTMLInputElement>(node);
        // FIXME: This laundry list of types is not a good way to factor this. Need a suitable function on HTMLInputElement itself.
#if ENABLE(INPUT_TYPE_COLOR)
        if (inputElement.isColorControl())
            return true;
#endif
        return inputElement.isTextField() || inputElement.isDateField() || inputElement.isDateTimeLocalField() || inputElement.isMonthField() || inputElement.isTimeField();
    }
    return node.isContentEditable();
}

void WebPage::getPositionInformation(const InteractionInformationRequest& request, CompletionHandler<void(InteractionInformationAtPosition&&)>&& reply)
{
    m_pendingSynchronousPositionInformationReply = WTFMove(reply);

    auto information = positionInformation(request);

    if (auto reply = WTFMove(m_pendingSynchronousPositionInformationReply))
        reply(WTFMove(information));
}

InteractionInformationAtPosition WebPage::positionInformation(const InteractionInformationRequest& request)
{
    InteractionInformationAtPosition info;
    info.request = request;

    FloatPoint adjustedPoint;
    Node* hitNode = m_page->mainFrame().nodeRespondingToClickEvents(request.point, adjustedPoint);

    info.nodeAtPositionIsFocusedElement = hitNode == m_focusedElement;
    if (m_focusedElement) {
        const Frame& frame = m_page->focusController().focusedOrMainFrame();
        if (frame.editor().hasComposition()) {
            const uint32_t kHitAreaWidth = 66;
            const uint32_t kHitAreaHeight = 66;
            FrameView& view = *frame.view();
            IntPoint adjustedPoint(view.rootViewToContents(request.point));
            IntPoint constrainedPoint = m_focusedElement ? constrainPoint(adjustedPoint, frame, *m_focusedElement) : adjustedPoint;
            VisiblePosition position = frame.visiblePositionForPoint(constrainedPoint);

            RefPtr<Range> compositionRange = frame.editor().compositionRange();
            if (position < compositionRange->startPosition())
                position = compositionRange->startPosition();
            else if (position > compositionRange->endPosition())
                position = compositionRange->endPosition();
            IntRect caretRect = view.contentsToRootView(position.absoluteCaretBounds());
            float deltaX = abs(caretRect.x() + (caretRect.width() / 2) - request.point.x());
            float deltaYFromTheTop = abs(caretRect.y() - request.point.y());
            float deltaYFromTheBottom = abs(caretRect.y() + caretRect.height() - request.point.y());

            info.isNearMarkedText = !(deltaX > kHitAreaWidth || deltaYFromTheTop > kHitAreaHeight || deltaYFromTheBottom > kHitAreaHeight);
        }
    }
    bool elementIsLinkOrImage = false;
    if (hitNode) {
        Element* element = is<Element>(*hitNode) ? downcast<Element>(hitNode) : nullptr;
        if (element) {
            info.isElement = true;
            info.idAttribute = element->getIdAttribute();
            Element* linkElement = nullptr;
            if (element->renderer() && element->renderer()->isRenderImage()) {
                elementIsLinkOrImage = true;
                linkElement = containingLinkElement(element);
            } else if (element->isLink()) {
                linkElement = element;
                elementIsLinkOrImage = true;
            }

            if (elementIsLinkOrImage) {
                if (linkElement) {
                    info.isLink = true;

                    if (request.includeSnapshot) {
                        // Ensure that the image contains at most 600K pixels, so that it is not too big.
                        if (RefPtr<WebImage> snapshot = snapshotNode(*element, SnapshotOptionsShareable, 600 * 1024))
                            info.image = &snapshot->bitmap();
                    }

                    if (request.includeLinkIndicator) {
                        auto linkRange = rangeOfContents(*linkElement);
                        float deviceScaleFactor = corePage()->deviceScaleFactor();
                        const float marginInPoints = 4;

                        auto textIndicator = TextIndicator::createWithRange(linkRange.get(), TextIndicatorOptionTightlyFitContent | TextIndicatorOptionRespectTextColor | TextIndicatorOptionPaintBackgrounds | TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges |
                            TextIndicatorOptionIncludeMarginIfRangeMatchesSelection, TextIndicatorPresentationTransition::None, FloatSize(marginInPoints * deviceScaleFactor, marginInPoints * deviceScaleFactor));

                        if (textIndicator)
                            info.linkIndicator = textIndicator->data();
                    }

#if ENABLE(DATA_DETECTION)
                    info.isDataDetectorLink = DataDetection::isDataDetectorLink(*element);
                    if (info.isDataDetectorLink) {
                        const int dataDetectionExtendedContextLength = 350;
                        info.dataDetectorIdentifier = DataDetection::dataDetectorIdentifier(*element);
                        info.dataDetectorResults = element->document().frame()->dataDetectionResults();
                        if (DataDetection::requiresExtendedContext(*element)) {
                            auto linkRange = Range::create(element->document());
                            linkRange->selectNodeContents(*element);
                            info.textBefore = plainTextReplacingNoBreakSpace(rangeExpandedByCharactersInDirectionAtWordBoundary(linkRange->startPosition(), dataDetectionExtendedContextLength, DirectionBackward).get(), TextIteratorDefaultBehavior, true);
                            info.textAfter = plainTextReplacingNoBreakSpace(rangeExpandedByCharactersInDirectionAtWordBoundary(linkRange->endPosition(), dataDetectionExtendedContextLength, DirectionForward).get(), TextIteratorDefaultBehavior, true);
                        }
                    }
#endif
                }
                if (element->renderer() && element->renderer()->isRenderImage()) {
                    info.isImage = true;
                    auto& renderImage = downcast<RenderImage>(*(element->renderer()));
                    if (renderImage.cachedImage() && !renderImage.cachedImage()->errorOccurred()) {
                        if (Image* image = renderImage.cachedImage()->imageForRenderer(&renderImage)) {
                            if (image->width() > 1 && image->height() > 1) {
                                info.imageURL = element->document().completeURL(renderImage.cachedImage()->url());
                                info.isAnimatedImage = image->isAnimated();

                                if (request.includeSnapshot) {
                                    FloatSize screenSizeInPixels = screenSize();
                                    screenSizeInPixels.scale(corePage()->deviceScaleFactor());
                                    FloatSize scaledSize = largestRectWithAspectRatioInsideRect(image->size().width() / image->size().height(), FloatRect(0, 0, screenSizeInPixels.width(), screenSizeInPixels.height())).size();
                                    FloatSize bitmapSize = scaledSize.width() < image->size().width() ? scaledSize : image->size();
                                    // FIXME: Only select ExtendedColor on images known to need wide gamut
                                    ShareableBitmap::Configuration bitmapConfiguration;
                                    bitmapConfiguration.colorSpace.cgColorSpace = screenColorSpace(m_page->mainFrame().view());
                                    if (auto sharedBitmap = ShareableBitmap::createShareable(IntSize(bitmapSize), bitmapConfiguration)) {
                                        auto graphicsContext = sharedBitmap->createGraphicsContext();
                                        graphicsContext->drawImage(*image, FloatRect(0, 0, bitmapSize.width(), bitmapSize.height()));
                                        info.image = sharedBitmap;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (linkElement)
                info.url = linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr)));
            info.title = element->attributeWithoutSynchronization(HTMLNames::titleAttr).string();
            if (linkElement && info.title.isEmpty())
                info.title = element->innerText();
            if (element->renderer())
                info.touchCalloutEnabled = element->renderer()->style().touchCalloutEnabled();

            if (RenderElement* renderer = element->renderer()) {
                if (renderer->isRenderImage())
                    info.bounds = downcast<RenderImage>(*renderer).absoluteContentQuad().enclosingBoundingBox();
                else
                    info.bounds = renderer->absoluteBoundingBoxRect();

                if (!renderer->document().frame()->isMainFrame()) {
                    FrameView *view = renderer->document().frame()->view();
                    info.bounds = view->contentsToRootView(info.bounds);
                }
            }
        }
    }

    if (!elementIsLinkOrImage) {
        HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(request.point, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent);
        hitNode = result.innerNode();
        // Hit test could return HTMLHtmlElement that has no renderer, if the body is smaller than the document.
        if (hitNode && hitNode->renderer()) {
            RenderObject* renderer = hitNode->renderer();
            m_page->focusController().setFocusedFrame(result.innerNodeFrame());
            info.bounds = renderer->absoluteBoundingBoxRect(true);
            // We don't want to select blocks that are larger than 97% of the visible area of the document.
            if (is<HTMLAttachmentElement>(*hitNode)) {
                info.isAttachment = true;
                const HTMLAttachmentElement& attachment = downcast<HTMLAttachmentElement>(*hitNode);
                info.title = attachment.attachmentTitle();
                if (attachment.file())
                    info.url = URL::fileURLWithFileSystemPath(downcast<HTMLAttachmentElement>(*hitNode).file()->path());
            } else {
                info.isSelectable = renderer->style().userSelect() != UserSelect::None;
                if (info.isSelectable && !hitNode->isTextNode())
                    info.isSelectable = !isAssistableElement(*downcast<Element>(hitNode)) && !rectIsTooBigForSelection(info.bounds, *result.innerNodeFrame());
            }
#if PLATFORM(IOSMAC)
            bool isInsideFixedPosition;
            VisiblePosition caretPosition(renderer->positionForPoint(request.point, nullptr));
            info.caretRect = caretPosition.absoluteCaretBounds(&isInsideFixedPosition);
#endif
        }
    }

    // Prevent the callout bar from showing when tapping on the datalist button.
#if ENABLE(DATALIST_ELEMENT)
    if (is<HTMLInputElement>(hitNode)) {
        const HTMLInputElement& input = downcast<HTMLInputElement>(*hitNode);
        if (input.list()) {
            HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(request.point, HitTestRequest::ReadOnly | HitTestRequest::Active);
            if (result.innerNode() == input.dataListButtonElement())
                info.preventTextInteraction = true;
        }
    }
#endif

#if ENABLE(DATA_INTERACTION)
    info.hasSelectionAtPosition = m_page->hasSelectionAtPosition(adjustedPoint);
#endif
    info.adjustedPointForNodeRespondingToClickEvents = adjustedPoint;

    return info;
}

void WebPage::requestPositionInformation(const InteractionInformationRequest& request)
{
    send(Messages::WebPageProxy::DidReceivePositionInformation(positionInformation(request)));
}

void WebPage::startInteractionWithElementAtPosition(const WebCore::IntPoint& point)
{
    FloatPoint adjustedPoint;
    m_interactionNode = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
}

void WebPage::stopInteraction()
{
    m_interactionNode = nullptr;
}

void WebPage::performActionOnElement(uint32_t action)
{
    if (!is<HTMLElement>(m_interactionNode.get()))
        return;

    HTMLElement& element = downcast<HTMLElement>(*m_interactionNode);
    if (!element.renderer())
        return;

    if (static_cast<SheetAction>(action) == SheetAction::Copy) {
        if (is<RenderImage>(*element.renderer())) {
            URL url;
            String title;
            if (auto* linkElement = containingLinkElement(&element)) {
                url = linkElement->href();
                title = linkElement->attributeWithoutSynchronization(HTMLNames::titleAttr);
                if (!title.length())
                    title = linkElement->textContent();
                title = stripLeadingAndTrailingHTMLSpaces(title);
            }
            m_interactionNode->document().frame()->editor().writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), element, url, title);
        } else if (element.isLink()) {
            m_interactionNode->document().frame()->editor().copyURL(element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(element.attributeWithoutSynchronization(HTMLNames::hrefAttr))), element.textContent());
        }
    } else if (static_cast<SheetAction>(action) == SheetAction::SaveImage) {
        if (!is<RenderImage>(*element.renderer()))
            return;
        CachedImage* cachedImage = downcast<RenderImage>(*element.renderer()).cachedImage();
        if (!cachedImage)
            return;
        RefPtr<SharedBuffer> buffer = cachedImage->resourceBuffer();
        if (!buffer)
            return;
        uint64_t bufferSize = buffer->size();
        RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(bufferSize);
        memcpy(sharedMemoryBuffer->data(), buffer->data(), bufferSize);
        SharedMemory::Handle handle;
        sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
        send(Messages::WebPageProxy::SaveImageToLibrary(handle, bufferSize));
    }
}

static inline Element* nextAssistableElement(Node* startNode, Page& page, bool isForward)
{
    if (!is<Element>(startNode))
        return nullptr;

    Element* nextElement = downcast<Element>(startNode);
    do {
        nextElement = isForward
            ? page.focusController().nextFocusableElement(*nextElement)
            : page.focusController().previousFocusableElement(*nextElement);
    } while (nextElement && !isAssistableElement(*nextElement));

    return nextElement;
}

void WebPage::focusNextFocusedElement(bool isForward, CallbackID callbackID)
{
    Element* nextElement = nextAssistableElement(m_focusedElement.get(), *m_page, isForward);
    m_userIsInteracting = true;
    if (nextElement)
        nextElement->focus();
    m_userIsInteracting = false;
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

static IntRect elementRectInRootViewCoordinates(const Node& node, const Frame& frame)
{
    auto* view = frame.view();
    if (!view)
        return { };

    auto* renderer = node.renderer();
    if (!renderer)
        return { };

    return view->contentsToRootView(renderer->absoluteBoundingBoxRect());
}

void WebPage::getFocusedElementInformation(FocusedElementInformation& information)
{
    layoutIfNeeded();

    information.lastInteractionLocation = m_lastInteractionLocation;

    if (auto* renderer = m_focusedElement->renderer()) {
        auto& elementFrame = m_page->focusController().focusedOrMainFrame();
        information.elementRect = elementRectInRootViewCoordinates(*m_focusedElement, elementFrame);
        information.nodeFontSize = renderer->style().fontDescription().computedSize();
        information.elementIsTransparentOrFullyClipped = renderer->isTransparentOrFullyClippedRespectingParentFrames();

        bool inFixed = false;
        renderer->localToContainerPoint(FloatPoint(), nullptr, UseTransforms, &inFixed);
        information.insideFixedPosition = inFixed;
        information.isRTL = renderer->style().direction() == TextDirection::RTL;

        auto* frameView = elementFrame.view();
        if (inFixed && elementFrame.isMainFrame() && !frameView->frame().settings().visualViewportEnabled()) {
            IntRect currentFixedPositionRect = frameView->customFixedPositionLayoutRect();
            frameView->setCustomFixedPositionLayoutRect(frameView->renderView()->documentRect());
            information.elementRect = frameView->contentsToRootView(renderer->absoluteBoundingBoxRect());
            frameView->setCustomFixedPositionLayoutRect(currentFixedPositionRect);
        }
    } else
        information.elementRect = IntRect();

    information.minimumScaleFactor = minimumPageScaleFactor();
    information.maximumScaleFactor = maximumPageScaleFactor();
    information.maximumScaleFactorIgnoringAlwaysScalable = maximumPageScaleFactorIgnoringAlwaysScalable();
    information.allowsUserScaling = m_viewportConfiguration.allowsUserScaling();
    information.allowsUserScalingIgnoringAlwaysScalable = m_viewportConfiguration.allowsUserScalingIgnoringAlwaysScalable();
    if (auto* nextElement = nextAssistableElement(m_focusedElement.get(), *m_page, true)) {
        if (auto* frame = nextElement->document().frame())
            information.nextNodeRect = elementRectInRootViewCoordinates(*nextElement, *frame);
        information.hasNextNode = true;
    }
    if (auto* previousElement = nextAssistableElement(m_focusedElement.get(), *m_page, false)) {
        if (auto* frame = previousElement->document().frame())
            information.previousNodeRect = elementRectInRootViewCoordinates(*previousElement, *frame);
        information.hasPreviousNode = true;
    }
    information.focusedElementIdentifier = m_currentFocusedElementIdentifier;

    if (is<LabelableElement>(*m_focusedElement)) {
        auto labels = downcast<LabelableElement>(*m_focusedElement).labels();
        Vector<Ref<Element>> associatedLabels;
        for (unsigned index = 0; index < labels->length(); ++index) {
            if (is<Element>(labels->item(index)) && labels->item(index)->renderer())
                associatedLabels.append(downcast<Element>(*labels->item(index)));
        }
        for (auto& labelElement : associatedLabels) {
            auto text = labelElement->innerText();
            if (!text.isEmpty()) {
                information.label = WTFMove(text);
                break;
            }
        }
    }

    information.title = m_focusedElement->title();
    information.ariaLabel = m_focusedElement->attributeWithoutSynchronization(HTMLNames::aria_labelAttr);

    if (is<HTMLSelectElement>(*m_focusedElement)) {
        HTMLSelectElement& element = downcast<HTMLSelectElement>(*m_focusedElement);
        information.elementType = InputType::Select;
        const Vector<HTMLElement*>& items = element.listItems();
        size_t count = items.size();
        int parentGroupID = 0;
        // The parent group ID indicates the group the option belongs to and is 0 for group elements.
        // If there are option elements in between groups, they are given it's own group identifier.
        // If a select does not have groups, all the option elements have group ID 0.
        for (size_t i = 0; i < count; ++i) {
            HTMLElement* item = items[i];
            if (is<HTMLOptionElement>(*item)) {
                HTMLOptionElement& option = downcast<HTMLOptionElement>(*item);
                information.selectOptions.append(OptionItem(option.text(), false, parentGroupID, option.selected(), option.hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr)));
            } else if (is<HTMLOptGroupElement>(*item)) {
                HTMLOptGroupElement& group = downcast<HTMLOptGroupElement>(*item);
                parentGroupID++;
                information.selectOptions.append(OptionItem(group.groupLabelText(), true, 0, false, group.hasAttributeWithoutSynchronization(WebCore::HTMLNames::disabledAttr)));
            }
        }
        information.selectedIndex = element.selectedIndex();
        information.isMultiSelect = element.multiple();
    } else if (is<HTMLTextAreaElement>(*m_focusedElement)) {
        HTMLTextAreaElement& element = downcast<HTMLTextAreaElement>(*m_focusedElement);
        information.autocapitalizeType = element.autocapitalizeType();
        information.isAutocorrect = element.shouldAutocorrect();
        information.elementType = InputType::TextArea;
        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
        information.autofillFieldName = WebCore::toAutofillFieldName(element.autofillData().fieldName);
        information.placeholder = element.attributeWithoutSynchronization(HTMLNames::placeholderAttr);
        information.inputMode = element.canonicalInputMode();
    } else if (is<HTMLInputElement>(*m_focusedElement)) {
        HTMLInputElement& element = downcast<HTMLInputElement>(*m_focusedElement);
        HTMLFormElement* form = element.form();
        if (form)
            information.formAction = form->getURLAttribute(WebCore::HTMLNames::actionAttr);
        if (auto autofillElements = WebCore::AutofillElements::computeAutofillElements(element)) {
            information.acceptsAutofilledLoginCredentials = true;
            information.isAutofillableUsernameField = autofillElements->username() == m_focusedElement;
        }
        information.representingPageURL = element.document().urlForBindings();
        information.autocapitalizeType = element.autocapitalizeType();
        information.isAutocorrect = element.shouldAutocorrect();
        information.placeholder = element.attributeWithoutSynchronization(HTMLNames::placeholderAttr);
        if (element.isPasswordField())
            information.elementType = InputType::Password;
        else if (element.isSearchField())
            information.elementType = InputType::Search;
        else if (element.isEmailField())
            information.elementType = InputType::Email;
        else if (element.isTelephoneField())
            information.elementType = InputType::Phone;
        else if (element.isNumberField())
            information.elementType = element.getAttribute("pattern") == "\\d*" || element.getAttribute("pattern") == "[0-9]*" ? InputType::NumberPad : InputType::Number;
        else if (element.isDateTimeLocalField())
            information.elementType = InputType::DateTimeLocal;
        else if (element.isDateField())
            information.elementType = InputType::Date;
        else if (element.isDateTimeField())
            information.elementType = InputType::DateTime;
        else if (element.isTimeField())
            information.elementType = InputType::Time;
        else if (element.isWeekField())
            information.elementType = InputType::Week;
        else if (element.isMonthField())
            information.elementType = InputType::Month;
        else if (element.isURLField())
            information.elementType = InputType::URL;
        else if (element.isText()) {
            const AtomicString& pattern = element.attributeWithoutSynchronization(HTMLNames::patternAttr);
            if (pattern == "\\d*" || pattern == "[0-9]*")
                information.elementType = InputType::NumberPad;
            else {
                information.elementType = InputType::Text;
                if (!information.formAction.isEmpty()
                    && (element.getNameAttribute().contains("search") || element.getIdAttribute().contains("search") || element.attributeWithoutSynchronization(HTMLNames::titleAttr).contains("search")))
                    information.elementType = InputType::Search;
            }
        }
#if ENABLE(INPUT_TYPE_COLOR)
        else if (element.isColorControl()) {
            information.elementType = InputType::Color;
#if ENABLE(DATALIST_ELEMENT)
            information.suggestedColors = element.suggestedColors();
#endif
        }
#endif

#if ENABLE(DATALIST_ELEMENT)
        information.hasSuggestions = !!element.list();
#endif
        information.inputMode = element.canonicalInputMode();
        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
        information.valueAsNumber = element.valueAsNumber();
        information.autofillFieldName = WebCore::toAutofillFieldName(element.autofillData().fieldName);
    } else if (is<HTMLImageElement>(*m_focusedElement) && downcast<HTMLImageElement>(*m_focusedElement).hasEditableImageAttribute()) {
        information.elementType = InputType::Drawing;
        information.embeddedViewID = downcast<HTMLImageElement>(*m_focusedElement).editableImageViewID();
    } else if (m_focusedElement->hasEditableStyle()) {
        information.elementType = InputType::ContentEditable;
        if (is<HTMLElement>(*m_focusedElement)) {
            auto& focusedElement = downcast<HTMLElement>(*m_focusedElement);
            information.isAutocorrect = focusedElement.shouldAutocorrect();
            information.autocapitalizeType = focusedElement.autocapitalizeType();
            information.inputMode = focusedElement.canonicalInputMode();
        } else {
            information.isAutocorrect = true;
            information.autocapitalizeType = AutocapitalizeTypeDefault;
        }
        information.isReadOnly = false;
    }
}

void WebPage::autofillLoginCredentials(const String& username, const String& password)
{
    if (is<HTMLInputElement>(m_focusedElement.get())) {
        if (auto autofillElements = AutofillElements::computeAutofillElements(downcast<HTMLInputElement>(*m_focusedElement)))
            autofillElements->autofill(username, password);
    }
}

// WebCore stores the page scale factor as float instead of double. When we get a scale from WebCore,
// we need to ignore differences that are within a small rounding error on floats.
static inline bool areEssentiallyEqualAsFloat(float a, float b)
{
    return WTF::areEssentiallyEqual(a, b);
}

void WebPage::setViewportConfigurationViewLayoutSize(const FloatSize& size, double scaleFactor, double minimumEffectiveDeviceWidth)
{
    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " setViewportConfigurationViewLayoutSize " << size << " scaleFactor " << scaleFactor << " minimumEffectiveDeviceWidth " << minimumEffectiveDeviceWidth);

    ZoomToInitialScale shouldZoomToInitialScale = ZoomToInitialScale::No;
    if (m_viewportConfiguration.layoutSizeScaleFactor() != scaleFactor && areEssentiallyEqualAsFloat(m_viewportConfiguration.initialScale(), pageScaleFactor()))
        shouldZoomToInitialScale = ZoomToInitialScale::Yes;

    if (m_viewportConfiguration.setViewLayoutSize(size, scaleFactor, minimumEffectiveDeviceWidth))
        viewportConfigurationChanged(shouldZoomToInitialScale);
}

void WebPage::setMaximumUnobscuredSize(const FloatSize& maximumUnobscuredSize)
{
    m_maximumUnobscuredSize = maximumUnobscuredSize;
    updateViewportSizeForCSSViewportUnits();
}

void WebPage::setDeviceOrientation(int32_t deviceOrientation)
{
    if (deviceOrientation == m_deviceOrientation)
        return;
    m_deviceOrientation = deviceOrientation;
    m_page->mainFrame().orientationChanged();
}

void WebPage::setOverrideViewportArguments(const Optional<WebCore::ViewportArguments>& arguments)
{
    if (auto* document = m_page->mainFrame().document())
        document->setOverrideViewportArguments(arguments);
}

void WebPage::resetTextAutosizing()
{
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document || !document->renderView())
            continue;
        document->renderView()->resetTextAutosizing();
    }
}

void WebPage::dynamicViewportSizeUpdate(const FloatSize& viewLayoutSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const WebCore::FloatRect& targetUnobscuredRectInScrollViewCoordinates, const WebCore::FloatBoxExtent& targetUnobscuredSafeAreaInsets, double targetScale, int32_t deviceOrientation, DynamicViewportSizeUpdateID dynamicViewportSizeUpdateID)
{
    SetForScope<bool> dynamicSizeUpdateGuard(m_inDynamicSizeUpdate, true);
    // FIXME: this does not handle the cases where the content would change the content size or scroll position from JavaScript.
    // To handle those cases, we would need to redo this computation on every change until the next visible content rect update.
    LOG_WITH_STREAM(VisibleRects, stream << "\nWebPage::dynamicViewportSizeUpdate - viewLayoutSize " << viewLayoutSize << " targetUnobscuredRect " << targetUnobscuredRect << " targetExposedContentRect " << targetExposedContentRect << " targetScale " << targetScale);

    FrameView& frameView = *m_page->mainFrame().view();
    IntSize oldContentSize = frameView.contentsSize();
    float oldPageScaleFactor = m_page->pageScaleFactor();

    m_dynamicSizeUpdateHistory.add(std::make_pair(oldContentSize, oldPageScaleFactor), frameView.scrollPosition());

    RefPtr<Node> oldNodeAtCenter;
    double visibleHorizontalFraction = 1;
    float relativeHorizontalPositionInNodeAtCenter = 0;
    float relativeVerticalPositionInNodeAtCenter = 0;
    {
        visibleHorizontalFraction = frameView.unobscuredContentSize().width() / oldContentSize.width();
        IntPoint unobscuredContentRectCenter = frameView.unobscuredContentRect().center();

        HitTestResult hitTestResult = HitTestResult(unobscuredContentRectCenter);

        if (RenderView* mainFrameRenderView = frameView.renderView())
            mainFrameRenderView->hitTest(HitTestRequest(), hitTestResult);

        if (Node* node = hitTestResult.innerNode()) {
            if (RenderObject* renderer = node->renderer()) {
                FrameView& containingView = *node->document().frame()->view();
                FloatRect boundingBox = containingView.contentsToRootView(renderer->absoluteBoundingBoxRect(true));
                relativeHorizontalPositionInNodeAtCenter = (unobscuredContentRectCenter.x() - boundingBox.x()) / boundingBox.width();
                relativeVerticalPositionInNodeAtCenter = (unobscuredContentRectCenter.y() - boundingBox.y()) / boundingBox.height();
                oldNodeAtCenter = node;
            }
        }
    }

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage::dynamicViewportSizeUpdate setting view layout size to " << viewLayoutSize);
    m_viewportConfiguration.setViewLayoutSize(viewLayoutSize);
    IntSize newLayoutSize = m_viewportConfiguration.layoutSize();

    if (setFixedLayoutSize(newLayoutSize))
        resetTextAutosizing();

    setMaximumUnobscuredSize(maximumUnobscuredSize);
    m_page->setUnobscuredSafeAreaInsets(targetUnobscuredSafeAreaInsets);

    frameView.updateLayoutAndStyleIfNeededRecursive();

    IntSize newContentSize = frameView.contentsSize();

    double scale = scaleAfterViewportWidthChange(targetScale, m_userHasChangedPageScaleFactor, m_viewportConfiguration, targetUnobscuredRectInScrollViewCoordinates.width(), newContentSize, oldContentSize, visibleHorizontalFraction);
    FloatRect newUnobscuredContentRect = targetUnobscuredRect;
    FloatRect newExposedContentRect = targetExposedContentRect;

    bool scaleChanged = !areEssentiallyEqualAsFloat(scale, targetScale);
    if (scaleChanged) {
        // The target scale the UI is using cannot be reached by the content. We need to compute new targets based
        // on the viewport constraint and report everything back to the UIProcess.

        // 1) Compute a new unobscured rect centered around the original one.
        double scaleDifference = targetScale / scale;
        double newUnobscuredRectWidth = targetUnobscuredRect.width() * scaleDifference;
        double newUnobscuredRectHeight = targetUnobscuredRect.height() * scaleDifference;
        double newUnobscuredRectX = targetUnobscuredRect.x() - (newUnobscuredRectWidth - targetUnobscuredRect.width()) / 2;
        double newUnobscuredRectY = targetUnobscuredRect.y() - (newUnobscuredRectHeight - targetUnobscuredRect.height()) / 2;
        newUnobscuredContentRect = FloatRect(newUnobscuredRectX, newUnobscuredRectY, newUnobscuredRectWidth, newUnobscuredRectHeight);

        // 2) Extend our new unobscuredRect by the obscured margins to get a new exposed rect.
        double obscuredTopMargin = (targetUnobscuredRect.y() - targetExposedContentRect.y()) * scaleDifference;
        double obscuredLeftMargin = (targetUnobscuredRect.x() - targetExposedContentRect.x()) * scaleDifference;
        double obscuredBottomMargin = (targetExposedContentRect.maxY() - targetUnobscuredRect.maxY()) * scaleDifference;
        double obscuredRightMargin = (targetExposedContentRect.maxX() - targetUnobscuredRect.maxX()) * scaleDifference;
        newExposedContentRect = FloatRect(newUnobscuredRectX - obscuredLeftMargin,
                                          newUnobscuredRectY - obscuredTopMargin,
                                          newUnobscuredRectWidth + obscuredLeftMargin + obscuredRightMargin,
                                          newUnobscuredRectHeight + obscuredTopMargin + obscuredBottomMargin);
    }

    if (oldContentSize != newContentSize || scaleChanged) {
        // Snap the new unobscured rect back into the content rect.
        newUnobscuredContentRect.setWidth(std::min(static_cast<float>(newContentSize.width()), newUnobscuredContentRect.width()));
        newUnobscuredContentRect.setHeight(std::min(static_cast<float>(newContentSize.height()), newUnobscuredContentRect.height()));

        bool positionWasRestoredFromSizeUpdateHistory = false;
        const auto& previousPosition = m_dynamicSizeUpdateHistory.find(std::pair<IntSize, float>(newContentSize, scale));
        if (previousPosition != m_dynamicSizeUpdateHistory.end()) {
            IntPoint restoredPosition = previousPosition->value;
            FloatPoint deltaPosition(restoredPosition.x() - newUnobscuredContentRect.x(), restoredPosition.y() - newUnobscuredContentRect.y());
            newUnobscuredContentRect.moveBy(deltaPosition);
            newExposedContentRect.moveBy(deltaPosition);
            positionWasRestoredFromSizeUpdateHistory = true;
        } else if (oldContentSize != newContentSize) {
            FloatPoint newRelativeContentCenter;

            if (RenderObject* renderer = oldNodeAtCenter ? oldNodeAtCenter->renderer() : nullptr) {
                FrameView& containingView = *oldNodeAtCenter->document().frame()->view();
                FloatRect newBoundingBox = containingView.contentsToRootView(renderer->absoluteBoundingBoxRect(true));
                newRelativeContentCenter = FloatPoint(newBoundingBox.x() + relativeHorizontalPositionInNodeAtCenter * newBoundingBox.width(), newBoundingBox.y() + relativeVerticalPositionInNodeAtCenter * newBoundingBox.height());
            } else
                newRelativeContentCenter = relativeCenterAfterContentSizeChange(targetUnobscuredRect, oldContentSize, newContentSize);

            FloatPoint newUnobscuredContentRectCenter = newUnobscuredContentRect.center();
            FloatPoint positionDelta(newRelativeContentCenter.x() - newUnobscuredContentRectCenter.x(), newRelativeContentCenter.y() - newUnobscuredContentRectCenter.y());
            newUnobscuredContentRect.moveBy(positionDelta);
            newExposedContentRect.moveBy(positionDelta);
        }

        // Make the top/bottom edges "sticky" within 1 pixel.
        if (!positionWasRestoredFromSizeUpdateHistory) {
            if (targetUnobscuredRect.maxY() > oldContentSize.height() - 1) {
                float bottomVerticalPosition = newContentSize.height() - newUnobscuredContentRect.height();
                newUnobscuredContentRect.setY(bottomVerticalPosition);
                newExposedContentRect.setY(bottomVerticalPosition);
            }
            if (targetUnobscuredRect.y() < 1) {
                newUnobscuredContentRect.setY(0);
                newExposedContentRect.setY(0);
            }

            bool likelyResponsiveDesignViewport = newLayoutSize.width() == viewLayoutSize.width() && areEssentiallyEqualAsFloat(scale, 1);
            bool contentBleedsOutsideLayoutWidth = newContentSize.width() > newLayoutSize.width();
            bool originalScrollPositionWasOnTheLeftEdge = targetUnobscuredRect.x() <= 0;
            if (likelyResponsiveDesignViewport && contentBleedsOutsideLayoutWidth && originalScrollPositionWasOnTheLeftEdge) {
                // This is a special heuristics for "responsive" design with odd layout. It is quite common for responsive design
                // to have content "bleeding" outside of the minimal layout width, usually from an image or table larger than expected.
                // In those cases, the design usually does not adapt to the new width and remain at the newLayoutSize except for the
                // large boxes.
                // It is worth revisiting this special case as web developers get better with responsive design.
                newExposedContentRect.setX(0);
                newUnobscuredContentRect.setX(0);
            }
        }

        float horizontalAdjustment = 0;
        if (newUnobscuredContentRect.maxX() > newContentSize.width())
            horizontalAdjustment -= newUnobscuredContentRect.maxX() - newContentSize.width();
        float verticalAdjustment = 0;
        if (newUnobscuredContentRect.maxY() > newContentSize.height())
            verticalAdjustment -= newUnobscuredContentRect.maxY() - newContentSize.height();
        if (newUnobscuredContentRect.x() < 0)
            horizontalAdjustment += - newUnobscuredContentRect.x();
        if (newUnobscuredContentRect.y() < 0)
            verticalAdjustment += - newUnobscuredContentRect.y();

        FloatPoint adjustmentDelta(horizontalAdjustment, verticalAdjustment);
        newUnobscuredContentRect.moveBy(adjustmentDelta);
        newExposedContentRect.moveBy(adjustmentDelta);
    }

    frameView.setScrollVelocity(0, 0, 0, MonotonicTime::now());

    IntPoint roundedUnobscuredContentRectPosition = roundedIntPoint(newUnobscuredContentRect.location());
    frameView.setUnobscuredContentSize(newUnobscuredContentRect.size());
    m_drawingArea->setExposedContentRect(newExposedContentRect);

    scalePage(scale, roundedUnobscuredContentRectPosition);

    frameView.updateLayoutAndStyleIfNeededRecursive();

    auto& settings = frameView.frame().settings();
    if (settings.visualViewportEnabled()) {
        LayoutRect documentRect = IntRect(frameView.scrollOrigin(), frameView.contentsSize());
        auto layoutViewportSize = FrameView::expandedLayoutViewportSize(frameView.baseLayoutViewportSize(), LayoutSize(documentRect.size()), settings.layoutViewportHeightExpansionFactor());
        LayoutRect layoutViewportRect = FrameView::computeUpdatedLayoutViewportRect(frameView.layoutViewportRect(), documentRect, LayoutSize(newUnobscuredContentRect.size()), LayoutRect(newUnobscuredContentRect), layoutViewportSize, frameView.minStableLayoutViewportOrigin(), frameView.maxStableLayoutViewportOrigin(), FrameView::LayoutViewportConstraint::ConstrainedToDocumentRect);
        frameView.setLayoutViewportOverrideRect(layoutViewportRect);
    } else {
        IntRect fixedPositionLayoutRect = enclosingIntRect(frameView.viewportConstrainedObjectsRect());
        frameView.setCustomFixedPositionLayoutRect(fixedPositionLayoutRect);
    }

    frameView.setCustomSizeForResizeEvent(expandedIntSize(targetUnobscuredRectInScrollViewCoordinates.size()));
    setDeviceOrientation(deviceOrientation);
    frameView.setScrollOffset(roundedUnobscuredContentRectPosition);

    m_drawingArea->scheduleCompositingLayerFlush();

    m_pendingDynamicViewportSizeUpdateID = dynamicViewportSizeUpdateID;
}

void WebPage::resetViewportDefaultConfiguration(WebFrame* frame, bool hasMobileDocType)
{
    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " resetViewportDefaultConfiguration");
    if (m_useTestingViewportConfiguration) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::testingParameters());
        return;
    }

    auto parametersForStandardFrame = [&] {
        if (m_page->settings().shouldIgnoreMetaViewport())
            return ViewportConfiguration::nativeWebpageParameters();

        return ViewportConfiguration::webpageParameters();
    };

    if (!frame) {
        m_viewportConfiguration.setDefaultConfiguration(parametersForStandardFrame());
        return;
    }

    if (hasMobileDocType) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::xhtmlMobileParameters());
        return;
    }

    Document* document = frame->coreFrame()->document();
    if (document->isImageDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::imageDocumentParameters());
    else if (document->isTextDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::textDocumentParameters());
    else
        m_viewportConfiguration.setDefaultConfiguration(parametersForStandardFrame());
}

void WebPage::viewportConfigurationChanged(ZoomToInitialScale zoomToInitialScale)
{
    if (setFixedLayoutSize(m_viewportConfiguration.layoutSize()))
        resetTextAutosizing();

    double initialScale = m_viewportConfiguration.initialScale();
    double scale;
    if (m_userHasChangedPageScaleFactor && zoomToInitialScale == ZoomToInitialScale::No)
        scale = std::max(std::min(pageScaleFactor(), m_viewportConfiguration.maximumScale()), m_viewportConfiguration.minimumScale());
    else
        scale = initialScale;

    LOG_WITH_STREAM(VisibleRects, stream << "WebPage " << m_pageID << " viewportConfigurationChanged - setting zoomedOutPageScaleFactor to " << m_viewportConfiguration.minimumScale() << " and scale to " << scale);

    m_page->setZoomedOutPageScaleFactor(m_viewportConfiguration.minimumScale());

    updateViewportSizeForCSSViewportUnits();

    FrameView& frameView = *mainFrameView();
    IntPoint scrollPosition = frameView.scrollPosition();
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        FloatSize minimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.viewLayoutSize();
        minimumLayoutSizeInScrollViewCoordinates.scale(1 / scale);
        IntSize minimumLayoutSizeInDocumentCoordinates = roundedIntSize(minimumLayoutSizeInScrollViewCoordinates);
        frameView.setUnobscuredContentSize(minimumLayoutSizeInDocumentCoordinates);
        frameView.setScrollVelocity(0, 0, 0, MonotonicTime::now());

        // FIXME: We could send down the obscured margins to find a better exposed rect and unobscured rect.
        // It is not a big deal at the moment because the tile coverage will always extend past the obscured bottom inset.
        if (!m_hasRestoredExposedContentRectAfterDidCommitLoad)
            m_drawingArea->setExposedContentRect(FloatRect(scrollPosition, minimumLayoutSizeInDocumentCoordinates));
    }
    scalePage(scale, scrollPosition);
    
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        // This takes scale into account, so do after the scale change.
        frameView.setCustomFixedPositionLayoutRect(enclosingIntRect(frameView.viewportConstrainedObjectsRect()));

        frameView.setCustomSizeForResizeEvent(expandedIntSize(m_viewportConfiguration.minimumLayoutSize()));
    }
}

void WebPage::updateViewportSizeForCSSViewportUnits()
{
    FloatSize largestUnobscuredSize = m_maximumUnobscuredSize;
    if (largestUnobscuredSize.isEmpty())
        largestUnobscuredSize = m_viewportConfiguration.viewLayoutSize();

    FrameView& frameView = *mainFrameView();
    largestUnobscuredSize.scale(1 / m_viewportConfiguration.initialScaleIgnoringContentSize());
    frameView.setViewportSizeForCSSViewportUnits(roundedIntSize(largestUnobscuredSize));
}

void WebPage::applicationWillResignActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillResignActiveNotification object:nil];
    if (m_page)
        m_page->applicationWillResignActive();
}

void WebPage::applicationDidEnterBackground(bool isSuspendedUnderLock)
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidEnterBackgroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": [NSNumber numberWithBool:isSuspendedUnderLock]}];

    m_isSuspendedUnderLock = isSuspendedUnderLock;
    freezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    if (m_page)
        m_page->applicationDidEnterBackground();
}

void WebPage::applicationDidFinishSnapshottingAfterEnteringBackground()
{
    markLayersVolatile();
}

void WebPage::applicationWillEnterForeground(bool isSuspendedUnderLock)
{
    m_isSuspendedUnderLock = false;
    cancelMarkLayersVolatile();
    unfreezeLayerTree(LayerTreeFreezeReason::BackgroundApplication);

    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillEnterForegroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": @(isSuspendedUnderLock)}];

    if (m_page)
        m_page->applicationWillEnterForeground();
}

void WebPage::applicationDidBecomeActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidBecomeActiveNotification object:nil];
    if (m_page)
        m_page->applicationDidBecomeActive();
}

static inline void adjustVelocityDataForBoundedScale(double& horizontalVelocity, double& verticalVelocity, double& scaleChangeRate, double exposedRectScale, double minimumScale, double maximumScale)
{
    if (scaleChangeRate) {
        horizontalVelocity = 0;
        verticalVelocity = 0;
    }

    if (exposedRectScale >= maximumScale || exposedRectScale <= minimumScale)
        scaleChangeRate = 0;
}

Optional<float> WebPage::scaleFromUIProcess(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo) const
{
    auto transactionIDForLastScaleFromUIProcess = visibleContentRectUpdateInfo.lastLayerTreeTransactionID();
    if (m_lastTransactionIDWithScaleChange > transactionIDForLastScaleFromUIProcess)
        return WTF::nullopt;

    float scaleFromUIProcess = visibleContentRectUpdateInfo.scale();
    float currentScale = m_page->pageScaleFactor();

    double scaleNoiseThreshold = 0.005;
    if (!m_isInStableState && fabs(scaleFromUIProcess - currentScale) < scaleNoiseThreshold) {
        // Tiny changes of scale during interactive zoom cause content to jump by one pixel, creating
        // visual noise. We filter those useless updates.
        scaleFromUIProcess = currentScale;
    }
    
    scaleFromUIProcess = std::min<float>(m_viewportConfiguration.maximumScale(), std::max<float>(m_viewportConfiguration.minimumScale(), scaleFromUIProcess));
    if (areEssentiallyEqualAsFloat(currentScale, scaleFromUIProcess))
        return WTF::nullopt;

    return scaleFromUIProcess;
}

static bool selectionIsInsideFixedPositionContainer(Frame& frame)
{
    auto& selection = frame.selection().selection();
    if (selection.isNone())
        return false;

    bool isInsideFixedPosition = false;
    if (selection.isCaret()) {
        frame.selection().absoluteCaretBounds(&isInsideFixedPosition);
        return isInsideFixedPosition;
    }

    selection.visibleStart().absoluteCaretBounds(&isInsideFixedPosition);
    if (isInsideFixedPosition)
        return true;

    selection.visibleEnd().absoluteCaretBounds(&isInsideFixedPosition);
    return isInsideFixedPosition;
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo, MonotonicTime oldestTimestamp)
{
    LOG_WITH_STREAM(VisibleRects, stream << "\nWebPage " << m_pageID << " updateVisibleContentRects " << visibleContentRectUpdateInfo);

    // Skip any VisibleContentRectUpdate that have been queued before DidCommitLoad suppresses the updates in the UIProcess.
    if (visibleContentRectUpdateInfo.lastLayerTreeTransactionID() < m_mainFrame->firstLayerTreeTransactionIDAfterDidCommitLoad() && !visibleContentRectUpdateInfo.isFirstUpdateForNewViewSize())
        return;

    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = true;
    m_isInStableState = visibleContentRectUpdateInfo.inStableState();

    auto scaleFromUIProcess = this->scaleFromUIProcess(visibleContentRectUpdateInfo);

    // Skip progressively redrawing tiles if pinch-zooming while the system is under memory pressure.
    if (scaleFromUIProcess && !m_isInStableState && MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    if (m_isInStableState)
        m_hasStablePageScaleFactor = true;
    else {
        if (!m_oldestNonStableUpdateVisibleContentRectsTimestamp)
            m_oldestNonStableUpdateVisibleContentRectsTimestamp = oldestTimestamp;
    }

    float scaleToUse = scaleFromUIProcess.valueOr(m_page->pageScaleFactor());
    FloatRect exposedContentRect = visibleContentRectUpdateInfo.exposedContentRect();
    FloatRect adjustedExposedContentRect = adjustExposedRectForNewScale(exposedContentRect, visibleContentRectUpdateInfo.scale(), scaleToUse);
    m_drawingArea->setExposedContentRect(adjustedExposedContentRect);

    IntPoint scrollPosition = roundedIntPoint(visibleContentRectUpdateInfo.unobscuredContentRect().location());

    bool hasSetPageScale = false;
    if (scaleFromUIProcess) {
        m_scaleWasSetByUIProcess = true;
        m_hasStablePageScaleFactor = m_isInStableState;

        m_dynamicSizeUpdateHistory.clear();

        m_page->setPageScaleFactor(scaleFromUIProcess.value(), scrollPosition, m_isInStableState);
        hasSetPageScale = true;
        send(Messages::WebPageProxy::PageScaleFactorDidChange(scaleFromUIProcess.value()));
    }
    
    if (!hasSetPageScale && m_isInStableState) {
        m_page->setPageScaleFactor(scaleToUse, scrollPosition, true);
        hasSetPageScale = true;
    }

    auto& frame = m_page->mainFrame();
    FrameView& frameView = *frame.view();
    if (scrollPosition != frameView.scrollPosition())
        m_dynamicSizeUpdateHistory.clear();

    if (m_viewportConfiguration.setCanIgnoreScalingConstraints(m_ignoreViewportScalingConstraints && visibleContentRectUpdateInfo.allowShrinkToFit()))
        viewportConfigurationChanged();

    frameView.setUnobscuredContentSize(visibleContentRectUpdateInfo.unobscuredContentRect().size());
    m_page->setContentInsets(visibleContentRectUpdateInfo.contentInsets());
    m_page->setObscuredInsets(visibleContentRectUpdateInfo.obscuredInsets());
    m_page->setUnobscuredSafeAreaInsets(visibleContentRectUpdateInfo.unobscuredSafeAreaInsets());
    m_page->setEnclosedInScrollableAncestorView(visibleContentRectUpdateInfo.enclosedInScrollableAncestorView());

    double horizontalVelocity = visibleContentRectUpdateInfo.horizontalVelocity();
    double verticalVelocity = visibleContentRectUpdateInfo.verticalVelocity();
    double scaleChangeRate = visibleContentRectUpdateInfo.scaleChangeRate();
    adjustVelocityDataForBoundedScale(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.scale(), m_viewportConfiguration.minimumScale(), m_viewportConfiguration.maximumScale());

    frameView.setScrollVelocity(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.timestamp());

    if (m_isInStableState) {
        if (frameView.frame().settings().visualViewportEnabled()) {
            if (visibleContentRectUpdateInfo.unobscuredContentRect() != visibleContentRectUpdateInfo.unobscuredContentRectRespectingInputViewBounds())
                frameView.setVisualViewportOverrideRect(LayoutRect(visibleContentRectUpdateInfo.unobscuredContentRectRespectingInputViewBounds()));
            else
                frameView.setVisualViewportOverrideRect(WTF::nullopt);

            LOG_WITH_STREAM(VisibleRects, stream << "WebPage::updateVisibleContentRects - setLayoutViewportOverrideRect " << visibleContentRectUpdateInfo.customFixedPositionRect());
            frameView.setLayoutViewportOverrideRect(LayoutRect(visibleContentRectUpdateInfo.customFixedPositionRect()));
            if (selectionIsInsideFixedPositionContainer(frame)) {
                // Ensure that the next layer tree commit contains up-to-date caret/selection rects.
                frameView.frame().selection().setCaretRectNeedsUpdate();
                sendPartialEditorStateAndSchedulePostLayoutUpdate();
            }

            frameView.didUpdateViewportOverrideRects();
        } else
            frameView.setCustomFixedPositionLayoutRect(enclosingIntRect(visibleContentRectUpdateInfo.customFixedPositionRect()));
    }

    if (!visibleContentRectUpdateInfo.isChangingObscuredInsetsInteractively())
        frameView.setCustomSizeForResizeEvent(expandedIntSize(visibleContentRectUpdateInfo.unobscuredRectInScrollViewCoordinates().size()));

    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator()) {
        ViewportRectStability viewportStability = ViewportRectStability::Stable;
        ScrollingLayerPositionAction layerAction = ScrollingLayerPositionAction::Sync;
        
        if (visibleContentRectUpdateInfo.isChangingObscuredInsetsInteractively()) {
            viewportStability = ViewportRectStability::ChangingObscuredInsetsInteractively;
            layerAction = ScrollingLayerPositionAction::SetApproximate;
        } else if (!m_isInStableState) {
            viewportStability = ViewportRectStability::Unstable;
            layerAction = ScrollingLayerPositionAction::SetApproximate;
        }
        scrollingCoordinator->reconcileScrollingState(frameView, scrollPosition, visibleContentRectUpdateInfo.customFixedPositionRect(), false, viewportStability, layerAction);
    }
}

void WebPage::willStartUserTriggeredZooming()
{
    m_page->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::webViewKey(), DiagnosticLoggingKeys::userZoomActionKey(), ShouldSample::No);
    m_userHasChangedPageScaleFactor = true;
}

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPage::dispatchAsynchronousTouchEvents(const Vector<WebTouchEvent, 1>& queue)
{
    bool ignored;
    for (const WebTouchEvent& event : queue)
        dispatchTouchEvent(event, ignored);
}
#endif

void WebPage::computePagesForPrintingAndDrawToPDF(uint64_t frameID, const PrintInfo& printInfo, CallbackID callbackID, Messages::WebPage::ComputePagesForPrintingAndDrawToPDF::DelayedReply&& reply)
{
    if (printInfo.snapshotFirstPage) {
        reply(1);
        IntSize snapshotSize { FloatSize { printInfo.availablePaperWidth, printInfo.availablePaperHeight } };
        IntRect snapshotRect { {0, 0}, snapshotSize };
        auto pdfData = pdfSnapshotAtSize(snapshotRect, snapshotSize, 0);
        send(Messages::WebPageProxy::DrawToPDFCallback(IPC::DataReference(CFDataGetBytePtr(pdfData.get()), CFDataGetLength(pdfData.get())), callbackID));
        return;
    }

    Vector<WebCore::IntRect> pageRects;
    double totalScaleFactor;
    computePagesForPrintingImpl(frameID, printInfo, pageRects, totalScaleFactor);

    ASSERT(pageRects.size() >= 1);
    std::size_t pageCount = pageRects.size();
    ASSERT(pageCount <= std::numeric_limits<uint32_t>::max());
    reply(pageCount);

    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, 0, pageCount, pdfPageData);
    send(Messages::WebPageProxy::DrawToPDFCallback(IPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get())), callbackID));

    endPrinting();
}

void WebPage::contentSizeCategoryDidChange(const String& contentSizeCategory)
{
    RenderThemeIOS::setContentSizeCategory(contentSizeCategory);
    Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

String WebPage::platformUserAgent(const URL&) const
{
    return String();
}

void WebPage::hardwareKeyboardAvailabilityChanged()
{
    if (auto* focusedFrame = m_page->focusController().focusedFrame())
        focusedFrame->eventHandler().capsLockStateMayHaveChanged();
}

void WebPage::updateStringForFind(const String& findString)
{
    send(Messages::WebPageProxy::UpdateStringForFind(findString));
}

#if USE(QUICK_LOOK)
void WebPage::didReceivePasswordForQuickLookDocument(const String& password)
{
    WebPreviewLoaderClient::didReceivePassword(password, m_pageID);
}
#endif

bool WebPage::platformPrefersTextLegibilityBasedZoomScaling() const
{
#if PLATFORM(WATCHOS)
    return true;
#else
    return false;
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
