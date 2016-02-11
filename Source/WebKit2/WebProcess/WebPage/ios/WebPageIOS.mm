/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "AccessibilityIOS.h"
#import "AssistedNodeInformation.h"
#import "DataReference.h"
#import "DrawingArea.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "GestureTypes.h"
#import "InteractionInformationAtPosition.h"
#import "PluginView.h"
#import "RemoteLayerTreeDrawingArea.h"
#import "UserData.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import "WebChromeClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebFrame.h"
#import "WebImage.h"
#import "WebKitSystemInterface.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <CoreText/CTFont.h>
#import <WebCore/Chrome.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DiagnosticLoggingClient.h>
#import <WebCore/DiagnosticLoggingKeys.h>
#import <WebCore/Element.h>
#import <WebCore/ElementAncestorIterator.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoaderClient.h>
#import <WebCore/FrameView.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HTMLElementTypeHelpers.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MainFrame.h>
#import <WebCore/MediaSessionManagerIOS.h>
#import <WebCore/MemoryPressureHandler.h>
#import <WebCore/Node.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/RenderBlock.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderThemeIOS.h>
#import <WebCore/RenderView.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/StyleProperties.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WKContentObservation.h>
#import <WebCore/WebEvent.h>
#import <wtf/MathExtras.h>
#import <wtf/TemporaryChange.h>

using namespace WebCore;

namespace WebKit {

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
    
void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
    notImplemented();
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

    // We only set the remaining EditorState entries if the layout is done. To compute these
    // entries, we need the layout to be done and we don't want to trigger a synchronous
    // layout as this would be bad for performance. If we have a composition, we send everything
    // right away as the UIProcess needs the caretRects ASAP for marked text.
    if (shouldIncludePostLayoutData == IncludePostLayoutDataHint::No && !frame.editor().hasComposition()) {
        result.isMissingPostLayoutData = true;
        return;
    }

    auto& postLayoutData = result.postLayoutData();
    FrameView* view = frame.view();
    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isCaret()) {
        postLayoutData.caretRectAtStart = view->contentsToRootView(frame.selection().absoluteCaretBounds());
        postLayoutData.caretRectAtEnd = postLayoutData.caretRectAtStart;
        // FIXME: The following check should take into account writing direction.
        postLayoutData.isReplaceAllowed = result.isContentEditable && atBoundaryOfGranularity(selection.start(), WordGranularity, DirectionForward);
        postLayoutData.wordAtSelection = plainTextReplacingNoBreakSpace(wordRangeFromPosition(selection.start()).get());
        if (selection.isContentEditable()) {
            charactersAroundPosition(selection.start(), postLayoutData.characterAfterSelection, postLayoutData.characterBeforeSelection, postLayoutData.twoCharacterBeforeSelection);
            Node* root = selection.rootEditableElement();
            postLayoutData.hasContent = root && root->hasChildNodes() && !isEndOfEditableOrNonEditableContent(firstPositionInNode(root));
        }
    } else if (selection.isRange()) {
        postLayoutData.caretRectAtStart = view->contentsToRootView(VisiblePosition(selection.start()).absoluteCaretBounds());
        postLayoutData.caretRectAtEnd = view->contentsToRootView(VisiblePosition(selection.end()).absoluteCaretBounds());
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
        postLayoutData.isReplaceAllowed = result.isContentEditable && !result.isInPasswordField && !selectedText.containsOnlyWhitespace();
    }
    if (!selection.isNone()) {
        if (m_assistedNode && m_assistedNode->renderer())
            postLayoutData.selectionClipRect = view->contentsToRootView(m_assistedNode->renderer()->absoluteBoundingBoxRect());
        
        Node* nodeToRemove;
        if (RenderStyle* style = Editor::styleForSelectionStart(&frame, nodeToRemove)) {
            CTFontRef font = style->fontCascade().primaryFont().getCTFont();
            CTFontSymbolicTraits traits = font ? CTFontGetSymbolicTraits(font) : 0;
            
            if (traits & kCTFontTraitBold)
                postLayoutData.typingAttributes |= AttributeBold;
            if (traits & kCTFontTraitItalic)
                postLayoutData.typingAttributes |= AttributeItalics;
            
            RefPtr<EditingStyle> typingStyle = frame.selection().typingStyle();
            if (typingStyle && typingStyle->style()) {
                String value = typingStyle->style()->getPropertyValue(CSSPropertyWebkitTextDecorationsInEffect);
                if (value.contains("underline"))
                    postLayoutData.typingAttributes |= AttributeUnderline;
            } else {
                if (style->textDecorationsInEffect() & TextDecorationUnderline)
                    postLayoutData.typingAttributes |= AttributeUnderline;
            }
            
            if (nodeToRemove)
                nodeToRemove->remove(ASSERT_NO_EXCEPTION);
        }
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

void WebPage::viewportPropertiesDidChange(const ViewportArguments& viewportArguments)
{
    if (m_viewportConfiguration.setViewportArguments(viewportArguments))
        viewportConfigurationChanged();
}

void WebPage::didReceiveMobileDocType(bool isMobileDoctype)
{
    if (isMobileDoctype)
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::xhtmlMobileParameters());
    else
        resetViewportDefaultConfiguration(m_mainFrame.get());
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
    double overscaledWidth = exposedRect.width();
    double missingHorizonalMargin = exposedRect.width() * exposedRectScale / newScale - overscaledWidth;

    double overscaledHeight = exposedRect.height();
    double missingVerticalMargin = exposedRect.height() * exposedRectScale / newScale - overscaledHeight;

    return FloatRect(exposedRect.x() - missingHorizonalMargin / 2, exposedRect.y() - missingVerticalMargin / 2, exposedRect.width() + missingHorizonalMargin, exposedRect.height() + missingVerticalMargin);
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

    FloatSize currentMinimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.minimumLayoutSize();
    if (historyItem.minimumLayoutSizeInScrollViewCoordinates() == currentMinimumLayoutSizeInScrollViewCoordinates) {
        float boundedScale = std::min<float>(m_viewportConfiguration.maximumScale(), std::max<float>(m_viewportConfiguration.minimumScale(), historyItem.pageScaleFactor()));
        scalePage(boundedScale, IntPoint());

        m_drawingArea->setExposedContentRect(historyItem.exposedContentRect());

        send(Messages::WebPageProxy::RestorePageState(historyItem.exposedContentRect(), boundedScale));
    } else {
        IntSize oldContentSize = historyItem.contentSize();
        FrameView& frameView = *m_page->mainFrame().view();
        IntSize newContentSize = frameView.contentsSize();
        double visibleHorizontalFraction = static_cast<float>(historyItem.unobscuredContentRect().width()) / oldContentSize.width();

        double newScale = scaleAfterViewportWidthChange(historyItem.pageScaleFactor(), !historyItem.scaleIsInitial(), m_viewportConfiguration, currentMinimumLayoutSizeInScrollViewCoordinates.width(), newContentSize, oldContentSize, visibleHorizontalFraction);

        FloatPoint newCenter;
        if (!oldContentSize.isEmpty() && !newContentSize.isEmpty() && newContentSize != oldContentSize)
            newCenter = relativeCenterAfterContentSizeChange(historyItem.unobscuredContentRect(), oldContentSize, newContentSize);
        else
            newCenter = FloatRect(historyItem.unobscuredContentRect()).center();

        FloatSize unobscuredRectAtNewScale = frameView.customSizeForResizeEvent();
        unobscuredRectAtNewScale.scale(1 / newScale, 1 / newScale);

        FloatRect oldExposedRect = frameView.exposedContentRect();
        FloatRect adjustedExposedRect = adjustExposedRectForNewScale(oldExposedRect, m_page->pageScaleFactor(), newScale);

        FloatPoint oldCenter = adjustedExposedRect.center();
        adjustedExposedRect.move(newCenter - oldCenter);

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

bool WebPage::allowsUserScaling() const
{
    return m_viewportConfiguration.allowsUserScaling();
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (!platformEvent)
        return false;

    // FIXME: Interpret the event immediately upon receiving it in UI process, without sending to WebProcess first.
    bool eventWasHandled = false;
    bool sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(editorState(), platformEvent->type() == PlatformKeyboardEvent::Char),
                                                                               Messages::WebPageProxy::InterpretKeyEvent::Reply(eventWasHandled), m_pageID);
    if (!sendResult)
        return false;

    return eventWasHandled;
}

void WebPage::sendComplexTextInputToPlugin(uint64_t, const String&)
{
    notImplemented();
}

void WebPage::performDictionaryLookupAtLocation(const FloatPoint&)
{
    notImplemented();
}

void WebPage::performDictionaryLookupForSelection(Frame*, const VisibleSelection&, TextIndicatorPresentationTransition)
{
    notImplemented();
}

void WebPage::performDictionaryLookupForRange(Frame*, Range&, NSDictionary *, TextIndicatorPresentationTransition)
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

void WebPage::getLookupContextAtPoint(const WebCore::IntPoint point, uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = frame.visiblePositionForPoint(point);
    String resultString;
    if (!position.isNull()) {
        // As context, we are going to use 250 characters of text before and after the point.
        RefPtr<Range> fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
        if (fullCharacterRange)
            resultString = plainText(fullCharacterRange.get());
    }
    send(Messages::WebPageProxy::StringCallback(resultString, callbackID));
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
    if (!m_page)
        return 0;
    
    if (PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame()))
        return pluginView->accessibilityObject();
    
    return 0;
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

bool WebPage::platformHasLocalDataForURL(const WebCore::URL&)
{
    notImplemented();
    return false;
}

String WebPage::cachedSuggestedFilenameForURL(const URL&)
{
    notImplemented();
    return String();
}

String WebPage::cachedResponseMIMETypeForURL(const URL&)
{
    notImplemented();
    return String();
}

PassRefPtr<SharedBuffer> WebPage::cachedResponseDataForURL(const URL&)
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
    if (!frame.editor().ignoreCompositionSelectionChange() && (frame.editor().hasComposition() || !frame.selection().selection().isNone()))
        didChangeSelection();
}

