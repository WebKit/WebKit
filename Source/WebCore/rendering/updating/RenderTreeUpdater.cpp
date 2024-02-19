/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "ComposedTreeAncestorIterator.h"
#include "ComposedTreeIterator.h"
#include "Document.h"
#include "Element.h"
#include "HTMLSlotElement.h"
#include "LayoutState.h"
#include "LayoutTreeBuilder.h"
#include "LegacyRenderSVGResource.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "NodeRenderStyle.h"
#include "PseudoElement.h"
#include "RenderDescendantIterator.h"
#include "RenderInline.h"
#include "RenderListItem.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderStyleConstants.h"
#include "RenderStyleInlines.h"
#include "RenderTreeUpdaterGeneratedContent.h"
#include "RenderTreeUpdaterViewTransition.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "StyleResolver.h"
#include "StyleTreeResolver.h"
#include "TextManipulationController.h"
#include <wtf/Scope.h>
#include <wtf/SystemTracing.h>

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#endif

namespace WebCore {

RenderTreeUpdater::Parent::Parent(ContainerNode& root)
    : element(dynamicDowncast<Element>(root))
    , renderTreePosition(RenderTreePosition(*root.renderer()))
{
}

RenderTreeUpdater::Parent::Parent(Element& element, const Style::ElementUpdate* update)
    : element(&element)
    , update(update)
    , renderTreePosition(element.renderer() ? std::make_optional(RenderTreePosition(*element.renderer())) : std::nullopt)
{
}

RenderTreeUpdater::RenderTreeUpdater(Document& document, Style::PostResolutionCallbackDisabler&)
    : m_document(document)
    , m_generatedContent(makeUnique<GeneratedContent>(*this))
    , m_viewTransition(makeUnique<ViewTransition>(*this))
    , m_builder(renderView())
{
}

RenderTreeUpdater::~RenderTreeUpdater() = default;

static Element* findRenderingAncestor(Node& node)
{
    for (auto& ancestor : composedTreeAncestors(node)) {
        if (ancestor.renderer())
            return &ancestor;
        if (!ancestor.hasDisplayContents())
            return nullptr;
    }
    return nullptr;
}

static ContainerNode* findRenderingRoot(ContainerNode& node)
{
    if (node.renderer())
        return &node;
    return findRenderingAncestor(node);
}

void RenderTreeUpdater::commit(std::unique_ptr<Style::Update> styleUpdate)
{
    ASSERT(&m_document == &styleUpdate->document());

    if (!m_document.shouldCreateRenderers() || !m_document.renderView())
        return;

    TraceScope scope(RenderTreeBuildStart, RenderTreeBuildEnd);

    m_styleUpdate = WTFMove(styleUpdate);

    updateRebuildRoots();

    updateRenderViewStyle();

    for (auto& root : m_styleUpdate->roots()) {
        if (&root->document() != &m_document)
            continue;
        auto* renderingRoot = findRenderingRoot(*root);
        if (!renderingRoot)
            continue;
        updateRenderTree(*renderingRoot);
    }

    generatedContent().updateRemainingQuotes();
    generatedContent().updateCounters();

    m_builder.updateAfterDescendants(renderView());

    m_styleUpdate = nullptr;
}

void RenderTreeUpdater::updateRebuildRoots()
{
    auto findNewRebuildRoot = [&](auto& root) -> Element* {
        auto* renderingAncestor = findRenderingAncestor(root);
        if (!renderingAncestor)
            return nullptr;
        if (!RenderTreeBuilder::isRebuildRootForChildren(*renderingAncestor->renderer()))
            return nullptr;
        return renderingAncestor;
    };

    auto addForRebuild = [&](auto& element) {
        auto* existingUpdate = m_styleUpdate->elementUpdate(element);
        if (existingUpdate) {
            if (existingUpdate->change == Style::Change::Renderer)
                return false;
            existingUpdate->change = Style::Change::Renderer;
            return true;
        }

        if (!element.renderer())
            return element.hasDisplayContents();

        auto* parent = composedTreeAncestors(element).first();
        m_styleUpdate->addElement(element, parent, Style::ElementUpdate {
            RenderStyle::clonePtr(element.renderer()->style()),
            Style::Change::Renderer
        });
        return true;
    };

    auto addSubtreeForRebuild = [&](auto& root) {
        if (!addForRebuild(root))
            return;
        auto descendants = composedTreeDescendants(root);
        auto it = descendants.begin();
        auto end = descendants.end();
        while (it != end) {
            auto* descendant = dynamicDowncast<Element>(*it);
            if (!descendant) {
                it.traverseNext();
                continue;
            }
            if (!addForRebuild(*descendant)) {
                it.traverseNextSkippingChildren();
                continue;
            }
            it.traverseNext();
        }
    };

    while (true) {
        auto rebuildRoots = m_styleUpdate->takeRebuildRoots();
        if (rebuildRoots.isEmpty())
            break;
        for (auto& rebuildRoot : rebuildRoots) {
            if (auto* newRebuildRoot = findNewRebuildRoot(*rebuildRoot))
                addSubtreeForRebuild(*newRebuildRoot);
        }
    }
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
        else if (auto* element = dynamicDowncast<Element>(node); element && element->hasDisplayContents())
            renderTreePosition().invalidateNextSibling();

        if (auto* text = dynamicDowncast<Text>(node)) {
            auto* textUpdate = m_styleUpdate->textUpdate(*text);
            bool didCreateParent = parent().update && parent().update->change == Style::Change::Renderer;
            bool mayNeedUpdateWhitespaceOnlyRenderer = renderingParent().didCreateOrDestroyChildRenderer && text->containsOnlyASCIIWhitespace();
            if (didCreateParent || textUpdate || mayNeedUpdateWhitespaceOnlyRenderer)
                updateTextRenderer(*text, textUpdate);

            storePreviousRenderer(*text);
            it.traverseNextSkippingChildren();
            continue;
        }

        auto& element = downcast<Element>(node);

        bool needsSVGRendererUpdate = element.needsSVGRendererUpdate();
        if (needsSVGRendererUpdate)
            updateSVGRenderer(element);

        auto* elementUpdate = m_styleUpdate->elementUpdate(element);

        // We hop through display: contents elements in findRenderingRoot, so
        // there may be other updates down the tree.
        if (!elementUpdate && !element.hasDisplayContents() && !needsSVGRendererUpdate) {
            storePreviousRenderer(element);
            it.traverseNextSkippingChildren();
            continue;
        }

        if (elementUpdate)
            updateElementRenderer(element, *elementUpdate);

        storePreviousRenderer(element);

        auto mayHaveRenderedDescendants = [&]() {
            if (element.renderer())
                return !(element.isInTopLayer() && element.renderer()->isSkippedContent());
            return element.hasDisplayContents() && shouldCreateRenderer(element, renderTreePosition().parent());
        }();

        if (!mayHaveRenderedDescendants) {
            it.traverseNextSkippingChildren();
            continue;
        }

        pushParent(element, elementUpdate);

        it.traverseNext();
    }

