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

#include "ContextDestructionObserverInlines.h"
#include "ElementRuleCollector.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderViewTransitionCapture.h"
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
void RenderTreeUpdater::ViewTransition::updatePseudoElementTree(RenderElement* documentElementRenderer, Style::DifferenceResult minimalStyleDifference)
{
    auto destroyPseudoElementTreeIfNeeded = [&]() {
        if (WeakPtr viewTransitionContainingBlock = m_updater.renderView().viewTransitionContainingBlock())
            m_updater.destroyAndCancelAnimationsForSubtree(*viewTransitionContainingBlock);
    };

    if (!documentElementRenderer) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    Ref document = documentElementRenderer->document();

    // Intentionally bail out early here to avoid computing the style.
    if (!document->hasViewTransitionPseudoElementTree() || !document->documentElement()) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    // Destroy pseudo element tree ::view-transition has display: none or no style.
    auto rootStyle = documentElementRenderer->getCachedPseudoStyle({ PseudoElementType::ViewTransition }, &documentElementRenderer->style());
    if (!rootStyle || rootStyle->display() == Style::DisplayType::None) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    RefPtr activeViewTransition = document->activeViewTransition();
    ASSERT(activeViewTransition);

    auto newRootStyle = RenderStyle::clone(*rootStyle);

    WeakPtr viewTransitionContainingBlock = documentElementRenderer->view().viewTransitionContainingBlock();
    if (!viewTransitionContainingBlock) {
        auto containingBlockStyle = RenderStyle::createAnonymousStyleWithDisplay(documentElementRenderer->view().style(), Style::DisplayType::BlockFlow);
        containingBlockStyle.setPosition(PositionType::Fixed);
        containingBlockStyle.setPointerEvents(PointerEvents::None);

        auto containingBlockRect = activeViewTransition->containingBlockRect();
        containingBlockStyle.setLeft(Style::InsetEdge::Fixed { containingBlockRect.x() });
        containingBlockStyle.setTop(Style::InsetEdge::Fixed { containingBlockRect.y() });
        containingBlockStyle.setWidth(Style::PreferredSize::Fixed { containingBlockRect.width() });
        containingBlockStyle.setHeight(Style::PreferredSize::Fixed { containingBlockRect.height() });

        auto newViewTransitionContainingBlock = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(containingBlockStyle), RenderObject::BlockFlowFlag::IsViewTransitionContainingBlock);
        newViewTransitionContainingBlock->initializeStyle();
        documentElementRenderer->view().setViewTransitionContainingBlock(*newViewTransitionContainingBlock.get());
        viewTransitionContainingBlock = newViewTransitionContainingBlock.get();
        m_updater.m_builder.attach(*documentElementRenderer->parent(), WTF::move(newViewTransitionContainingBlock));
    }

    // Create ::view-transition as needed.
    WeakPtr viewTransitionRoot = dynamicDowncast<RenderBlockFlow>(viewTransitionContainingBlock->firstChildBox());
    if (viewTransitionRoot)
        viewTransitionRoot->setStyle(WTF::move(newRootStyle), minimalStyleDifference);
    else {
        auto newViewTransitionRoot = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTF::move(newRootStyle));
        newViewTransitionRoot->initializeStyle();
        viewTransitionRoot = newViewTransitionRoot.get();
        m_updater.m_builder.attach(*viewTransitionContainingBlock, WTF::move(newViewTransitionRoot));
    }

    // No groups. The map is constant during the duration of the transition, so we don't need to handle deletions.
    if (activeViewTransition->namedElements().isEmpty())
        return;

    // Traverse named elements map to update/build all ::view-transition-group().
    Vector<SingleThreadWeakPtr<RenderElement>> descendantsToDelete;
    auto* currentGroup = viewTransitionRoot->firstChildBox();
    for (auto& name : activeViewTransition->namedElements().keys()) {
        ASSERT(!currentGroup || currentGroup->style().pseudoElementType() == PseudoElementType::ViewTransitionGroup);
        if (currentGroup && name == currentGroup->style().pseudoElementNameArgument()) {
            auto style = documentElementRenderer->getCachedPseudoStyle({ PseudoElementType::ViewTransitionGroup, name }, &documentElementRenderer->style());
            if (!style || style->display() == Style::DisplayType::None) {
                documentElementRenderer->view().removeViewTransitionGroup(name);
                descendantsToDelete.append(currentGroup);
            } else
                updatePseudoElementGroup(*style, *currentGroup, *documentElementRenderer, minimalStyleDifference);
            currentGroup = currentGroup->nextSiblingBox();
        } else
            buildPseudoElementGroup(*viewTransitionRoot, name, *documentElementRenderer, currentGroup);
    }

    for (auto& descendant : descendantsToDelete) {
        if (descendant)
            m_updater.destroyAndCancelAnimationsForSubtree(*descendant);
    }
}

