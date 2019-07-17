/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "Frame.h"

#if PLATFORM(IOS_FAMILY)

#import "CSSAnimationController.h"
#import "CommonVM.h"
#import "DOMWindow.h"
#import "Document.h"
#import "DocumentMarkerController.h"
#import "Editor.h"
#import "EditorClient.h"
#import "EventHandler.h"
#import "EventNames.h"
#import "FormController.h"
#import "FrameSelection.h"
#import "FrameView.h"
#import "HTMLAreaElement.h"
#import "HTMLBodyElement.h"
#import "HTMLDocument.h"
#import "HTMLHtmlElement.h"
#import "HTMLNames.h"
#import "HTMLObjectElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "Logging.h"
#import "NodeRenderStyle.h"
#import "NodeTraversal.h"
#import "Page.h"
#import "PageTransitionEvent.h"
#import "PlatformScreen.h"
#import "PropertySetCSSStyleDeclaration.h"
#import "RenderLayer.h"
#import "RenderLayerCompositor.h"
#import "RenderTextControl.h"
#import "RenderView.h"
#import "RenderedDocumentMarker.h"
#import "ShadowRoot.h"
#import "TextBoundaries.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleUnits.h"
#import "WAKWindow.h"
#import <JavaScriptCore/JSLock.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/text/TextStream.h>

using namespace WebCore::HTMLNames;
using namespace WTF::Unicode;

using JSC::JSLockHolder;

