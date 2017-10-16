/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderTreeUpdater.h"

#include "AXObjectCache.h"
#include "CSSAnimationController.h"
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "Element.h"
#include "FlowThreadController.h"
#include "HTMLSlotElement.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderStyle.h"
#include "PseudoElement.h"
#include "RenderDescendantIterator.h"
#include "RenderFullScreen.h"
#include "RenderListItem.h"
#include "RenderNamedFlowThread.h"
#include "RenderQuote.h"
#include "RenderTreeUpdaterFirstLetter.h"
#include "RenderTreeUpdaterListItem.h"
#include "StyleResolver.h"
#include "StyleTreeResolver.h"
#include <wtf/SystemTracing.h>

#if PLATFORM(IOS)
#include "WKContentObservation.h"
#include "WKContentObservationInternal.h"
#endif

namespace WebCore {

#if PLATFORM(IOS)
class CheckForVisibilityChange {
public:
    CheckForVisibilityChange(const Element&);
    ~CheckForVisibilityChange();

private:
    const Element& m_element;
    EDisplay m_previousDisplay;
    EVisibility m_previousVisibility;
    EVisibility m_previousImplicitVisibility;
};
#endif // PLATFORM(IOS)

RenderTreeUpdater::Parent::Parent(ContainerNode& root)
    : element(is<Document>(root) ? nullptr : downcast<Element>(&root))
    , renderTreePosition(RenderTreePosition(*root.renderer()))
{
}

RenderTreeUpdater::Parent::Parent(Element& element, Style::Change styleChange)
    : element(&element)
    , styleChange(styleChange)
    , renderTreePosition(element.renderer() ? std::make_optional(RenderTreePosition(*element.renderer())) : std::nullopt)
{
}


RenderTreeUpdater::RenderTreeUpdater(Document& document)
    : m_document(document)
{
}

static ContainerNode* findRenderingRoot(ContainerNode& node)
{
    if (node.renderer())
        return &node;
    for (auto& ancestor : composedTreeAncestors(node)) {
        if (ancestor.renderer())
            return &ancestor;
        if (!ancestor.hasDisplayContents())
            return nullptr;
    }
    return &node.document();
}

static ListHashSet<ContainerNode*> findRenderingRoots(const Style::Update& update)
{
    ListHashSet<ContainerNode*> renderingRoots;
    for (auto* root : update.roots()) {
        auto* renderingRoot = findRenderingRoot(*root);
        if (!renderingRoot)
            continue;
        renderingRoots.add(renderingRoot);
    }
    return renderingRoots;
}

void RenderTreeUpdater::commit(std::unique_ptr<const Style::Update> styleUpdate)
{
    ASSERT(&m_document == &styleUpdate->document());

    if (!m_document.shouldCreateRenderers() || !m_document.renderView())
        return;
    
    TraceScope scope(RenderTreeBuildStart, RenderTreeBuildEnd);

    Style::PostResolutionCallbackDisabler callbackDisabler(m_document);

    m_styleUpdate = WTFMove(styleUpdate);

    for (auto* root : findRenderingRoots(*m_styleUpdate))
        updateRenderTree(*root);

    if (m_document.renderView()->hasQuotesNeedingUpdate()) {
        updateQuotesUpTo(nullptr);
        m_previousUpdatedQuote = nullptr;
        m_document.renderView()->setHasQuotesNeedingUpdate(false);
    }

    m_styleUpdate = nullptr;
}

static bool shouldCreateRenderer(const Element& element, const RenderElement& parentRenderer)
{
    if (!parentRenderer.canHaveChildren() && !(element.isPseudoElement() && parentRenderer.canHaveGeneratedChildren()))
        return false;
    if (parentRenderer.element() && !parentRenderer.element()->childShouldCreateRenderer(element))
        return false;
    return true;
}

void RenderTreeUpdater::updateRenderTree(ContainerNode& root)
{
    ASSERT(root.renderer());
    ASSERT(m_parentStack.isEmpty());

    m_parentStack.append(Parent(root));

    auto descendants = composedTreeDescendants(root);
    auto it = descendants.begin();
    auto end = descendants.end();

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=156172
    it.dropAssertions();

    while (it != end) {
        popParentsToDepth(it.depth());

        auto& node = *it;

        if (auto* renderer = node.renderer())
            renderTreePosition().invalidateNextSibling(*renderer);
        else if (is<Element>(node) && downcast<Element>(node).hasDisplayContents())
            renderTreePosition().invalidateNextSibling();

        if (is<Text>(node)) {
            auto& text = downcast<Text>(node);
            auto* textUpdate = m_styleUpdate->textUpdate(text);
            if (parent().styleChange == Style::Detach || textUpdate || m_invalidatedWhitespaceOnlyTextSiblings.contains(&text))
                updateTextRenderer(text, textUpdate);

            it.traverseNextSkippingChildren();
            continue;
        }

        auto& element = downcast<Element>(node);

        auto* elementUpdate = m_styleUpdate->elementUpdate(element);

        // We hop through display: contents elements in findRenderingRoot, so
        // there may be other updates down the tree.
        if (!elementUpdate && !element.hasDisplayContents()) {
            it.traverseNextSkippingChildren();
            continue;
        }

        if (elementUpdate)
            updateElementRenderer(element, *elementUpdate);

        bool mayHaveRenderedDescendants = element.renderer() || (element.hasDisplayContents() && shouldCreateRenderer(element, renderTreePosition().parent()));
        if (!mayHaveRenderedDescendants) {
            it.traverseNextSkippingChildren();
            continue;
        }

        pushParent(element, elementUpdate ? elementUpdate->change : Style::NoChange);

        it.traverseNext();
    }

    popParentsToDepth(0);

    m_invalidatedWhitespaceOnlyTextSiblings.clear();
}

RenderTreePosition& RenderTreeUpdater::renderTreePosition()
{
    for (unsigned i = m_parentStack.size(); i; --i) {
        if (auto& position = m_parentStack[i - 1].renderTreePosition)
            return *position;
    }
    ASSERT_NOT_REACHED();
    return *m_parentStack.last().renderTreePosition;
}

void RenderTreeUpdater::pushParent(Element& element, Style::Change changeType)
{
    m_parentStack.append(Parent(element, changeType));

    updateBeforeDescendants(element);
}

void RenderTreeUpdater::popParent()
{
    auto& parent = m_parentStack.last();
    if (parent.element)
        updateAfterDescendants(*parent.element, parent.styleChange);

    m_parentStack.removeLast();
}

void RenderTreeUpdater::popParentsToDepth(unsigned depth)
{
    ASSERT(m_parentStack.size() >= depth);

    while (m_parentStack.size() > depth)
        popParent();
}

void RenderTreeUpdater::updateBeforeDescendants(Element& element)
{
    updateBeforeOrAfterPseudoElement(element, BEFORE);
}

void RenderTreeUpdater::updateAfterDescendants(Element& element, Style::Change styleChange)
{
    updateBeforeOrAfterPseudoElement(element, AFTER);

    auto* renderer = element.renderer();
    if (!renderer)
        return;

    // These functions do render tree mutations that require descendant renderers.
    if (is<RenderBlock>(*renderer))
        FirstLetter::update(downcast<RenderBlock>(*renderer));
    if (is<RenderListItem>(*renderer))
        ListItem::updateMarker(downcast<RenderListItem>(*renderer));

    if (element.hasCustomStyleResolveCallbacks() && styleChange == Style::Detach)
        element.didAttachRenderers();
}

static bool pseudoStyleCacheIsInvalid(RenderElement* renderer, RenderStyle* newStyle)
{
    const RenderStyle& currentStyle = renderer->style();

    const PseudoStyleCache* pseudoStyleCache = currentStyle.cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    for (auto& cache : *pseudoStyleCache) {
        PseudoId pseudoId = cache->styleType();
        std::unique_ptr<RenderStyle> newPseudoStyle = renderer->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *cache) {
            newStyle->addCachedPseudoStyle(WTFMove(newPseudoStyle));
            return true;
        }
    }
    return false;
}