void WebPage::handleSyntheticClick(Node* nodeRespondingToClick, const WebCore::FloatPoint& location)
{
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    Frame& mainframe = m_page->mainFrame();

    WKBeginObservingContentChanges(true);

    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, 0, WebCore::ForceAtClick));
    mainframe.document()->updateStyleIfNeeded();

    WKStopObservingContentChanges();

    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = FloatPoint();

    switch (WKObservedContentChange()) {
    case WKContentVisibilityChange:
        // The move event caused new contents to appear. Don't send the click event.
        return;
    case WKContentIndeterminateChange:
        // Wait for callback to completePendingSyntheticClickForContentChangeObserver() to decide whether to send the click event.
        m_pendingSyntheticClickNode = nodeRespondingToClick;
        m_pendingSyntheticClickLocation = location;
        return;
    case WKContentNoChange:
        completeSyntheticClick(nodeRespondingToClick, location);
        return;
    }
    ASSERT_NOT_REACHED();
}

void WebPage::completePendingSyntheticClickForContentChangeObserver()
{
    if (!m_pendingSyntheticClickNode)
        return;
    // Only dispatch the click if the document didn't get changed by any timers started by the move event.
    if (WKObservedContentChange() == WKContentNoChange)
        completeSyntheticClick(m_pendingSyntheticClickNode.get(), m_pendingSyntheticClickLocation);

    m_pendingSyntheticClickNode = nullptr;
    m_pendingSyntheticClickLocation = FloatPoint();
}

void WebPage::completeSyntheticClick(Node* nodeRespondingToClick, const WebCore::FloatPoint& location)
{
    IntPoint roundedAdjustedPoint = roundedIntPoint(location);
    Frame& mainframe = m_page->mainFrame();

    RefPtr<Frame> oldFocusedFrame = m_page->focusController().focusedFrame();
    RefPtr<Element> oldFocusedElement = oldFocusedFrame ? oldFocusedFrame->document()->focusedElement() : nullptr;
    m_userIsInteracting = true;

    bool tapWasHandled = false;
    m_lastInteractionLocation = roundedAdjustedPoint;
    tapWasHandled |= mainframe.eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, 0, WebCore::ForceAtClick));
    tapWasHandled |= mainframe.eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MouseReleased, 1, false, false, false, false, 0, WebCore::ForceAtClick));

    RefPtr<Frame> newFocusedFrame = m_page->focusController().focusedFrame();
    RefPtr<Element> newFocusedElement = newFocusedFrame ? newFocusedFrame->document()->focusedElement() : nullptr;

    // If the focus has not changed, we need to notify the client anyway, since it might be
    // necessary to start assisting the node.
    // If the node has been focused by JavaScript without user interaction, the
    // keyboard is not on screen.
    if (newFocusedElement && newFocusedElement == oldFocusedElement)
        elementDidFocus(newFocusedElement.get());

    m_userIsInteracting = false;

    if (!tapWasHandled || !nodeRespondingToClick || !nodeRespondingToClick->isElementNode())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(location)));
}

void WebPage::handleTap(const IntPoint& point, uint64_t lastLayerTreeTransactionId)
{
    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad())
        send(Messages::WebPageProxy::DidNotHandleTapAsClick(roundedIntPoint(m_potentialTapLocation)));
    else
        handleSyntheticClick(nodeRespondingToClick, adjustedPoint);
}

void WebPage::sendTapHighlightForNodeIfNecessary(uint64_t requestID, Node* node)
{
#if ENABLE(TOUCH_EVENTS)
    if (!node)
        return;

    if (is<Element>(*node)) {
        ASSERT(m_page);
        m_page->mainFrame().loader().client().prefetchDNS(downcast<Element>(*node).absoluteLinkURL().host());
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

        send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, highlightColor, quads, roundedIntSize(borderRadii.topLeft()), roundedIntSize(borderRadii.topRight()), roundedIntSize(borderRadii.bottomLeft()), roundedIntSize(borderRadii.bottomRight())));
    }
#else
    UNUSED_PARAM(requestID);
    UNUSED_PARAM(node);
#endif
}

void WebPage::potentialTapAtPosition(uint64_t requestID, const WebCore::FloatPoint& position)
{
    m_potentialTapNode = m_page->mainFrame().nodeRespondingToClickEvents(position, m_potentialTapLocation);
    sendTapHighlightForNodeIfNecessary(requestID, m_potentialTapNode.get());
#if ENABLE(TOUCH_EVENTS)
    if (m_potentialTapNode && !m_potentialTapNode->allowsDoubleTapGesture())
        send(Messages::WebPageProxy::DisableDoubleTapGesturesDuringTapIfNecessary(requestID));
#endif
}

void WebPage::commitPotentialTap(uint64_t lastLayerTreeTransactionId)
{
    if (!m_potentialTapNode || !m_potentialTapNode->renderer()) {
        commitPotentialTapFailed();
        return;
    }

    FloatPoint adjustedPoint;
    Node* nodeRespondingToClick = m_page->mainFrame().nodeRespondingToClickEvents(m_potentialTapLocation, adjustedPoint);
    Frame* frameRespondingToClick = nodeRespondingToClick ? nodeRespondingToClick->document().frame() : nullptr;

    if (!frameRespondingToClick || lastLayerTreeTransactionId < WebFrame::fromCoreFrame(*frameRespondingToClick)->firstLayerTreeTransactionIDAfterDidCommitLoad()) {
        commitPotentialTapFailed();
        return;
    }

    if (m_potentialTapNode == nodeRespondingToClick) {
        if (is<Element>(*nodeRespondingToClick) && DataDetection::shouldCancelDefaultAction(&downcast<Element>(*nodeRespondingToClick))) {
            requestPositionInformation(roundedIntPoint(m_potentialTapLocation));
            commitPotentialTapFailed();
        } else
            handleSyntheticClick(nodeRespondingToClick, adjustedPoint);
    } else
        commitPotentialTapFailed();

    m_potentialTapNode = nullptr;
    m_potentialTapLocation = FloatPoint();
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

    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(adjustedPoint, adjustedPoint, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, 0, 0));
    mainframe.document()->updateStyleIfNeeded();
}

void WebPage::inspectorNodeSearchEndedAtPosition(const FloatPoint& position)
{
    if (Node* node = m_page->mainFrame().deepestNodeAtLocation(position))
        node->inspect();
}

void WebPage::blurAssistedNode()
{
    if (is<Element>(m_assistedNode.get()))
        downcast<Element>(*m_assistedNode).blur();
}

void WebPage::setAssistedNodeValue(const String& value)
{
    if (is<HTMLInputElement>(m_assistedNode.get())) {
        HTMLInputElement& element = downcast<HTMLInputElement>(*m_assistedNode);
        element.setValue(value, DispatchInputAndChangeEvent);
    }
    // FIXME: should also handle the case of HTMLSelectElement.
}

void WebPage::setAssistedNodeValueAsNumber(double value)
{
    if (is<HTMLInputElement>(m_assistedNode.get())) {
        HTMLInputElement& element = downcast<HTMLInputElement>(*m_assistedNode);
        element.setValueAsNumber(value, ASSERT_NO_EXCEPTION, DispatchInputAndChangeEvent);
    }
}

void WebPage::setAssistedNodeSelectedIndex(uint32_t index, bool allowMultipleSelection)
{
    if (!is<HTMLSelectElement>(m_assistedNode.get()))
        return;
    HTMLSelectElement& select = downcast<HTMLSelectElement>(*m_assistedNode);
    select.optionSelectedByUser(index, true, allowMultipleSelection);
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

static FloatQuad innerFrameQuad(const Frame& frame, const Node& assistedNode)
{
    frame.document()->updateLayoutIgnorePendingStylesheets();
    RenderElement* renderer = nullptr;
    if (assistedNode.hasTagName(HTMLNames::textareaTag) || assistedNode.hasTagName(HTMLNames::inputTag) || assistedNode.hasTagName(HTMLNames::selectTag))
        renderer = downcast<RenderElement>(assistedNode.renderer());
    else if (Element* rootEditableElement = assistedNode.rootEditableElement())
        renderer = rootEditableElement->renderer();
    
    if (!renderer)
        return FloatQuad();

    RenderStyle& style = renderer->style();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);

    boundingBox.move(style.borderLeftWidth(), style.borderTopWidth());
    boundingBox.setWidth(boundingBox.width() - style.borderLeftWidth() - style.borderRightWidth());
    boundingBox.setHeight(boundingBox.height() - style.borderBottomWidth() - style.borderTopWidth());

    return FloatQuad(boundingBox);
}

static IntPoint constrainPoint(const IntPoint& point, const Frame& frame, const Node& assistedNode)
{
    ASSERT(&assistedNode.document() == frame.document());
    const int DEFAULT_CONSTRAIN_INSET = 2;
    IntRect innerFrame = innerFrameQuad(frame, assistedNode).enclosingBoundingBox();
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

PassRefPtr<Range> WebPage::rangeForWebSelectionAtPosition(const IntPoint& point, const VisiblePosition& position, SelectionFlags& flags)
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent | HitTestRequest::AllowChildFrameContent);

    Node* currentNode = result.innerNode();
    RefPtr<Range> range;
    FloatRect boundingRectInScrollViewCoordinates;

    if (currentNode->isTextNode()) {
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        if (!range || range->collapsed())
            range = Range::create(currentNode->document(), position, position);
        if (!range)
            return nullptr;

        boundingRectInScrollViewCoordinates = selectionBoxForRange(range.get());
        boundingRectInScrollViewCoordinates.scale(m_page->pageScaleFactor());
        if (boundingRectInScrollViewCoordinates.width() > m_blockSelectionDesiredSize.width() && boundingRectInScrollViewCoordinates.height() > m_blockSelectionDesiredSize.height())
            return wordRangeFromPosition(position);

        currentNode = range->commonAncestorContainer();
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
    if (!renderer || renderer->style().userSelect() == SELECT_NONE)
        return nullptr;

    if (renderer->childrenInline() && (is<RenderBlock>(*renderer) && !downcast<RenderBlock>(*renderer).inlineElementContinuation()) && !renderer->isTable()) {
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

    flags = IsBlockSelection;
    range = Range::create(bestChoice->document());
    range->selectNodeContents(bestChoice, ASSERT_NO_EXCEPTION);
    return range->collapsed() ? nullptr : range;
}

PassRefPtr<Range> WebPage::rangeForBlockAtPoint(const IntPoint& point)
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent | HitTestRequest::IgnoreClipping);

    Node* currentNode = result.innerNode();
    RefPtr<Range> range;

    if (currentNode->isTextNode()) {
        range = enclosingTextUnitOfGranularity(m_page->focusController().focusedOrMainFrame().visiblePositionForPoint(point), ParagraphGranularity, DirectionForward);
        if (range && !range->collapsed())
            return range;
    }

    if (!currentNode->isElementNode())
        currentNode = currentNode->parentElement();

    if (!currentNode)
        return nullptr;

    range = Range::create(currentNode->document());
    range->selectNodeContents(currentNode, ASSERT_NO_EXCEPTION);
    return range;
}