namespace WebCore {

// Create <html><body (style="...")></body></html> doing minimal amount of work.
void Frame::initWithSimpleHTMLDocument(const String& style, const URL& url)
{
    m_loader->initForSynthesizedDocument(url);

    auto document = HTMLDocument::createSynthesizedDocument(*this, url);
    document->setCompatibilityMode(DocumentCompatibilityMode::LimitedQuirksMode);
    document->createDOMWindow();
    setDocument(document.copyRef());

    auto rootElement = HTMLHtmlElement::create(document);

    auto body = HTMLBodyElement::create(document);
    if (!style.isEmpty())
        body->setAttribute(HTMLNames::styleAttr, style);

    rootElement->appendChild(body);
    document->appendChild(rootElement);
}

const ViewportArguments& Frame::viewportArguments() const
{
    return m_viewportArguments;
}

void Frame::setViewportArguments(const ViewportArguments& arguments)
{
    m_viewportArguments = arguments;
}

NSArray *Frame::wordsInCurrentParagraph() const
{
    document()->updateLayout();

    if (!page() || !page()->selection().isCaret())
        return nil;

    VisiblePosition position(page()->selection().start(), page()->selection().affinity());
    VisiblePosition end(position);
    if (!isStartOfParagraph(end)) {
        VisiblePosition previous = end.previous();
        UChar c(previous.characterAfter());
        if (!iswpunct(c) && !isSpaceOrNewline(c) && c != noBreakSpace)
            end = startOfWord(end);
    }
    VisiblePosition start(startOfParagraph(end));

    RefPtr<Range> searchRange(rangeOfContents(*document()));
    setStart(searchRange.get(), start);
    setEnd(searchRange.get(), end);

    if (searchRange->collapsed())
        return nil;

    NSMutableArray *words = [NSMutableArray array];

    WordAwareIterator it(*searchRange);
    while (!it.atEnd()) {
        StringView text = it.text();
        int length = text.length();
        if (length > 1 || !isSpaceOrNewline(text[0])) {
            int startOfWordBoundary = 0;
            for (int i = 1; i < length; i++) {
                if (isSpaceOrNewline(text[i]) || text[i] == noBreakSpace) {
                    int wordLength = i - startOfWordBoundary;
                    if (wordLength > 0) {
                        RetainPtr<NSString> chunk = text.substring(startOfWordBoundary, wordLength).createNSString();
                        [words addObject:chunk.get()];
                    }
                    startOfWordBoundary += wordLength + 1;
                }
            }
            if (startOfWordBoundary < length) {
                RetainPtr<NSString> chunk = text.substring(startOfWordBoundary, length - startOfWordBoundary).createNSString();
                [words addObject:chunk.get()];
            }
        }
        it.advance();
    }

    if ([words count] > 0 && isEndOfParagraph(position) && !isStartOfParagraph(position)) {
        VisiblePosition previous = position.previous();
        UChar c(previous.characterAfter());
        if (!isSpaceOrNewline(c) && c != noBreakSpace)
            [words removeLastObject];
    }

    return words;
}

#define CHECK_FONT_SIZE 0
#define RECT_LOGGING 0

CGRect Frame::renderRectForPoint(CGPoint point, bool* isReplaced, float* fontSize) const
{
    *isReplaced = false;
    *fontSize = 0;

    if (!m_doc || !m_doc->renderBox())
        return CGRectZero;

    // FIXME: why this layer check?
    RenderLayer* layer = m_doc->renderBox()->layer();
    if (!layer)
        return CGRectZero;

    HitTestResult result = eventHandler().hitTestResultAtPoint(IntPoint(roundf(point.x), roundf(point.y)), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent);

    Node* node = result.innerNode();
    if (!node)
        return CGRectZero;

    RenderObject* hitRenderer = node->renderer();
    RenderObject* renderer = hitRenderer;
#if RECT_LOGGING
    printf("\n%f %f\n", point.x, point.y);
#endif
    while (renderer && !renderer->isBody() && !renderer->isDocumentElementRenderer()) {
#if RECT_LOGGING
        CGRect rect = renderer->absoluteBoundingBoxRect(true);
        if (renderer->node()) {
            const char *nodeName = renderer->node()->nodeName().ascii().data();
            printf("%s %f %f %f %f\n", nodeName, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
        }
#endif
        if (renderer->isRenderBlock() || renderer->isInlineBlockOrInlineTable() || renderer->isReplaced()) {
            *isReplaced = renderer->isReplaced();
#if CHECK_FONT_SIZE
            for (RenderObject* textRenderer = hitRenderer; textRenderer; textRenderer = textRenderer->traverseNext(hitRenderer)) {
                if (textRenderer->isText()) {
                    *fontSize = textRenderer->font(true).pixelSize();
                    break;
                }
            }
#endif
            IntRect targetRect = renderer->absoluteBoundingBoxRect(true);
            for (Widget* currView = &(renderer->view().frameView()); currView && currView != view(); currView = currView->parent())
                targetRect = currView->convertToContainingView(targetRect);

            return targetRect;
        }
        renderer = renderer->parent();
    }

    return CGRectZero;
}

void Frame::betterApproximateNode(const IntPoint& testPoint, const NodeQualifier& nodeQualifierFunction, Node*& best, Node* failedNode, IntPoint& bestPoint, IntRect& bestRect, const IntRect& testRect)
{
    IntRect candidateRect;
    Node* candidate = nodeQualifierFunction(eventHandler().hitTestResultAtPoint(testPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowVisibleChildFrameContentOnly), failedNode, &candidateRect);

    // Bail if we have no candidate, or the candidate is already equal to our current best node,
    // or our candidate is the avoidedNode and there is a current best node.
    if (!candidate || candidate == best)
        return;

    // The document should never be considered the best alternative.
    if (candidate->isDocumentNode())
        return;

    if (best) {
        IntRect bestIntersect = intersection(testRect, bestRect);
        IntRect candidateIntersect = intersection(testRect, candidateRect);
        // if the candidate intersection is smaller than the current best intersection, bail.
        if (candidateIntersect.width() * candidateIntersect.height() <= bestIntersect.width() * bestIntersect.height())
            return;
    }

    // At this point we either don't have a previous best, or our current candidate has a better intersection.
    best = candidate;
    bestPoint = testPoint;
    bestRect = candidateRect;
}

bool Frame::hitTestResultAtViewportLocation(const FloatPoint& viewportLocation, HitTestResult& hitTestResult, IntPoint& center)
{
    if (!m_doc || !m_doc->renderView())
        return false;

    FrameView* view = m_view.get();
    if (!view)
        return false;

    center = view->windowToContents(roundedIntPoint(viewportLocation));
    HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowVisibleChildFrameContentOnly;
    hitTestResult = eventHandler().hitTestResultAtPoint(center, hitType);
    return true;
}

Node* Frame::qualifyingNodeAtViewportLocation(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation, const NodeQualifier& nodeQualifierFunction, bool shouldApproximate)
{
    adjustedViewportLocation = viewportLocation;

    IntPoint testCenter;
    HitTestResult candidateInfo;
    if (!hitTestResultAtViewportLocation(viewportLocation, candidateInfo, testCenter))
        return nullptr;

    IntPoint bestPoint = testCenter;

    // We have the candidate node at the location, check whether it or one of its ancestors passes
    // the qualifier function, which typically checks if the node responds to a particular event type.
    Node* approximateNode = nodeQualifierFunction(candidateInfo, 0, 0);

#if USE(UIKIT_EDITING)
    if (approximateNode && approximateNode->isContentEditable()) {
        // If we are in editable content, we look for the root editable element.
        approximateNode = approximateNode->rootEditableElement();
        // If we have a focusable node, there is no need to approximate.
        if (approximateNode)
            shouldApproximate = false;
    }
#endif


    float scale = page() ? page()->pageScaleFactor() : 1;
    float ppiFactor = screenPPIFactor();

    static const float unscaledSearchRadius = 15;
    int searchRadius = static_cast<int>(unscaledSearchRadius * ppiFactor / scale);

    if (approximateNode && shouldApproximate) {
        const float testOffsets[] = {
            -.3f, -.3f,
            -.6f, -.6f,
            +.3f, +.3f,
            -.9f, -.9f,
        };

        Node* originalApproximateNode = approximateNode;
        for (unsigned n = 0; n < WTF_ARRAY_LENGTH(testOffsets); n += 2) {
            IntSize testOffset(testOffsets[n] * searchRadius, testOffsets[n + 1] * searchRadius);
            IntPoint testPoint = testCenter + testOffset;

            HitTestResult candidateInfo = eventHandler().hitTestResultAtPoint(testPoint, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowUserAgentShadowContent | HitTestRequest::AllowChildFrameContent);
            Node* candidateNode = nodeQualifierFunction(candidateInfo, 0, 0);
            if (candidateNode && candidateNode->isDescendantOf(originalApproximateNode)) {
                approximateNode = candidateNode;
                bestPoint = testPoint;
                break;
            }
        }
    } else if (!approximateNode && shouldApproximate) {
        // Grab the closest parent element of our failed candidate node.
        Node* candidate = candidateInfo.innerNode();
        Node* failedNode = candidate;

        while (candidate && !candidate->isElementNode())
            candidate = candidate->parentInComposedTree();

        if (candidate)
            failedNode = candidate;

        // The center point was tested earlier.
        const float testOffsets[] = {
            -.3f, -.3f,
            +.3f, -.3f,
            -.3f, +.3f,
            +.3f, +.3f,
            -.6f, -.6f,
            +.6f, -.6f,
            -.6f, +.6f,
            +.6f, +.6f,
            -1.f, 0,
            +1.f, 0,
            0, +1.f,
            0, -1.f,
        };
        IntRect bestFrame;
        IntRect testRect(testCenter, IntSize());
        testRect.inflate(searchRadius);
        int currentTestRadius = 0;
        for (unsigned n = 0; n < WTF_ARRAY_LENGTH(testOffsets); n += 2) {
            IntSize testOffset(testOffsets[n] * searchRadius, testOffsets[n + 1] * searchRadius);
            IntPoint testPoint = testCenter + testOffset;
            int testRadius = std::max(abs(testOffset.width()), abs(testOffset.height()));
            if (testRadius > currentTestRadius) {
                // Bail out with the best match within a radius
                currentTestRadius = testRadius;
                if (approximateNode)
                    break;
            }
            betterApproximateNode(testPoint, nodeQualifierFunction, approximateNode, failedNode, bestPoint, bestFrame, testRect);
        }
    }

    if (approximateNode) {
        IntPoint p = m_view->contentsToWindow(bestPoint);
        adjustedViewportLocation = p;
#if USE(UIKIT_EDITING)
        if (approximateNode->isContentEditable()) {
            // When in editable content, look for the root editable node again,
            // since this could be the node found with the approximation.
            approximateNode = approximateNode->rootEditableElement();
        }
#endif
    }

    return approximateNode;
}

Node* Frame::deepestNodeAtLocation(const FloatPoint& viewportLocation)
{
    IntPoint center;
    HitTestResult hitTestResult;
    if (!hitTestResultAtViewportLocation(viewportLocation, hitTestResult, center))
        return nullptr;

    return hitTestResult.innerNode();
}

Node* Frame::approximateNodeAtViewportLocationLegacy(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation)
{
    // This function is only used for UIWebView.
    auto&& ancestorRespondingToClickEvents = [](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> Node* {
        bool bodyHasBeenReached = false;
        bool pointerCursorStillValid = true;

        if (nodeBounds)
            *nodeBounds = IntRect();

        auto node = hitTestResult.innerNode();
        if (!node)
            return nullptr;

        Node* pointerCursorNode = nullptr;
        for (; node && node != terminationNode; node = node->parentInComposedTree()) {
            // We only accept pointer nodes before reaching the body tag.
            if (node->hasTagName(HTMLNames::bodyTag)) {
#if USE(UIKIT_EDITING)
                // Make sure we cover the case of an empty editable body.
                if (!pointerCursorNode && node->isContentEditable())
                    pointerCursorNode = node;
#endif
                bodyHasBeenReached = true;
                pointerCursorStillValid = false;
            }

            // If we already have a pointer, and we reach a table, don't accept it.
            if (pointerCursorNode && (node->hasTagName(HTMLNames::tableTag) || node->hasTagName(HTMLNames::tbodyTag)))
                pointerCursorStillValid = false;

            // If we haven't reached the body, and we are still paying attention to pointer cursors, and the node has a pointer cursor.
            if (pointerCursorStillValid && node->renderStyle() && node->renderStyle()->cursor() == CursorType::Pointer)
                pointerCursorNode = node;
            else if (pointerCursorNode) {
                // We want the lowest unbroken chain of pointer cursors.
                pointerCursorStillValid = false;
            }

            if (node->willRespondToMouseClickEvents() || node->willRespondToMouseMoveEvents() || (is<Element>(*node) && downcast<Element>(*node).isMouseFocusable())) {
                // If we're at the body or higher, use the pointer cursor node (which may be null).
                if (bodyHasBeenReached)
                    node = pointerCursorNode;

                // If we are interested about the frame, use it.
                if (nodeBounds) {
                    // This is a check to see whether this node is an area element. The only way this can happen is if this is the first check.
                    if (node == hitTestResult.innerNode() && node != hitTestResult.innerNonSharedNode() && is<HTMLAreaElement>(*node))
                        *nodeBounds = snappedIntRect(downcast<HTMLAreaElement>(*node).computeRect(hitTestResult.innerNonSharedNode()->renderer()));
                    else if (node && node->renderer())
                        *nodeBounds = node->renderer()->absoluteBoundingBoxRect(true);
                }

                return node;
            }
        }

        return nullptr;
    };

    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTFMove(ancestorRespondingToClickEvents), true);
}

Node* Frame::nodeRespondingToClickEvents(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation, SecurityOrigin* securityOrigin)
{
    auto&& ancestorRespondingToClickEvents = [securityOrigin](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> Node* {
        if (nodeBounds)
            *nodeBounds = IntRect();

        auto node = hitTestResult.innerNode();
        if (!node || (securityOrigin && !securityOrigin->isSameOriginAs(node->document().securityOrigin())))
            return nullptr;

        for (; node && node != terminationNode; node = node->parentInComposedTree()) {
            if (node->willRespondToMouseClickEvents() || node->willRespondToMouseMoveEvents() || (is<Element>(*node) && downcast<Element>(*node).isMouseFocusable())) {
                // If we are interested about the frame, use it.
                if (nodeBounds) {
                    // This is a check to see whether this node is an area element. The only way this can happen is if this is the first check.
                    if (node == hitTestResult.innerNode() && node != hitTestResult.innerNonSharedNode() && is<HTMLAreaElement>(*node))
                        *nodeBounds = snappedIntRect(downcast<HTMLAreaElement>(*node).computeRect(hitTestResult.innerNonSharedNode()->renderer()));
                    else if (node && node->renderer())
                        *nodeBounds = node->renderer()->absoluteBoundingBoxRect(true);
                }

                return node;
            }
        }

        return nullptr;
    };

    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTFMove(ancestorRespondingToClickEvents), true);
}

Node* Frame::nodeRespondingToDoubleClickEvent(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation)
{
    auto&& ancestorRespondingToDoubleClickEvent = [](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> Node* {
        if (nodeBounds)
            *nodeBounds = IntRect();

        auto* node = hitTestResult.innerNode();
        if (!node)
            return nullptr;

        for (; node && node != terminationNode; node = node->parentInComposedTree()) {
            if (!node->hasEventListeners(eventNames().dblclickEvent))
                continue;
#if ENABLE(TOUCH_EVENTS)
            if (!node->allowsDoubleTapGesture())
                continue;
#endif
            if (nodeBounds && node->renderer())
                *nodeBounds = node->renderer()->absoluteBoundingBoxRect(true);
            return node;
        }
        return nullptr;
    };

    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTFMove(ancestorRespondingToDoubleClickEvent), true);
}

Node* Frame::nodeRespondingToScrollWheelEvents(const FloatPoint& viewportLocation)
{
    auto&& ancestorRespondingToScrollWheelEvents = [](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> Node* {
        if (nodeBounds)
            *nodeBounds = IntRect();

        Node* scrollingAncestor = nullptr;
        for (Node* node = hitTestResult.innerNode(); node && node != terminationNode && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;

            if ((renderer->isTextField() || renderer->isTextArea()) && downcast<RenderTextControl>(*renderer).canScroll()) {
                scrollingAncestor = node;
                continue;
            }

            auto& style = renderer->style();

            if (renderer->hasOverflowClip()
                && (style.overflowY() == Overflow::Auto || style.overflowY() == Overflow::Scroll
                || style.overflowX() == Overflow::Auto || style.overflowX() == Overflow::Scroll)) {
                scrollingAncestor = node;
            }
        }

        return scrollingAncestor;
    };

    FloatPoint adjustedViewportLocation;
    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTFMove(ancestorRespondingToScrollWheelEvents), false);
}

int Frame::preferredHeight() const
{
    Document* document = this->document();
    if (!document)
        return 0;

    document->updateLayout();

    auto* body = document->bodyOrFrameset();
    if (!body)
        return 0;

    RenderObject* renderer = body->renderer();
    if (!is<RenderBlock>(renderer))
        return 0;

    RenderBlock& block = downcast<RenderBlock>(*renderer);
    return block.height() + block.marginTop() + block.marginBottom();
}

void Frame::updateLayout() const
{
    Document* document = this->document();
    if (!document)
        return;

    document->updateLayout();

    if (FrameView* view = this->view())
        view->adjustViewSize();
}

NSRect Frame::caretRect()
{
    VisibleSelection visibleSelection = selection().selection();
    if (visibleSelection.isNone())
        return CGRectZero;
    return visibleSelection.isCaret() ? selection().absoluteCaretBounds() : VisiblePosition(visibleSelection.end()).absoluteCaretBounds();
}

NSRect Frame::rectForScrollToVisible()
{
    VisibleSelection selection(this->selection().selection());

    if (selection.isNone())
        return CGRectZero;

    if (selection.isCaret())
        return caretRect();

    return unionRect(selection.visibleStart().absoluteCaretBounds(), selection.visibleEnd().absoluteCaretBounds());
}

unsigned Frame::formElementsCharacterCount() const
{
    Document* document = this->document();
    if (!document)
        return 0;
    return document->formController().formElementsCharacterCount();
}

void Frame::setTimersPaused(bool paused)
{
    if (!m_page)
        return;
    JSLockHolder lock(commonVM());
    if (paused)
        m_page->suspendActiveDOMObjectsAndAnimations();
    else
        m_page->resumeActiveDOMObjectsAndAnimations();
}

void Frame::dispatchPageHideEventBeforePause()
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    for (Frame* frame = this; frame; frame = frame->tree().traverseNext(this))
        frame->document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, true), document());
}