    popParentsToDepth(0);
}

auto RenderTreeUpdater::renderingParent() -> Parent&
{
    for (unsigned i = m_parentStack.size(); i--;) {
        if (m_parentStack[i].renderTreePosition)
            return m_parentStack[i];
    }
    ASSERT_NOT_REACHED();
    return m_parentStack.last();
}

RenderTreePosition& RenderTreeUpdater::renderTreePosition()
{
    return *renderingParent().renderTreePosition;
}

void RenderTreeUpdater::pushParent(Element& element, const Style::ElementUpdate* update)
{
    m_parentStack.append(Parent(element, update));

    updateBeforeDescendants(element, update);
}

void RenderTreeUpdater::popParent()
{
    auto& parent = m_parentStack.last();
    if (parent.element)
        updateAfterDescendants(*parent.element, parent.update);

    if (&parent != &renderingParent())
        renderTreePosition().invalidateNextSibling();

    m_parentStack.removeLast();
}

void RenderTreeUpdater::popParentsToDepth(unsigned depth)
{
    ASSERT(m_parentStack.size() >= depth);

    while (m_parentStack.size() > depth)
        popParent();
}

void RenderTreeUpdater::updateBeforeDescendants(Element& element, const Style::ElementUpdate* update)
{
    if (update)
        generatedContent().updatePseudoElement(element, *update, PseudoId::Before);
}

void RenderTreeUpdater::updateAfterDescendants(Element& element, const Style::ElementUpdate* update)
{
    if (update)
        generatedContent().updatePseudoElement(element, *update, PseudoId::After);

    auto* renderer = element.renderer();
    if (!renderer)
        return;

    generatedContent().updateBackdropRenderer(*renderer);
    if (&element == element.document().documentElement())
        viewTransition().updatePseudoElementTree(*renderer);

    m_builder.updateAfterDescendants(*renderer);

    if (element.hasCustomStyleResolveCallbacks() && update && update->change == Style::Change::Renderer)
        element.didAttachRenderers();
}

