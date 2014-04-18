/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#import "AssistedNodeInformation.h"
#import "DataReference.h"
#import "DrawingArea.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "GestureTypes.h"
#import "InjectedBundleUserMessageCoders.h"
#import "InteractionInformationAtPosition.h"
#import "PluginView.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import "WebChromeClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebFrame.h"
#import "WebKitSystemInterface.h"
#import "WebKitSystemInterfaceIOS.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <CoreText/CTFont.h>
#import <WebCore/Chrome.h>
#import <WebCore/Element.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/HTMLElementTypeHelpers.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLTextAreaElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MainFrame.h>
#import <WebCore/MediaSessionManagerIOS.h>
#import <WebCore/Node.h>
#import <WebCore/NotImplemented.h>
#import <WebCore/Page.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PlatformMouseEvent.h>
#import <WebCore/RenderBlock.h>
#import <WebCore/RenderImage.h>
#import <WebCore/ResourceBuffer.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WKContentObservation.h>
#import <WebCore/WebEvent.h>

using namespace WebCore;

namespace WebKit {

const int blockSelectionStartWidth = 100;
const int blockSelectionStartHeight = 100;

void WebPage::platformInitialize()
{
    platformInitializeAccessibility();
}

void WebPage::platformInitializeAccessibility()
{
    m_mockAccessibilityElement = adoptNS([[WKAccessibilityWebPageObject alloc] init]);
    [m_mockAccessibilityElement setWebPage:this];
    
    RetainPtr<CFUUIDRef> uuid = adoptCF(CFUUIDCreate(kCFAllocatorDefault));
    NSData *remoteToken = WKAXRemoteToken(uuid.get());
    
    IPC::DataReference dataToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
}
    
void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
    notImplemented();
}

FloatSize WebPage::viewportScreenSize() const
{
    return m_viewportScreenSize;
}

void WebPage::viewportPropertiesDidChange(const ViewportArguments& viewportArguments)
{
    m_viewportConfiguration.setViewportArguments(viewportArguments);
    viewportConfigurationChanged();
}

void WebPage::didReceiveMobileDocType(bool isMobileDoctype)
{
    if (isMobileDoctype)
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::xhtmlMobileParameters());
    else
        m_viewportConfiguration.setDefaultConfiguration(ViewportConfiguration::webpageParameters());
}

double WebPage::minimumPageScaleFactor() const
{
    return m_viewportConfiguration.minimumScale();
}

double WebPage::maximumPageScaleFactor() const
{
    return m_viewportConfiguration.maximumScale();
}

bool WebPage::allowsUserScaling() const
{
    return m_viewportConfiguration.allowsUserScaling();
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    // FIXME: Interpret the event immediately upon receiving it in UI process, without sending to WebProcess first.
    bool eventWasHandled = false;
    bool sendResult = WebProcess::shared().parentProcessConnection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(editorState(), event->keyEvent()->type() == PlatformKeyboardEvent::Char),
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

void WebPage::performDictionaryLookupForSelection(Frame*, const VisibleSelection&)
{
    notImplemented();
}

void WebPage::performDictionaryLookupForRange(Frame*, Range&, NSDictionary *)
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

void WebPage::handleTap(const IntPoint& point)
{
    Frame& mainframe = m_page->mainFrame();
    FloatPoint adjustedPoint;
    mainframe.nodeRespondingToClickEvents(point, adjustedPoint);
    IntPoint roundedAdjustedPoint = roundedIntPoint(adjustedPoint);

    WKBeginObservingContentChanges(true);
    mainframe.eventHandler().mouseMoved(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, NoButton, PlatformEvent::MouseMoved, 0, false, false, false, false, 0));
    mainframe.document()->updateStyleIfNeeded();
    WKStopObservingContentChanges();
    if (WKObservedContentChange() != WKContentNoChange)
        return;

    m_lastInteractionLocation = roundedAdjustedPoint;
    mainframe.eventHandler().handleMousePressEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MousePressed, 1, false, false, false, false, 0));
    mainframe.eventHandler().handleMouseReleaseEvent(PlatformMouseEvent(roundedAdjustedPoint, roundedAdjustedPoint, LeftButton, PlatformEvent::MouseReleased, 1, false, false, false, false, 0));
}

void WebPage::tapHighlightAtPosition(uint64_t requestID, const FloatPoint& position)
{
    Frame& mainframe = m_page->mainFrame();
    FloatPoint adjustedPoint;
    Node* node = mainframe.nodeRespondingToClickEvents(position, adjustedPoint);

    if (!node)
        return;

    RenderObject *renderer = node->renderer();

    Vector<FloatQuad> quads;
    if (renderer) {
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
        if (renderer->isBox()) {
            RenderBox* box = toRenderBox(renderer);
            borderRadii = box->borderRadii();
        }

        send(Messages::WebPageProxy::DidGetTapHighlightGeometries(requestID, highlightColor, quads, roundedIntSize(borderRadii.topLeft()), roundedIntSize(borderRadii.topRight()), roundedIntSize(borderRadii.bottomLeft()), roundedIntSize(borderRadii.bottomRight())));
    }
}

