/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 * Copyright (C) 2021, 2023, 2024 Igalia S.L.
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

#include "Logging.h"
#include "RenderAncestorIterator.h"
#include "RenderChildIterator.h"
#include "RenderSVGInline.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGResourceGradient.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShape.h"
#include "RenderSVGText.h"
#include "RenderSVGTransformableContainer.h"
#include "RenderSVGViewportContainer.h"
#include "SVGRenderSupport.h"

namespace WebCore {

SVGContainerLayout::SVGContainerLayout(RenderLayerModelObject& container)
    : m_container(container)
{
}

void SVGContainerLayout::layoutChildren(bool containerNeedsLayout)
{
    bool layoutSizeChanged = layoutSizeOfNearestViewportChanged();
    bool transformChanged = transformToRootChanged(m_container.ptr());

    m_positionedChildren.clear();
    for (auto& child : childrenOfType<RenderObject>(m_container)) {
        if (child.isSVGLayerAwareRenderer())
            m_positionedChildren.append(downcast<RenderLayerModelObject>(child));

        bool needsLayout = containerNeedsLayout;
        bool childEverHadLayout = child.everHadLayout();

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (CheckedPtr text = dynamicDowncast<RenderSVGText>(child))
                text->setNeedsTextMetricsUpdate();
            needsLayout = true;
        }

        if (layoutSizeChanged) {
            if (child.isAnonymous()) {
                ASSERT(is<RenderSVGViewportContainer>(child));
                needsLayout = true;
            } else if (RefPtr element = dynamicDowncast<SVGElement>(child.node())) {
                if (element->hasRelativeLengths()) {
                    // When containerNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths

                    // When the layout size changed and when using relative values tell the RenderSVGShape to update its shape object
                    if (CheckedPtr shape = dynamicDowncast<RenderSVGShape>(child)) {
                        shape->setNeedsShapeUpdate();
                        needsLayout = true;
                    } else if (CheckedPtr svgText = dynamicDowncast<RenderSVGText>(child)) {
                        svgText->setNeedsTextMetricsUpdate();
                        svgText->setNeedsPositioningValuesUpdate();
                        needsLayout = true;
                    } else if (CheckedPtr resource = dynamicDowncast<RenderSVGResourceGradient>(child))
                        resource->invalidateGradient();
                    // FIXME: [LBSE] Add pattern support.
                }
            }
        }

        if (needsLayout)
            child.setNeedsLayout(MarkOnlyThis);

        if (CheckedPtr element = dynamicDowncast<RenderElement>(child)) {
            if (element->needsLayout())
                element->layout();

            if (!childEverHadLayout && element->checkForRepaintDuringLayout())
                element->repaint();
        }

        ASSERT(!child.needsLayout());
    }
}

void SVGContainerLayout::positionChildrenRelativeToContainer()
{
    if (m_positionedChildren.isEmpty())
        return;

    auto verifyPositionedChildRendererExpectation = [](RenderObject& renderer) {
#if !defined(NDEBUG)
        ASSERT(renderer.isSVGLayerAwareRenderer()); // Pre-condition to enter m_positionedChildren
        ASSERT(!renderer.isRenderSVGRoot()); // There is only one outermost RenderSVGRoot object
        ASSERT(!renderer.isRenderSVGInline()); // Inlines are only allowed within a RenderSVGText tree

        if (is<RenderSVGModelObject>(renderer) || is<RenderSVGBlock>(renderer))
            return;

        ASSERT_NOT_REACHED();
        return;
#else
        UNUSED_PARAM(renderer);
#endif
    };

    auto computeContainerLayoutLocation = [&]() -> LayoutPoint {
        // The nominal SVG layout location (== flooredLayoutPoint(objectBoundingBoxWithoutTransformsTopLeft), where
        // objectBoundingBoxWithoutTransforms = union of child boxes, not mapped through their tranforms) is
        // only meaningful for the children of the RenderSVGRoot. RenderSVGRoot itself is positioned according to
        // the CSS box model object, where we need to respect border & padding, encoded in the contentBoxLocation().
        // -> Position all RenderSVGRoot children relative to the contentBoxLocation() to avoid intruding border/padding area.
        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(m_container.get()))
            return -svgRoot->contentBoxLocation();

        // For (inner) RenderSVGViewportContainer nominalSVGLayoutLocation() returns the viewport boundaries,
        // including the effect of the 'x'/'y' attribute values. Do not subtract the location, otherwise the
        // effect of the x/y translation is removed.
        if (is<RenderSVGViewportContainer>(m_container) && !m_container->isAnonymous())
            return { };

        return m_container->nominalSVGLayoutLocation();
    };

    // Arrange layout location for all child renderers relative to the container layout location.
    auto parentLayoutLocation = computeContainerLayoutLocation();
    for (RenderLayerModelObject& child : m_positionedChildren) {
        verifyPositionedChildRendererExpectation(child);

        auto desiredLayoutLocation = toLayoutPoint(child.nominalSVGLayoutLocation() - parentLayoutLocation);
        if (child.currentSVGLayoutLocation() != desiredLayoutLocation)
            child.setCurrentSVGLayoutLocation(desiredLayoutLocation);
    }
}

