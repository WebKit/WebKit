/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RenderTreeUpdaterViewTransition.h"

#include "ContainerNodeInlines.h"
#include "ContextDestructionObserverInlines.h"
#include "ElementRuleCollector.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderViewTransitionCapture.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleTreeResolver.h"
#include "ViewTransition.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeUpdater::ViewTransition);

RenderTreeUpdater::ViewTransition::ViewTransition(RenderTreeUpdater& updater)
    : m_updater(updater)
{
}

// The contents and ordering of the named elements map should remain stable during the duration of the transition.
// We should only need to handle changes in the `display` CSS property by recreating / deleting renderers as needed.
void RenderTreeUpdater::ViewTransition::updatePseudoElementTree(Element& host, RenderElement* hostRenderer, Style::DifferenceResult minimalStyleDifference)
{
    Ref document = host.document();
    bool isDocumentHost = &host == document->documentElement();

    auto destroyPseudoElementTreeIfNeeded = [&]() {
        SingleThreadWeakPtr<RenderBlockFlow> viewTransitionContainingBlock;
        if (isDocumentHost)
            viewTransitionContainingBlock = m_updater.renderView().viewTransitionContainingBlock();
        else if (hostRenderer) {
            for (auto& child : childrenOfType<RenderBlockFlow>(*hostRenderer)) {
                if (child.isViewTransitionContainingBlock()) {
                    viewTransitionContainingBlock = child;
                    break;
                }
            }
        }
        if (viewTransitionContainingBlock) {
            // The renderer is destroyed by this call, so it cannot be protected.
            SUPPRESS_UNCHECKED_ARG m_updater.destroyAndCancelAnimationsForSubtree(*viewTransitionContainingBlock);
        }
        if (!isDocumentHost)
            document->unregisterViewTransitionPseudoTreeHost(host);
    };

    RefPtr activeViewTransition = document->viewTransitionHostedOn(host);

    if (!hostRenderer || !activeViewTransition) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    // Destroy pseudo element tree ::view-transition has display: none or no style.
    CheckedPtr rootStyle = hostRenderer->style().pseudoElementStyle({ PseudoElementType::ViewTransition });
    if (!rootStyle || rootStyle->display() == Style::DisplayType::None) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    auto newRootStyle = Style::ComputedStyle::clone(*rootStyle);

    SingleThreadWeakPtr<RenderBlockFlow> viewTransitionContainingBlock;
    if (isDocumentHost)
        viewTransitionContainingBlock = hostRenderer->view().viewTransitionContainingBlock();
    else
        viewTransitionContainingBlock = activeViewTransition->scopedViewTransitionContainingBlock();
    if (!viewTransitionContainingBlock) {
        auto containingBlockStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(protect(isDocumentHost ? hostRenderer->view().style() : hostRenderer->style()), Style::DisplayType::BlockFlow);
        containingBlockStyle->setPointerEvents(PointerEvents::None);

        if (isDocumentHost) {
            containingBlockStyle->setPosition(PositionType::Fixed);
            auto containingBlockRect = activeViewTransition->containingBlockRect();
            containingBlockStyle->setLeft(Style::InsetEdge::Fixed { containingBlockRect.x() });
            containingBlockStyle->setTop(Style::InsetEdge::Fixed { containingBlockRect.y() });
            containingBlockStyle->setWidth(Style::PreferredSize::Fixed { containingBlockRect.width() });
            containingBlockStyle->setHeight(Style::PreferredSize::Fixed { containingBlockRect.height() });
        } else {
            // The scoped pseudo element tree covers the scope's padding box; the scope
            // is its containing block through the layout containment UA rule.
            containingBlockStyle->setPosition(PositionType::Absolute);
            containingBlockStyle->setLeft(Style::InsetEdge::Fixed { 0 });
            containingBlockStyle->setTop(Style::InsetEdge::Fixed { 0 });
            containingBlockStyle->setRight(Style::InsetEdge::Fixed { 0 });
            containingBlockStyle->setBottom(Style::InsetEdge::Fixed { 0 });
        }

        auto newViewTransitionContainingBlock = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(containingBlockStyle), RenderObject::BlockFlowFlag::IsViewTransitionContainingBlock);
        newViewTransitionContainingBlock->initializeStyle();
        if (isDocumentHost)
            hostRenderer->view().setViewTransitionContainingBlock(*newViewTransitionContainingBlock.get());
        else
            activeViewTransition->setScopedViewTransitionContainingBlock(*newViewTransitionContainingBlock.get());
        viewTransitionContainingBlock = newViewTransitionContainingBlock.get();
        if (isDocumentHost)
            m_updater.m_builder.attach(*protect(hostRenderer->parent()), WTF::move(newViewTransitionContainingBlock));
        else
            m_updater.m_builder.attach(*hostRenderer, WTF::move(newViewTransitionContainingBlock));
    }

    // Create ::view-transition as needed.
    WeakPtr viewTransitionRoot = dynamicDowncast<RenderBlockFlow>(viewTransitionContainingBlock->firstChildBox());
    if (viewTransitionRoot)
        protect(viewTransitionRoot)->setStyle(WTF::move(newRootStyle), minimalStyleDifference);
    else {
        auto newViewTransitionRoot = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(newRootStyle));
        newViewTransitionRoot->initializeStyle();
        viewTransitionRoot = newViewTransitionRoot.get();
        m_updater.m_builder.attach(*protect(viewTransitionContainingBlock), WTF::move(newViewTransitionRoot));
    }

    // No groups. The map is constant during the duration of the transition, so we don't need to handle deletions.
    if (activeViewTransition->namedElements().isEmpty())
        return;

    // Traverse named elements map to update/build all ::view-transition-group().
    HashMap<AtomString, unsigned> nameOrder;
    for (auto& name : activeViewTransition->namedElements().keys())
        nameOrder.add(name, nameOrder.size());

    Vector<SingleThreadWeakPtr<RenderElement>> descendantsToDelete;
    for (auto& name : activeViewTransition->namedElements().keys()) {
        SingleThreadWeakPtr<RenderBox> existingGroup = isDocumentHost ? hostRenderer->view().viewTransitionGroupForName(name) : activeViewTransition->scopedViewTransitionGroupForName(name);
        if (existingGroup) {
            CheckedPtr style = hostRenderer->style().pseudoElementStyle({ PseudoElementType::ViewTransitionGroup, name });
            if (!style || style->display() == Style::DisplayType::None) {
                if (isDocumentHost)
                    protect(hostRenderer->view())->removeViewTransitionGroup(name);
                else
                    activeViewTransition->removeScopedViewTransitionGroup(name);
                descendantsToDelete.append(existingGroup.get());
            } else
                updatePseudoElementGroup(*activeViewTransition, *style, protect(*existingGroup), *hostRenderer, minimalStyleDifference);
        } else if (CheckedPtr parent = parentForPseudoElementGroup(*activeViewTransition, name, *hostRenderer, protect(*viewTransitionRoot), isDocumentHost)) {
            // Rebuilt groups keep the paint order of the named elements map.
            RenderObject* beforeChild = nullptr;
            for (auto& sibling : childrenOfType<RenderBox>(*parent)) {
                if (sibling.style().pseudoElementType() != PseudoElementType::ViewTransitionGroup)
                    continue;
                auto it = nameOrder.find(sibling.style().pseudoElementNameArgument());
                if (it != nameOrder.end() && it->value > nameOrder.get(name)) {
                    beforeChild = &sibling;
                    break;
                }
            }
            buildPseudoElementGroup(*activeViewTransition, *parent, name, *hostRenderer, beforeChild);
        }
    }

    // A pseudo-element hidden mid-transition (e.g. by an animation stepping through
    // display: none) keeps its animations, which live on the host styleable; only the
    // full tree teardown cancels them.
    for (auto& descendant : descendantsToDelete) {
        if (descendant)
            // The renderer is destroyed by this call, so it cannot be protected.
            SUPPRESS_UNCHECKED_ARG m_updater.m_builder.destroy(*descendant);
    }
}