void WebPage::blurAssistedNode()
{
    if (m_assistedNode && m_assistedNode->isElementNode())
        toElement(m_assistedNode.get())->blur();
}

void WebPage::setAssistedNodeValue(const String& value)
{
    if (!m_assistedNode)
        return;
    if (isHTMLInputElement(m_assistedNode.get())) {
        HTMLInputElement *element = toHTMLInputElement(m_assistedNode.get());
        element->setValue(value, DispatchInputAndChangeEvent);
    }
    // FIXME: should also handle the case of HTMLSelectElement.
}

void WebPage::setAssistedNodeValueAsNumber(double value)
{
    if (!m_assistedNode)
        return;
    if (isHTMLInputElement(m_assistedNode.get())) {
        HTMLInputElement *element = toHTMLInputElement(m_assistedNode.get());
        element->setValueAsNumber(value, ASSERT_NO_EXCEPTION, DispatchInputAndChangeEvent);
    }
}

void WebPage::setAssistedNodeSelectedIndex(uint32_t index, bool allowMultipleSelection)
{
    if (!m_assistedNode || !isHTMLSelectElement(m_assistedNode.get()))
        return;
    HTMLSelectElement* select = toHTMLSelectElement(m_assistedNode.get());
    select->optionSelectedByUser(index, true, allowMultipleSelection);
}

#if ENABLE(INSPECTOR)
void WebPage::showInspectorIndication()
{
    send(Messages::WebPageProxy::ShowInspectorIndication());
}

void WebPage::hideInspectorIndication()
{
    send(Messages::WebPageProxy::HideInspectorIndication());
}
#endif

static FloatQuad innerFrameQuad(Frame* frame, Node* assistedNode)
{
    frame->document()->updateLayoutIgnorePendingStylesheets();
    RenderObject* renderer;
    if (assistedNode->hasTagName(HTMLNames::textareaTag) || assistedNode->hasTagName(HTMLNames::inputTag) || assistedNode->hasTagName(HTMLNames::selectTag))
        renderer = assistedNode->renderer();
    else
        renderer = assistedNode->rootEditableElement()->renderer();
    
    if (!renderer)
        return FloatQuad();

    RenderStyle& style = renderer->style();
    IntRect boundingBox = renderer->absoluteBoundingBoxRect(true /* use transforms*/);

    boundingBox.move(style.borderLeftWidth(), style.borderTopWidth());
    boundingBox.setWidth(boundingBox.width() - style.borderLeftWidth() - style.borderRightWidth());
    boundingBox.setHeight(boundingBox.height() - style.borderBottomWidth() - style.borderTopWidth());

    return FloatQuad(boundingBox);
}

static IntPoint constrainPoint(const IntPoint& point, Frame* frame, Node* assistedNode)
{
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

PassRefPtr<Range> WebPage::rangeForWebSelectionAtPosition(const IntPoint& point, const VisiblePosition& position, SelectionFlags& flags)
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent | HitTestRequest::AllowChildFrameContent);

    Node* currentNode = result.innerNode();
    RefPtr<Range> range;
    IntRect boundingRect;

    if (currentNode->isTextNode()) {
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        if (!range || range->collapsed(ASSERT_NO_EXCEPTION))
            range = Range::create(currentNode->document(), position, position);
        if (!range)
            return nullptr;

        Vector<SelectionRect> selectionRects;
        range->collectSelectionRects(selectionRects);
        unsigned size = selectionRects.size();

        for (unsigned i = 0; i < size; i++) {
            const IntRect &coreRect = selectionRects[i].rect();
            if (i == 0)
                boundingRect = coreRect;
            else
                boundingRect.unite(coreRect);
        }
        if (boundingRect.width() > m_blockSelectionDesiredSize.width() && boundingRect.height() > m_blockSelectionDesiredSize.height())
            return wordRangeFromPosition(position);

        currentNode = range->commonAncestorContainer(ASSERT_NO_EXCEPTION);
    }

    if (!currentNode->isElementNode())
        currentNode = currentNode->parentElement();

    Node* bestChoice = currentNode;
    while (currentNode) {
        boundingRect = currentNode->renderer()->absoluteBoundingBoxRect(true);
        if (boundingRect.width() > m_blockSelectionDesiredSize.width() && boundingRect.height() > m_blockSelectionDesiredSize.height())
            break;
        bestChoice = currentNode;
        currentNode = currentNode->parentElement();
    }

    if (!bestChoice)
        return nullptr;

    RenderObject* renderer = bestChoice->renderer();
    if (renderer && renderer->childrenInline() && (renderer->isRenderBlock() && toRenderBlock(renderer)->inlineElementContinuation() == nil) && !renderer->isTable()) {
        range = enclosingTextUnitOfGranularity(position, WordGranularity, DirectionBackward);
        if (range && !range->collapsed(ASSERT_NO_EXCEPTION))
            return range;
    }
    flags = IsBlockSelection;
    range = Range::create(bestChoice->document());
    range->selectNodeContents(bestChoice, ASSERT_NO_EXCEPTION);
    return range->collapsed(ASSERT_NO_EXCEPTION) ? nullptr : range;
}

