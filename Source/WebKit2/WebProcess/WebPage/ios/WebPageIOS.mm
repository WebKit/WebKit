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
#import "EditorState.h"
#import "InteractionInformationAtPosition.h"
#import "PluginView.h"
#import "LayerTreeHost.h"
#import "VisibleContentRectUpdateInfo.h"
#import "WebChromeClient.h"
#import "WebCoreArgumentCoders.h"
#import "WebKitSystemInterfaceIOS.h"
#import "WebFrame.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import "WKAccessibilityWebPageObjectIOS.h"
#import "WKGestureTypes.h"
#import <CoreText/CTFont.h>
#import <WebCore/Chrome.h>
#import <WebCore/Element.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/HTMLElementTypeHelpers.h>
#import <WebCore/HTMLFormElement.h>
#import <WebCore/HTMLInputElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLOptGroupElement.h>
#import <WebCore/HTMLOptionElement.h>
#import <WebCore/HTMLParserIdioms.h>
#import <WebCore/HTMLSelectElement.h>
#import <WebCore/HTMLTextAreaElement.h>
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

bool WebPage::executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>&, KeyboardEvent*)
{
    notImplemented();
    return false;
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

void WebPage::setComposition(const String& text, Vector<WebCore::CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (frame.selection().selection().isContentEditable())
        frame.editor().setComposition(text, underlines, selectionStart, selectionEnd);
}

void WebPage::confirmComposition()
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().confirmComposition();
}

void WebPage::cancelComposition(EditorState&)
{
    notImplemented();
}

static PassRefPtr<Range> convertToRange(Frame* frame, NSRange nsrange)
{
    if (nsrange.location > INT_MAX)
        return 0;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;
    
    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection
    // We have to do this because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so serialization is problematic. Our solution is
    // to use the root editable element of the selection start as the positional base.
    // That fits with AppKit's idea of an input context.
    return TextIterator::rangeFromLocationAndLength(frame->selection().rootEditableElementOrDocumentElement(), nsrange.location, nsrange.length);
}

void WebPage::insertText(const String& text, uint64_t replacementRangeStart, uint64_t replacementRangeEnd)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    
    if (replacementRangeStart != NSNotFound) {
        RefPtr<Range> replacementRange = convertToRange(&frame, NSMakeRange(replacementRangeStart, replacementRangeEnd - replacementRangeStart));
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(replacementRange.get(), SEL_DEFAULT_AFFINITY));
    }
    
    if (!frame.editor().hasComposition()) {
        // An insertText: might be handled by other responders in the chain if we don't handle it.
        // One example is space bar that results in scrolling down the page.
        frame.editor().insertText(text, nullptr);
    } else
        frame.editor().confirmComposition(text);
}

void WebPage::insertDictatedText(const String&, uint64_t, uint64_t, const Vector<WebCore::DictationAlternative>&, bool&, EditorState&)
{
    notImplemented();
}

void WebPage::getMarkedRange(uint64_t&, uint64_t&)
{
    notImplemented();
}

void WebPage::getSelectedRange(uint64_t&, uint64_t&)
{
    notImplemented();
}

void WebPage::getAttributedSubstringFromRange(uint64_t, uint64_t, AttributedString&)
{
    notImplemented();
}

void WebPage::characterIndexForPoint(IntPoint, uint64_t&)
{
    notImplemented();
}

void WebPage::firstRectForCharacterRange(uint64_t, uint64_t, WebCore::IntRect&)
{
    notImplemented();
}

void WebPage::executeKeypressCommands(const Vector<WebCore::KeypressCommand>&, bool&, EditorState&)
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

void WebPage::performDictionaryLookupForRange(Frame*, Range*, NSDictionary *)
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