static bool pseudoStyleCacheIsInvalid(RenderElement* renderer, RenderStyle* newStyle)
{
    const RenderStyle& currentStyle = renderer->style();

    const PseudoStyleCache* pseudoStyleCache = currentStyle.cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    for (auto& cache : pseudoStyleCache->styles) {
        PseudoId pseudoId = cache->pseudoElementType();
        std::unique_ptr<RenderStyle> newPseudoStyle = renderer->getUncachedPseudoStyle({ pseudoId }, newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *cache) {
            newStyle->addCachedPseudoStyle(WTFMove(newPseudoStyle));
            return true;
        }
    }
    return false;
}

void RenderTreeUpdater::updateRendererStyle(RenderElement& renderer, RenderStyle&& newStyle, StyleDifference minimalStyleDifference)
{
    auto oldStyle = RenderStyle::clone(renderer.style());
    renderer.setStyle(WTFMove(newStyle), minimalStyleDifference);
    m_builder.normalizeTreeAfterStyleChange(renderer, oldStyle);
}

void RenderTreeUpdater::updateSVGRenderer(Element& element)
{
    ASSERT(element.needsSVGRendererUpdate());
    element.setNeedsSVGRendererUpdate(false);

    auto* renderer = element.renderer();
    if (!renderer)
        return;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (element.document().settings().layerBasedSVGEngineEnabled()) {
        renderer->setNeedsLayout();
        return;
    }
#endif

    LegacyRenderSVGResource::markForLayoutAndParentResourceInvalidation(*renderer);
}

void RenderTreeUpdater::updateElementRenderer(Element& element, const Style::ElementUpdate& elementUpdate)
{
    if (!elementUpdate.style)
        return;

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    ContentChangeObserver::StyleChangeScope observingScope(m_document, element);
#endif

    auto elementUpdateStyle = RenderStyle::cloneIncludingPseudoElements(*elementUpdate.style);

    bool shouldTearDownRenderers = [&]() {
        if (element.isInTopLayer() && elementUpdate.change == Style::Change::Inherited && elementUpdate.style->hasSkippedContent())
            return true;
        return elementUpdate.change == Style::Change::Renderer && (element.renderer() || element.hasDisplayContents());
    }();

    if (shouldTearDownRenderers) {
        if (!element.renderer()) {
            // We may be tearing down a descendant renderer cached in renderTreePosition.
            renderTreePosition().invalidateNextSibling();
        }

        // display:none cancels animations.
        auto teardownType = elementUpdate.style->display() == DisplayType::None ? TeardownType::RendererUpdateCancelingAnimations : TeardownType::RendererUpdate;
        tearDownRenderers(element, teardownType, m_builder);

        renderingParent().didCreateOrDestroyChildRenderer = true;
    }

    bool hasDisplayContents = elementUpdate.style->display() == DisplayType::Contents;
    bool hasDisplayNonePreventingRendererCreation = elementUpdate.style->display() == DisplayType::None && !element.rendererIsNeeded(elementUpdateStyle) && !shouldCreateRenderer(element, renderTreePosition().parent());
    bool hasDisplayContentsOrNone = hasDisplayContents || hasDisplayNonePreventingRendererCreation;
    if (hasDisplayContentsOrNone)
        element.storeDisplayContentsOrNoneStyle(makeUnique<RenderStyle>(WTFMove(elementUpdateStyle)));
    else
        element.clearDisplayContentsOrNoneStyle();

    if (!hasDisplayContentsOrNone) {
        if (!elementUpdateStyle.containIntrinsicLogicalWidthHasAuto())
            element.clearLastRememberedLogicalWidth();
        if (!elementUpdateStyle.containIntrinsicLogicalHeightHasAuto())
            element.clearLastRememberedLogicalHeight();
    }

    auto scopeExit = makeScopeExit([&] {
        if (!hasDisplayContentsOrNone) {
            auto* box = element.renderBox();
            if (box && box->style().hasAutoLengthContainIntrinsicSize() && !box->isSkippedContentRoot())
                m_document.observeForContainIntrinsicSize(element);
            else
                m_document.unobserveForContainIntrinsicSize(element);
        }
    });

    bool shouldCreateNewRenderer = !element.renderer() && !hasDisplayContents && !(element.isInTopLayer() && renderTreePosition().parent().style().hasSkippedContent());
    if (shouldCreateNewRenderer) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willAttachRenderers();
        createRenderer(element, WTFMove(elementUpdateStyle));

        renderingParent().didCreateOrDestroyChildRenderer = true;
        return;
    }

    if (!element.renderer())
        return;
    auto& renderer = *element.renderer();

    if (elementUpdate.recompositeLayer) {
        updateRendererStyle(renderer, WTFMove(elementUpdateStyle), StyleDifference::RecompositeLayer);
        return;
    }

    if (elementUpdate.change == Style::Change::None) {
        if (pseudoStyleCacheIsInvalid(&renderer, &elementUpdateStyle)) {
            updateRendererStyle(renderer, WTFMove(elementUpdateStyle), StyleDifference::Equal);
            return;
        }
        return;
    }

    updateRendererStyle(renderer, WTFMove(elementUpdateStyle), StyleDifference::Equal);
}