PassRefPtr<Range> WebPage::rangeForBlockAtPoint(const IntPoint& point)
{
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint((point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);

    Node* currentNode = result.innerNode();
    RefPtr<Range> range;

    if (currentNode->isTextNode()) {
        range = enclosingTextUnitOfGranularity(m_page->focusController().focusedOrMainFrame().visiblePositionForPoint(point), ParagraphGranularity, DirectionForward);
        if (range && !range->collapsed(ASSERT_NO_EXCEPTION))
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

void WebPage::selectWithGesture(const IntPoint& point, uint32_t granularity, uint32_t gestureType, uint32_t gestureState, uint64_t callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    IntPoint adjustedPoint(frame.view()->rootViewToContents(point));

    IntPoint constrainedPoint = m_assistedNode ? constrainPoint(adjustedPoint, &frame, m_assistedNode.get()) : adjustedPoint;
    VisiblePosition position = frame.visiblePositionForPoint(constrainedPoint);
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

            result = wordRange->startPosition();
            if (distanceBetweenPositions(position, result) > 1)
                result = wordRange->endPosition();
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
        range = Range::create(*frame.document(), position, position);
        break;

    case GestureType::TapAndAHalf:
        switch (wkGestureState) {
        case GestureRecognizerState::Began:
            range = wordRangeFromPosition(position);
            m_currentWordRange = Range::create(*frame.document(), range->startPosition(), range->endPosition());
            break;
        case GestureRecognizerState::Changed:
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
    return (first->commonAncestorContainer(ASSERT_NO_EXCEPTION)->ownerDocument() == second->commonAncestorContainer(ASSERT_NO_EXCEPTION)->ownerDocument()
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

    return Range::create(rangeA->ownerDocument(), start->startContainer(), start->startOffset(), end->endContainer(), end->endOffset());
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
    // FIXME: We should use boundingRect() instead of boundingBox() in this function when <rdar://problem/16063723> is fixed.
    IntRect currentBox = currentRange->boundingBox();
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

        RefPtr<Range> newRange;
        RefPtr<Range> rangeAtPosition = rangeForBlockAtPoint(testPoint);
        if (containsRange(rangeAtPosition.get(), currentRange))
            newRange = rangeAtPosition;
        else if (containsRange(currentRange, rangeAtPosition.get()))
            newRange = currentRange;
        else
            newRange = unionDOMRanges(currentRange, rangeAtPosition.get());

        IntRect copyRect = newRange->boundingBox();

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

        distance = ceilf(distance * multiple);
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

    // FIXME: We should use boundingRect() instead of boundingBox() in this function when <rdar://problem/16063723> is fixed.
    IntRect currentBox = currentRange->boundingBox();
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

        RefPtr<Range> newRange = rangeForBlockAtPoint(testPoint);
        if (handlePosition == SelectionHandlePosition::Top || handlePosition == SelectionHandlePosition::Left)
            newRange = Range::create(newRange->startContainer()->document(), newRange->startPosition(), currentRange->endPosition());
        else
            newRange = Range::create(newRange->startContainer()->document(), currentRange->startPosition(), newRange->endPosition());

        IntRect copyRect = newRange->boundingBox();
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

        distance *= multiple;
    }

    // If we can shrink down to text only, the only reason we wouldn't is that
    // there are multiple sub-element blocks beneath us.  If we didn't find
    // multiple sub-element blocks, don't shrink to a sub-element block.
    if (!bestRange) {
        bestRange = currentRange;
        Node* node = currentRange->commonAncestorContainer(ASSERT_NO_EXCEPTION);
        if (!node->isElementNode())
            node = node->parentElement();

        RenderObject* renderer = node->renderer();
        if (renderer && renderer->childrenInline() && (renderer->isRenderBlock() && !toRenderBlock(renderer)->inlineElementContinuation()) && !renderer->isTable()) {
            flags = None;
        }
    }

    return bestRange;
}

void WebPage::computeExpandAndShrinkThresholdsForHandle(const IntPoint& point, SelectionHandlePosition handlePosition, float& growThreshold, float& shrinkThreshold)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> currentRange = m_currentBlockSelection ? m_currentBlockSelection.get() : frame.selection().selection().toNormalizedRange();
    ASSERT(currentRange);

    RefPtr<Range> expandedRange = expandedRangeFromHandle(currentRange.get(), handlePosition);
    SelectionFlags flags;
    RefPtr<Range> contractedRange = contractedRangeFromHandle(currentRange.get(), handlePosition, flags);

    // FIXME: We should use boundingRect() instead of boundingBox() in this function when <rdar://problem/16063723> is fixed.
    IntRect currentBounds = currentRange->boundingBox();
    IntRect expandedBounds = expandedRange->boundingBox();
    IntRect contractedBounds = contractedRange->boundingBox();

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
    // FIXME: We should use boundingRect() instead of boundingBox() in this function when <rdar://problem/16063723> is fixed.
    IntRect currentRect = currentRange->boundingBox();

    RefPtr<Range> newRange = shouldExpand(handlePosition, currentRect, point) ? expandedRangeFromHandle(currentRange.get(), handlePosition) : contractedRangeFromHandle(currentRange.get(), handlePosition, flags);

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
        selectedText = plainText(frame.selection().selection().toNormalizedRange().get());

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
            contextBefore = plainText(Range::create(*frame.document(), lastPosition, startPosition).get());
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
            contextAfter = plainText(Range::create(*frame.document(), endPosition, lastPosition).get());
    }

    send(Messages::WebPageProxy::DictationContextCallback(selectedText, contextBefore, contextAfter, callbackID));
}

void WebPage::replaceSelectedText(const String& oldText, const String& newText)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isRange())
        return;

    if (plainText(frame.selection().toNormalizedRange().get()) != oldText)
        return;

    frame.editor().insertText(newText, 0);
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

    if (plainText(range.get()) != oldText)
        return;

    // We don't want to notify the client that the selection has changed until we are done inserting the new text.
    frame.editor().setIgnoreCompositionSelectionChange(true);
    frame.selection().setSelectedRange(range.get(), UPSTREAM, true);
    frame.editor().insertText(newText, 0);
    frame.editor().setIgnoreCompositionSelectionChange(false);
}