void WebPage::selectWithGesture(const IntPoint& point, uint32_t granularity, uint32_t gestureType, uint32_t gestureState, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);

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
        // move the the position at the end of the word
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
            range = wordRangeFromPosition(position);
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
                range->setStart(position.deepEquivalent(), ASSERT_NO_EXCEPTION);
            if (position > range->endPosition())
                range->setEnd(position.deepEquivalent(), ASSERT_NO_EXCEPTION);
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
        if (wkGestureState == GestureRecognizerState::Ended && flags & IsBlockSelection)
            m_currentBlockSelection = range;
        break;

    default:
        break;
    }
    if (range)
        frame.selection().setSelectedRange(range.get(), position.affinity(), true);

    send(Messages::WebPageProxy::GestureCallback(point, gestureType, gestureState, static_cast<uint32_t>(flags), callbackID));
}

static PassRefPtr<Range> rangeForPosition(Frame* frame, const VisiblePosition& position, bool baseIsStart)
{
    RefPtr<Range> range;
    VisiblePosition result = position;

    if (baseIsStart) {
        VisiblePosition selectionStart = frame->selection().selection().visibleStart();
        bool wouldFlip = position <= selectionStart;

        if (wouldFlip)
            result = selectionStart.next();

        if (result.isNotNull())
            range = Range::create(*frame->document(), selectionStart, result);
    } else {
        VisiblePosition selectionEnd = frame->selection().selection().visibleEnd();
        bool wouldFlip = position >= selectionEnd;

        if (wouldFlip)
            result = selectionEnd.previous();

        if (result.isNotNull())
            range = Range::create(*frame->document(), result, selectionEnd);
    }

    return range.release();
}

static PassRefPtr<Range> rangeAtWordBoundaryForPosition(Frame* frame, const VisiblePosition& position, bool baseIsStart, SelectionDirection direction)
{
    SelectionDirection sameDirection = baseIsStart ? DirectionForward : DirectionBackward;
    SelectionDirection oppositeDirection = baseIsStart ? DirectionBackward : DirectionForward;
    VisiblePosition base = baseIsStart ? frame->selection().selection().visibleStart() : frame->selection().selection().visibleEnd();
    VisiblePosition extent = baseIsStart ? frame->selection().selection().visibleEnd() : frame->selection().selection().visibleStart();
    VisiblePosition initialExtent = position;

    if (atBoundaryOfGranularity(extent, WordGranularity, sameDirection)) {
        // This is a word boundary. Leave selection where it is.
        return 0;
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
           && !atBoundaryOfGranularity(extent, LineBoundary, sameDirection)
           && !atBoundaryOfGranularity(extent, LineBoundary, oppositeDirection)) {
        extent = baseIsStart ? extent.next() : extent.previous();
    }

    // Don't let the smart extension make the extent equal the base.
    // Expand out to word boundary.
    if (extent.isNull() || extent == base)
        extent = wordBoundary;
    if (extent.isNull())
        return 0;

    return (base < extent) ? Range::create(*frame->document(), base, extent) : Range::create(*frame->document(), extent, base);
}

static const int maxHitTests = 10;

static inline float distanceBetweenRectsForPosition(IntRect& first, IntRect& second, SelectionHandlePosition handlePosition)
{
    switch (handlePosition) {
    case SelectionHandlePosition::Top:
        return abs(first.y() - second.y());
    case SelectionHandlePosition::Right:
        return abs(first.maxX() - second.maxX());
    case SelectionHandlePosition::Bottom:
        return abs(first.maxY() - second.maxY());
    case SelectionHandlePosition::Left:
        return abs(first.x() - second.x());
    }
}

static inline bool rectsEssentiallyTheSame(IntRect& first, IntRect& second, float allowablePercentDifference)
{
    const float minMagnitudeRatio = 1.0 - allowablePercentDifference;
    const float maxDisplacementRatio = allowablePercentDifference;

    float xOriginShiftRatio = abs(first.x() - second.x())/std::min(first.width(), second.width());
    float yOriginShiftRatio = abs(first.y() - second.y())/std::min(first.height(), second.height());

    float widthRatio = std::min(first.width() / second.width(), second.width() / first.width());
    float heightRatio = std::min(first.height() / second.height(), second.height() / first.height());
    return ((widthRatio > minMagnitudeRatio && xOriginShiftRatio < maxDisplacementRatio)
        && (heightRatio > minMagnitudeRatio && yOriginShiftRatio < maxDisplacementRatio));
}

static inline bool containsRange(Range* first, Range* second)
{
    if (!first || !second)
        return false;
    return (first->commonAncestorContainer()->ownerDocument() == second->commonAncestorContainer()->ownerDocument()
        && first->compareBoundaryPoints(Range::START_TO_START, second, ASSERT_NO_EXCEPTION) <= 0
        && first->compareBoundaryPoints(Range::END_TO_END, second, ASSERT_NO_EXCEPTION) >= 0);
}

static inline RefPtr<Range> unionDOMRanges(Range* rangeA, Range* rangeB)
{
    if (!rangeB)
        return rangeA;
    if (!rangeA)
        return rangeB;

    Range* start = rangeA->compareBoundaryPoints(Range::START_TO_START, rangeB, ASSERT_NO_EXCEPTION) <= 0 ? rangeA : rangeB;
    Range* end = rangeA->compareBoundaryPoints(Range::END_TO_END, rangeB, ASSERT_NO_EXCEPTION) <= 0 ? rangeB : rangeA;

    return Range::create(rangeA->ownerDocument(), &start->startContainer(), start->startOffset(), &end->endContainer(), end->endOffset());
}

static inline IntPoint computeEdgeCenter(const IntRect& box, SelectionHandlePosition handlePosition)
{
    switch (handlePosition) {
    case SelectionHandlePosition::Top:
        return IntPoint(box.x() + box.width() / 2, box.y());
    case SelectionHandlePosition::Right:
        return IntPoint(box.maxX(), box.y() + box.height() / 2);
    case SelectionHandlePosition::Bottom:
        return IntPoint(box.x() + box.width() / 2, box.maxY());
    case SelectionHandlePosition::Left:
        return IntPoint(box.x(), box.y() + box.height() / 2);
    }
}

PassRefPtr<Range> WebPage::expandedRangeFromHandle(Range* currentRange, SelectionHandlePosition handlePosition)
{
    IntRect currentBox = selectionBoxForRange(currentRange);
    IntPoint edgeCenter = computeEdgeCenter(currentBox, handlePosition);
    static const float maxDistance = 1000;
    const float multiple = powf(maxDistance, 1.0/(maxHitTests - 1));
    float distance = 1;

    RefPtr<Range> bestRange;
    IntRect bestRect;

    while (distance < maxDistance) {
        if (bestRange) {
            if (distanceBetweenRectsForPosition(bestRect, currentBox, handlePosition) < distance) {
                // Break early, we're unlikely to do any better.
                break;
            }
        }

        IntPoint testPoint = edgeCenter;
        switch (handlePosition) {
        case SelectionHandlePosition::Top:
            testPoint.move(0, -distance);
            break;
        case SelectionHandlePosition::Right:
            testPoint.move(distance, 0);
            break;
        case SelectionHandlePosition::Bottom:
            testPoint.move(0, distance);
            break;
        case SelectionHandlePosition::Left:
            testPoint.move(-distance, 0);
            break;
        }

        distance = ceilf(distance * multiple);

        RefPtr<Range> newRange;
        RefPtr<Range> rangeAtPosition = rangeForBlockAtPoint(testPoint);
        if (!rangeAtPosition || &currentRange->ownerDocument() != &rangeAtPosition->ownerDocument())
            continue;

        if (containsRange(rangeAtPosition.get(), currentRange))
            newRange = rangeAtPosition;
        else if (containsRange(currentRange, rangeAtPosition.get()))
            newRange = currentRange;
        else
            newRange = unionDOMRanges(currentRange, rangeAtPosition.get());

        IntRect copyRect = selectionBoxForRange(newRange.get());

        // Is it different and bigger than the current?
        bool isBetterChoice = !(rectsEssentiallyTheSame(copyRect, currentBox, .05));
        if (isBetterChoice) {
            switch (handlePosition) {
            case SelectionHandlePosition::Top:
            case SelectionHandlePosition::Bottom:
                isBetterChoice = (copyRect.height() > currentBox.height());
                break;
            case SelectionHandlePosition::Right:
            case SelectionHandlePosition::Left:
                isBetterChoice = (copyRect.width() > currentBox.width());
                break;
            }

        }

        if (bestRange && isBetterChoice) {
            // Furtherore, is it smaller than the best we've found so far?
            switch (handlePosition) {
            case SelectionHandlePosition::Top:
            case SelectionHandlePosition::Bottom:
                isBetterChoice = (copyRect.height() < bestRect.height());
                break;
            case SelectionHandlePosition::Right:
            case SelectionHandlePosition::Left:
                isBetterChoice = (copyRect.width() < bestRect.width());
                break;
            }
        }

        if (isBetterChoice) {
            bestRange = newRange;
            bestRect = copyRect;
        }
    }

    if (bestRange)
        return bestRange;

    return currentRange;
}