void RenderTreeUpdater::updateElementRenderer(Element& element, const Style::ElementUpdate& update)
{
#if PLATFORM(IOS)
    CheckForVisibilityChange checkForVisibilityChange(element);
#endif

    bool shouldTearDownRenderers = update.change == Style::Detach
        && (element.renderer() || element.isNamedFlowContentElement() || element.hasDisplayContents());
    if (shouldTearDownRenderers) {
        if (!element.renderer()) {
            // We may be tearing down a descendant renderer cached in renderTreePosition.
            renderTreePosition().invalidateNextSibling();
        }
        tearDownRenderers(element, TeardownType::KeepHoverAndActive);
    }

    bool hasDisplayContents = update.style->display() == CONTENTS;
    if (hasDisplayContents)
        element.storeDisplayContentsStyle(RenderStyle::clonePtr(*update.style));
    else
        element.resetComputedStyle();

    bool shouldCreateNewRenderer = !element.renderer() && !hasDisplayContents;
    if (shouldCreateNewRenderer) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willAttachRenderers();
        createRenderer(element, RenderStyle::clone(*update.style));
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(element);
        return;
    }

    if (!element.renderer())
        return;
    auto& renderer = *element.renderer();

    if (update.recompositeLayer) {
        renderer.setStyle(RenderStyle::clone(*update.style), StyleDifferenceRecompositeLayer);
        return;
    }

    if (update.change == Style::NoChange) {
        if (pseudoStyleCacheIsInvalid(&renderer, update.style.get())) {
            renderer.setStyle(RenderStyle::clone(*update.style), StyleDifferenceEqual);
            return;
        }
        return;
    }

    renderer.setStyle(RenderStyle::clone(*update.style), StyleDifferenceEqual);
}