void WebPage::requestAutocorrectionData(const String& textForAutocorrection, uint64_t callbackID)
{
    RefPtr<Range> range;
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    ASSERT(frame.selection().isCaret());
    VisiblePosition position = frame.selection().selection().start();
    Vector<SelectionRect> selectionRects;

    range = wordRangeFromPosition(position);
    String textForRange = plainText(range.get());
    const unsigned maxSearchAttempts = 5;
    for (size_t i = 0;  i < maxSearchAttempts && textForRange != textForAutocorrection; ++i)
    {
        position = range->startPosition().previous();
        if (position.isNull() || position == range->startPosition())
            break;
        range = Range::create(*frame.document(), wordRangeFromPosition(position)->startPosition(), range->endPosition());
        textForRange = plainText(range.get());
    }
    if (textForRange == textForAutocorrection)
        range->collectSelectionRects(selectionRects);

    Vector<FloatRect> rectsForText;
    rectsForText.resize(selectionRects.size());

    convertSelectionRectsToRootView(frame.view(), selectionRects);
    for (size_t i = 0; i < selectionRects.size(); i++)
        rectsForText[i] = selectionRects[i].rect();

    bool multipleFonts = false;
    CTFontRef font = nil;
    if (const SimpleFontData* fontData = frame.editor().fontForSelection(multipleFonts))
        font = fontData->getCTFont();

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

void WebPage::syncApplyAutocorrection(const String& correction, const String& originalText, bool& correctionApplied)
{
    RefPtr<Range> range;
    correctionApplied = false;
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (!frame.selection().isCaret())
        return;
    VisiblePosition position = frame.selection().selection().start();
    
    range = wordRangeFromPosition(position);
    String textForRange = plainText(range.get());
    if (textForRange != originalText) {
        for (size_t i = 0; i < originalText.length(); ++i)
            position = position.previous();
        if (position.isNull())
            position = startOfDocument(static_cast<Node*>(frame.document()->documentElement()));
        range = Range::create(*frame.document(), position, frame.selection().selection().start());
        if (range)
            textForRange = (range) ? plainText(range.get()) : emptyString();
        unsigned loopCount = 0;
        const unsigned maxPositionsAttempts = 10;
        while (textForRange.length() && textForRange.length() > originalText.length() && loopCount < maxPositionsAttempts) {
            position = position.next();
            if (position.isNotNull() && position >= frame.selection().selection().start())
                range = NULL;
            else
                range = Range::create(*frame.document(), position, frame.selection().selection().start());
            textForRange = (range) ? plainText(range.get()) : emptyString();
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
        selectedText = plainText(frame.selection().selection().toNormalizedRange().get());

    if (frame.editor().hasComposition()) {
        range = Range::create(*frame.document(), frame.editor().compositionRange()->startPosition(), startPosition);
        String markedTextBefore;
        if (range)
            markedTextBefore = plainText(range.get());
        range = Range::create(*frame.document(), endPosition, frame.editor().compositionRange()->endPosition());
        String markedTextAfter;
        if (range)
            markedTextAfter = plainText(range.get());
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
                String currentWord = plainText(Range::create(*frame.document(), previousPosition, currentPosition).get());
                totalContextLength += currentWord.length();
                if (totalContextLength >= maxContextLength)
                    break;
                currentPosition = previousPosition;
            }
            if (currentPosition.isNotNull() && currentPosition != startPosition) {
                contextBefore = plainText(Range::create(*frame.document(), currentPosition, startPosition).get());
                if (atBoundaryOfGranularity(currentPosition, ParagraphGranularity, DirectionBackward))
                    contextBefore = ASCIILiteral("\n ") + contextBefore;
            }
        }

        if (endPosition != endOfEditableContent(endPosition)) {
            VisiblePosition nextPosition;
            if (!atBoundaryOfGranularity(endPosition, WordGranularity, DirectionForward) && withinTextUnitOfGranularity(endPosition, WordGranularity, DirectionForward))
                nextPosition = positionOfNextBoundaryOfGranularity(endPosition, WordGranularity, DirectionForward);
            if (nextPosition.isNotNull())
                contextAfter = plainText(Range::create(*frame.document(), endPosition, nextPosition).get());
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
    for (Element* currentElement = element; currentElement; currentElement = currentElement->parentElement())
        if (currentElement->isLink())
            return currentElement;
    return 0;
}

void WebPage::getPositionInformation(const IntPoint& point, InteractionInformationAtPosition& info)
{
    FloatPoint adjustedPoint;
    Node* hitNode = m_page->mainFrame().nodeRespondingToClickEvents(point, adjustedPoint);

    info.point = point;
    info.nodeAtPositionIsAssistedNode = (hitNode == m_assistedNode);
    if (m_assistedNode) {
        Frame& frame = m_page->focusController().focusedOrMainFrame();
        if (frame.editor().hasComposition()) {
            const uint32_t kHitAreaWidth = 66;
            const uint32_t kHitAreaHeight = 66;
            FrameView& view = *frame.view();
            IntPoint adjustedPoint(view.rootViewToContents(point));
            IntPoint constrainedPoint = m_assistedNode ? constrainPoint(adjustedPoint, &frame, m_assistedNode.get()) : adjustedPoint;
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
        info.clickableElementName = hitNode->nodeName();

        Element* element = hitNode->isElementNode() ? toElement(hitNode) : 0;
        if (element) {
            Element* linkElement = nullptr;
            if (element->renderer() && element->renderer()->isRenderImage()) {
                elementIsLinkOrImage = true;
                linkElement = containingLinkElement(element);
            } else if (element->isLink()) {
                linkElement = element;
                elementIsLinkOrImage = true;
            }
            if (linkElement)
                info.url = linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr)));
            info.title = element->fastGetAttribute(HTMLNames::titleAttr).string();
            if (element->renderer())
                info.bounds = element->renderer()->absoluteBoundingBoxRect(true);
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
    if (!m_interactionNode || !m_interactionNode->isHTMLElement())
        return;

    HTMLElement* element = toHTMLElement(m_interactionNode.get());
    if (!element->renderer())
        return;

    if (static_cast<SheetAction>(action) == SheetAction::Copy) {
        if (element->renderer()->isRenderImage()) {
            Element* linkElement = containingLinkElement(element);
        
            if (!linkElement)
                m_interactionNode->document().frame()->editor().writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), *element, toRenderImage(element->renderer())->cachedImage()->url(), String());
            else
                m_interactionNode->document().frame()->editor().copyURL(linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr))), linkElement->textContent());
        } else if (element->isLink()) {
            m_interactionNode->document().frame()->editor().copyURL(element->document().completeURL(stripLeadingAndTrailingHTMLSpaces(element->getAttribute(HTMLNames::hrefAttr))), element->textContent());
        }
    } else if (static_cast<SheetAction>(action) == SheetAction::SaveImage) {
        if (!element->renderer()->isRenderImage())
            return;
        CachedImage* cachedImage = toRenderImage(element->renderer())->cachedImage();
        if (cachedImage) {
            SharedMemory::Handle handle;
            RefPtr<SharedBuffer> buffer = cachedImage->resourceBuffer()->sharedBuffer();
            if (buffer) {
                uint64_t bufferSize = buffer->size();
                RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(bufferSize);
                memcpy(sharedMemoryBuffer->data(), buffer->data(), bufferSize);
                sharedMemoryBuffer->createHandle(handle, SharedMemory::ReadOnly);
                send(Messages::WebPageProxy::SaveImageToLibrary(handle, bufferSize));
            }
        }
    }
}