PassRefPtr<Range> WebPage::contractedRangeFromHandle(Range* currentRange, SelectionHandlePosition handlePosition, SelectionFlags& flags)
{
    // Shrinking with a base and extent will always give better results. If we only have a single element,
    // see if we can break that down to a base and extent. Shrinking base and extent is comparatively straightforward.
    // Shrinking down to another element is unlikely to move just one edge, but we can try that as a fallback.

    IntRect currentBox = selectionBoxForRange(currentRange);
    IntPoint edgeCenter = computeEdgeCenter(currentBox, handlePosition);
    flags = IsBlockSelection;

    float maxDistance;

    switch (handlePosition) {
    case SelectionHandlePosition::Top:
    case SelectionHandlePosition::Bottom:
        maxDistance = currentBox.height();
        break;
    case SelectionHandlePosition::Right:
    case SelectionHandlePosition::Left:
        maxDistance = currentBox.width();
        break;
    }

    const float multiple = powf(maxDistance - 1, 1.0/(maxHitTests - 1));
    float distance = 1;
    RefPtr<Range> bestRange;
    IntRect bestRect;

    while (distance < maxDistance) {
        if (bestRange) {
            float shrankDistance;
            switch (handlePosition) {
            case SelectionHandlePosition::Top:
            case SelectionHandlePosition::Bottom:
                shrankDistance = abs(currentBox.height() - bestRect.height());
                break;
            case SelectionHandlePosition::Right:
            case SelectionHandlePosition::Left:
                shrankDistance = abs(currentBox.width() - bestRect.width());
                break;
            }
            if (shrankDistance > distance) {
                // Certainly not going to do any better than that.
                break;
            }
        }

        IntPoint testPoint = edgeCenter;
        switch (handlePosition) {
        case SelectionHandlePosition::Top:
            testPoint.move(0, distance);
            break;
        case SelectionHandlePosition::Right:
            testPoint.move(-distance, 0);
            break;
        case SelectionHandlePosition::Bottom:
            testPoint.move(0, -distance);
            break;
        case SelectionHandlePosition::Left:
            testPoint.move(distance, 0);
            break;
        }

        distance *= multiple;

        RefPtr<Range> newRange = rangeForBlockAtPoint(testPoint);
        if (!newRange || &newRange->ownerDocument() != &currentRange->ownerDocument())
            continue;

        if (handlePosition == SelectionHandlePosition::Top || handlePosition == SelectionHandlePosition::Left)
            newRange = Range::create(newRange->startContainer().document(), newRange->endPosition(), currentRange->endPosition());
        else
            newRange = Range::create(newRange->startContainer().document(), currentRange->startPosition(), newRange->startPosition());

        IntRect copyRect = selectionBoxForRange(newRange.get());
        if (copyRect.isEmpty()) {
            // If the new range is an empty rectangle, we try the block at the current point
            // and see if that has a rectangle that is a better choice.
            newRange = rangeForBlockAtPoint(testPoint);
            copyRect = selectionBoxForRange(newRange.get());
        }
        bool isBetterChoice;
        switch (handlePosition) {
        case SelectionHandlePosition::Top:
        case SelectionHandlePosition::Bottom:
            isBetterChoice = (copyRect.height() < currentBox.height());
            break;
        case SelectionHandlePosition::Left:
        case SelectionHandlePosition::Right:
            isBetterChoice = (copyRect.width() > bestRect.width());
            break;
        }

        isBetterChoice = isBetterChoice && !areRangesEqual(newRange.get(), currentRange);
        if (bestRange && isBetterChoice) {
            switch (handlePosition) {
            case SelectionHandlePosition::Top:
            case SelectionHandlePosition::Bottom:
                isBetterChoice = (copyRect.height() > bestRect.height());
                break;
            case SelectionHandlePosition::Left:
            case SelectionHandlePosition::Right:
                isBetterChoice = (copyRect.width() > bestRect.width());
                break;
            }
        }
        if (isBetterChoice) {
            bestRange = newRange;
            bestRect = copyRect;
        }

    }

    if (!bestRange)
        bestRange = currentRange;
    
    // If we can shrink down to text only, the only reason we wouldn't is that
    // there are multiple sub-element blocks beneath us.  If we didn't find
    // multiple sub-element blocks, don't shrink to a sub-element block.

    Node* node = bestRange->commonAncestorContainer();
    if (!node->isElementNode())
        node = node->parentElement();

    RenderObject* renderer = node->renderer();
    if (renderer && renderer->childrenInline() && (is<RenderBlock>(*renderer) && !downcast<RenderBlock>(*renderer).inlineElementContinuation()) && !renderer->isTable())
        flags = None;

    return bestRange;
}

void WebPage::computeExpandAndShrinkThresholdsForHandle(const IntPoint& point, SelectionHandlePosition handlePosition, float& growThreshold, float& shrinkThreshold)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> currentRange = m_currentBlockSelection ? m_currentBlockSelection.get() : frame.selection().selection().toNormalizedRange();

    if (!currentRange)
        return;

    RefPtr<Range> expandedRange = expandedRangeFromHandle(currentRange.get(), handlePosition);
    SelectionFlags flags;
    RefPtr<Range> contractedRange = contractedRangeFromHandle(currentRange.get(), handlePosition, flags);

    IntRect currentBounds = selectionBoxForRange(currentRange.get());
    IntRect expandedBounds = selectionBoxForRange(expandedRange.get());
    IntRect contractedBounds = selectionBoxForRange(contractedRange.get());

    float current;
    float expanded;
    float contracted;
    float maxThreshold;
    float minThreshold;

    switch (handlePosition) {
    case SelectionHandlePosition::Top: {
        current = currentBounds.y();
        expanded = expandedBounds.y();
        contracted = contractedBounds.y();
        maxThreshold = FLT_MIN;
        minThreshold = FLT_MAX;
        break;
    }
    case SelectionHandlePosition::Right: {
        current = currentBounds.maxX();
        expanded = expandedBounds.maxX();
        contracted = contractedBounds.maxX();
        maxThreshold = FLT_MAX;
        minThreshold = FLT_MIN;
        break;
    }
    case SelectionHandlePosition::Bottom: {
        current = currentBounds.maxY();
        expanded = expandedBounds.maxY();
        contracted = contractedBounds.maxY();
        maxThreshold = FLT_MAX;
        minThreshold = FLT_MIN;
        break;
    }
    case SelectionHandlePosition::Left: {
        current = currentBounds.x();
        expanded = expandedBounds.x();
        contracted = contractedBounds.x();
        maxThreshold = FLT_MIN;
        minThreshold = FLT_MAX;
        break;
    }
    }

    static const float fractionToGrow = 0.3;

    growThreshold = current + (expanded - current) * fractionToGrow;
    shrinkThreshold = current + (contracted - current) * (1 - fractionToGrow);
    if (areRangesEqual(expandedRange.get(), currentRange.get()))
        growThreshold = maxThreshold;

    if (flags & IsBlockSelection && areRangesEqual(contractedRange.get(), currentRange.get()))
        shrinkThreshold = minThreshold;
}

static inline bool shouldExpand(SelectionHandlePosition handlePosition, const IntRect& rect, const IntPoint& point)
{
    switch (handlePosition) {
    case SelectionHandlePosition::Top:
        return (point.y() < rect.y());
    case SelectionHandlePosition::Left:
        return (point.x() < rect.x());
    case SelectionHandlePosition::Right:
        return (point.x() > rect.maxX());
    case SelectionHandlePosition::Bottom:
        return (point.y() > rect.maxY());
    }
}

PassRefPtr<WebCore::Range> WebPage::changeBlockSelection(const IntPoint& point, SelectionHandlePosition handlePosition, float& growThreshold, float& shrinkThreshold, SelectionFlags& flags)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> currentRange = m_currentBlockSelection ? m_currentBlockSelection.get() : frame.selection().selection().toNormalizedRange();
    if (!currentRange)
        return nullptr;
    RefPtr<Range> newRange = shouldExpand(handlePosition, selectionBoxForRange(currentRange.get()), point) ? expandedRangeFromHandle(currentRange.get(), handlePosition) : contractedRangeFromHandle(currentRange.get(), handlePosition, flags);

    if (newRange) {
        m_currentBlockSelection = newRange;
        frame.selection().setSelectedRange(newRange.get(), VP_DEFAULT_AFFINITY, true);
    }

    computeExpandAndShrinkThresholdsForHandle(point, handlePosition, growThreshold, shrinkThreshold);
    return newRange;
}

void WebPage::updateBlockSelectionWithTouch(const IntPoint& point, uint32_t touch, uint32_t handlePosition)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    IntPoint adjustedPoint = frame.view()->rootViewToContents(point);

    float growThreshold = 0;
    float shrinkThreshold = 0;
    SelectionFlags flags = IsBlockSelection;

    switch (static_cast<SelectionTouch>(touch)) {
    case SelectionTouch::Started:
        computeExpandAndShrinkThresholdsForHandle(adjustedPoint, static_cast<SelectionHandlePosition>(handlePosition), growThreshold, shrinkThreshold);
        break;
    case SelectionTouch::Ended:
        break;
    case SelectionTouch::Moved:
        changeBlockSelection(adjustedPoint, static_cast<SelectionHandlePosition>(handlePosition), growThreshold, shrinkThreshold, flags);
        break;
    default:
        return;
    }

    send(Messages::WebPageProxy::DidUpdateBlockSelectionWithTouch(touch, static_cast<uint32_t>(flags), growThreshold, shrinkThreshold));
}