// ::view-transition-group-children() mirrors its element's border widths so
// nested group geometry stays relative to the padding box, and inherits a
// non-visible overflow as a padding box clip.
static void adjustGroupChildrenStyle(WebCore::ViewTransition& activeViewTransition, const AtomString& name, Style::ComputedStyle& style)
{
    const auto* capturedElement = activeViewTransition.namedElements().find(name);
    if (!capturedElement)
        return;
    auto styleable = capturedElement->newElement.styleable();
    if (!styleable)
        return;
    CheckedPtr renderer = dynamicDowncast<RenderBoxModelObject>(styleable->element->renderer());
    if (!renderer)
        return;
    style.setBorderLeftWidth(Style::LineWidth { Style::LineWidth::Length { renderer->borderLeft().toFloat() } });
    style.setBorderRightWidth(Style::LineWidth { Style::LineWidth::Length { renderer->borderRight().toFloat() } });
    style.setBorderTopWidth(Style::LineWidth { Style::LineWidth::Length { renderer->borderTop().toFloat() } });
    style.setBorderBottomWidth(Style::LineWidth { Style::LineWidth::Length { renderer->borderBottom().toFloat() } });
    CheckedRef elementStyle = renderer->style();
    if (elementStyle->overflowX() == Overflow::Visible && elementStyle->overflowY() == Overflow::Visible)
        return;
    style.setOverflowX(Overflow::Clip);
    style.setOverflowY(Overflow::Clip);
}