static inline bool isAssistableNode(Node* node)
{
    if (isHTMLSelectElement(node))
        return true;
    if (isHTMLTextAreaElement(node))
        return !toHTMLTextAreaElement(node)->isReadOnlyNode();
    if (isHTMLInputElement(node)) {
        HTMLInputElement* element = toHTMLInputElement(node);
        return !element->isReadOnlyNode() && (element->isTextField() || element->isDateField() || element->isDateTimeLocalField() || element->isMonthField() || element->isTimeField());
    }

    return node->isContentEditable();
}

static inline Element* nextFocusableElement(Node* startNode, Page* page, bool isForward)
{
    RefPtr<KeyboardEvent> key = KeyboardEvent::create();

    Element* nextElement = toElement(startNode);
    do {
        nextElement = isForward ? page->focusController().nextFocusableElement(FocusNavigationScope::focusNavigationScopeOf(&nextElement->document()), nextElement, key.get())
            : page->focusController().previousFocusableElement(FocusNavigationScope::focusNavigationScopeOf(&nextElement->document()), nextElement, key.get());
    } while (nextElement && !isAssistableNode(nextElement));

    return nextElement;
}

static inline bool hasFocusableElement(Node* startNode, Page* page, bool isForward)
{
    return nextFocusableElement(startNode, page, isForward) != nil;
}

void WebPage::focusNextAssistedNode(bool isForward)
{
    Element* nextElement = nextFocusableElement(m_assistedNode.get(), m_page.get(), isForward);
    if (nextElement)
        nextElement->focus();
}