void WebPage::clearSelection()
{
    m_currentBlockSelection = nullptr;
    m_page->focusController().focusedOrMainFrame().selection().clear();
}

void WebPage::updateSelectionWithTouches(const IntPoint& point, uint32_t touches, bool baseIsStart, uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = frame.visiblePositionForPoint(frame.view()->rootViewToContents(point));
    if (position.isNull()) {
        send(Messages::WebPageProxy::TouchesCallback(point, touches, callbackID));
        return;
    }

    RefPtr<Range> range;
    VisiblePosition result;
    
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
            range = rangeForPosition(&frame, position, baseIsStart);
        break;

    case SelectionTouch::EndedMovingForward:
        range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionForward);
        break;
        
    case SelectionTouch::EndedMovingBackward:
        range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionBackward);
        break;

    case SelectionTouch::Moved:
        range = rangeForPosition(&frame, position, baseIsStart);
        break;
    }
    if (range)
        frame.selection().setSelectedRange(range.get(), position.affinity(), true);

    send(Messages::WebPageProxy::TouchesCallback(point, touches, callbackID));
}

void WebPage::selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, uint32_t gestureType, uint32_t gestureState, uint64_t callbackID)
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
        frame.selection().setSelectedRange(range.get(), fromPosition.affinity(), true);
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
    frame.selection().setSelectedRange(wordRangeFromPosition(position).get(), position.affinity(), true);
}

void WebPage::selectWordBackward()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaret())
        return;

    VisiblePosition position = frame.selection().selection().start();
    VisiblePosition startPosition = positionOfNextBoundaryOfGranularity(position, WordGranularity, DirectionBackward);
    if (startPosition.isNotNull() && startPosition != position)
        frame.selection().setSelectedRange(Range::create(*frame.document(), startPosition, position).ptr(), position.affinity(), true);
}

void WebPage::moveSelectionByOffset(int32_t offset, uint64_t callbackID)
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
        frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), position.affinity(), true);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

VisiblePosition WebPage::visiblePositionInFocusedNodeForPoint(const Frame& frame, const IntPoint& point, bool isInteractingWithAssistedNode)
{
    IntPoint adjustedPoint(frame.view()->rootViewToContents(point));
    IntPoint constrainedPoint = m_assistedNode && isInteractingWithAssistedNode ? constrainPoint(adjustedPoint, frame, *m_assistedNode) : adjustedPoint;
    return frame.visiblePositionForPoint(constrainedPoint);
}

void WebPage::selectPositionAtPoint(const WebCore::IntPoint& point, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);
    
    if (position.isNotNull())
        frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), position.affinity(), true);
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::selectPositionAtBoundaryWithDirection(const WebCore::IntPoint& point, uint32_t granularity, uint32_t direction, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);

    if (position.isNotNull()) {
        position = positionOfNextBoundaryOfGranularity(position, static_cast<WebCore::TextGranularity>(granularity), static_cast<SelectionDirection>(direction));
        if (position.isNotNull())
            frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), UPSTREAM, true);
    }
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::moveSelectionAtBoundaryWithDirection(uint32_t granularity, uint32_t direction, uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    
    if (!frame.selection().selection().isNone()) {
        bool isForward = (direction == DirectionForward || direction == DirectionRight);
        VisiblePosition position = (isForward) ? frame.selection().selection().visibleEnd() : frame.selection().selection().visibleStart();
        position = positionOfNextBoundaryOfGranularity(position, static_cast<WebCore::TextGranularity>(granularity), static_cast<SelectionDirection>(direction));
        if (position.isNotNull())
            frame.selection().setSelectedRange(Range::create(*frame.document(), position, position).ptr(), isForward? UPSTREAM : DOWNSTREAM, true);
    }
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

PassRefPtr<Range> WebPage::rangeForGranularityAtPoint(const Frame& frame, const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithAssistedNode)
{
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);

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

void WebPage::selectTextWithGranularityAtPoint(const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> range = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithAssistedNode);

    if (range)
        frame.selection().setSelectedRange(range.get(), UPSTREAM, true);
    m_initialSelection = range;
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::beginSelectionInDirection(uint32_t direction, uint64_t callbackID)
{
    m_selectionAnchor = (static_cast<SelectionDirection>(direction) == DirectionLeft) ? Start : End;
    send(Messages::WebPageProxy::UnsignedCallback(m_selectionAnchor == Start, callbackID));
}

void WebPage::updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint& point, uint32_t granularity, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);
    RefPtr<Range> newRange = rangeForGranularityAtPoint(frame, point, granularity, isInteractingWithAssistedNode);
    
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
        frame.selection().setSelectedRange(range.get(), UPSTREAM, true);
    
    send(Messages::WebPageProxy::UnsignedCallback(selectionStart == m_initialSelection->startPosition(), callbackID));
}

void WebPage::updateSelectionWithExtentPoint(const WebCore::IntPoint& point, bool isInteractingWithAssistedNode, uint64_t callbackID)
{
    const Frame& frame = m_page->focusController().focusedOrMainFrame();
    VisiblePosition position = visiblePositionInFocusedNodeForPoint(frame, point, isInteractingWithAssistedNode);

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
        frame.selection().setSelectedRange(range.get(), UPSTREAM, true);

    send(Messages::WebPageProxy::UnsignedCallback(m_selectionAnchor == Start, callbackID));
}

void WebPage::convertSelectionRectsToRootView(FrameView* view, Vector<SelectionRect>& selectionRects)
{
    for (size_t i = 0; i < selectionRects.size(); ++i) {
        SelectionRect& currentRect = selectionRects[i];
        currentRect.setRect(view->contentsToRootView(currentRect.rect()));
    }
}

void WebPage::requestDictationContext(uint64_t callbackID)
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

    send(Messages::WebPageProxy::DictationContextCallback(selectedText, contextBefore, contextAfter, callbackID));
}

void WebPage::replaceSelectedText(const String& oldText, const String& newText)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> wordRange = frame.selection().isCaret() ? wordRangeFromPosition(frame.selection().selection().start()) : frame.selection().toNormalizedRange();
    if (plainTextReplacingNoBreakSpace(wordRange.get()) != oldText)
        return;
    
    frame.editor().setIgnoreCompositionSelectionChange(true);
    frame.selection().setSelectedRange(wordRange.get(), UPSTREAM, true);
    frame.editor().insertText(newText, 0);
    frame.editor().setIgnoreCompositionSelectionChange(false);
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
    RefPtr<Range> range = Range::create(*frame.document(), position, frame.selection().selection().start());

    if (plainTextReplacingNoBreakSpace(range.get()) != oldText)
        return;

    // We don't want to notify the client that the selection has changed until we are done inserting the new text.
    frame.editor().setIgnoreCompositionSelectionChange(true);
    frame.selection().setSelectedRange(range.get(), UPSTREAM, true);
    frame.editor().insertText(newText, 0);
    frame.editor().setIgnoreCompositionSelectionChange(false);
}

void WebPage::requestAutocorrectionData(const String& textForAutocorrection, uint64_t callbackID)
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
    rectsForText.resize(selectionRects.size());

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

void WebPage::applyAutocorrection(const String& correction, const String& originalText, uint64_t callbackID)
{
    bool correctionApplied;
    syncApplyAutocorrection(correction, originalText, correctionApplied);
    send(Messages::WebPageProxy::StringCallback(correctionApplied ? correction : String(), callbackID));
}