static RenderPtr<RenderBox> createRendererIfNeeded(WebCore::ViewTransition& activeViewTransition, RenderElement& hostRenderer, const AtomString& name, PseudoElementType pseudoElementType)
{
    CheckedRef hostStyle = hostRenderer.style();
    CheckedPtr style = hostStyle->pseudoElementStyle({ pseudoElementType, name });
    if (!style || style->display() == Style::DisplayType::None)
        return nullptr;

    Ref document = hostRenderer.document();
    RenderPtr<RenderBox> renderer;
    if (pseudoElementType == PseudoElementType::ViewTransitionOld || pseudoElementType == PseudoElementType::ViewTransitionNew) {
        const auto* capturedElement = activeViewTransition.namedElements().find(name);
        ASSERT(capturedElement);
        if (pseudoElementType == PseudoElementType::ViewTransitionOld && !capturedElement->oldImage)
            return nullptr;
        if (pseudoElementType == PseudoElementType::ViewTransitionNew && !capturedElement->newElement)
            return nullptr;

        auto& state = pseudoElementType == PseudoElementType::ViewTransitionOld ? capturedElement->oldState : capturedElement->newState;

        auto newStyle = Style::ComputedStyle::clone(*style);
        RenderPtr<RenderViewTransitionCapture> rendererViewTransition = WebCore::createRenderer<RenderViewTransitionCapture>(RenderObject::Type::ViewTransitionCapture, document, WTF::move(newStyle), state.isRootElement);
        rendererViewTransition->setCapturedSize(state.size, state.overflowRect, state.layerToLayoutOffset);
        if (pseudoElementType == PseudoElementType::ViewTransitionOld) {
            rendererViewTransition->setImage(capturedElement->oldImage.value_or(nullptr));
            rendererViewTransition->setDisplayOnlyImage(RefPtr { capturedElement->oldDisplayOnlyImage });
        }
        renderer = WTF::move(rendererViewTransition);
    } else {
        auto newStyle = Style::ComputedStyle::clone(*style);
        if (pseudoElementType == PseudoElementType::ViewTransitionGroupChildren)
            adjustGroupChildrenStyle(activeViewTransition, name, newStyle.get());
        renderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(newStyle));
    }

    renderer->initializeStyle();
    return renderer;
}