void WebPage::getAssistedNodeInformation(AssistedNodeInformation& information)
{
    information.elementRect = m_page->focusController().focusedOrMainFrame().view()->contentsToRootView(m_assistedNode->renderer()->absoluteBoundingBoxRect());
    information.minimumScaleFactor = m_viewportConfiguration.minimumScale();
    information.maximumScaleFactor = m_viewportConfiguration.maximumScale();
    information.hasNextNode = hasFocusableElement(m_assistedNode.get(), m_page.get(), true);
    information.hasPreviousNode = hasFocusableElement(m_assistedNode.get(), m_page.get(), false);

    if (isHTMLSelectElement(m_assistedNode.get())) {
        HTMLSelectElement* element = toHTMLSelectElement(m_assistedNode.get());
        information.elementType = WKTypeSelect;
        size_t count = element->listItems().size();
        int parentGroupID = 0;
        // The parent group ID indicates the group the option belongs to and is 0 for group elements.
        // If there are option elements in between groups, they are given it's own group identifier.
        // If a select does not have groups, all the option elements have group ID 0.
        for (size_t i = 0; i < count; ++i) {
            HTMLElement* item = element->listItems()[i];
            if (isHTMLOptionElement(item)) {
                HTMLOptionElement* option = toHTMLOptionElement(item);
                information.selectOptions.append(WKOptionItem(option->text(), false, parentGroupID, option->selected(), option->fastHasAttribute(WebCore::HTMLNames::disabledAttr)));
            } else if (isHTMLOptGroupElement(item)) {
                HTMLOptGroupElement* group = toHTMLOptGroupElement(item);
                parentGroupID++;
                information.selectOptions.append(WKOptionItem(group->groupLabelText(), true, 0, false, group->fastHasAttribute(WebCore::HTMLNames::disabledAttr)));
            }
        }
        information.selectedIndex = element->selectedIndex();
        information.isMultiSelect = element->multiple();
    } else if (isHTMLTextAreaElement(m_assistedNode.get())) {
        HTMLTextAreaElement* element = toHTMLTextAreaElement(m_assistedNode.get());
        information.autocapitalizeType = static_cast<WebAutocapitalizeType>(element->autocapitalizeType());
        information.isAutocorrect = element->autocorrect();
        information.elementType = WKTypeTextArea;
        information.isReadOnly = element->isReadOnly();
        information.value = element->value();
    } else if (isHTMLInputElement(m_assistedNode.get())) {
        HTMLInputElement* element = toHTMLInputElement(m_assistedNode.get());
        HTMLFormElement* form = element->form();
        if (form)
            information.formAction = form->getURLAttribute(WebCore::HTMLNames::actionAttr);
        information.autocapitalizeType = static_cast<WebAutocapitalizeType>(element->autocapitalizeType());
        information.isAutocorrect = element->autocorrect();
        if (element->isPasswordField())
            information.elementType = WKTypePassword;
        else if (element->isSearchField())
            information.elementType = WKTypeSearch;
        else if (element->isEmailField())
            information.elementType = WKTypeEmail;
        else if (element->isTelephoneField())
            information.elementType = WKTypePhone;
        else if (element->isNumberField())
            information.elementType = element->getAttribute("pattern") == "\\d*" || element->getAttribute("pattern") == "[0-9]*" ? WKTypeNumberPad : WKTypeNumber;
        else if (element->isDateTimeLocalField())
            information.elementType = WKTypeDateTimeLocal;
        else if (element->isDateField())
            information.elementType = WKTypeDate;
        else if (element->isDateTimeField())
            information.elementType = WKTypeDateTime;
        else if (element->isTimeField())
            information.elementType = WKTypeTime;
        else if (element->isWeekField())
            information.elementType = WKTypeWeek;
        else if (element->isMonthField())
            information.elementType = WKTypeMonth;
        else if (element->isURLField())
            information.elementType = WKTypeURL;
        else if (element->isText())
            information.elementType = element->getAttribute("pattern") == "\\d*" || element->getAttribute("pattern") == "[0-9]*" ? WKTypeNumberPad : WKTypeText;

        information.isReadOnly = element->isReadOnly();
        information.value = element->value();
        information.valueAsNumber = element->valueAsNumber();
        information.title = element->title();
    } else if (m_assistedNode->hasEditableStyle()) {
        information.elementType = WKTypeContentEditable;
        information.isAutocorrect = true;   // FIXME: Should we look at the attribute?
        information.autocapitalizeType = WebAutocapitalizeTypeSentences; // FIXME: Should we look at the attribute?
        information.isReadOnly = false;
    }
}

void WebPage::elementDidFocus(WebCore::Node* node)
{
    if (node->hasTagName(WebCore::HTMLNames::selectTag) || node->hasTagName(WebCore::HTMLNames::inputTag) || node->hasTagName(WebCore::HTMLNames::textareaTag) || node->hasEditableStyle()) {
        m_assistedNode = node;
        AssistedNodeInformation information;
        getAssistedNodeInformation(information);
        RefPtr<API::Object> userData;
        m_formClient->willBeginInputSession(this, toElement(node), WebFrame::fromCoreFrame(*node->document().frame()), userData);
        send(Messages::WebPageProxy::StartAssistingNode(information, InjectedBundleUserMessageEncoder(userData.get())));
    }
}