static RenderPtr<RenderBox> createRendererIfNeeded(RenderElement& documentElementRenderer, const AtomString& name, PseudoElementType pseudoElementType)
{
    auto& documentElementStyle = documentElementRenderer.style();
    auto style = documentElementRenderer.getCachedPseudoStyle({ pseudoElementType, name }, &documentElementStyle);
    if (!style || style->display() == Style::DisplayType::None)
        return nullptr;

    Ref document = documentElementRenderer.document();
    RenderPtr<RenderBox> renderer;
    if (pseudoElementType == PseudoElementType::ViewTransitionOld || pseudoElementType == PseudoElementType::ViewTransitionNew) {
        const auto* capturedElement = document->activeViewTransition()->namedElements().find(name);
        ASSERT(capturedElement);
        if (pseudoElementType == PseudoElementType::ViewTransitionOld && !capturedElement->oldImage)
            return nullptr;
        if (pseudoElementType == PseudoElementType::ViewTransitionNew && !capturedElement->newElement)
            return nullptr;

        auto& state = pseudoElementType == PseudoElementType::ViewTransitionOld ? capturedElement->oldState : capturedElement->newState;

        RenderPtr<RenderViewTransitionCapture> rendererViewTransition = WebCore::createRenderer<RenderViewTransitionCapture>(RenderObject::Type::ViewTransitionCapture, document, RenderStyle::clone(*style), state.isRootElement);
        if (pseudoElementType == PseudoElementType::ViewTransitionOld)
            rendererViewTransition->setImage(capturedElement->oldImage.value_or(nullptr));
        rendererViewTransition->setCapturedSize(state.size, state.overflowRect, state.layerToLayoutOffset);
        renderer = WTF::move(rendererViewTransition);
    } else
        renderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, RenderStyle::clone(*style));

    renderer->initializeStyle();
    return renderer;
}


void RenderTreeUpdater::ViewTransition::buildPseudoElementGroup(RenderBlockFlow& viewTransitionRoot, const AtomString& name, RenderElement& documentElementRenderer, RenderObject* beforeChild)
{
    auto viewTransitionGroup = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionGroup);
    auto viewTransitionImagePair = viewTransitionGroup ? createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionImagePair) : nullptr;
    auto viewTransitionOld = viewTransitionImagePair ? createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionOld) : nullptr;
    auto viewTransitionNew = viewTransitionImagePair ? createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionNew) : nullptr;

    if (viewTransitionOld)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTF::move(viewTransitionOld));

    if (viewTransitionNew)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTF::move(viewTransitionNew));

    if (viewTransitionImagePair)
        m_updater.m_builder.attach(*viewTransitionGroup, WTF::move(viewTransitionImagePair));

    if (viewTransitionGroup) {
        documentElementRenderer.view().addViewTransitionGroup(name, *viewTransitionGroup.get());
        m_updater.m_builder.attach(viewTransitionRoot, WTF::move(viewTransitionGroup), beforeChild);
    }
}