// A group whose captured element resolved a containing group nests inside that
// group's ::view-transition-group-children(), created on demand.
RenderElement* RenderTreeUpdater::ViewTransition::parentForPseudoElementGroup(WebCore::ViewTransition& activeViewTransition, const AtomString& name, RenderElement& hostRenderer, RenderBlockFlow& viewTransitionRoot, bool isDocumentHost)
{
    const auto* capturedElement = activeViewTransition.namedElements().find(name);
    if (!capturedElement || capturedElement->containingGroupName.isNull())
        return &viewTransitionRoot;

    auto& containingGroupName = capturedElement->containingGroupName;
    CheckedPtr containingGroup = isDocumentHost ? hostRenderer.view().viewTransitionGroupForName(containingGroupName) : activeViewTransition.scopedViewTransitionGroupForName(containingGroupName);
    if (!containingGroup)
        return &viewTransitionRoot;

    for (auto& child : childrenOfType<RenderBox>(*containingGroup)) {
        if (child.style().pseudoElementType() == PseudoElementType::ViewTransitionGroupChildren)
            return &child;
    }

    auto groupChildren = createRendererIfNeeded(activeViewTransition, hostRenderer, containingGroupName, PseudoElementType::ViewTransitionGroupChildren);
    if (!groupChildren)
        return &viewTransitionRoot;
    SingleThreadWeakPtr<RenderElement> result = groupChildren.get();
    m_updater.m_builder.attach(*containingGroup, WTF::move(groupChildren));
    return result.get();
}

void RenderTreeUpdater::ViewTransition::buildPseudoElementGroup(WebCore::ViewTransition& activeViewTransition, RenderElement& parent, const AtomString& name, RenderElement& hostRenderer, RenderObject* beforeChild)
{
    auto viewTransitionGroup = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionGroup);
    auto viewTransitionImagePair = viewTransitionGroup ? createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionImagePair) : nullptr;
    auto viewTransitionOld = viewTransitionImagePair ? createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionOld) : nullptr;
    auto viewTransitionNew = viewTransitionImagePair ? createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionNew) : nullptr;

    if (viewTransitionOld)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTF::move(viewTransitionOld));

    if (viewTransitionNew)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTF::move(viewTransitionNew));

    if (viewTransitionImagePair)
        m_updater.m_builder.attach(*viewTransitionGroup, WTF::move(viewTransitionImagePair));

    if (viewTransitionGroup) {
        if (activeViewTransition.scopeElement())
            activeViewTransition.addScopedViewTransitionGroup(name, *viewTransitionGroup.get());
        else
            protect(hostRenderer.view())->addViewTransitionGroup(name, *viewTransitionGroup.get());
        m_updater.m_builder.attach(parent, WTF::move(viewTransitionGroup), beforeChild);
    }
}

