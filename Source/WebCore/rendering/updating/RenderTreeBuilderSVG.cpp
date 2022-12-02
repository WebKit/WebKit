/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RenderTreeBuilderSVG.h"

#include "LegacyRenderSVGContainer.h"
#include "LegacyRenderSVGRoot.h"
#include "RenderSVGInline.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderSVGViewportContainer.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderTreeBuilderBlockFlow.h"
#include "RenderTreeBuilderInline.h"
#include "SVGResourcesCache.h"

namespace WebCore {

RenderTreeBuilder::SVG::SVG(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::SVG::attach(LegacyRenderSVGRoot& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
    SVGResourcesCache::clientWasAddedToTree(childToAdd);
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
void RenderTreeBuilder::SVG::attach(RenderSVGContainer& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
    SVGResourcesCache::clientWasAddedToTree(childToAdd);
}
#endif

void RenderTreeBuilder::SVG::attach(LegacyRenderSVGContainer& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
    SVGResourcesCache::clientWasAddedToTree(childToAdd);
}

void RenderTreeBuilder::SVG::attach(RenderSVGInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.inlineBuilder().attach(parent, WTFMove(child), beforeChild);
    SVGResourcesCache::clientWasAddedToTree(childToAdd);

    if (auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(parent))
        textAncestor->subtreeChildWasAdded(&childToAdd);
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
void RenderTreeBuilder::SVG::attach(RenderSVGRoot& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.attachToRenderElement(findOrCreateParentForChild(parent), WTFMove(child), beforeChild);

    SVGResourcesCache::clientWasAddedToTree(childToAdd);
}
#endif

void RenderTreeBuilder::SVG::attach(RenderSVGText& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    auto& childToAdd = *child;
    m_builder.blockFlowBuilder().attach(parent, WTFMove(child), beforeChild);

    SVGResourcesCache::clientWasAddedToTree(childToAdd);
    parent.subtreeChildWasAdded(&childToAdd);
}

RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(LegacyRenderSVGRoot& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    return m_builder.detachFromRenderElement(parent, child);
}

RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(RenderSVGText& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);

    Vector<SVGTextLayoutAttributes*, 2> affectedAttributes;
    parent.subtreeChildWillBeRemoved(&child, affectedAttributes);
    auto takenChild = m_builder.blockBuilder().detach(parent, child);
    parent.subtreeChildWasRemoved(affectedAttributes);
    return takenChild;
}

RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(RenderSVGInline& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);

    auto* textAncestor = RenderSVGText::locateRenderSVGTextAncestor(parent);
    if (!textAncestor)
        return m_builder.detachFromRenderElement(parent, child);

    Vector<SVGTextLayoutAttributes*, 2> affectedAttributes;
    textAncestor->subtreeChildWillBeRemoved(&child, affectedAttributes);
    auto takenChild = m_builder.detachFromRenderElement(parent, child);
    textAncestor->subtreeChildWasRemoved(affectedAttributes);
    return takenChild;
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(RenderSVGContainer& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    return m_builder.detachFromRenderElement(parent, child);
}
#endif

RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(LegacyRenderSVGContainer& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    return m_builder.detachFromRenderElement(parent, child);
}

#if ENABLE(LAYER_BASED_SVG_ENGINE)
RenderPtr<RenderObject> RenderTreeBuilder::SVG::detach(RenderSVGRoot& parent, RenderObject& child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);
    return m_builder.detachFromRenderElement(parent, child);
}
#endif

#if ENABLE(LAYER_BASED_SVG_ENGINE)
RenderSVGViewportContainer& RenderTreeBuilder::SVG::findOrCreateParentForChild(RenderSVGRoot& parent)
{
    if (auto* viewportContainer = parent.viewportContainer())
        return *viewportContainer;
    return createViewportContainer(parent);
}

RenderSVGViewportContainer& RenderTreeBuilder::SVG::createViewportContainer(RenderSVGRoot& parent)
{
    auto viewportContainerStyle = RenderStyle::createAnonymousStyleWithDisplay(parent.style(), RenderStyle::initialDisplay());
    viewportContainerStyle.setUsedZIndex(0); // Enforce a stacking context.
    viewportContainerStyle.setTransformOriginX(Length(0, LengthType::Fixed));
    viewportContainerStyle.setTransformOriginY(Length(0, LengthType::Fixed));

    auto viewportContainer = createRenderer<RenderSVGViewportContainer>(parent, WTFMove(viewportContainerStyle));
    viewportContainer->initializeStyle();

    auto* viewportContainerRenderer = viewportContainer.get();
    m_builder.attachToRenderElement(parent, WTFMove(viewportContainer), nullptr);
    return *viewportContainerRenderer;
}

void RenderTreeBuilder::SVG::updateAfterDescendants(RenderSVGRoot& svgRoot)
{
    // Usually the anonymous RenderSVGViewportContainer, wrapping all children of RenderSVGRoot,
    // is created when the first <svg> child element is inserted into the render tree. We'll
    // only reach this point with viewportContainer=nullptr, if the <svg> had no children -- we
    // still need to ensure the creation of the RenderSVGViewportContainer, otherwise computing
    // e.g. getCTM() would ignore the presence of a 'viewBox' induced transform (and ignore zoom/pan).
    if (svgRoot.viewportContainer())
        return;
    createViewportContainer(svgRoot);
}
#endif

}
