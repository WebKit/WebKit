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

#include "ElementRuleCollector.h"
#include "RenderDescendantIterator.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "StyleTreeResolver.h"
#include "ViewTransition.h"

namespace WebCore {

RenderTreeUpdater::ViewTransition::ViewTransition(RenderTreeUpdater& updater)
    : m_updater(updater)
{
}

// The contents and ordering of the named elements map should remain stable during the duration of the transition.
// We should only need to handle changes in the `display` CSS property by recreating / deleting renderers as needed.
void RenderTreeUpdater::ViewTransition::updatePseudoElementTree(RenderElement& documentElementRenderer)
{
    auto destroyPseudoElementTreeIfNeeded = [&documentElementRenderer, this]() {
        if (WeakPtr viewTransitionRoot = documentElementRenderer.view().viewTransitionRoot())
            m_updater.m_builder.destroy(*viewTransitionRoot);
    };

    Ref document = documentElementRenderer.document();

    // Intentionally bail out early here to avoid computing the style.
    if (!document->hasViewTransitionPseudoElementTree() || !document->documentElement()) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    // Destroy pseudo element tree ::view-transition has display: none or no style.
    auto rootStyle = documentElementRenderer.getCachedPseudoStyle(PseudoId::ViewTransition, &documentElementRenderer.style());
    if (!rootStyle || rootStyle->display() == DisplayType::None) {
        destroyPseudoElementTreeIfNeeded();
        return;
    }

    RefPtr activeViewTransition = document->activeViewTransition();
    ASSERT(activeViewTransition);

    auto newRootStyle = RenderStyle::clone(*rootStyle);

    // Create ::view-transition as needed.
    WeakPtr viewTransitionRoot = documentElementRenderer.view().viewTransitionRoot();
    if (viewTransitionRoot)
        viewTransitionRoot->setStyle(WTFMove(newRootStyle));
    else {
        auto newViewTransitionRoot = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, documentElementRenderer.document(), WTFMove(newRootStyle));
        newViewTransitionRoot->initializeStyle();
        documentElementRenderer.view().setViewTransitionRoot(*newViewTransitionRoot.get());
        viewTransitionRoot = newViewTransitionRoot.get();
        m_updater.m_builder.attach(documentElementRenderer, WTFMove(newViewTransitionRoot));
    }

    // No groups. The map is constant during the duration of the transition, so we don't need to handle deletions.
    if (activeViewTransition->namedElements().isEmpty())
        return;

    // Traverse named elements map to update/build all ::view-transition-group().
    Vector<SingleThreadWeakPtr<RenderObject>> descendantsToDelete;
    auto* currentGroup = documentElementRenderer.view().viewTransitionRoot()->firstChild();
    for (auto& name : activeViewTransition->namedElements().keys()) {
        ASSERT(!currentGroup || currentGroup->style().pseudoElementType() == PseudoId::ViewTransitionGroup);
        if (currentGroup && name == currentGroup->style().pseudoElementNameArgument()) {
            auto style = documentElementRenderer.getUncachedPseudoStyle({ PseudoId::ViewTransitionGroup, name }, &documentElementRenderer.style());
            if (!style || style->display() == DisplayType::None)
                descendantsToDelete.append(currentGroup);
            else
                updatePseudoElementGroup(*style, downcast<RenderElement>(*currentGroup), documentElementRenderer);
        } else {
            buildPseudoElementGroup(name, documentElementRenderer, currentGroup);
            currentGroup = currentGroup ? currentGroup->previousSibling() : documentElementRenderer.view().viewTransitionRoot()->firstChild();
        }
        currentGroup = currentGroup ? currentGroup->nextSibling() : nullptr;
    }

    for (auto& descendant : descendantsToDelete) {
        if (descendant)
            m_updater.m_builder.destroy(*descendant);
    }
}

void RenderTreeUpdater::ViewTransition::buildPseudoElementGroup(const AtomString& name, RenderElement& documentElementRenderer, RenderObject* beforeChild)
{
    Ref document = documentElementRenderer.document();
    auto& documentElementStyle = documentElementRenderer.style();
    auto createRendererIfNeeded = [&](const AtomString& name, PseudoId pseudoId) -> RenderPtr<RenderBlockFlow> {
        auto style = documentElementRenderer.getUncachedPseudoStyle({ pseudoId, name }, &documentElementStyle);
        if (!style || style->display() == DisplayType::None)
            return nullptr;
        auto newStyle = RenderStyle::clone(*style);
        auto renderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, document, WTFMove(newStyle));
        renderer->initializeStyle();
        return renderer;
    };

    auto viewTransitionGroup = createRendererIfNeeded(name, PseudoId::ViewTransitionGroup);
    auto viewTransitionImagePair = viewTransitionGroup ? createRendererIfNeeded(name, PseudoId::ViewTransitionImagePair) : nullptr;
    auto viewTransitionOld = viewTransitionImagePair ? createRendererIfNeeded(name, PseudoId::ViewTransitionOld) : nullptr;
    auto viewTransitionNew = viewTransitionImagePair ? createRendererIfNeeded(name, PseudoId::ViewTransitionNew) : nullptr;

    if (viewTransitionOld)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTFMove(viewTransitionOld));

    if (viewTransitionNew)
        m_updater.m_builder.attach(*viewTransitionImagePair, WTFMove(viewTransitionNew));

    if (viewTransitionImagePair)
        m_updater.m_builder.attach(*viewTransitionGroup, WTFMove(viewTransitionImagePair));

    if (viewTransitionGroup)
        m_updater.m_builder.attach(*documentElementRenderer.view().viewTransitionRoot(), WTFMove(viewTransitionGroup), beforeChild);
}