void RenderTreeUpdater::ViewTransition::updatePseudoElementGroup(const RenderStyle& groupStyle, RenderBox& group, RenderElement& documentElementRenderer, Style::DifferenceResult minimalStyleDifference)
{
    auto& documentElementStyle = documentElementRenderer.style();
    auto name = groupStyle.pseudoElementNameArgument();

    auto newGroupStyle = RenderStyle::clone(groupStyle);
    group.setStyle(WTF::move(newGroupStyle), minimalStyleDifference);

    enum class ShouldDeleteRenderer : bool { No, Yes };
    auto updateRenderer = [&](auto& renderer) -> ShouldDeleteRenderer {
        auto style = documentElementRenderer.getCachedPseudoStyle({ *renderer.style().pseudoElementType(), name }, &documentElementStyle);
        if (!style || style->display() == Style::DisplayType::None)
            return ShouldDeleteRenderer::Yes;

        auto newStyle = RenderStyle::clone(*style);
        renderer.setStyle(WTF::move(newStyle), minimalStyleDifference);
        return ShouldDeleteRenderer::No;
    };

    // Create / remove ::view-transtion-image-pair itself.
    SingleThreadWeakPtr<RenderBox> imagePair = group.firstChildBox();
    if (imagePair) {
        ASSERT(imagePair->style().pseudoElementType() == PseudoElementType::ViewTransitionImagePair);
        auto shouldDeleteRenderer = updateRenderer(*imagePair);
        if (shouldDeleteRenderer == ShouldDeleteRenderer::Yes) {
            m_updater.destroyAndCancelAnimationsForSubtree(*imagePair);
            return;
        }
    } else if (auto newImagePair = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionImagePair)) {
        imagePair = newImagePair.get();
        m_updater.m_builder.attach(group, WTF::move(newImagePair));
    } else
        return;

    auto* imagePairFirstChild = imagePair->firstChildBox();
    // Build the ::view-transition-image-pair children if needed.
    if (!imagePairFirstChild) {
        if (auto viewTransitionOld = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionOld))
            m_updater.m_builder.attach(*imagePair, WTF::move(viewTransitionOld));
        if (auto viewTransitionNew = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionNew))
            m_updater.m_builder.attach(*imagePair, WTF::move(viewTransitionNew));
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
        shouldDeleteViewTransitionOld = updateRenderer(*viewTransitionOld);
        viewTransitionNew = viewTransitionOld->nextSiblingBox();
        ASSERT(!viewTransitionNew || viewTransitionNew->style().pseudoElementType() == PseudoElementType::ViewTransitionNew);
    } else {
        ASSERT(imagePairFirstChild->style().pseudoElementType() == PseudoElementType::ViewTransitionNew);
        viewTransitionNew = imagePairFirstChild;
        newViewTransitionOld = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionOld);
    }

    auto shouldDeleteViewTransitionNew = ShouldDeleteRenderer::No;
    if (!viewTransitionNew)
        newViewTransitionNew = createRendererIfNeeded(documentElementRenderer, name, PseudoElementType::ViewTransitionNew);
    else
        shouldDeleteViewTransitionNew = updateRenderer(*viewTransitionNew);

    if (shouldDeleteViewTransitionNew == ShouldDeleteRenderer::Yes)
        m_updater.destroyAndCancelAnimationsForSubtree(*viewTransitionNew);
    else if (newViewTransitionNew)
        m_updater.m_builder.attach(*imagePair, WTF::move(newViewTransitionNew));

    if (shouldDeleteViewTransitionOld == ShouldDeleteRenderer::Yes)
        m_updater.destroyAndCancelAnimationsForSubtree(*viewTransitionOld);
    else if (newViewTransitionOld)
        m_updater.m_builder.attach(*imagePair, WTF::move(newViewTransitionOld), viewTransitionNew.get());
}


}