PassRefPtr<Range> WebPage::rangeForWebSelectionAtPosition(const IntPoint& point, const VisiblePosition& position, WKSelectionFlags& flags)
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
    flags = WKIsBlockSelection;
    range = Range::create(bestChoice->document());
    range->selectNodeContents(bestChoice, ASSERT_NO_EXCEPTION);
    return range;
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
    WKSelectionFlags flags = WKNone;
    WKGestureRecognizerState wkGestureState = static_cast<WKGestureRecognizerState>(gestureState);
    switch (static_cast<WKGestureType>(gestureType)) {
    case WKGestureOneFingerTap:
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
        if (range)
            m_shouldReturnWordAtSelection = true;
    }
        break;

    case WKGestureLoupe:
        range = Range::create(*frame.document(), position, position);
        break;

    case WKGestureTapAndAHalf:
        switch (wkGestureState) {
        case WKGestureRecognizerStateBegan:
            range = wordRangeFromPosition(position);
            m_currentWordRange = Range::create(*frame.document(), range->startPosition(), range->endPosition());
            break;
        case WKGestureRecognizerStateChanged:
            range = Range::create(*frame.document(), m_currentWordRange->startPosition(), m_currentWordRange->endPosition());
            if (position < range->startPosition())
                range->setStart(position.deepEquivalent(), ASSERT_NO_EXCEPTION);
            if (position > range->endPosition())
                range->setEnd(position.deepEquivalent(), ASSERT_NO_EXCEPTION);
            break;
        case WKGestureRecognizerStateEnded:
        case WKGestureRecognizerStateCancelled:
            m_currentWordRange = nullptr;
            break;
        case WKGestureRecognizerStateFailed:
        case WKGestureRecognizerStatePossible:
            ASSERT_NOT_REACHED();
        }
        break;

    case WKGestureOneFingerDoubleTap:
        if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
            // Double-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines,
            // paragraphs, and documents.
            range = Range::create(*frame.document(), position, position);
         } else
            range = wordRangeFromPosition(position);
        break;

    case WKGestureTwoFingerSingleTap:
        // Single tap with two fingers selects the entire paragraph.
        range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        break;

    case WKGestureOneFingerTripleTap:
        if (atBoundaryOfGranularity(position, LineGranularity, DirectionForward)) {
            // Triple-tap at end of line only places insertion point there.
            // This helps to get the callout for pasting at ends of lines,
            // paragraphs, and documents.
            range = Range::create(*frame.document(), position, position);
        } else
            range = enclosingTextUnitOfGranularity(position, ParagraphGranularity, DirectionForward);
        break;

    case WKGestureMakeWebSelection:
        if (wkGestureState == WKGestureRecognizerStateBegan) {
            m_blockSelectionDesiredSize.setWidth(blockSelectionStartWidth);
            m_blockSelectionDesiredSize.setHeight(blockSelectionStartHeight);
            m_currentBlockSelection = nullptr;
        }
        range = rangeForWebSelectionAtPosition(point, position, flags);
        if (wkGestureState == WKGestureRecognizerStateEnded && flags & WKIsBlockSelection)
            m_currentBlockSelection = range;
        break;

    default:
        break;
    }
    if (range)
        frame.selection().setSelectedRange(range.get(), position.affinity(), true);

    send(Messages::WebPageProxy::GestureCallback(point, gestureType, gestureState, flags, callbackID));
    m_shouldReturnWordAtSelection = false;
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