void RenderTreeUpdater::ViewTransition::updatePseudoElementGroup(const RenderStyle& groupStyle, RenderElement& group, RenderElement& documentElementRenderer)
{
    auto& documentElementStyle = documentElementRenderer.style();
    auto name = groupStyle.pseudoElementNameArgument();

    auto newGroupStyle = RenderStyle::clone(groupStyle);
    group.setStyle(WTFMove(newGroupStyle));

    auto createRendererIfNeeded = [&](PseudoId pseudoId) -> RenderPtr<RenderBlockFlow> {
        auto style = documentElementRenderer.getUncachedPseudoStyle({ pseudoId, name }, &documentElementStyle);
        if (!style || style->display() == DisplayType::None)
            return nullptr;
        auto newStyle = RenderStyle::clone(*style);
        auto renderer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, documentElementRenderer.document(), WTFMove(newStyle));
        renderer->initializeStyle();
        return renderer;
    };

    enum class ShouldDeleteRenderer : bool { No, Yes };
    auto updateRenderer = [&](RenderObject& renderer) -> ShouldDeleteRenderer {
        auto style = documentElementRenderer.getUncachedPseudoStyle({ renderer.style().pseudoElementType(), name }, &documentElementStyle);
        if (!style || style->display() == DisplayType::None)
            return ShouldDeleteRenderer::Yes;

        auto newStyle = RenderStyle::clone(*style);
        downcast<RenderElement>(renderer).setStyle(WTFMove(newStyle));
        return ShouldDeleteRenderer::No;
    };

    // Create / remove ::view-transtion-image-pair itself.
    SingleThreadWeakPtr<RenderElement> imagePair = downcast<RenderElement>(group.firstChild());
    if (imagePair) {
        ASSERT(imagePair->style().pseudoElementType() == PseudoId::ViewTransitionImagePair);
        auto shouldDeleteRenderer = updateRenderer(*imagePair);
        if (shouldDeleteRenderer == ShouldDeleteRenderer::Yes) {
            m_updater.m_builder.destroy(*imagePair);
            return;
        }
    } else if (auto newImagePair = createRendererIfNeeded(PseudoId::ViewTransitionImagePair)) {
        imagePair = newImagePair.get();
        m_updater.m_builder.attach(group, WTFMove(newImagePair));
    } else
        return;

    auto* imagePairFirstChild = imagePair->firstChild();
    // Build the ::view-transition-image-pair children if needed.
    if (!imagePairFirstChild) {
        if (auto viewTransitionOld = createRendererIfNeeded(PseudoId::ViewTransitionOld))
            m_updater.m_builder.attach(*imagePair, WTFMove(viewTransitionOld));
        if (auto viewTransitionNew = createRendererIfNeeded(PseudoId::ViewTransitionNew))
            m_updater.m_builder.attach(*imagePair, WTFMove(viewTransitionNew));
        return;
    }

    // Update pre-existing ::view-transition-image-pair children.
    auto shouldDeleteViewTransitionOld = ShouldDeleteRenderer::No;

    SingleThreadWeakPtr<RenderObject> viewTransitionOld;
    SingleThreadWeakPtr<RenderObject> viewTransitionNew;

    RenderPtr<RenderBlockFlow> newViewTransitionOld;
    RenderPtr<RenderBlockFlow> newViewTransitionNew;
    if (imagePairFirstChild->style().pseudoElementType() == PseudoId::ViewTransitionOld) {
        viewTransitionOld = imagePairFirstChild;
        shouldDeleteViewTransitionOld = updateRenderer(*viewTransitionOld);
        viewTransitionNew = viewTransitionOld->nextSibling();
        ASSERT(!viewTransitionNew || viewTransitionNew->style().pseudoElementType() == PseudoId::ViewTransitionNew);
    } else {
        ASSERT(imagePairFirstChild->style().pseudoElementType() == PseudoId::ViewTransitionNew);
        viewTransitionNew = imagePairFirstChild;
        newViewTransitionOld = createRendererIfNeeded(PseudoId::ViewTransitionOld);
    }

    auto shouldDeleteViewTransitionNew = ShouldDeleteRenderer::No;
    if (!viewTransitionNew)
        newViewTransitionNew = createRendererIfNeeded(PseudoId::ViewTransitionNew);
    else
        shouldDeleteViewTransitionNew = updateRenderer(*viewTransitionNew);

    if (shouldDeleteViewTransitionNew == ShouldDeleteRenderer::Yes)
        m_updater.m_builder.destroy(*viewTransitionNew);
    else if (newViewTransitionNew)
        m_updater.m_builder.attach(*imagePair, WTFMove(newViewTransitionNew));

    if (shouldDeleteViewTransitionOld == ShouldDeleteRenderer::Yes)
        m_updater.m_builder.destroy(*viewTransitionOld);
    else if (newViewTransitionOld)
        m_updater.m_builder.attach(*imagePair, WTFMove(newViewTransitionOld), viewTransitionNew.get());
}


}