void RenderTreeUpdater::ViewTransition::updatePseudoElementGroup(WebCore::ViewTransition& activeViewTransition, const Style::ComputedStyle& groupStyle, RenderBox& group, RenderElement& hostRenderer, Style::DifferenceResult minimalStyleDifference)
{
    CheckedRef hostStyle = hostRenderer.style();
    auto name = groupStyle.pseudoElementNameArgument();

    auto newGroupStyle = Style::ComputedStyle::clone(groupStyle);
    group.setStyle(WTF::move(newGroupStyle), minimalStyleDifference);

    enum class ShouldDeleteRenderer : bool { No, Yes };
    auto updateRenderer = [&](RenderBox& renderer) -> ShouldDeleteRenderer {
        CheckedPtr style = hostStyle->pseudoElementStyle({ *renderer.style().pseudoElementType(), name });
        if (!style || style->display() == Style::DisplayType::None)
            return ShouldDeleteRenderer::Yes;

        auto newStyle = Style::ComputedStyle::clone(*style);
        renderer.setStyle(WTF::move(newStyle), minimalStyleDifference);
        return ShouldDeleteRenderer::No;
    };

    for (CheckedRef child : childrenOfType<RenderBox>(group)) {
        if (child->style().pseudoElementType() == PseudoElementType::ViewTransitionGroupChildren) {
            if (CheckedPtr style = hostStyle->pseudoElementStyle({ PseudoElementType::ViewTransitionGroupChildren, name })) {
                auto newStyle = Style::ComputedStyle::clone(*style);
                adjustGroupChildrenStyle(activeViewTransition, name, newStyle.get());
                child->setStyle(WTF::move(newStyle), minimalStyleDifference);
            }
            break;
        }
    }

    // Create / remove ::view-transtion-image-pair itself.
    SingleThreadWeakPtr<RenderBox> imagePair;
    for (auto& child : childrenOfType<RenderBox>(group)) {
        if (child.style().pseudoElementType() == PseudoElementType::ViewTransitionImagePair) {
            imagePair = child;
            break;
        }
    }
    if (imagePair) {
        ASSERT(imagePair->style().pseudoElementType() == PseudoElementType::ViewTransitionImagePair);
        auto shouldDeleteRenderer = updateRenderer(protect(*imagePair));
        if (shouldDeleteRenderer == ShouldDeleteRenderer::Yes) {
            // The renderer is destroyed by this call, so it cannot be protected.
            SUPPRESS_UNCHECKED_ARG m_updater.m_builder.destroy(*imagePair);
            return;
        }
    } else if (auto newImagePair = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionImagePair)) {
        imagePair = newImagePair.get();
        CheckedPtr groupFirstChild = group.firstChildBox();
        m_updater.m_builder.attach(group, WTF::move(newImagePair), groupFirstChild.get());
    } else
        return;

    SingleThreadWeakPtr<RenderBox> imagePairFirstChild = imagePair->firstChildBox();
    // Build the ::view-transition-image-pair children if needed.
    if (!imagePairFirstChild) {
        if (auto viewTransitionOld = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionOld))
            m_updater.m_builder.attach(*protect(imagePair), WTF::move(viewTransitionOld));
        if (auto viewTransitionNew = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionNew))
            m_updater.m_builder.attach(*protect(imagePair), WTF::move(viewTransitionNew));
        return;
    }

    // Update pre-existing ::view-transition-image-pair children.
    auto shouldDeleteViewTransitionOld = ShouldDeleteRenderer::No;

    SingleThreadWeakPtr<RenderBox> viewTransitionOld;
    SingleThreadWeakPtr<RenderBox> viewTransitionNew;

    RenderPtr<RenderBox> newViewTransitionOld;
    RenderPtr<RenderBox> newViewTransitionNew;
    if (imagePairFirstChild->style().pseudoElementType() == PseudoElementType::ViewTransitionOld) {
        viewTransitionOld = imagePairFirstChild;
        shouldDeleteViewTransitionOld = updateRenderer(protect(*viewTransitionOld));
        viewTransitionNew = viewTransitionOld->nextSiblingBox();
        ASSERT(!viewTransitionNew || viewTransitionNew->style().pseudoElementType() == PseudoElementType::ViewTransitionNew);
    } else {
        ASSERT(imagePairFirstChild->style().pseudoElementType() == PseudoElementType::ViewTransitionNew);
        viewTransitionNew = imagePairFirstChild;
        newViewTransitionOld = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionOld);
    }

    auto shouldDeleteViewTransitionNew = ShouldDeleteRenderer::No;
    if (!viewTransitionNew)
        newViewTransitionNew = createRendererIfNeeded(activeViewTransition, hostRenderer, name, PseudoElementType::ViewTransitionNew);
    else
        shouldDeleteViewTransitionNew = updateRenderer(protect(*viewTransitionNew));

    if (shouldDeleteViewTransitionNew == ShouldDeleteRenderer::Yes)
        // The renderer is destroyed by this call, so it cannot be protected.
        SUPPRESS_UNCHECKED_ARG m_updater.m_builder.destroy(*viewTransitionNew);
    else if (newViewTransitionNew)
        m_updater.m_builder.attach(*protect(imagePair), WTF::move(newViewTransitionNew));

    if (shouldDeleteViewTransitionOld == ShouldDeleteRenderer::Yes)
        // The renderer is destroyed by this call, so it cannot be protected.
        SUPPRESS_UNCHECKED_ARG m_updater.m_builder.destroy(*viewTransitionOld);
    else if (newViewTransitionOld)
        m_updater.m_builder.attach(*protect(imagePair), WTF::move(newViewTransitionOld), protect(viewTransitionNew));
}


}