#if ENABLE(CSS_REGIONS)
static void registerElementForFlowThreadIfNeeded(Element& element, const RenderStyle& style)
{
    if (!element.shouldMoveToFlowThread(style))
        return;
    FlowThreadController& flowThreadController = element.document().renderView()->flowThreadController();
    flowThreadController.registerNamedFlowContentElement(element, flowThreadController.ensureRenderFlowThreadWithName(style.flowThread()));
}
#endif

void RenderTreeUpdater::createRenderer(Element& element, RenderStyle&& style)
{
    auto computeInsertionPosition = [this, &element, &style] () {
#if ENABLE(CSS_REGIONS)
        if (element.shouldMoveToFlowThread(style))
            return RenderTreePosition::insertionPositionForFlowThread(renderTreePosition().parent().element(), element, style);
#endif
        renderTreePosition().computeNextSibling(element);
        return renderTreePosition();
    };
    
    if (!shouldCreateRenderer(element, renderTreePosition().parent()))
        return;

#if ENABLE(CSS_REGIONS)
    // Even display: none elements need to be registered in FlowThreadController.
    registerElementForFlowThreadIfNeeded(element, style);
#endif

    if (!element.rendererIsNeeded(style))
        return;

    RenderTreePosition insertionPosition = computeInsertionPosition();
    RenderElement* newRenderer = element.createElementRenderer(WTFMove(style), insertionPosition).leakPtr();
    if (!newRenderer)
        return;
    if (!insertionPosition.canInsert(*newRenderer)) {
        newRenderer->destroy();
        return;
    }

    // Make sure the RenderObject already knows it is going to be added to a RenderFlowThread before we set the style
    // for the first time. Otherwise code using inRenderFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(insertionPosition.parent().flowThreadState());

    element.setRenderer(newRenderer);

    auto& initialStyle = newRenderer->style();
    std::unique_ptr<RenderStyle> animatedStyle;
    newRenderer->animation().updateAnimations(*newRenderer, initialStyle, animatedStyle);
    if (animatedStyle) {
        newRenderer->setStyleInternal(WTFMove(*animatedStyle));
        newRenderer->setHasInitialAnimatedStyle(true);
    }

    newRenderer->initializeStyle();

#if ENABLE(FULLSCREEN_API)
    if (m_document.webkitIsFullScreen() && m_document.webkitCurrentFullScreenElement() == &element) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, &insertionPosition.parent(), m_document);
        if (!newRenderer)
            return;
    }
#endif
    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    insertionPosition.insert(*newRenderer);

    if (AXObjectCache* cache = m_document.axObjectCache())
        cache->updateCacheAfterNodeIsAttached(&element);
}