void Frame::dispatchPageShowEventBeforeResume()
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    for (Frame* frame = this; frame; frame = frame->tree().traverseNext(this))
        frame->document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pageshowEvent, true), document());
}

void Frame::setRangedSelectionBaseToCurrentSelection()
{
    m_rangedSelectionBase = selection().selection();
}

void Frame::setRangedSelectionBaseToCurrentSelectionStart()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionBase = VisibleSelection(visibleSelection.start(), visibleSelection.affinity());
}

void Frame::setRangedSelectionBaseToCurrentSelectionEnd()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionBase = VisibleSelection(visibleSelection.end(), visibleSelection.affinity());
}

VisibleSelection Frame::rangedSelectionBase() const
{
    return m_rangedSelectionBase;
}

void Frame::clearRangedSelectionInitialExtent()
{
    m_rangedSelectionInitialExtent = VisibleSelection();
}

void Frame::setRangedSelectionInitialExtentToCurrentSelectionStart()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionInitialExtent = VisibleSelection(visibleSelection.start(), visibleSelection.affinity());
}

void Frame::setRangedSelectionInitialExtentToCurrentSelectionEnd()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionInitialExtent = VisibleSelection(visibleSelection.end(), visibleSelection.affinity());
}

VisibleSelection Frame::rangedSelectionInitialExtent() const
{
    return m_rangedSelectionInitialExtent;
}

