/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
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
#import "LocalFrame.h"

#if PLATFORM(IOS_FAMILY)

#import "CSSStyleProperties.h"
#import "CommonVM.h"
#import "ComposedTreeIterator.h"
#import "ContainerNodeInlines.h"
#import "DocumentMarkerController.h"
#import "DocumentMarkers.h"
#import "DocumentSecurityOrigin.h"
#import "Editor.h"
#import "EditorClient.h"
#import "ElementRareData.h"
#import "EventHandler.h"
#import "EventNames.h"
#import "EventTargetInlines.h"
#import "FormController.h"
#import "FrameSelection.h"
#import "HTMLAreaElement.h"
#import "HTMLBodyElement.h"
#import "HTMLDocument.h"
#import "HTMLHtmlElement.h"
#import "HTMLNames.h"
#import "HTMLObjectElement.h"
#import "HitTestRequest.h"
#import "HitTestResult.h"
#import "LocalDOMWindow.h"
#import "LocalFrameView.h"
#import "Logging.h"
#import "NodeInlines.h"
#import "NodeRenderStyle.h"
#import "NodeTraversal.h"
#import "Page.h"
#import "PageTransitionEvent.h"
#import "PlatformScreen.h"
#import "Range.h"
#import "RenderLayer.h"
#import "RenderLayerCompositor.h"
#import "RenderLayerScrollableArea.h"
#import "RenderObjectInlines.h"
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
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/TextStream.h>

using namespace WebCore::HTMLNames;
using namespace WTF::Unicode;

using JSC::JSLockHolder;