void RenderTreeUpdater::createRenderer(Element& element, RenderStyle&& style)
{
    auto computeInsertionPosition = [this, &element] () {
        renderTreePosition().computeNextSibling(element);
        return renderTreePosition();
    };

    if (!shouldCreateRenderer(element, renderTreePosition().parent()))
        return;

    if (!element.rendererIsNeeded(style))
        return;

    RenderTreePosition insertionPosition = computeInsertionPosition();
    auto newRenderer = element.createElementRenderer(WTFMove(style), insertionPosition);
    if (!newRenderer)
        return;

    if (!insertionPosition.parent().isChildAllowed(*newRenderer, newRenderer->style()))
        return;

    element.setRenderer(newRenderer.get());

    newRenderer->initializeStyle();

    m_builder.attach(insertionPosition.parent(), WTFMove(newRenderer), insertionPosition.nextSibling());

    auto* textManipulationController = m_document.textManipulationControllerIfExists();
    if (UNLIKELY(textManipulationController))
        textManipulationController->didAddOrCreateRendererForNode(element);

    if (auto* cache = m_document.axObjectCache())
        cache->onRendererCreated(element);
}

bool RenderTreeUpdater::textRendererIsNeeded(const Text& textNode)
{
    auto& renderingParent = this->renderingParent();
    auto& parentRenderer = renderingParent.renderTreePosition->parent();
    if (!parentRenderer.canHaveChildren())
        return false;
    if (parentRenderer.element() && !parentRenderer.element()->childShouldCreateRenderer(textNode))
        return false;
    if (textNode.isEditingText())
        return true;
    if (!textNode.length())
        return false;
    if (!textNode.containsOnlyASCIIWhitespace())
        return true;
    if (is<RenderText>(renderingParent.previousChildRenderer))
        return true;
    // This text node has nothing but white space. We may still need a renderer in some cases.
    if (parentRenderer.isRenderTable() || parentRenderer.isRenderTableRow() || parentRenderer.isRenderTableSection() || parentRenderer.isRenderTableCol() || parentRenderer.isRenderFrameSet() || parentRenderer.isRenderGrid() || (parentRenderer.isRenderFlexibleBox() && !parentRenderer.isRenderButton()))
        return false;
    if (parentRenderer.style().preserveNewline()) // pre/pre-wrap/pre-line always make renderers.
        return true;

    auto* previousRenderer = renderingParent.previousChildRenderer;
    if (previousRenderer && previousRenderer->isBR()) // <span><br/> <br/></span>
        return false;

    if (parentRenderer.isRenderInline()) {
        // <span><div/> <div/></span>
        if (previousRenderer && !previousRenderer->isInline() && !previousRenderer->isOutOfFlowPositioned())
            return false;
    } else {
        if (parentRenderer.isRenderBlock() && !parentRenderer.childrenInline() && (!previousRenderer || !previousRenderer->isInline()))
            return false;

        RenderObject* first = parentRenderer.firstChild();
        while (first && first->isFloatingOrOutOfFlowPositioned())
            first = first->nextSibling();
        RenderObject* nextRenderer = textNode.renderer() ? textNode.renderer() :  renderTreePosition().nextSiblingRenderer(textNode);
        if (!first || nextRenderer == first) {
            // Whitespace at the start of a block just goes away. Don't even make a render object for this text.
            return false;
        }
    }
    return true;
}