void Frame::recursiveSetUpdateAppearanceEnabled(bool enabled)
{
    selection().setUpdateAppearanceEnabled(enabled);
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->recursiveSetUpdateAppearanceEnabled(enabled);
}

// FIXME: Break this function up into pieces with descriptive function names so that it's easier to follow.
NSArray *Frame::interpretationsForCurrentRoot() const
{
    if (!document())
        return nil;

    auto* root = selection().selection().selectionType() == VisibleSelection::NoSelection ? document()->bodyOrFrameset() : selection().selection().rootEditableElement();
    unsigned rootChildCount = root->countChildNodes();
    auto rangeOfRootContents = Range::create(*document(), createLegacyEditingPosition(root, 0), createLegacyEditingPosition(root, rootChildCount));

    auto markersInRoot = document()->markers().markersInRange(rangeOfRootContents, DocumentMarker::DictationPhraseWithAlternatives);

    // There are no phrases with alternatives, so there is just one interpretation.
    if (markersInRoot.isEmpty())
        return [NSArray arrayWithObject:plainText(rangeOfRootContents.ptr())];

    // The number of interpretations will be i1 * i2 * ... * iN, where iX is the number of interpretations for the Xth phrase with alternatives.
    size_t interpretationsCount = 1;

    for (auto* marker : markersInRoot)
        interpretationsCount *= marker->alternatives().size() + 1;

    Vector<Vector<UChar>> interpretations;
    interpretations.grow(interpretationsCount);

    Position precedingTextStartPosition = createLegacyEditingPosition(root, 0);

    unsigned combinationsSoFar = 1;

    Node* pastLastNode = rangeOfRootContents->pastLastNode();
    for (Node* node = rangeOfRootContents->firstNode(); node != pastLastNode; node = NodeTraversal::next(*node)) {
        ASSERT(node);
        for (auto* marker : document()->markers().markersFor(*node, DocumentMarker::DictationPhraseWithAlternatives)) {
            // First, add text that precede the marker.
            if (precedingTextStartPosition != createLegacyEditingPosition(node, marker->startOffset())) {
                auto precedingTextRange = Range::create(*document(), precedingTextStartPosition, createLegacyEditingPosition(node, marker->startOffset()));
                String precedingText = plainText(precedingTextRange.ptr());
                if (!precedingText.isEmpty()) {
                    for (auto& interpretation : interpretations)
                        append(interpretation, precedingText);
                }
            }

            auto rangeForMarker = Range::create(*document(), createLegacyEditingPosition(node, marker->startOffset()), createLegacyEditingPosition(node, marker->endOffset()));
            String visibleTextForMarker = plainText(rangeForMarker.ptr());
            size_t interpretationsCountForCurrentMarker = marker->alternatives().size() + 1;
            for (size_t i = 0; i < interpretationsCount; ++i) {
                // Determine text for the ith interpretation. It will either be the visible text, or one of its
                // alternatives stored in the marker.

                size_t indexOfInterpretationForCurrentMarker = (i / combinationsSoFar) % interpretationsCountForCurrentMarker;
                if (!indexOfInterpretationForCurrentMarker)
                    append(interpretations[i], visibleTextForMarker);
                else
                    append(interpretations[i], marker->alternatives().at(i % marker->alternatives().size()));
            }

            combinationsSoFar *= interpretationsCountForCurrentMarker;

            precedingTextStartPosition = createLegacyEditingPosition(node, marker->endOffset());
        }
    }

    // Finally, add any text after the last marker.
    auto afterLastMarkerRange = Range::create(*document(), precedingTextStartPosition, createLegacyEditingPosition(root, rootChildCount));
    String textAfterLastMarker = plainText(afterLastMarkerRange.ptr());
    if (!textAfterLastMarker.isEmpty()) {
        for (auto& interpretation : interpretations)
            append(interpretation, textAfterLastMarker);
    }

    NSMutableArray *result = [NSMutableArray array];
    for (auto& interpretation : interpretations)
        [result addObject:adoptNS([[NSString alloc] initWithCharacters:interpretation.data() length:interpretation.size()]).get()];

    return result;
}