void WebPage::executeEditCommandWithCallback(const String& commandName, uint64_t callbackID)
{
    executeEditCommand(commandName, String());
    if (commandName == "toggleBold" || commandName == "toggleItalic" || commandName == "toggleUnderline")
        send(Messages::WebPageProxy::EditorStateChanged(editorState()));
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

std::chrono::milliseconds WebPage::eventThrottlingDelay() const
{
    if (m_isInStableState || m_estimatedLatency <= std::chrono::milliseconds(1000 / 60))
        return std::chrono::milliseconds::zero();

    return std::chrono::milliseconds(std::min<std::chrono::milliseconds::rep>(m_estimatedLatency.count() * 2, 1000));
}

void WebPage::syncApplyAutocorrection(const String& correction, const String& originalText, bool& correctionApplied)
{
    RefPtr<Range> range;
    correctionApplied = false;
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaret())
        return;
    VisiblePosition position = frame.selection().selection().start();
    
    range = wordRangeFromPosition(position);
    String textForRange = plainTextReplacingNoBreakSpace(range.get());
    if (textForRange != originalText) {
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
    if (textForRange != originalText) {
        correctionApplied = false;
        return;
    }
    
    frame.selection().setSelectedRange(range.get(), UPSTREAM, true);
    if (correction.length())
        frame.editor().insertText(correction, 0);
    else
        frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    correctionApplied = true;
}

static void computeAutocorrectionContext(Frame& frame, String& contextBefore, String& markedText, String& selectedText, String& contextAfter, uint64_t& location, uint64_t& length)
{
    RefPtr<Range> range;
    VisiblePosition startPosition = frame.selection().selection().start();
    VisiblePosition endPosition = frame.selection().selection().end();
    location = NSNotFound;
    length = 0;
    const unsigned minContextWordCount = 3;
    const unsigned minContextLenght = 12;
    const unsigned maxContextLength = 30;

    if (frame.selection().isRange())
        selectedText = plainTextReplacingNoBreakSpace(frame.selection().selection().toNormalizedRange().get());

    if (frame.editor().hasComposition()) {
        range = Range::create(*frame.document(), frame.editor().compositionRange()->startPosition(), startPosition);
        String markedTextBefore;
        if (range)
            markedTextBefore = plainTextReplacingNoBreakSpace(range.get());
        range = Range::create(*frame.document(), endPosition, frame.editor().compositionRange()->endPosition());
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
                    contextBefore = ASCIILiteral("\n ") + contextBefore;
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
}

void WebPage::requestAutocorrectionContext(uint64_t callbackID)
{
    String contextBefore;
    String contextAfter;
    String selectedText;
    String markedText;
    uint64_t location;
    uint64_t length;

    computeAutocorrectionContext(m_page->focusController().focusedOrMainFrame(), contextBefore, markedText, selectedText, contextAfter, location, length);

    send(Messages::WebPageProxy::AutocorrectionContextCallback(contextBefore, markedText, selectedText, contextAfter, location, length, callbackID));
}

void WebPage::getAutocorrectionContext(String& contextBefore, String& markedText, String& selectedText, String& contextAfter, uint64_t& location, uint64_t& length)
{
    computeAutocorrectionContext(m_page->focusController().focusedOrMainFrame(), contextBefore, markedText, selectedText, contextAfter, location, length);
}

static Element* containingLinkElement(Element* element)
{
    for (auto& currentElement : elementLineage(element)) {
        if (currentElement.isLink())
            return &currentElement;
    }
    return nullptr;
}

void WebPage::getPositionInformation(const IntPoint& point, InteractionInformationAtPosition& info)
{
    FloatPoint adjustedPoint;
    Node* hitNode = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);

    info.point = point;
    info.nodeAtPositionIsAssistedNode = (hitNode == m_assistedNode);
    if (m_assistedNode) {
        const Frame& frame = m_page->focusController().focusedOrMainFrame();
        if (frame.editor().hasComposition()) {
            const uint32_t kHitAreaWidth = 66;
            const uint32_t kHitAreaHeight = 66;
            FrameView& view = *frame.view();
            IntPoint adjustedPoint(view.rootViewToContents(point));
            IntPoint constrainedPoint = m_assistedNode ? constrainPoint(adjustedPoint, frame, *m_assistedNode) : adjustedPoint;
            VisiblePosition position = frame.visiblePositionForPoint(constrainedPoint);

            RefPtr<Range> compositionRange = frame.editor().compositionRange();
            if (position < compositionRange->startPosition())
                position = compositionRange->startPosition();
            else if (position > compositionRange->endPosition())
                position = compositionRange->endPosition();
            IntRect caretRect = view.contentsToRootView(position.absoluteCaretBounds());
            float deltaX = abs(caretRect.x() + (caretRect.width() / 2) - point.x());
            float deltaYFromTheTop = abs(caretRect.y() - point.y());
            float deltaYFromTheBottom = abs(caretRect.y() + caretRect.height() - point.y());

            info.isNearMarkedText = !(deltaX > kHitAreaWidth || deltaYFromTheTop > kHitAreaHeight || deltaYFromTheBottom > kHitAreaHeight);
        }
    }
    bool elementIsLinkOrImage = false;
    if (hitNode) {
        Element* element = is<Element>(*hitNode) ? downcast<Element>(hitNode) : nullptr;
        if (element) {
            info.isClickableElement = true;
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

                    // Ensure that the image contains at most 600K pixels, so that it is not too big.
                    if (RefPtr<WebImage> snapshot = snapshotNode(*element, SnapshotOptionsShareable, 600 * 1024))
                        info.image = snapshot->bitmap();

                    RefPtr<Range> linkRange = rangeOfContents(*linkElement);
                    if (linkRange) {
                        float deviceScaleFactor = corePage()->deviceScaleFactor();
                        const float marginInPoints = 4;

                        RefPtr<TextIndicator> textIndicator = TextIndicator::createWithRange(*linkRange, TextIndicatorOptionTightlyFitContent | TextIndicatorOptionRespectTextColor | TextIndicatorOptionPaintBackgrounds | TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges |
                            TextIndicatorOptionIncludeMarginIfRangeMatchesSelection, TextIndicatorPresentationTransition::None, FloatSize(marginInPoints * deviceScaleFactor, marginInPoints * deviceScaleFactor));
                        if (textIndicator)
                            info.linkIndicator = textIndicator->data();
                    }
#if ENABLE(DATA_DETECTION)
                    info.isDataDetectorLink = DataDetection::isDataDetectorLink(element);
                    if (info.isDataDetectorLink) {
                        info.dataDetectorIdentifier = DataDetection::dataDetectorIdentifier(element);
                        info.dataDetectorResults = element->document().frame()->dataDetectionResults();
                    }
#endif
                } else if (element->renderer() && element->renderer()->isRenderImage()) {
                    info.isImage = true;
                    auto& renderImage = downcast<RenderImage>(*(element->renderer()));
                    if (renderImage.cachedImage() && !renderImage.cachedImage()->errorOccurred()) {
                        if (Image* image = renderImage.cachedImage()->imageForRenderer(&renderImage)) {
                            if (image->width() > 1 && image->height() > 1) {
                                info.imageURL = [(NSURL *)element->document().completeURL(renderImage.cachedImage()->url()) absoluteString];
                                info.isAnimatedImage = image->isAnimated();
                                FloatSize screenSizeInPixels = screenSize();
                                screenSizeInPixels.scale(corePage()->deviceScaleFactor());
                                FloatSize scaledSize = largestRectWithAspectRatioInsideRect(image->size().width() / image->size().height(), FloatRect(0, 0, screenSizeInPixels.width(), screenSizeInPixels.height())).size();
                                FloatSize bitmapSize = scaledSize.width() < image->size().width() ? scaledSize : image->size();
                                if (RefPtr<ShareableBitmap> sharedBitmap = ShareableBitmap::createShareable(IntSize(bitmapSize), ShareableBitmap::SupportsAlpha)) {
                                    auto graphicsContext = sharedBitmap->createGraphicsContext();
                                    graphicsContext->drawImage(*image, FloatRect(0, 0, bitmapSize.width(), bitmapSize.height()));
                                    info.image = sharedBitmap;
                                }
                            }
                        }
                    }
                }
            }
            if (linkElement)
                info.url = [(NSURL *)linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr))) absoluteString];
            info.title = element->fastGetAttribute(HTMLNames::titleAttr).string();
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
        HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent | HitTestRequest::AllowChildFrameContent);
        hitNode = result.innerNode();
        // Hit test could return HTMLHtmlElement that has no renderer, if the body is smaller than the document.
        if (hitNode && hitNode->renderer()) {
            RenderObject* renderer = hitNode->renderer();
            m_page->focusController().setFocusedFrame(result.innerNodeFrame());
            info.bounds = renderer->absoluteBoundingBoxRect(true);
            // We don't want to select blocks that are larger than 97% of the visible area of the document.
            const static CGFloat factor = 0.97;
            info.isSelectable = renderer->style().userSelect() != SELECT_NONE && info.bounds.height() < result.innerNodeFrame()->view()->unobscuredContentRect().height() * factor;
        }
    }
}