static bool textRendererIsNeeded(const Text& textNode, const RenderTreePosition& renderTreePosition)
{
    const RenderElement& parentRenderer = renderTreePosition.parent();
    if (!parentRenderer.canHaveChildren())
        return false;
    if (parentRenderer.element() && !parentRenderer.element()->childShouldCreateRenderer(textNode))
        return false;
    if (textNode.isEditingText())
        return true;
    if (!textNode.length())
        return false;
    if (!textNode.containsOnlyWhitespace())
        return true;
    // This text node has nothing but white space. We may still need a renderer in some cases.
    if (parentRenderer.isTable() || parentRenderer.isTableRow() || parentRenderer.isTableSection() || parentRenderer.isRenderTableCol() || parentRenderer.isFrameSet() || (parentRenderer.isFlexibleBox() && !parentRenderer.isRenderButton()))
        return false;
    if (parentRenderer.style().preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;

    RenderObject* previousRenderer = renderTreePosition.previousSiblingRenderer(textNode);
    if (previousRenderer && previousRenderer->isBR()) // <span><br/> <br/></span>
        return false;

    if (parentRenderer.isRenderInline()) {
        // <span><div/> <div/></span>
        if (previousRenderer && !previousRenderer->isInline())
            return false;
    } else {
        if (parentRenderer.isRenderBlock() && !parentRenderer.childrenInline() && (!previousRenderer || !previousRenderer->isInline()))
            return false;
        
        RenderObject* first = parentRenderer.firstChild();
        while (first && first->isFloatingOrOutOfFlowPositioned())
            first = first->nextSibling();
        RenderObject* nextRenderer = renderTreePosition.nextSiblingRenderer(textNode);
        if (!first || nextRenderer == first) {
            // Whitespace at the start of a block just goes away. Don't even make a render object for this text.
            return false;
        }
    }
    return true;
}

static void createTextRenderer(Text& textNode, RenderTreePosition& renderTreePosition)
{
    ASSERT(!textNode.renderer());

    auto newRenderer = textNode.createTextRenderer(renderTreePosition.parent().style());
    ASSERT(newRenderer);

    renderTreePosition.computeNextSibling(textNode);

    if (!renderTreePosition.canInsert(*newRenderer))
        return;

    textNode.setRenderer(newRenderer.get());
    renderTreePosition.insert(*newRenderer.leakPtr());
}

void RenderTreeUpdater::updateTextRenderer(Text& text, const Style::TextUpdate* textUpdate)
{
    auto* existingRenderer = text.renderer();
    bool needsRenderer = textRendererIsNeeded(text, renderTreePosition());
    if (existingRenderer) {
        if (needsRenderer) {
            if (textUpdate)
                existingRenderer->setTextWithOffset(text.data(), textUpdate->offset, textUpdate->length);
            return;
        }
        tearDownRenderer(text);
        invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
        return;
    }
    if (!needsRenderer)
        return;
    createTextRenderer(text, renderTreePosition());
    invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(text);
}

void RenderTreeUpdater::invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(Node& current)
{
    // FIXME: This needs to traverse in composed tree order.

    // This function finds sibling text renderers where the results of textRendererIsNeeded may have changed as a result of
    // the current node gaining or losing the renderer. This can only affect white space text nodes.
    for (Node* sibling = current.nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (is<Element>(*sibling)) {
            if (m_styleUpdate->elementUpdate(downcast<Element>(*sibling)))
                return;
            // Text renderers beyond rendered elements can't be affected.
            if (!sibling->renderer() || RenderTreePosition::isRendererReparented(*sibling->renderer()))
                continue;
            return;
        }
        if (!is<Text>(*sibling))
            continue;
        Text& textSibling = downcast<Text>(*sibling);
        if (m_styleUpdate->textUpdate(textSibling))
            return;
        if (!textSibling.containsOnlyWhitespace())
            continue;
        m_invalidatedWhitespaceOnlyTextSiblings.add(&textSibling);
    }
}

static bool needsPseudoElement(Element& current, PseudoId pseudoId)
{
    if (!current.renderer() || !current.renderer()->canHaveGeneratedChildren())
        return false;
    if (current.isPseudoElement())
        return false;
    if (!pseudoElementRendererIsNeeded(current.renderer()->getCachedPseudoStyle(pseudoId)))
        return false;
    return true;
}

void RenderTreeUpdater::updateBeforeOrAfterPseudoElement(Element& current, PseudoId pseudoId)
{
    PseudoElement* pseudoElement = pseudoId == BEFORE ? current.beforePseudoElement() : current.afterPseudoElement();

    if (auto* renderer = pseudoElement ? pseudoElement->renderer() : nullptr)
        renderTreePosition().invalidateNextSibling(*renderer);

    bool needsPseudoElement = WebCore::needsPseudoElement(current, pseudoId);
    if (!needsPseudoElement) {
        if (pseudoElement) {
            if (pseudoId == BEFORE)
                current.clearBeforePseudoElement();
            else
                current.clearAfterPseudoElement();
        }
        return;
    }

    RefPtr<PseudoElement> newPseudoElement;
    if (!pseudoElement) {
        newPseudoElement = PseudoElement::create(current, pseudoId);
        pseudoElement = newPseudoElement.get();
    }

    auto newStyle = RenderStyle::clonePtr(*current.renderer()->getCachedPseudoStyle(pseudoId, &current.renderer()->style()));

    auto elementUpdate = Style::TreeResolver::createAnimatedElementUpdate(WTFMove(newStyle), *pseudoElement, Style::NoChange);

    if (elementUpdate.change == Style::NoChange)
        return;

    if (newPseudoElement) {
        InspectorInstrumentation::pseudoElementCreated(m_document.page(), *newPseudoElement);
        if (pseudoId == BEFORE)
            current.setBeforePseudoElement(newPseudoElement.releaseNonNull());
        else
            current.setAfterPseudoElement(newPseudoElement.releaseNonNull());
    }

    updateElementRenderer(*pseudoElement, elementUpdate);

    auto* pseudoRenderer = pseudoElement->renderer();
    if (!pseudoRenderer)
        return;

    if (elementUpdate.change == Style::Detach)
        pseudoElement->didAttachRenderers();
    else
        pseudoElement->didRecalcStyle(elementUpdate.change);

    if (m_document.renderView()->hasQuotesNeedingUpdate()) {
        for (auto& child : descendantsOfType<RenderQuote>(*pseudoRenderer))
            updateQuotesUpTo(&child);
    }
    if (is<RenderListItem>(*pseudoRenderer))
        ListItem::updateMarker(downcast<RenderListItem>(*pseudoRenderer));
}

void RenderTreeUpdater::tearDownRenderers(Element& root, TeardownType teardownType)
{
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    Vector<Element*, 30> teardownStack;

    auto push = [&] (Element& element) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willDetachRenderers();
        teardownStack.append(&element);
    };

    auto pop = [&] (unsigned depth) {
        while (teardownStack.size() > depth) {
            auto& element = *teardownStack.takeLast();

            if (teardownType != TeardownType::KeepHoverAndActive)
                element.clearHoverAndActiveStatusBeforeDetachingRenderer();
            element.clearStyleDerivedDataBeforeDetachingRenderer();

            if (auto* renderer = element.renderer()) {
                renderer->destroyAndCleanupAnonymousWrappers();
                element.setRenderer(nullptr);
            }
            if (element.hasCustomStyleResolveCallbacks())
                element.didDetachRenderers();
        }
    };

    push(root);

    auto descendants = composedTreeDescendants(root);
    for (auto it = descendants.begin(), end = descendants.end(); it != end; ++it) {
        pop(it.depth());

        if (is<Text>(*it)) {
            tearDownRenderer(downcast<Text>(*it));
            continue;
        }

        push(downcast<Element>(*it));
    }

    pop(0);
}