void Frame::viewportOffsetChanged(ViewportOffsetChangeType changeType)
{
    LOG_WITH_STREAM(Scrolling, stream << "Frame::viewportOffsetChanged - " << (changeType == IncrementalScrollOffset ? "incremental" : "completed"));

    if (changeType == IncrementalScrollOffset) {
        if (RenderView* root = contentRenderer())
            root->compositor().didChangeVisibleRect();
    }

    if (changeType == CompletedScrollOffset) {
        if (RenderView* root = contentRenderer())
            root->compositor().updateCompositingLayers(CompositingUpdateType::OnScroll);
    }
}

bool Frame::containsTiledBackingLayers() const
{
    if (RenderView* root = contentRenderer())
        return root->compositor().hasNonMainLayersWithTiledBacking();

    return false;
}

void Frame::overflowScrollPositionChangedForNode(const IntPoint& position, Node* node, bool isUserScroll)
{
    LOG_WITH_STREAM(Scrolling, stream << "Frame::overflowScrollPositionChangedForNode " << node << " position " << position);

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->hasLayer())
        return;

    RenderLayer& layer = *downcast<RenderBoxModelObject>(*renderer).layer();

    auto oldScrollType = layer.currentScrollType();
    layer.setCurrentScrollType(isUserScroll ? ScrollType::User : ScrollType::Programmatic);
    layer.scrollToOffsetWithoutAnimation(position);
    layer.setCurrentScrollType(oldScrollType);

    layer.didEndScroll(); // FIXME: Should we always call this?
}

void Frame::resetAllGeolocationPermission()
{
    if (document()->domWindow())
        document()->domWindow()->resetAllGeolocationPermission();

    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->resetAllGeolocationPermission();
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