void RenderTreeUpdater::createTextRenderer(Text& textNode, const Style::TextUpdate* textUpdate)
{
    ASSERT(!textNode.renderer());

    auto& renderTreePosition = this->renderTreePosition();
    auto textRenderer = textNode.createTextRenderer(renderTreePosition.parent().style());

    renderTreePosition.computeNextSibling(textNode);

    if (!renderTreePosition.parent().isChildAllowed(*textRenderer, renderTreePosition.parent().style()))
        return;

    textNode.setRenderer(textRenderer.get());

    if (textUpdate && textUpdate->inheritedDisplayContentsStyle && *textUpdate->inheritedDisplayContentsStyle) {
        // Wrap text renderer into anonymous inline so we can give it a style.
        // This is to support "<div style='display:contents;color:green'>text</div>" type cases
        auto newDisplayContentsAnonymousWrapper = WebCore::createRenderer<RenderInline>(RenderObject::Type::Inline, textNode.document(), RenderStyle::clone(**textUpdate->inheritedDisplayContentsStyle));
        newDisplayContentsAnonymousWrapper->initializeStyle();
        auto& displayContentsAnonymousWrapper = *newDisplayContentsAnonymousWrapper;
        m_builder.attach(renderTreePosition.parent(), WTFMove(newDisplayContentsAnonymousWrapper), renderTreePosition.nextSibling());

        textRenderer->setInlineWrapperForDisplayContents(&displayContentsAnonymousWrapper);
        m_builder.attach(displayContentsAnonymousWrapper, WTFMove(textRenderer));
        return;
    }

    m_builder.attach(renderTreePosition.parent(), WTFMove(textRenderer), renderTreePosition.nextSibling());

    auto* textManipulationController = m_document.textManipulationControllerIfExists();
    if (UNLIKELY(textManipulationController))
        textManipulationController->didAddOrCreateRendererForNode(textNode);
}

void RenderTreeUpdater::updateTextRenderer(Text& text, const Style::TextUpdate* textUpdate)
{
    auto* existingRenderer = text.renderer();
    bool needsRenderer = textRendererIsNeeded(text);

    if (existingRenderer && textUpdate && textUpdate->inheritedDisplayContentsStyle) {
        if (existingRenderer->inlineWrapperForDisplayContents() || *textUpdate->inheritedDisplayContentsStyle) {
            // FIXME: We could update without teardown.
            tearDownTextRenderer(text, m_builder);
            existingRenderer = nullptr;
        }
    }

    if (existingRenderer) {
        if (needsRenderer) {
            if (textUpdate)
                existingRenderer->setTextWithOffset(text.data(), textUpdate->offset, textUpdate->length);
            return;
        }
        tearDownTextRenderer(text, m_builder);
        renderingParent().didCreateOrDestroyChildRenderer = true;
        return;
    }
    if (!needsRenderer)
        return;
    createTextRenderer(text, textUpdate);
    renderingParent().didCreateOrDestroyChildRenderer = true;
}

void RenderTreeUpdater::storePreviousRenderer(Node& node)
{
    auto* renderer = node.renderer();
    if (!renderer)
        return;
    ASSERT(renderingParent().previousChildRenderer != renderer);
    renderingParent().previousChildRenderer = renderer;
}

void RenderTreeUpdater::updateRenderViewStyle()
{
    if (m_styleUpdate->initialContainingBlockUpdate())
        m_document.renderView()->setStyle(RenderStyle::clone(*m_styleUpdate->initialContainingBlockUpdate()));
}

static void invalidateRebuildRootIfNeeded(Node& node)
{
    auto* ancestor = findRenderingAncestor(node);
    if (!ancestor)
        return;
    if (!RenderTreeBuilder::isRebuildRootForChildren(*ancestor->renderer()))
        return;
    ancestor->invalidateRenderer();
}

void RenderTreeUpdater::tearDownRenderers(Element& root)
{
    if (!root.renderer() && !root.hasDisplayContents())
        return;
    auto* view = root.document().renderView();
    if (!view)
        return;

    RenderTreeBuilder builder(*view);
    tearDownRenderers(root, TeardownType::Full, builder);
    invalidateRebuildRootIfNeeded(root);
}

void RenderTreeUpdater::tearDownRenderersAfterSlotChange(Element& host)
{
    ASSERT(host.shadowRoot());
    if (!host.renderer() && !host.hasDisplayContents())
        return;
    auto* view = host.document().renderView();
    if (!view)
        return;

    RenderTreeBuilder builder(*view);
    tearDownRenderers(host, TeardownType::FullAfterSlotChange, builder);
    invalidateRebuildRootIfNeeded(host);
}