namespace WebCore {

// Create <html><body (style="...")></body></html> doing minimal amount of work.
void LocalFrame::initWithSimpleHTMLDocument(const AtomString& style, const URL& url)
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

const ViewportArguments& LocalFrame::viewportArguments() const
{
    return m_viewportArguments;
}

void LocalFrame::setViewportArguments(const ViewportArguments& arguments)
{
    m_viewportArguments.get() = arguments;
}

NSArray *LocalFrame::wordsInCurrentParagraph() const
{
    protect(document())->updateLayout();

    if (!page() || !page()->selection().isCaret())
        return nil;

    VisiblePosition position(page()->selection().start(), page()->selection().affinity());
    VisiblePosition end(position);

    if (!isStartOfParagraph(end)) {
        VisiblePosition previous = end.previous();
        char16_t c(previous.characterAfter());
        // FIXME: Should use something from ICU or ASCIICType that is not subject to POSIX current language rather than iswpunct.
        if (!iswpunct(c) && !deprecatedIsSpaceOrNewline(c) && c != noBreakSpace)
            end = startOfWord(end);
    }
    VisiblePosition start(startOfParagraph(end));

    auto searchRange = makeSimpleRange(start, end);
    if (!searchRange || searchRange->collapsed())
        return nil;

    NSMutableArray *words = [NSMutableArray array];

    WordAwareIterator it(*searchRange);
    while (!it.atEnd()) {
        StringView text = it.text();
        int length = text.length();
        if (length > 1 || !deprecatedIsSpaceOrNewline(text[0])) {
            int startOfWordBoundary = 0;
            for (int i = 1; i < length; i++) {
                if (deprecatedIsSpaceOrNewline(text[i]) || text[i] == noBreakSpace) {
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
        char16_t c(previous.characterAfter());
        if (!deprecatedIsSpaceOrNewline(c) && c != noBreakSpace)
            [words removeLastObject];
    }

    return words;
}

#define CHECK_FONT_SIZE 0
#define RECT_LOGGING 0

CGRect LocalFrame::renderRectForPoint(CGPoint point, bool* isReplaced, float* fontSize) const
{
    *isReplaced = false;
    *fontSize = 0;

    if (!m_doc || !m_doc->renderBox())
        return CGRectZero;

    // FIXME: Why this layer check?
    RenderLayer* layer = m_doc->renderBox()->layer();
    if (!layer)
        return CGRectZero;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    auto result = eventHandler().hitTestResultAtPoint(IntPoint(roundf(point.x), roundf(point.y)), hitType);

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
        if (renderer->isRenderBlock() || renderer->isNonReplacedAtomicInlineLevelBox() || renderer->isBlockLevelReplacedOrAtomicInline()) {
            *isReplaced = renderer->isBlockLevelReplacedOrAtomicInline();
#if CHECK_FONT_SIZE
            for (RenderObject* textRenderer = hitRenderer; textRenderer; textRenderer = textRenderer->traverseNext(hitRenderer)) {
                if (textRenderer->isText()) {
                    *fontSize = textRenderer->font(true).size();
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

Node* LocalFrame::deepestNodeAtLocation(const FloatPoint& viewportLocation)
{
    IntPoint center;
    HitTestResult hitTestResult;
    if (!hitTestResultAtViewportLocation(viewportLocation, hitTestResult, center))
        return nullptr;

    return hitTestResult.innerNode();
}

RefPtr<Node> LocalFrame::approximateNodeAtViewportLocationLegacy(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation)
{
    // This function is only used for UIWebView.
    auto&& ancestorRespondingToClickEvents = [](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> RefPtr<Node> {
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
                // Make sure we cover the case of an empty editable body.
                if (!pointerCursorNode && node->isContentEditable())
                    pointerCursorNode = node;
                bodyHasBeenReached = true;
                pointerCursorStillValid = false;
            }

            // If we already have a pointer, and we reach a table, don't accept it.
            if (pointerCursorNode && (node->hasTagName(HTMLNames::tableTag) || node->hasTagName(HTMLNames::tbodyTag)))
                pointerCursorStillValid = false;

            // If we haven't reached the body, and we are still paying attention to pointer cursors, and the node has a pointer cursor.
            if (pointerCursorStillValid && node->renderStyle() && node->renderStyle()->cursorType() == CursorType::Pointer)
                pointerCursorNode = node;
            else if (pointerCursorNode) {
                // We want the lowest unbroken chain of pointer cursors.
                pointerCursorStillValid = false;
            }

            if (LocalFrame::nodeWillRespondToMouseEvents(*node)) {
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

    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTF::move(ancestorRespondingToClickEvents), ShouldApproximate::Yes);
}

RefPtr<Node> LocalFrame::nodeRespondingToScrollWheelEvents(const FloatPoint& viewportLocation)
{
    auto&& ancestorRespondingToScrollWheelEvents = [](const HitTestResult& hitTestResult, Node* terminationNode, IntRect* nodeBounds) -> RefPtr<Node> {
        if (nodeBounds)
            *nodeBounds = IntRect();

        Node* scrollingAncestor = nullptr;
        for (Node* node = hitTestResult.innerNode(); node && node != terminationNode && !node->hasTagName(HTMLNames::bodyTag); node = node->parentNode()) {
            RenderObject* renderer = node->renderer();
            if (!renderer)
                continue;

            if ((renderer->isRenderTextControlSingleLine() || renderer->isRenderTextControlMultiLine()) && downcast<RenderTextControl>(*renderer).canScroll()) {
                scrollingAncestor = node;
                continue;
            }

            auto& style = renderer->style();

            if (renderer->hasNonVisibleOverflow()
                && (style.overflowY() == Overflow::Auto || style.overflowY() == Overflow::Scroll
                || style.overflowX() == Overflow::Auto || style.overflowX() == Overflow::Scroll)) {
                scrollingAncestor = node;
            }
        }

        return scrollingAncestor;
    };

    FloatPoint adjustedViewportLocation;
    return qualifyingNodeAtViewportLocation(viewportLocation, adjustedViewportLocation, WTF::move(ancestorRespondingToScrollWheelEvents), ShouldApproximate::No);
}

int LocalFrame::preferredHeight() const
{
    RefPtr document = this->document();
    if (!document)
        return 0;

    document->updateLayout();

    auto* body = document->bodyOrFrameset();
    if (!body)
        return 0;

    auto* block = dynamicDowncast<RenderBlock>(body->renderer());
    if (!block)
        return 0;

    return block->height() + block->marginTop() + block->marginBottom();
}

void LocalFrame::updateLayout() const
{
    RefPtr document = this->document();
    if (!document)
        return;

    document->updateLayout();

    if (auto* view = this->view())
        view->adjustViewSize();
}

IntRect LocalFrame::caretRect()
{
    VisibleSelection visibleSelection = selection().selection();
    if (visibleSelection.isNone())
        return { };
    return visibleSelection.isCaret() ? selection().absoluteCaretBounds() : VisiblePosition(visibleSelection.end()).absoluteCaretBounds();
}

IntRect LocalFrame::rectForScrollToVisible()
{
    VisibleSelection selection(this->selection().selection());

    if (selection.isNone())
        return { };

    if (selection.isCaret())
        return caretRect();

    return unionRect(selection.visibleStart().absoluteCaretBounds(), selection.visibleEnd().absoluteCaretBounds());
}

void LocalFrame::setTimersPaused(bool paused)
{
    auto* page = this->page();
    if (!page)
        return;
    JSLockHolder lock(commonVM());
    if (paused)
        page->suspendActiveDOMObjectsAndAnimations();
    else
        page->resumeActiveDOMObjectsAndAnimations();
}

void LocalFrame::dispatchPageHideEventBeforePause()
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    Page::forEachDocumentFromMainFrame(*this, [](Document& document) {
        document.dispatchPagehideEvent(PageshowEventPersistence::Persisted);
    });
}

void LocalFrame::dispatchPageShowEventBeforeResume()
{
    ASSERT(isMainFrame());
    if (!isMainFrame())
        return;

    Page::forEachDocumentFromMainFrame(*this, [](Document& document) {
        document.dispatchPageshowEvent(PageshowEventPersistence::Persisted);
    });
}

void LocalFrame::setRangedSelectionBaseToCurrentSelection()
{
    m_rangedSelectionBase.get() = selection().selection();
}

void LocalFrame::setRangedSelectionBaseToCurrentSelectionStart()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionBase.get() = VisibleSelection(visibleSelection.start(), visibleSelection.affinity());
}

void LocalFrame::setRangedSelectionBaseToCurrentSelectionEnd()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionBase.get() = VisibleSelection(visibleSelection.end(), visibleSelection.affinity());
}

VisibleSelection LocalFrame::rangedSelectionBase() const
{
    return m_rangedSelectionBase.get();
}

void LocalFrame::clearRangedSelectionInitialExtent()
{
    m_rangedSelectionInitialExtent.get() = VisibleSelection();
}

void LocalFrame::setRangedSelectionInitialExtentToCurrentSelectionStart()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionInitialExtent.get() = VisibleSelection(visibleSelection.start(), visibleSelection.affinity());
}

void LocalFrame::setRangedSelectionInitialExtentToCurrentSelectionEnd()
{
    const VisibleSelection& visibleSelection = selection().selection();
    m_rangedSelectionInitialExtent.get() = VisibleSelection(visibleSelection.end(), visibleSelection.affinity());
}

VisibleSelection LocalFrame::rangedSelectionInitialExtent() const
{
    return m_rangedSelectionInitialExtent.get();
}

void LocalFrame::recursiveSetUpdateAppearanceEnabled(bool enabled)
{
    selection().setUpdateAppearanceEnabled(enabled);
    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child.get());
        if (!localChild)
            continue;
        localChild->recursiveSetUpdateAppearanceEnabled(enabled);
    }
}

// FIXME: Break this function up into pieces with descriptive function names so that it's easier to follow.
NSArray *LocalFrame::interpretationsForCurrentRoot() const
{
    if (!document())
        return nil;

    auto* root = selection().isNone() ? document()->bodyOrFrameset() : selection().selection().rootEditableElement();
    auto rangeOfRootContents = makeRangeSelectingNodeContents(*root);

    auto markersInRoot = document()->markers().markersInRange(rangeOfRootContents, DocumentMarkerType::DictationPhraseWithAlternatives);

    // There are no phrases with alternatives, so there is just one interpretation.
    if (markersInRoot.isEmpty())
        return @[plainText(rangeOfRootContents).createNSString().get()];

    // The number of interpretations will be i1 * i2 * ... * iN, where iX is the number of interpretations for the Xth phrase with alternatives.
    size_t interpretationsCount = 1;

    for (auto& marker : markersInRoot)
        interpretationsCount *= std::get<Vector<String>>(marker->data()).size() + 1;

    Vector<Vector<char16_t>> interpretations;
    interpretations.grow(interpretationsCount);

    Position precedingTextStartPosition = makeDeprecatedLegacyPosition(root, 0);

    unsigned combinationsSoFar = 1;

    for (auto& node : intersectingNodes(rangeOfRootContents)) {
        for (auto& marker : document()->markers().markersFor(node, DocumentMarkerType::DictationPhraseWithAlternatives)) {
            auto& alternatives = std::get<Vector<String>>(marker->data());

            auto rangeForMarker = makeSimpleRange(node, *marker);

            if (auto precedingTextRange = makeSimpleRange(precedingTextStartPosition, rangeForMarker.start)) {
                String precedingText = plainText(*precedingTextRange);
                if (!precedingText.isEmpty()) {
                    for (auto& interpretation : interpretations)
                        append(interpretation, precedingText);
                }
            }

            String visibleTextForMarker = plainText(rangeForMarker);
            size_t interpretationsCountForCurrentMarker = alternatives.size() + 1;
            for (size_t i = 0; i < interpretationsCount; ++i) {
                size_t indexOfInterpretationForCurrentMarker = (i / combinationsSoFar) % interpretationsCountForCurrentMarker;
                if (!indexOfInterpretationForCurrentMarker)
                    append(interpretations[i], visibleTextForMarker);
                else
                    append(interpretations[i], alternatives[i % alternatives.size()]);
            }

            combinationsSoFar *= interpretationsCountForCurrentMarker;

            precedingTextStartPosition = makeDeprecatedLegacyPosition(rangeForMarker.end);
        }
    }

    // Finally, add any text after the last marker.
    if (auto range = makeSimpleRange(precedingTextStartPosition, rangeOfRootContents.end)) {
        String textAfterLastMarker = plainText(*range);
        if (!textAfterLastMarker.isEmpty()) {
            for (auto& interpretation : interpretations)
                append(interpretation, textAfterLastMarker);
        }
    }

    return createNSArray(interpretations, [] (auto& interpretation) {
        return adoptNS([[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(interpretation.span().data()) length:interpretation.size()]);
    }).autorelease();
}

void LocalFrame::viewportOffsetChanged(ViewportOffsetChangeType changeType)
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

bool LocalFrame::containsTiledBackingLayers() const
{
    if (RenderView* root = contentRenderer())
        return root->compositor().hasNonMainLayersWithTiledBacking();

    return false;
}

void LocalFrame::overflowScrollPositionChangedForNode(const IntPoint& position, Node* node, bool isUserScroll)
{
    LOG_WITH_STREAM(Scrolling, stream << "Frame::overflowScrollPositionChangedForNode " << node << " position " << position);

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->hasLayer())
        return;

    CheckedPtr layer = downcast<RenderBoxModelObject>(*renderer).layer();
    if (!layer)
        return;

    CheckedPtr scrollableArea = layer->ensureLayerScrollableArea();
    {
        auto scope = ScrollTypeScope(*scrollableArea, isUserScroll ? ScrollType::User : ScrollType::Programmatic);
        scrollableArea->scrollToOffsetWithoutAnimation(position);
    }

    scrollableArea->didEndScroll(); // FIXME: Should we always call this?
}

void LocalFrame::resetAllGeolocationPermission()
{
    if (document()->window())
        document()->window()->resetAllGeolocationPermission();

    for (RefPtr child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        auto* localChild = dynamicDowncast<LocalFrame>(child.get());
        if (!localChild)
            continue;
        localChild->resetAllGeolocationPermission();
    }
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