void SVGContainerLayout::verifyLayoutLocationConsistency(const RenderLayerModelObject& renderer)
{
    if (renderer.isSVGLayerAwareRenderer() && !renderer.isRenderSVGRoot()) {
        auto currentLayoutLocation = renderer.currentSVGLayoutLocation();

        auto expectedLayoutLocation = currentLayoutLocation;
        for (auto& ancestor : ancestorsOfType<RenderLayerModelObject>(renderer)) {
            ASSERT(ancestor.isSVGLayerAwareRenderer());
            if (ancestor.isRenderSVGRoot())
                break;
            expectedLayoutLocation.moveBy(ancestor.currentSVGLayoutLocation());
        }

        auto initialLayoutLocation = renderer.nominalSVGLayoutLocation();
        if (expectedLayoutLocation == initialLayoutLocation) {
            LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName().characters() << ")"
                << " - verifyLayoutLocationConsistency() currentSVGLayoutLocation / nominalSVGLayoutLocation are in sync.");
        } else {
            LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName().characters() << ")"
                << " - verifyLayoutLocationConsistency() currentSVGLayoutLocation / nominalSVGLayoutLocation invariant violated -- out of sync due to partial layout?"
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

    for (auto& child : childrenOfType<RenderLayerModelObject>(renderer)) {
        if (child.isSVGLayerAwareRenderer())
            verifyLayoutLocationConsistency(child);
    }

#if !defined(NDEBUG)
    if (renderer.isRenderSVGRoot()) {
        LOG_WITH_STREAM(SVG, stream << "--> SVGContainerLayout renderer " << &renderer << " (" << renderer.renderName().characters() << ")"
            << " - verifyLayoutLocationConsistency() end");
    }
#endif
}

bool SVGContainerLayout::layoutSizeOfNearestViewportChanged() const
{
    RenderElement* ancestor = m_container.ptr();
    while (ancestor && !is<RenderSVGRoot>(ancestor) && !is<RenderSVGViewportContainer>(ancestor))
        ancestor = ancestor->parent();

    ASSERT(ancestor);
    if (auto* viewportContainer = dynamicDowncast<RenderSVGViewportContainer>(ancestor))
        return viewportContainer->isLayoutSizeChanged();

    if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(ancestor))
        return svgRoot->isLayoutSizeChanged();

    return false;
}

bool SVGContainerLayout::transformToRootChanged(const RenderObject* ancestor)
{
    while (ancestor) {
        if (CheckedPtr container = dynamicDowncast<RenderSVGTransformableContainer>(*ancestor))
            return container->didTransformToRootUpdate();

        if (CheckedPtr container = dynamicDowncast<RenderSVGViewportContainer>(*ancestor))
            return container->didTransformToRootUpdate();

        if (CheckedPtr svgRoot = dynamicDowncast<RenderSVGRoot>(*ancestor))
            return svgRoot->didTransformToRootUpdate();
        ancestor = ancestor->parent();
    }

    return false;
}

}

