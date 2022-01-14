/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGContainerLayout.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "Logging.h"
#include "RenderAncestorIterator.h"
#include "RenderChildIterator.h"
#include "RenderSVGForeignObject.h"
#include "RenderSVGHiddenContainer.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderSVGText.h"
#include "RenderSVGTransformableContainer.h"
#include "RenderSVGViewportContainer.h"
#include "SVGRenderSupport.h"
#include "SVGRenderingContext.h"
#include "SVGResources.h"
#include "SVGResourcesCache.h"

namespace WebCore {

SVGContainerLayout::SVGContainerLayout(RenderLayerModelObject& container)
    : m_container(container)
{
}

void SVGContainerLayout::layoutChildren(bool containerNeedsLayout)
{
    bool layoutSizeChanged = layoutSizeOfNearestViewportChanged();
    bool transformChanged = transformToRootChanged(&m_container);
    HashSet<RenderElement*> elementsThatDidNotReceiveLayout;

    m_positionedChildren.clear();
    for (auto& child : childrenOfType<RenderObject>(m_container)) {
        if (child.isSVGLayerAwareRenderer()) {
            ASSERT(child.hasLayer());
            m_positionedChildren.append(downcast<RenderLayerModelObject>(child));
        }

        bool needsLayout = containerNeedsLayout;
        bool childEverHadLayout = child.everHadLayout();

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (is<RenderSVGText>(child))
                downcast<RenderSVGText>(child).setNeedsTextMetricsUpdate();
            needsLayout = true;
        }

        if (layoutSizeChanged && child.node() && is<SVGElement>(*child.node())) {
            // When containerNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            auto& element = downcast<SVGElement>(*child.node());
            if (element.hasRelativeLengths() && 1) {
                // When the layout size changed and when using relative values tell the RenderSVGShape to update its shape object
                if (is<RenderSVGShape>(child))
                    downcast<RenderSVGShape>(child).setNeedsShapeUpdate();
                else if (is<RenderSVGText>(child)) {
                    auto& svgText = downcast<RenderSVGText>(child);
                    svgText.setNeedsTextMetricsUpdate();
                    svgText.setNeedsPositioningValuesUpdate();
                }

                needsLayout = true;
            }
        }

        if (needsLayout)
            child.setNeedsLayout(MarkOnlyThis);

        bool childNeededLayout = child.needsLayout();
        if (is<RenderElement>(child)) {
            auto& element = downcast<RenderElement>(child);
            if (childNeededLayout) {
                layoutDifferentRootIfNeeded(element);
                element.layout();
            } else if (layoutSizeChanged)
                elementsThatDidNotReceiveLayout.add(&element);

            if (!childEverHadLayout && element.checkForRepaintDuringLayout())
                child.repaint();
        }

        ASSERT(!child.needsLayout());
    }

    if (layoutSizeChanged) {
        // If the layout size changed, invalidate all resources of all children that didn't go through the layout() code path.
        for (auto* element : elementsThatDidNotReceiveLayout)
            invalidateResourcesOfChildren(*element);
    } else
        ASSERT(elementsThatDidNotReceiveLayout.isEmpty());
}

static inline LayoutPoint layoutLocationFromRenderer(const RenderObject& renderer)
{
    if (is<RenderSVGModelObject>(renderer))
        return downcast<RenderSVGModelObject>(renderer).layoutLocation();

    if (is<RenderSVGBlock>(renderer)) // <foreignObject> / <text>
        return downcast<RenderSVGBlock>(renderer).location();

    if (is<RenderSVGInline>(renderer)) // <tspan> / <textPath>
        return { };

    ASSERT_NOT_REACHED();
    return { };
}

static inline void setLayoutLocationForRenderer(RenderObject& renderer, const LayoutPoint& newLocation)
{
    if (is<RenderSVGModelObject>(renderer)) {
        downcast<RenderSVGModelObject>(renderer).setLayoutLocation(newLocation);
        return;
    }

    // <foreignObject> / <text>
    if (is<RenderSVGBlock>(renderer)) {
        downcast<RenderSVGBlock>(renderer).setLocation(newLocation);
        return;
    }

    // <tspan> / <textPath>
    if (is<RenderSVGInline>(renderer))
        return;

    ASSERT_NOT_REACHED();
}