void WebPage::elementDidBlur(WebCore::Node* node)
{
    if (m_assistedNode == node) {
        send(Messages::WebPageProxy::StopAssistingNode());
        m_assistedNode = 0;
    }
}

void WebPage::setViewportConfigurationMinimumLayoutSize(const IntSize& size)
{
    m_viewportConfiguration.setMinimumLayoutSize(size);
    viewportConfigurationChanged();
}

void WebPage::dynamicViewportSizeUpdate(const IntSize& minimumLayoutSize, const FloatRect& targetExposedContentRect, const FloatRect& targetUnobscuredRect, double targetScale)
{
    // FIXME: this does not handle the cases where the content would change the content size or scroll position from JavaScript.
    // To handle those cases, we would need to redo this computation on every change until the next visible content rect update.

    FrameView& frameView = *m_page->mainFrame().view();
    IntSize oldContentSize = frameView.contentsSize();

    m_viewportConfiguration.setMinimumLayoutSize(minimumLayoutSize);
    IntSize newLayoutSize = m_viewportConfiguration.layoutSize();
    setFixedLayoutSize(newLayoutSize);
    frameView.updateLayoutAndStyleIfNeededRecursive();

    IntSize newContentSize = frameView.contentsSize();

    double scale;
    if (!m_userHasChangedPageScaleFactor)
        scale = m_viewportConfiguration.initialScale();
    else
        scale = std::max(std::min(targetScale, m_viewportConfiguration.maximumScale()), m_viewportConfiguration.minimumScale());

    FloatRect newUnobscuredContentRect = targetUnobscuredRect;
    FloatRect newExposedContentRect = targetExposedContentRect;

    if (scale != targetScale) {
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

        // FIXME: Adjust the rects based on the content.
    }

    if (oldContentSize != newContentSize || scale != targetScale) {
        // Snap the new unobscured rect back into the content rect.
        newUnobscuredContentRect.setWidth(std::min(static_cast<float>(newContentSize.width()), newExposedContentRect.width()));
        newUnobscuredContentRect.setHeight(std::min(static_cast<float>(newContentSize.height()), newExposedContentRect.height()));

        if (oldContentSize != newContentSize) {
            // If the content size has changed, keep the same relative position.
            FloatPoint oldContentCenter = targetUnobscuredRect.center();
            float relativeHorizontalPosition = oldContentCenter.x() / oldContentSize.width();
            float relativeVerticalPosition =  oldContentCenter.y() / oldContentSize.height();
            FloatPoint newRelativeContentCenter(relativeHorizontalPosition * newContentSize.width(), relativeVerticalPosition * newContentSize.height());
            FloatPoint newUnobscuredContentRectCenter = newUnobscuredContentRect.center();
            FloatPoint positionDelta(newRelativeContentCenter.x() - newUnobscuredContentRectCenter.x(), newRelativeContentCenter.y() - newUnobscuredContentRectCenter.y());
            newUnobscuredContentRect.moveBy(positionDelta);
            newExposedContentRect.moveBy(positionDelta);
        }

        // Make the top/bottom edges "sticky" within 1 pixel.
        if (targetUnobscuredRect.maxY() > oldContentSize.height() - 1) {
            float bottomVerticalPosition = newContentSize.height() - newUnobscuredContentRect.height();
            newUnobscuredContentRect.setY(bottomVerticalPosition);
            newExposedContentRect.setY(bottomVerticalPosition);
        }
        if (targetUnobscuredRect.y() < 1) {
            newUnobscuredContentRect.setY(0);
            newExposedContentRect.setY(0);
        }

        float horizontalAdjustment = 0;
        if (newExposedContentRect.maxX() > newContentSize.width())
            horizontalAdjustment -= newUnobscuredContentRect.maxX() - newContentSize.width();
        float verticalAdjustment = 0;
        if (newExposedContentRect.maxY() > newContentSize.height())
            verticalAdjustment -= newUnobscuredContentRect.maxY() - newContentSize.height();
        if (newExposedContentRect.x() < 0)
            horizontalAdjustment += - newUnobscuredContentRect.x();
        if (newExposedContentRect.y() < 0)
            verticalAdjustment += - newUnobscuredContentRect.y();

        FloatPoint adjustmentDelta(horizontalAdjustment, verticalAdjustment);
        newUnobscuredContentRect.moveBy(adjustmentDelta);
        newExposedContentRect.moveBy(adjustmentDelta);
    }

    frameView.setScrollVelocity(0, 0, 0, monotonicallyIncreasingTime());

    IntRect roundedUnobscuredContentRect = roundedIntRect(newUnobscuredContentRect);
    frameView.setScrollOffset(roundedUnobscuredContentRect.location());
    frameView.setUnobscuredContentRect(roundedUnobscuredContentRect);
    m_drawingArea->setExposedContentRect(newExposedContentRect);

    if (scale == targetScale)
        scalePage(scale, frameView.scrollPosition());

    if (scale != targetScale || roundedIntPoint(targetUnobscuredRect.location()) != roundedUnobscuredContentRect.location())
        send(Messages::WebPageProxy::DynamicViewportUpdateChangedTarget(scale, frameView.scrollPosition()));
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

    m_page->setZoomedOutPageScaleFactor(initialScale);

    FrameView& frameView = *m_page->mainFrame().view();
    IntPoint scrollPosition = frameView.scrollPosition();
    if (!m_hasReceivedVisibleContentRectsAfterDidCommitLoad) {
        IntSize minimumLayoutSizeInDocumentCoordinate = m_viewportConfiguration.minimumLayoutSize();
        minimumLayoutSizeInDocumentCoordinate.scale(scale);

        IntRect unobscuredContentRect(scrollPosition, minimumLayoutSizeInDocumentCoordinate);
        frameView.setUnobscuredContentRect(unobscuredContentRect);
        frameView.setScrollVelocity(0, 0, 0, monotonicallyIncreasingTime());

        // FIXME: We could send down the obscured margins to find a better exposed rect and unobscured rect.
        // It is not a big deal at the moment because the tile coverage will always extend past the obscured bottom inset.
        m_drawingArea->setExposedContentRect(unobscuredContentRect);
    }
    scalePage(scale, scrollPosition);
}