void WebPage::requestPositionInformation(const IntPoint& point)
{
    InteractionInformationAtPosition info;

    getPositionInformation(point, info);
    send(Messages::WebPageProxy::DidReceivePositionInformation(info));
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
            Element* linkElement = containingLinkElement(&element);
            if (!linkElement)
                m_interactionNode->document().frame()->editor().writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), element, downcast<RenderImage>(*element.renderer()).cachedImage()->url(), String());
            else
                m_interactionNode->document().frame()->editor().copyURL(linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->fastGetAttribute(HTMLNames::hrefAttr))), linkElement->textContent());
        } else if (element.isLink()) {
            m_interactionNode->document().frame()->editor().copyURL(element.document().completeURL(stripLeadingAndTrailingHTMLSpaces(element.fastGetAttribute(HTMLNames::hrefAttr))), element.textContent());
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

static inline bool isAssistableElement(Element& node)
{
    if (is<HTMLSelectElement>(node))
        return true;
    if (is<HTMLTextAreaElement>(node))
        return true;
    if (is<HTMLInputElement>(node)) {
        HTMLInputElement& inputElement = downcast<HTMLInputElement>(node);
        // FIXME: This laundry list of types is not a good way to factor this. Need a suitable function on HTMLInputElement itself.
        return inputElement.isTextField() || inputElement.isDateField() || inputElement.isDateTimeLocalField() || inputElement.isMonthField() || inputElement.isTimeField();
    }
    return node.isContentEditable();
}

static inline Element* nextAssistableElement(Node* startNode, Page& page, bool isForward)
{
    if (!is<Element>(startNode))
        return nullptr;

    RefPtr<KeyboardEvent> key = KeyboardEvent::createForBindings();

    Element* nextElement = downcast<Element>(startNode);
    do {
        nextElement = isForward
            ? page.focusController().nextFocusableElement(FocusNavigationScope::focusNavigationScopeOf(&nextElement->document()), nextElement, key.get())
            : page.focusController().previousFocusableElement(FocusNavigationScope::focusNavigationScopeOf(&nextElement->document()), nextElement, key.get());
    } while (nextElement && !isAssistableElement(*nextElement));

    return nextElement;
}

static inline bool hasAssistableElement(Node* startNode, Page& page, bool isForward)
{
    return nextAssistableElement(startNode, page, isForward);
}

void WebPage::focusNextAssistedNode(bool isForward, uint64_t callbackID)
{
    Element* nextElement = nextAssistableElement(m_assistedNode.get(), *m_page, isForward);
    m_userIsInteracting = true;
    if (nextElement)
        nextElement->focus();
    m_userIsInteracting = false;
    send(Messages::WebPageProxy::VoidCallback(callbackID));
}

void WebPage::getAssistedNodeInformation(AssistedNodeInformation& information)
{
    layoutIfNeeded();

    // FIXME: This should return the selection rect, but when this is called at focus time
    // we don't have a selection yet. Using the last interaction location is a reasonable approximation for now.
    // FIXME: should the selection rect always be inside the elementRect?
    information.selectionRect = IntRect(m_lastInteractionLocation, IntSize(1, 1));

    if (RenderObject* renderer = m_assistedNode->renderer()) {
        Frame& elementFrame = m_page->focusController().focusedOrMainFrame();
        information.elementRect = elementFrame.view()->contentsToRootView(renderer->absoluteBoundingBoxRect());
        information.nodeFontSize = renderer->style().fontDescription().computedSize();

        bool inFixed = false;
        renderer->localToContainerPoint(FloatPoint(), nullptr, UseTransforms, &inFixed);
        information.insideFixedPosition = inFixed;
        
        if (inFixed && elementFrame.isMainFrame()) {
            FrameView* frameView = elementFrame.view();
            IntRect currentFixedPositionRect = frameView->customFixedPositionLayoutRect();
            frameView->setCustomFixedPositionLayoutRect(frameView->renderView()->documentRect());
            information.elementRect = frameView->contentsToRootView(renderer->absoluteBoundingBoxRect());
            frameView->setCustomFixedPositionLayoutRect(currentFixedPositionRect);
            
            if (!information.elementRect.contains(information.selectionRect))
                information.selectionRect.setLocation(information.elementRect.location());
        }
    } else
        information.elementRect = IntRect();

    information.minimumScaleFactor = minimumPageScaleFactor();
    information.maximumScaleFactor = maximumPageScaleFactor();
    information.allowsUserScaling = m_viewportConfiguration.allowsUserScaling();
    information.hasNextNode = hasAssistableElement(m_assistedNode.get(), *m_page, true);
    information.hasPreviousNode = hasAssistableElement(m_assistedNode.get(), *m_page, false);

    if (is<HTMLSelectElement>(*m_assistedNode)) {
        HTMLSelectElement& element = downcast<HTMLSelectElement>(*m_assistedNode);
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
                information.selectOptions.append(OptionItem(option.text(), false, parentGroupID, option.selected(), option.fastHasAttribute(WebCore::HTMLNames::disabledAttr)));
            } else if (is<HTMLOptGroupElement>(*item)) {
                HTMLOptGroupElement& group = downcast<HTMLOptGroupElement>(*item);
                parentGroupID++;
                information.selectOptions.append(OptionItem(group.groupLabelText(), true, 0, false, group.fastHasAttribute(WebCore::HTMLNames::disabledAttr)));
            }
        }
        information.selectedIndex = element.selectedIndex();
        information.isMultiSelect = element.multiple();
    } else if (is<HTMLTextAreaElement>(*m_assistedNode)) {
        HTMLTextAreaElement& element = downcast<HTMLTextAreaElement>(*m_assistedNode);
        information.autocapitalizeType = static_cast<WebAutocapitalizeType>(element.autocapitalizeType());
        information.isAutocorrect = element.autocorrect();
        information.elementType = InputType::TextArea;
        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
    } else if (is<HTMLInputElement>(*m_assistedNode)) {
        HTMLInputElement& element = downcast<HTMLInputElement>(*m_assistedNode);
        HTMLFormElement* form = element.form();
        if (form)
            information.formAction = form->getURLAttribute(WebCore::HTMLNames::actionAttr);
        information.autocapitalizeType = static_cast<WebAutocapitalizeType>(element.autocapitalizeType());
        information.isAutocorrect = element.autocorrect();
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
            const AtomicString& pattern = element.fastGetAttribute(HTMLNames::patternAttr);
            if (pattern == "\\d*" || pattern == "[0-9]*")
                information.elementType = InputType::NumberPad;
            else {
                information.elementType = InputType::Text;
                if (!information.formAction.isEmpty()
                    && (element.getNameAttribute().contains("search") || element.getIdAttribute().contains("search") || element.fastGetAttribute(HTMLNames::titleAttr).contains("search")))
                    information.elementType = InputType::Search;
            }
        }

        information.isReadOnly = element.isReadOnly();
        information.value = element.value();
        information.valueAsNumber = element.valueAsNumber();
        information.title = element.title();
    } else if (m_assistedNode->hasEditableStyle()) {
        information.elementType = InputType::ContentEditable;
        information.isAutocorrect = true;   // FIXME: Should we look at the attribute?
        information.autocapitalizeType = WebAutocapitalizeTypeSentences; // FIXME: Should we look at the attribute?
        information.isReadOnly = false;
    }
}

void WebPage::resetAssistedNodeForFrame(WebFrame* frame)
{
    if (!m_assistedNode)
        return;
    if (m_assistedNode->document().frame() == frame->coreFrame()) {
        send(Messages::WebPageProxy::StopAssistingNode());
        m_assistedNode = nullptr;
    }
}

void WebPage::elementDidFocus(WebCore::Node* node)
{
    if (m_assistedNode == node && m_hasFocusedDueToUserInteraction)
        return;

    if (node->hasTagName(WebCore::HTMLNames::selectTag) || node->hasTagName(WebCore::HTMLNames::inputTag) || node->hasTagName(WebCore::HTMLNames::textareaTag) || node->hasEditableStyle()) {
        m_assistedNode = node;
        m_hasFocusedDueToUserInteraction |= m_userIsInteracting;
        AssistedNodeInformation information;
        getAssistedNodeInformation(information);
        RefPtr<API::Object> userData;

        // FIXME: We check m_userIsInteracting so that we don't begin an input session for a
        // programmatic focus that doesn't cause the keyboard to appear. But this misses the case of
        // a programmatic focus happening while the keyboard is already shown. Once we have a way to
        // know the keyboard state in the Web Process, we should refine the condition.
        if (m_userIsInteracting)
            m_formClient->willBeginInputSession(this, downcast<Element>(node), WebFrame::fromCoreFrame(*node->document().frame()), userData);

        send(Messages::WebPageProxy::StartAssistingNode(information, m_userIsInteracting, m_hasPendingBlurNotification, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
        m_hasPendingBlurNotification = false;
    }
}

void WebPage::elementDidBlur(WebCore::Node* node)
{
    if (m_assistedNode == node) {
        m_hasPendingBlurNotification = true;
        RefPtr<WebPage> protectedThis(this);
        dispatch_async(dispatch_get_main_queue(), [protectedThis] {
            if (protectedThis->m_hasPendingBlurNotification)
                protectedThis->send(Messages::WebPageProxy::StopAssistingNode());
            protectedThis->m_hasPendingBlurNotification = false;
        });
        m_hasFocusedDueToUserInteraction = false;
        m_assistedNode = nullptr;
    }
}

void WebPage::setViewportConfigurationMinimumLayoutSize(const FloatSize& size)
{
    resetTextAutosizingBeforeLayoutIfNeeded(m_viewportConfiguration.minimumLayoutSize(), size);
    
    if (m_viewportConfiguration.setMinimumLayoutSize(size))
        viewportConfigurationChanged();
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

// WebCore stores the page scale factor as float instead of double. When we get a scale from WebCore,
// we need to ignore differences that are within a small rounding error on floats.
static inline bool areEssentiallyEqualAsFloat(float a, float b)
{
    return WTF::areEssentiallyEqual(a, b);
}

void WebPage::resetTextAutosizingBeforeLayoutIfNeeded(const FloatSize& oldSize, const FloatSize& newSize)
{
    if (oldSize.width() == newSize.width())
        return;

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document || !document->renderView())
            continue;
        document->renderView()->resetTextAutosizing();
    }
}

void WebPage::dynamicViewportSizeUpdate(const FloatSize& minimumLayoutSize, const WebCore::FloatSize& maximumUnobscuredSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, const WebCore::FloatRect& targetUnobscuredRectInScrollViewCoordinates, double targetScale, int32_t deviceOrientation, uint64_t dynamicViewportSizeUpdateID)
{
    TemporaryChange<bool> dynamicSizeUpdateGuard(m_inDynamicSizeUpdate, true);
    // FIXME: this does not handle the cases where the content would change the content size or scroll position from JavaScript.
    // To handle those cases, we would need to redo this computation on every change until the next visible content rect update.

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

    resetTextAutosizingBeforeLayoutIfNeeded(m_viewportConfiguration.minimumLayoutSize(), minimumLayoutSize);
    m_viewportConfiguration.setMinimumLayoutSize(minimumLayoutSize);
    IntSize newLayoutSize = m_viewportConfiguration.layoutSize();

    setFixedLayoutSize(newLayoutSize);
    setMaximumUnobscuredSize(maximumUnobscuredSize);

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

            bool likelyResponsiveDesignViewport = newLayoutSize.width() == minimumLayoutSize.width() && areEssentiallyEqualAsFloat(scale, 1);
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

    frameView.setScrollVelocity(0, 0, 0, monotonicallyIncreasingTime());

    IntPoint roundedUnobscuredContentRectPosition = roundedIntPoint(newUnobscuredContentRect.location());
    frameView.setUnobscuredContentSize(newUnobscuredContentRect.size());
    m_drawingArea->setExposedContentRect(newExposedContentRect);

    scalePage(scale, roundedUnobscuredContentRectPosition);

    frameView.updateLayoutAndStyleIfNeededRecursive();
    IntRect fixedPositionLayoutRect = enclosingIntRect(frameView.viewportConstrainedObjectsRect());
    frameView.setCustomFixedPositionLayoutRect(fixedPositionLayoutRect);

    frameView.setCustomSizeForResizeEvent(expandedIntSize(targetUnobscuredRectInScrollViewCoordinates.size()));
    setDeviceOrientation(deviceOrientation);
    frameView.setScrollOffset(roundedUnobscuredContentRectPosition);

    m_drawingArea->scheduleCompositingLayerFlush();

    send(Messages::WebPageProxy::DynamicViewportUpdateChangedTarget(pageScaleFactor(), frameView.scrollPosition(), dynamicViewportSizeUpdateID));
}

void WebPage::synchronizeDynamicViewportUpdate(double& newTargetScale, FloatPoint& newScrollPosition, uint64_t& nextValidLayerTreeTransactionID)
{
    newTargetScale = pageScaleFactor();
    newScrollPosition = m_page->mainFrame().view()->scrollPosition();
    nextValidLayerTreeTransactionID = downcast<RemoteLayerTreeDrawingArea>(*m_drawingArea).nextTransactionID();
}

void WebPage::resetViewportDefaultConfiguration(WebFrame* frame)
{
    if (m_useTestingViewportConfiguration) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::testingParameters());
        return;
    }

    if (!frame) {
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::webpageParameters());
        return;
    }

    Document* document = frame->coreFrame()->document();
    if (document->isImageDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::imageDocumentParameters());
    else if (document->isTextDocument())
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::textDocumentParameters());
    else
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::webpageParameters());
}