void RenderTreeUpdater::tearDownRenderer(Text& text)
{
    auto* renderer = text.renderer();
    if (!renderer)
        return;
    renderer->destroyAndCleanupAnonymousWrappers();
    text.setRenderer(nullptr);
}

void RenderTreeUpdater::updateQuotesUpTo(RenderQuote* lastQuote)
{
    auto quoteRenderers = descendantsOfType<RenderQuote>(*m_document.renderView());
    auto it = m_previousUpdatedQuote ? ++quoteRenderers.at(*m_previousUpdatedQuote) : quoteRenderers.begin();
    auto end = quoteRenderers.end();
    for (; it != end; ++it) {
        auto& quote = *it;
        // Quote character depends on quote depth so we chain the updates.
        quote.updateRenderer(m_previousUpdatedQuote);
        m_previousUpdatedQuote = &quote;
        if (&quote == lastQuote)
            return;
    }
    ASSERT(!lastQuote);
}

#if PLATFORM(IOS)
static EVisibility elementImplicitVisibility(const Element& element)
{
    auto* renderer = element.renderer();
    if (!renderer)
        return VISIBLE;

    auto& style = renderer->style();

    auto width = style.width();
    auto height = style.height();
    if ((width.isFixed() && width.value() <= 0) || (height.isFixed() && height.value() <= 0))
        return HIDDEN;

    auto top = style.top();
    auto left = style.left();
    if (left.isFixed() && width.isFixed() && -left.value() >= width.value())
        return HIDDEN;

    if (top.isFixed() && height.isFixed() && -top.value() >= height.value())
        return HIDDEN;
    return VISIBLE;
}

CheckForVisibilityChange::CheckForVisibilityChange(const Element& element)
    : m_element(element)
    , m_previousDisplay(element.renderStyle() ? element.renderStyle()->display() : NONE)
    , m_previousVisibility(element.renderStyle() ? element.renderStyle()->visibility() : HIDDEN)
    , m_previousImplicitVisibility(WKObservingContentChanges() && WKObservedContentChange() != WKContentVisibilityChange ? elementImplicitVisibility(element) : VISIBLE)
{
}

CheckForVisibilityChange::~CheckForVisibilityChange()
{
    if (!WKObservingContentChanges())
        return;
    if (m_element.isInUserAgentShadowTree())
        return;
    auto* style = m_element.renderStyle();
    if (!style)
        return;
    if ((m_previousDisplay == NONE && style->display() != NONE) || (m_previousVisibility == HIDDEN && style->visibility() != HIDDEN)
        || (m_previousImplicitVisibility == HIDDEN && elementImplicitVisibility(m_element) == VISIBLE))
        WKSetObservedContentChange(WKContentVisibilityChange);
}
#endif

}