void WebPage::applicationWillResignActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillResignActiveNotification object:nil];
}

void WebPage::applicationWillEnterForeground()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationWillEnterForegroundNotification object:nil];
}

void WebPage::applicationDidBecomeActive()
{
    [[NSNotificationCenter defaultCenter] postNotificationName:WebUIApplicationDidBecomeActiveNotification object:nil];
}

static inline FloatRect adjustExposedRectForBoundedScale(const FloatRect& exposedRect, double exposedRectScale, double boundedScale)
{
    if (exposedRectScale < boundedScale)
        return exposedRect;

    double overscaledWidth = exposedRect.width();
    double missingHorizonalMargin = exposedRect.width() * exposedRectScale / boundedScale - overscaledWidth;

    double overscaledHeight = exposedRect.height();
    double missingVerticalMargin = exposedRect.height() * exposedRectScale / boundedScale - overscaledHeight;

    return FloatRect(exposedRect.x() - missingHorizonalMargin / 2, exposedRect.y() - missingVerticalMargin / 2, exposedRect.width() + missingHorizonalMargin, exposedRect.height() + missingVerticalMargin);
}

static inline void adjustVelocityDataForBoundedScale(double& horizontalVelocity, double& verticalVelocity, double& scaleChangeRate, double exposedRectScale, double boundedScale)
{
    if (scaleChangeRate) {
        horizontalVelocity = 0;
        verticalVelocity = 0;
    }

    if (exposedRectScale != boundedScale)
        scaleChangeRate = 0;
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo)
{
    m_hasReceivedVisibleContentRectsAfterDidCommitLoad = true;
    m_lastVisibleContentRectUpdateID = visibleContentRectUpdateInfo.updateID();

    double boundedScale = std::min(m_viewportConfiguration.maximumScale(), std::max(m_viewportConfiguration.minimumScale(), visibleContentRectUpdateInfo.scale()));

    FloatRect exposedRect = visibleContentRectUpdateInfo.exposedRect();
    FloatRect adjustedExposedRect = adjustExposedRectForBoundedScale(exposedRect, visibleContentRectUpdateInfo.scale(), boundedScale);
    m_drawingArea->setExposedContentRect(enclosingIntRect(adjustedExposedRect));

    IntRect roundedUnobscuredRect = roundedIntRect(visibleContentRectUpdateInfo.unobscuredRect());
    IntPoint scrollPosition = roundedUnobscuredRect.location();

    float floatBoundedScale = boundedScale;
    if (floatBoundedScale != m_page->pageScaleFactor()) {
        m_scaleWasSetByUIProcess = true;

        m_page->setPageScaleFactor(floatBoundedScale, scrollPosition);
        if (m_drawingArea->layerTreeHost())
            m_drawingArea->layerTreeHost()->deviceOrPageScaleFactorChanged();
        send(Messages::WebPageProxy::PageScaleFactorDidChange(floatBoundedScale));
    }

    m_page->mainFrame().view()->setScrollOffset(scrollPosition);
    m_page->mainFrame().view()->setUnobscuredContentRect(roundedUnobscuredRect);

    double horizontalVelocity = visibleContentRectUpdateInfo.horizontalVelocity();
    double verticalVelocity = visibleContentRectUpdateInfo.verticalVelocity();
    double scaleChangeRate = visibleContentRectUpdateInfo.scaleChangeRate();
    adjustVelocityDataForBoundedScale(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.scale(), boundedScale);

    m_page->mainFrame().view()->setScrollVelocity(horizontalVelocity, verticalVelocity, scaleChangeRate, visibleContentRectUpdateInfo.timestamp());

    if (visibleContentRectUpdateInfo.inStableState())
        m_page->mainFrame().view()->setCustomFixedPositionLayoutRect(enclosingIntRect(visibleContentRectUpdateInfo.customFixedPositionRect()));
}

void WebPage::willStartUserTriggeredZooming()
{
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

} // namespace WebKit

#endif // PLATFORM(IOS)