void RenderTreeUpdater::tearDownRenderer(Text& text)
{
    auto* view = text.document().renderView();
    if (!view)
        return;

    RenderTreeBuilder builder(*view);
    tearDownTextRenderer(text, builder);
    invalidateRebuildRootIfNeeded(text);
}

void RenderTreeUpdater::tearDownRenderers(Element& root, TeardownType teardownType, RenderTreeBuilder& builder)
{
    Vector<Element*, 30> teardownStack;

    auto push = [&] (Element& element) {
        if (element.hasCustomStyleResolveCallbacks())
            element.willDetachRenderers();
        teardownStack.append(&element);
    };

    auto pop = [&] (unsigned depth) {
        while (teardownStack.size() > depth) {
            auto& element = *teardownStack.takeLast();
            auto styleable = Styleable::fromElement(element);

            // Make sure we don't leave any renderers behind in nodes outside the composed tree.
            // See ComposedTreeIterator::ComposedTreeIterator().
            if (is<HTMLSlotElement>(element) || element.shadowRoot())
                tearDownLeftoverChildrenOfComposedTree(element, builder);

            switch (teardownType) {
            case TeardownType::FullAfterSlotChange:
                if (&element == &root) {
                    // Keep animations going on the host.
                    styleable.willChangeRenderer();
                    break;
                }
                element.clearHoverAndActiveStatusBeforeDetachingRenderer();
                break;
            case TeardownType::Full:
                styleable.cancelStyleOriginatedAnimations();
                element.clearHoverAndActiveStatusBeforeDetachingRenderer();
                break;
            case TeardownType::RendererUpdateCancelingAnimations:
                styleable.cancelStyleOriginatedAnimations();
                break;
            case TeardownType::RendererUpdate:
                styleable.willChangeRenderer();
                break;
            }

            GeneratedContent::removeBeforePseudoElement(element, builder);
            GeneratedContent::removeAfterPseudoElement(element, builder);

            if (!is<PseudoElement>(element)) {
                // ::before and ::after cannot have a ::marker pseudo-element addressable via
                // CSS selectors, and as such cannot possibly have animations on them. Additionally,
                // we cannot create a Styleable with a PseudoElement.
                if (auto* renderListItem = dynamicDowncast<RenderListItem>(element.renderer())) {
                    if (renderListItem->markerRenderer())
                        Styleable(element, PseudoId::Marker).cancelStyleOriginatedAnimations();
                }
            }

            if (auto* renderer = element.renderer()) {
                builder.destroyAndCleanUpAnonymousWrappers(*renderer);
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

        if (auto* text = dynamicDowncast<Text>(*it)) {
            tearDownTextRenderer(*text, builder);
            continue;
        }

        push(downcast<Element>(*it));
    }

    pop(0);

    tearDownLeftoverPaginationRenderersIfNeeded(root, builder);
}

void RenderTreeUpdater::tearDownTextRenderer(Text& text, RenderTreeBuilder& builder)
{
    auto* renderer = text.renderer();
    if (!renderer)
        return;
    builder.destroyAndCleanUpAnonymousWrappers(*renderer);
    text.setRenderer(nullptr);
}

void RenderTreeUpdater::tearDownLeftoverPaginationRenderersIfNeeded(Element& root, RenderTreeBuilder& builder)
{
    if (&root != root.document().documentElement())
        return;
    for (auto* child = root.document().renderView()->firstChild(); child;) {
        auto* nextSibling = child->nextSibling();
        if (is<RenderMultiColumnFlow>(*child) || is<RenderMultiColumnSet>(*child))
            builder.destroyAndCleanUpAnonymousWrappers(*child);
        child = nextSibling;
    }
}

void RenderTreeUpdater::tearDownLeftoverChildrenOfComposedTree(Element& element, RenderTreeBuilder& builder)
{
    for (auto* child = element.firstChild(); child; child = child->nextSibling()) {
        if (!child->renderer())
            continue;
        if (auto* text = dynamicDowncast<Text>(*child)) {
            tearDownTextRenderer(*text, builder);
            continue;
        }
        if (auto* element = dynamicDowncast<Element>(*child))
            tearDownRenderers(*element, TeardownType::Full, builder);
    }
}

RenderView& RenderTreeUpdater::renderView()
{
    return *m_document.renderView();
}

}