void SVGContainerLayout::positionChildrenRelativeToContainer()
{
    if (m_positionedChildren.isEmpty())
        return;

    auto verifyPositionedChildRendererExpectation = [](RenderObject& renderer) {
#if !defined(NDEBUG)
        ASSERT(renderer.isSVGLayerAwareRenderer()); // Pre-condition to enter m_positionedChildren
        ASSERT(!renderer.isSVGRoot()); // There is only one outermost RenderSVGRoot object
        ASSERT(!renderer.isSVGInline()); // Inlines are only allowed within a RenderSVGText tree

        if (is<RenderSVGModelObject>(renderer) || is<RenderSVGBlock>(renderer))
            return;

        ASSERT_NOT_REACHED();
        return;
#else
        UNUSED_PARAM(renderer);
#endif
    };

    // Arrange layout location for all child renderers relative to the container layout location.
    auto parentLayoutLocation = flooredLayoutPoint(m_container.objectBoundingBox().minXMinYCorner());
    for (RenderObject& child : m_positionedChildren) {
        verifyPositionedChildRendererExpectation(child);

        auto objectBoundingBoxChild = child.objectBoundingBox();
        auto layoutLocation = flooredLayoutPoint(objectBoundingBoxChild.minXMinYCorner());
        auto desiredLayoutLocation = toLayoutPoint(layoutLocation - parentLayoutLocation);
        auto currentLayoutLocation = layoutLocationFromRenderer(child);
        if (currentLayoutLocation == desiredLayoutLocation)
            continue;
        setLayoutLocationForRenderer(child, desiredLayoutLocation);
    }
}

void SVGContainerLayout::verifyLayoutLocationConsistency(const RenderElement& renderer)
{
    if (renderer.isSVGLayerAwareRenderer() && !renderer.isSVGRoot()) {
        auto currentLayoutLocation = layoutLocationFromRenderer(renderer);

        auto expectedLayoutLocation = currentLayoutLocation;
        for (auto& ancestor : ancestorsOfType<RenderElement>(renderer)) {
            ASSERT(ancestor.isSVGLayerAwareRenderer());
            if (ancestor.isSVGRoot())
                break;
            expectedLayoutLocation.moveBy(layoutLocationFromRenderer(ancestor));
        }

        auto initialLayoutLocation = flooredLayoutPoint(renderer.objectBoundingBox().minXMinYCorner());
        if (expectedLayoutLocation == initialLayoutLocation) {
            LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName() << ")"
                << " - verifyLayoutLocationConsistency() objectBoundingBox / layoutLocation are in sync.");
        } else {
            LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName() << ")"
                << " - verifyLayoutLocationConsistency() objectBoundingBox / layoutLocation invariant violated -- out of sync due to partial layout?"
                << " currentLayoutLocation=" << currentLayoutLocation
                << "  (expectedLayoutLocation=" << expectedLayoutLocation
                << " != initialLayoutLocation=" << initialLayoutLocation << ")"
                << " -> aborting with a render tree dump");

#if ENABLE(TREE_DEBUGGING)
            showRenderTree(&renderer);
#endif

            ASSERT_NOT_REACHED();
        }
    }

    for (auto& child : childrenOfType<RenderElement>(renderer)) {
        if (child.isSVGLayerAwareRenderer())
            verifyLayoutLocationConsistency(child);
    }

#if !defined(NDEBUG)
    if (renderer.isSVGRoot()) {
        LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName() << ")"
            << " - verifyLayoutLocationConsistency() end");
    }
#endif
}

void SVGContainerLayout::layoutDifferentRootIfNeeded(const RenderElement& renderer)
{
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer)) {
        auto* svgRoot = SVGRenderSupport::findTreeRootObject(renderer);
        ASSERT(svgRoot);
        resources->layoutDifferentRootIfNeeded(svgRoot);
    }
}

void SVGContainerLayout::invalidateResourcesOfChildren(RenderElement& renderer)
{
    ASSERT(!renderer.needsLayout());
    if (auto* resources = SVGResourcesCache::cachedResourcesForRenderer(renderer))
        resources->removeClientFromCache(renderer, false);

    for (auto& child : childrenOfType<RenderElement>(renderer))
        invalidateResourcesOfChildren(child);
}

bool SVGContainerLayout::layoutSizeOfNearestViewportChanged() const
{
    RenderElement* ancestor = &m_container;
    while (ancestor && !is<RenderSVGRoot>(*ancestor) && !is<RenderSVGViewportContainer>(*ancestor))
        ancestor = ancestor->parent();

    ASSERT(ancestor);
    if (is<RenderSVGViewportContainer>(*ancestor))
        return downcast<RenderSVGViewportContainer>(*ancestor).isLayoutSizeChanged();

    return downcast<RenderSVGRoot>(*ancestor).isLayoutSizeChanged();
}

bool SVGContainerLayout::transformToRootChanged(const RenderObject* ancestor)
{
    while (ancestor) {
        // FIXME: [LBSE] Upstream RenderSVGTransformableContainer changes
        // if (is<RenderSVGTransformableContainer>(*ancestor))
        //     return downcast<const RenderSVGTransformableContainer>(*ancestor).didTransformToRootUpdate();

        // FIXME: [LBSE] Upstream RenderSVGTransformableContainer changes
        // if (is<RenderSVGViewportContainer>(*ancestor))
        //     return downcast<const RenderSVGViewportContainer>(*ancestor).didTransformToRootUpdate();

        if (is<RenderSVGRoot>(*ancestor))
            return downcast<const RenderSVGRoot>(*ancestor).didTransformToRootUpdate();
        ancestor = ancestor->parent();
    }

    return false;
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