void WebPage::viewportConfigurationChanged()
{
    setFixedLayoutSize(m_viewportConfiguration.layoutSize());

    double initialScale = m_viewportConfiguration.initialScale();
    double scale;
    if (m_userHasChangedPageScaleFactor)
        scale = std::max(std::min(pageScaleFactor(), m_viewportConfiguration.maximumScale()), m_viewportConfiguration.minimumScale());
    else
        scale = initialScale;

    m_page->setZoomedOutPageScaleFactor(m_viewportConfiguration.minimumScale());

    updateViewportSizeForCSSViewportUnits();

    FrameView& frameView = *mainFrameView();
    IntPoint scrollPosition = frameView.scrollPosition();
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        FloatSize minimumLayoutSizeInScrollViewCoordinates = m_viewportConfiguration.minimumLayoutSize();
        minimumLayoutSizeInScrollViewCoordinates.scale(1 / scale);
        IntSize minimumLayoutSizeInDocumentCoordinates = roundedIntSize(minimumLayoutSizeInScrollViewCoordinates);
        frameView.setUnobscuredContentSize(minimumLayoutSizeInDocumentCoordinates);
        frameView.setScrollVelocity(0, 0, 0, monotonicallyIncreasingTime());

        // FIXME: We could send down the obscured margins to find a better exposed rect and unobscured rect.
        // It is not a big deal at the moment because the tile coverage will always extend past the obscured bottom inset.
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
    FloatSize largestUnobscuredRect = m_maximumUnobscuredSize;
    if (largestUnobscuredRect.isEmpty())
        largestUnobscuredRect = m_viewportConfiguration.minimumLayoutSize();

    FrameView& frameView = *mainFrameView();
    largestUnobscuredRect.scale(1 / m_viewportConfiguration.initialScaleIgnoringContentSize());
    frameView.setViewportSizeForCSSViewportUnits(roundedIntSize(largestUnobscuredRect));
}

void WebPage::applicationWillResignActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillResignActiveNotification object:nil];
}

void WebPage::volatilityTimerFired()
{
    if (!markLayersVolatileImmediatelyIfPossible())
        return;

    m_volatilityTimer.stop();
}

void WebPage::applicationDidEnterBackground(bool isSuspendedUnderLock)
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidEnterBackgroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": [NSNumber numberWithBool:isSuspendedUnderLock]}];

    setLayerTreeStateIsFrozen(true);
    if (markLayersVolatileImmediatelyIfPossible())
        return;

    m_volatilityTimer.startRepeating(std::chrono::milliseconds(200));
}

void WebPage::applicationWillEnterForeground(bool isSuspendedUnderLock)
{
    m_volatilityTimer.stop();
    setLayerTreeStateIsFrozen(false);

    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillEnterForegroundNotification object:nil userInfo:@{@"isSuspendedUnderLock": @(isSuspendedUnderLock)}];
}

void WebPage::applicationDidBecomeActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidBecomeActiveNotification object:nil];
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

static inline FloatRect adjustExposedRectForBoundedScale(const FloatRect& exposedRect, double exposedRectScale, double newScale)
{
    if (exposedRectScale < newScale)
        return exposedRect;

    return adjustExposedRectForNewScale(exposedRect, exposedRectScale, newScale);
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo, double oldestTimestamp)
{
    // Skip any VisibleContentRectUpdate that have been queued before DidCommitLoad suppresses the updates in the UIProcess.
    if (visibleContentRectUpdateInfo.lastLayerTreeTransactionID() < m_mainFrame->firstLayerTreeTransactionIDAfterDidCommitLoad())
        return;

    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = true;
    m_isInStableState = visibleContentRectUpdateInfo.inStableState();

    double scaleNoiseThreshold = 0.005;
    double filteredScale = visibleContentRectUpdateInfo.scale();
    double currentScale = m_page->pageScaleFactor();
    if (!m_isInStableState && fabs(filteredScale - m_page->pageScaleFactor()) < scaleNoiseThreshold) {
        // Tiny changes of scale during interactive zoom cause content to jump by one pixel, creating
        // visual noise. We filter those useless updates.
        filteredScale = currentScale;
    }

    double boundedScale = std::min(m_viewportConfiguration.maximumScale(), std::max(m_viewportConfiguration.minimumScale(), filteredScale));

    // Skip progressively redrawing tiles if pinch-zooming while the system is under memory pressure.
    if (boundedScale != currentScale && !m_isInStableState && MemoryPressureHandler::singleton().isUnderMemoryPressure())
        return;

    if (m_isInStableState)
        m_hasStablePageScaleFactor = true;
    else {
        if (m_oldestNonStableUpdateVisibleContentRectsTimestamp == std::chrono::milliseconds::zero())
            m_oldestNonStableUpdateVisibleContentRectsTimestamp = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(oldestTimestamp * 1000));
    }

    FloatRect exposedRect = visibleContentRectUpdateInfo.exposedRect();
    FloatRect adjustedExposedRect = adjustExposedRectForBoundedScale(exposedRect, visibleContentRectUpdateInfo.scale(), boundedScale);
    m_drawingArea->setExposedContentRect(adjustedExposedRect);

    IntPoint scrollPosition = roundedIntPoint(visibleContentRectUpdateInfo.unobscuredRect().location());

    float floatBoundedScale = boundedScale;
    bool hasSetPageScale = false;
    if (floatBoundedScale != currentScale) {
        m_scaleWasSetByUIProcess = true;
        m_hasStablePageScaleFactor = m_isInStableState;

        m_dynamicSizeUpdateHistory.clear();

        m_page->setPageScaleFactor(floatBoundedScale, scrollPosition, m_isInStableState);
        hasSetPageScale = true;
        send(Messages::WebPageProxy::PageScaleFactorDidChange(floatBoundedScale));
    }

    if (!hasSetPageScale && m_isInStableState) {
        m_page->setPageScaleFactor(floatBoundedScale, scrollPosition, true);
        hasSetPageScale = true;
    }

    FrameView& frameView = *m_page->mainFrame().view();
    if (scrollPosition != frameView.scrollPosition())
        m_dynamicSizeUpdateHistory.clear();

    if (m_viewportConfiguration.setCanIgnoreScalingConstraints(m_ignoreViewportScalingConstraints && visibleContentRectUpdateInfo.allowShrinkToFit()))
        viewportConfigurationChanged();

    frameView.setUnobscuredContentSize(visibleContentRectUpdateInfo.unobscuredRect().size());

    double horizontalVelocity = visibleContentRectUpdateInfo.horizontalVelocity();
    double verticalVelocity = visibleContentRectUpdateInfo.verticalVelocity();
    double scaleChangeRate = visibleContentRectUpdateInfo.scaleChangeRate();
    adjustVelocityDataForBoundedScale(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.scale(), m_viewportConfiguration.minimumScale(), m_viewportConfiguration.maximumScale());

    frameView.setViewportIsStable(m_isInStableState);
    frameView.setScrollVelocity(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.timestamp());

    if (m_isInStableState)
        frameView.setCustomFixedPositionLayoutRect(enclosingIntRect(visibleContentRectUpdateInfo.customFixedPositionRect()));

    if (!visibleContentRectUpdateInfo.isChangingObscuredInsetsInteractively())
        frameView.setCustomSizeForResizeEvent(expandedIntSize(visibleContentRectUpdateInfo.unobscuredRectInScrollViewCoordinates().size()));

    frameView.setConstrainsScrollingToContentEdge(false);
    frameView.setScrollOffset(frameView.scrollOffsetFromPosition(scrollPosition));
    frameView.setConstrainsScrollingToContentEdge(true);
}

void WebPage::willStartUserTriggeredZooming()
{
    m_page->mainFrame().diagnosticLoggingClient().logDiagnosticMessageWithValue(DiagnosticLoggingKeys::webViewKey(), DiagnosticLoggingKeys::userKey(), DiagnosticLoggingKeys::zoomedKey(), ShouldSample::No);
    m_userHasChangedPageScaleFactor = true;
}

#if ENABLE(WEBGL)
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame*, const String&)
{
    return WKShouldBlockWebGL() ? WebGLBlockCreation : WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame*, const String&)
{
    return WKShouldBlockWebGL() ? WebGLBlockCreation : WebGLAllowCreation;
}
#endif

void WebPage::zoomToRect(FloatRect rect, double minimumScale, double maximumScale)
{
    send(Messages::WebPageProxy::ZoomToRect(rect, minimumScale, maximumScale));
}

#if ENABLE(IOS_TOUCH_EVENTS)
void WebPage::dispatchAsynchronousTouchEvents(const Vector<WebTouchEvent, 1>& queue)
{
    bool ignored;
    for (const WebTouchEvent& event : queue)
        dispatchTouchEvent(event, ignored);
}
#endif

void WebPage::computePagesForPrintingAndStartDrawingToPDF(uint64_t frameID, const PrintInfo& printInfo, uint32_t firstPage, PassRefPtr<Messages::WebPage::ComputePagesForPrintingAndStartDrawingToPDF::DelayedReply> reply)
{
    Vector<WebCore::IntRect> pageRects;
    double totalScaleFactor = 1;
    computePagesForPrintingImpl(frameID, printInfo, pageRects, totalScaleFactor);
    std::size_t pageCount = pageRects.size();
    reply->send(WTFMove(pageRects), totalScaleFactor);

    RetainPtr<CFMutableDataRef> pdfPageData;
    drawPagesToPDFImpl(frameID, printInfo, firstPage, pageCount - firstPage, pdfPageData);
    send(Messages::WebPageProxy::DidFinishDrawingPagesToPDF(IPC::DataReference(CFDataGetBytePtr(pdfPageData.get()), CFDataGetLength(pdfPageData.get()))));
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

} // namespace WebKit

#endif // PLATFORM(IOS)