static inline float distanceBetweenRectsForPosition(IntRect& first, IntRect& second, WKHandlePosition handlePosition)
{
    switch (handlePosition) {
    case WKHandleTop:
        return abs(first.y() - second.y());
    case WKHandleRight:
        return abs(first.maxX() - second.maxX());
    case WKHandleBottom:
        return abs(first.maxY() - second.maxY());
    case WKHandleLeft:
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

static inline IntPoint computeEdgeCenter(const IntRect& box, WKHandlePosition handlePosition)
{
    switch (handlePosition) {
    case WKHandleTop:
        return IntPoint(box.x() + box.width() / 2, box.y());
    case WKHandleRight:
        return IntPoint(box.maxX(), box.y() + box.height() / 2);
    case WKHandleBottom:
        return IntPoint(box.x() + box.width() / 2, box.maxY());
    case WKHandleLeft:
        return IntPoint(box.x(), box.y() + box.height() / 2);
    }
}

PassRefPtr<Range> WebPage::expandedRangeFromHandle(Range* currentRange, WKHandlePosition handlePosition)
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
        case WKHandleTop:
            testPoint.move(0, -distance);
            break;
        case WKHandleRight:
            testPoint.move(distance, 0);
            break;
        case WKHandleBottom:
            testPoint.move(0, distance);
            break;
        case WKHandleLeft:
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
            case WKHandleTop:
            case WKHandleBottom:
                isBetterChoice = (copyRect.height() > currentBox.height());
                break;
            case WKHandleRight:
            case WKHandleLeft:
                isBetterChoice = (copyRect.width() > currentBox.width());
                break;
            }

        }

        if (bestRange && isBetterChoice) {
            // Furtherore, is it smaller than the best we've found so far?
            switch (handlePosition) {
            case WKHandleTop:
            case WKHandleBottom:
                isBetterChoice = (copyRect.height() < bestRect.height());
                break;
            case WKHandleRight:
            case WKHandleLeft:
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

PassRefPtr<Range> WebPage::contractedRangeFromHandle(Range* currentRange, WKHandlePosition handlePosition, WKSelectionFlags& flags)
{
    // Shrinking with a base and extent will always give better results. If we only have a single element,
    // see if we can break that down to a base and extent. Shrinking base and extent is comparatively straightforward.
    // Shrinking down to another element is unlikely to move just one edge, but we can try that as a fallback.

    // FIXME: We should use boundingRect() instead of boundingBox() in this function when <rdar://problem/16063723> is fixed.
    IntRect currentBox = currentRange->boundingBox();
    IntPoint edgeCenter = computeEdgeCenter(currentBox, handlePosition);
    flags = WKIsBlockSelection;

    float maxDistance;

    switch (handlePosition) {
    case WKHandleTop:
    case WKHandleBottom:
        maxDistance = currentBox.height();
        break;
    case WKHandleRight:
    case WKHandleLeft:
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
            case WKHandleTop:
            case WKHandleBottom:
                shrankDistance = abs(currentBox.height() - bestRect.height());
                break;
            case WKHandleRight:
            case WKHandleLeft:
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
        case WKHandleTop:
            testPoint.move(0, distance);
            break;
        case WKHandleRight:
            testPoint.move(-distance, 0);
            break;
        case WKHandleBottom:
            testPoint.move(0, -distance);
            break;
        case WKHandleLeft:
            testPoint.move(distance, 0);
            break;
        }

        RefPtr<Range> newRange = rangeForBlockAtPoint(testPoint);
        if (handlePosition == WKHandleTop || handlePosition == WKHandleLeft)
            newRange = Range::create(newRange->startContainer()->document(), newRange->startPosition(), currentRange->endPosition());
        else
            newRange = Range::create(newRange->startContainer()->document(), currentRange->startPosition(), newRange->endPosition());

        IntRect copyRect = newRange->boundingBox();
        bool isBetterChoice;
        switch (handlePosition) {
        case WKHandleTop:
        case WKHandleBottom:
            isBetterChoice = (copyRect.height() < currentBox.height());
            break;
        case WKHandleLeft:
        case WKHandleRight:
            isBetterChoice = (copyRect.width() > bestRect.width());
            break;
        }

        isBetterChoice = isBetterChoice && !areRangesEqual(newRange.get(), currentRange);
        if (bestRange && isBetterChoice) {
            switch (handlePosition) {
            case WKHandleTop:
            case WKHandleBottom:
                isBetterChoice = (copyRect.height() > bestRect.height());
                break;
            case WKHandleLeft:
            case WKHandleRight:
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
            flags = WKNone;
        }
    }

    return bestRange;
}

void WebPage::computeExpandAndShrinkThresholdsForHandle(const IntPoint& point, WKHandlePosition handlePosition, float& growThreshold, float& shrinkThreshold)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    RefPtr<Range> currentRange = m_currentBlockSelection ? m_currentBlockSelection.get() : frame.selection().selection().toNormalizedRange();
    ASSERT(currentRange);

    RefPtr<Range> expandedRange = expandedRangeFromHandle(currentRange.get(), handlePosition);
    WKSelectionFlags flags;
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
    case WKHandleTop: {
        current = currentBounds.y();
        expanded = expandedBounds.y();
        contracted = contractedBounds.y();
        maxThreshold = FLT_MIN;
        minThreshold = FLT_MAX;
        break;
    }
    case WKHandleRight: {
        current = currentBounds.maxX();
        expanded = expandedBounds.maxX();
        contracted = contractedBounds.maxX();
        maxThreshold = FLT_MAX;
        minThreshold = FLT_MIN;
        break;
    }
    case WKHandleBottom: {
        current = currentBounds.maxY();
        expanded = expandedBounds.maxY();
        contracted = contractedBounds.maxY();
        maxThreshold = FLT_MAX;
        minThreshold = FLT_MIN;
        break;
    }
    case WKHandleLeft: {
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

    if ((flags & WKIsBlockSelection) && areRangesEqual(contractedRange.get(), currentRange.get()))
        shrinkThreshold = minThreshold;
}

static inline bool shouldExpand(WKHandlePosition handlePosition, const IntRect& rect, const IntPoint& point)
{
    switch (handlePosition) {
    case WKHandleTop:
        return (point.y() < rect.y());
    case WKHandleLeft:
        return (point.x() < rect.x());
    case WKHandleRight:
        return (point.x() > rect.maxX());
    case WKHandleBottom:
        return (point.y() > rect.maxY());
    }
}

PassRefPtr<WebCore::Range> WebPage::changeBlockSelection(const IntPoint& point, WKHandlePosition handlePosition, float& growThreshold, float& shrinkThreshold, WKSelectionFlags& flags)
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
    WKSelectionFlags flags = WKIsBlockSelection;

    switch (static_cast<WKSelectionTouch>(touch)) {
    case WKSelectionTouchStarted:
        computeExpandAndShrinkThresholdsForHandle(adjustedPoint, static_cast<WKHandlePosition>(handlePosition), growThreshold, shrinkThreshold);
        break;
    case WKSelectionTouchEnded:
        break;
    case WKSelectionTouchMoved:
        changeBlockSelection(adjustedPoint, static_cast<WKHandlePosition>(handlePosition), growThreshold, shrinkThreshold, flags);
        break;
    default:
        return;
    }

    send(Messages::WebPageProxy::DidUpdateBlockSelectionWithTouch(touch, flags, growThreshold, shrinkThreshold));
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
    
    switch (static_cast<WKSelectionTouch>(touches)) {
        case WKSelectionTouchStarted:
        case WKSelectionTouchEndedNotMoving:
            break;
        
        case WKSelectionTouchEnded:
            if (frame.selection().selection().isContentEditable()) {
                result = closestWordBoundaryForPosition(position);
                if (result.isNotNull())
                    range = Range::create(*frame.document(), result, result);
            } else
                range = rangeForPosition(&frame, position, baseIsStart);
            break;

        case WKSelectionTouchEndedMovingForward:
            range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionForward);
            break;
            
        case WKSelectionTouchEndedMovingBackward:
            range = rangeAtWordBoundaryForPosition(&frame, position, baseIsStart, DirectionBackward);
            break;

        case WKSelectionTouchMoved:
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
    // For the moment we handle only WordGranularity.
    if (granularity != WordGranularity)
        return;

    Frame& frame = m_page->focusController().focusedOrMainFrame();
    ASSERT(frame.selection().isCaret());
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
        if (hitNode) {
            m_page->focusController().setFocusedFrame(result.innerNodeFrame());
            info.selectionRects.append(SelectionRect(hitNode->renderer()->absoluteBoundingBoxRect(true), true, 0));
            info.bounds = hitNode->renderer()->absoluteBoundingBoxRect();
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

    if (static_cast<WKSheetActions>(action) == WKSheetActionCopy) {
        if (element->renderer()->isRenderImage()) {
            Element* linkElement = containingLinkElement(element);
        
            if (!linkElement)
                m_interactionNode->document().frame()->editor().writeImageToPasteboard(*Pasteboard::createForCopyAndPaste(), *element, toRenderImage(element->renderer())->cachedImage()->url(), String());
            else
                m_interactionNode->document().frame()->editor().copyURL(linkElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(linkElement->getAttribute(HTMLNames::hrefAttr))), linkElement->textContent());
        } else if (element->isLink()) {
            m_interactionNode->document().frame()->editor().copyURL(element->document().completeURL(stripLeadingAndTrailingHTMLSpaces(element->getAttribute(HTMLNames::hrefAttr))), element->textContent());
        }
    } else if (static_cast<WKSelectionTouch>(action) == WKSheetActionSaveImage) {
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
    m_assistedNode = node;
    if (node->hasTagName(WebCore::HTMLNames::selectTag) || node->hasTagName(WebCore::HTMLNames::inputTag) || node->hasTagName(WebCore::HTMLNames::textareaTag) || node->hasEditableStyle()) {
        AssistedNodeInformation information;
        getAssistedNodeInformation(information);
        send(Messages::WebPageProxy::StartAssistingNode(information));
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

void WebPage::viewportConfigurationChanged()
{
    setFixedLayoutSize(m_viewportConfiguration.layoutSize());

    double scale;
    if (m_userHasChangedPageScaleFactor)
        scale = std::max(std::min(pageScaleFactor(), m_viewportConfiguration.maximumScale()), m_viewportConfiguration.minimumScale());
    else
        scale = m_viewportConfiguration.initialScale();

    scalePage(scale, m_page->mainFrame().view()->scrollPosition());
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

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo& visibleContentRectUpdateInfo)
{
    m_lastVisibleContentRectUpdateID = visibleContentRectUpdateInfo.updateID();

    FloatRect exposedRect = visibleContentRectUpdateInfo.exposedRect();
    m_drawingArea->setExposedContentRect(enclosingIntRect(exposedRect));

    IntPoint scrollPosition = roundedIntPoint(visibleContentRectUpdateInfo.unobscuredRect().location());

    double boundedScale = std::min(m_viewportConfiguration.maximumScale(), std::max(m_viewportConfiguration.minimumScale(), visibleContentRectUpdateInfo.scale()));
    float floatBoundedScale = boundedScale;
    if (floatBoundedScale != m_page->pageScaleFactor()) {
        m_scaleWasSetByUIProcess = true;

        m_page->setPageScaleFactor(floatBoundedScale, scrollPosition);
        if (m_drawingArea->layerTreeHost())
            m_drawingArea->layerTreeHost()->deviceOrPageScaleFactorChanged();
        send(Messages::WebPageProxy::PageScaleFactorDidChange(floatBoundedScale));
    }

    m_page->mainFrame().view()->setScrollOffset(scrollPosition);
    
    if (visibleContentRectUpdateInfo.inStableState())
        m_page->mainFrame().view()->setCustomFixedPositionLayoutRect(enclosingIntRect(visibleContentRectUpdateInfo.customFixedPositionRect()));

    // FIXME: we should also update the frame view from unobscured rect. Altenatively, we can have it pull the values from ScrollView.
}

void WebPage::willStartUserTriggeredZooming()
{
    m_userHasChangedPageScaleFactor = true;
}

} // namespace WebKit

#endif // PLATFORM(IOS)
