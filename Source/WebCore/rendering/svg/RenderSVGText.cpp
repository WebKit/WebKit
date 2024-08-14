/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 * Copyright (C) 2012-2023 Google Inc.
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
#include "RenderSVGText.h"

#include "FloatQuad.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LayoutRepainter.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGRoot.h"
#include "PointerEventsHitRules.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderIterator.h"
#include "RenderSVGBlockInlines.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGRoot.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLengthList.h"
#include "SVGRenderStyle.h"
#include "SVGResourcesCache.h"
#include "SVGRootInlineBox.h"
#include "SVGTextElement.h"
#include "SVGURIReference.h"
#include "SVGVisitedRendererTracking.h"
#include "TransformState.h"
#include "VisiblePosition.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGText);

RenderSVGText::RenderSVGText(SVGTextElement& element, RenderStyle&& style)
    : RenderSVGBlock(Type::SVGText, element, WTFMove(style))
{
    ASSERT(isRenderSVGText());
}

RenderSVGText::~RenderSVGText()
{
    ASSERT(m_layoutAttributes.isEmpty());
}

SVGTextElement& RenderSVGText::textElement() const
{
    return downcast<SVGTextElement>(RenderSVGBlock::graphicsElement());
}

Ref<SVGTextElement> RenderSVGText::protectedTextElement() const
{
    return textElement();
}

bool RenderSVGText::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isInline();
}

RenderSVGText* RenderSVGText::locateRenderSVGTextAncestor(RenderObject& start)
{
    return lineageOfType<RenderSVGText>(start).first();
}

const RenderSVGText* RenderSVGText::locateRenderSVGTextAncestor(const RenderObject& start)
{
    return lineageOfType<RenderSVGText>(start).first();
}

static inline void collectLayoutAttributes(RenderObject* text, Vector<SVGTextLayoutAttributes*>& attributes)
{
    for (RenderObject* descendant = text; descendant; descendant = descendant->nextInPreOrder(text)) {
        if (auto* svgInline = dynamicDowncast<RenderSVGInlineText>(*descendant))
            attributes.append(svgInline->layoutAttributes());
    }
}

static inline bool findPreviousAndNextAttributes(RenderElement& start, RenderSVGInlineText* locateElement, bool& stopAfterNext, SVGTextLayoutAttributes*& previous, SVGTextLayoutAttributes*& next)
{
    ASSERT(locateElement);
    // FIXME: Make this iterative.
    for (CheckedRef child : childrenOfType<RenderObject>(start)) {
        if (auto* text = dynamicDowncast<RenderSVGInlineText>(child.get())) {
            if (locateElement != text) {
                if (stopAfterNext) {
                    next = text->layoutAttributes();
                    return true;
                }

                previous = text->layoutAttributes();
                continue;
            }

            stopAfterNext = true;
            continue;
        }

        auto* childSVGInline = dynamicDowncast<RenderSVGInline>(child.get());
        if (!childSVGInline)
            continue;

        if (findPreviousAndNextAttributes(*childSVGInline, locateElement, stopAfterNext, previous, next))
            return true;
    }

    return false;
}

inline bool RenderSVGText::shouldHandleSubtreeMutations() const
{
    if (beingDestroyed() || !everHadLayout()) {
        ASSERT(m_layoutAttributes.isEmpty());
        ASSERT(!m_layoutAttributesBuilder.numberOfTextPositioningElements());
        return false;
    }
    return true;
}

void RenderSVGText::subtreeChildWasAdded(RenderObject* child)
{
    ASSERT(child);
    if (!shouldHandleSubtreeMutations() || renderTreeBeingDestroyed())
        return;

    // The positioning elements cache doesn't include the new 'child' yet. Clear the
    // cache, as the next buildLayoutAttributesForTextRenderer() call rebuilds it.
    m_layoutAttributesBuilder.clearTextPositioningElements();

    if (!child->isRenderSVGInlineText() && !child->isRenderSVGInline())
        return;

    // Detect changes in layout attributes and only measure those text parts that have changed!
    Vector<SVGTextLayoutAttributes*> newLayoutAttributes;
    collectLayoutAttributes(this, newLayoutAttributes);
    if (newLayoutAttributes.isEmpty()) {
        ASSERT(m_layoutAttributes.isEmpty());
        return;
    }

    // Compare m_layoutAttributes with newLayoutAttributes to figure out which attribute got added.
    size_t size = newLayoutAttributes.size();
    SVGTextLayoutAttributes* attributes = 0;
    for (size_t i = 0; i < size; ++i) {
        attributes = newLayoutAttributes[i];
        if (m_layoutAttributes.find(attributes) == notFound) {
            // Every time this is invoked, there's only a single new entry in the newLayoutAttributes list, compared to the old in m_layoutAttributes.
            bool stopAfterNext = false;
            SVGTextLayoutAttributes* previous = 0;
            SVGTextLayoutAttributes* next = 0;
            ASSERT_UNUSED(child, &attributes->context() == child);
            findPreviousAndNextAttributes(*this, &attributes->context(), stopAfterNext, previous, next);

            if (previous)
                m_layoutAttributesBuilder.buildLayoutAttributesForTextRenderer(previous->context());
            m_layoutAttributesBuilder.buildLayoutAttributesForTextRenderer(attributes->context());
            if (next)
                m_layoutAttributesBuilder.buildLayoutAttributesForTextRenderer(next->context());
            break;
        }
    }

#ifndef NDEBUG
    // Verify that m_layoutAttributes only differs by a maximum of one entry.
    for (size_t i = 0; i < size; ++i)
        ASSERT(m_layoutAttributes.find(newLayoutAttributes[i]) != notFound || newLayoutAttributes[i] == attributes);
#endif

    m_layoutAttributes = newLayoutAttributes;
}

static inline void checkLayoutAttributesConsistency(RenderSVGText* text, Vector<SVGTextLayoutAttributes*>& expectedLayoutAttributes)
{
#ifndef NDEBUG
    Vector<SVGTextLayoutAttributes*> newLayoutAttributes;
    collectLayoutAttributes(text, newLayoutAttributes);
    ASSERT_UNUSED(expectedLayoutAttributes, newLayoutAttributes == expectedLayoutAttributes);
#else
    UNUSED_PARAM(text);
    UNUSED_PARAM(expectedLayoutAttributes);
#endif
}

void RenderSVGText::willBeDestroyed()
{
    m_layoutAttributes.clear();
    m_layoutAttributesBuilder.clearTextPositioningElements();

    RenderSVGBlock::willBeDestroyed();
}

void RenderSVGText::subtreeChildWillBeRemoved(RenderObject* child, Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes)
{
    ASSERT(child);
    if (!shouldHandleSubtreeMutations())
        return;

    checkLayoutAttributesConsistency(this, m_layoutAttributes);

    // The positioning elements cache depends on the size of each text renderer in the
    // subtree. If this changes, clear the cache. It's going to be rebuilt below.
    m_layoutAttributesBuilder.clearTextPositioningElements();
    if (m_layoutAttributes.isEmpty() || !child->isRenderSVGInlineText())
        return;

    // This logic requires that the 'text' child is still inserted in the tree.
    auto& text = downcast<RenderSVGInlineText>(*child);
    bool stopAfterNext = false;
    SVGTextLayoutAttributes* previous = nullptr;
    SVGTextLayoutAttributes* next = nullptr;
    if (!renderTreeBeingDestroyed())
        findPreviousAndNextAttributes(*this, &text, stopAfterNext, previous, next);

    if (previous)
        affectedAttributes.append(previous);
    if (next)
        affectedAttributes.append(next);

    bool removed = m_layoutAttributes.removeFirst(text.layoutAttributes());
    ASSERT_UNUSED(removed, removed);
}

void RenderSVGText::subtreeChildWasRemoved(const Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes)
{
    if (!shouldHandleSubtreeMutations() || renderTreeBeingDestroyed()) {
        ASSERT(affectedAttributes.isEmpty());
        return;
    }

    // This is called immediately after subtreeChildWillBeDestroyed, once the RenderSVGInlineText::willBeDestroyed() method
    // passes on to the base class, which removes us from the render tree. At this point we can update the layout attributes.
    unsigned size = affectedAttributes.size();
    for (unsigned i = 0; i < size; ++i)
        m_layoutAttributesBuilder.buildLayoutAttributesForTextRenderer(affectedAttributes[i]->context());
}

void RenderSVGText::subtreeTextDidChange(RenderSVGInlineText* text)
{
    ASSERT(text);
    ASSERT(!beingDestroyed());
    if (!everHadLayout()) {
        ASSERT(m_layoutAttributes.isEmpty());
        ASSERT(!m_layoutAttributesBuilder.numberOfTextPositioningElements());
        return;
    }
    // Text transforms can cause text change to be signaled during addChild before m_layoutAttributes has been updated.
    if (!m_layoutAttributes.contains(text->layoutAttributes())) {
        ASSERT(!text->everHadLayout());
        return;
    }

    // The positioning elements cache depends on the size of each text renderer in the
    // subtree. If this changes, clear the cache. If this changes, clear the cache and mark it for rebuilding in the next layout.
    m_layoutAttributesBuilder.clearTextPositioningElements();

    checkLayoutAttributesConsistency(this, m_layoutAttributes);
    setNeedsPositioningValuesUpdate();
    setNeedsLayout();
}

static inline void updateFontInAllDescendants(RenderSVGText& text)
{
    for (RenderObject* descendant = &text; descendant; descendant = descendant->nextInPreOrder(&text)) {
        if (CheckedPtr text = dynamicDowncast<RenderSVGInlineText>(*descendant))
            text->updateScaledFont();
    }
}

void RenderSVGText::layout()
{
    auto isLayerBasedSVGEngineEnabled = [&]() {
        return document().settings().layerBasedSVGEngineEnabled();
    };

    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    if (shouldHandleSubtreeMutations() && !renderTreeBeingDestroyed())
        checkLayoutAttributesConsistency(this, m_layoutAttributes);

    auto checkForRepaintOverride = !isLayerBasedSVGEngineEnabled() ? std::make_optional(SVGRenderSupport::checkForSVGRepaintDuringLayout(*this)) : std::nullopt;
    LayoutRepainter repainter(*this, checkForRepaintOverride);

    bool updateCachedBoundariesInParents = false;
    auto previousReferenceBoxRect = transformReferenceBoxRect();

    // We update the transform now because updateScaledFont() needs it, but we do it a second time at the end of the layout,
    // since the transform reference box may change because of the font change.
    if (!isLayerBasedSVGEngineEnabled() && m_needsTransformUpdate) {
        m_localTransform = textElement().animatedLocalTransform();
        updateCachedBoundariesInParents = true;
    }

    if (!everHadLayout()) {
        // When laying out initially, collect all layout attributes, build the character data map,
        // and propogate resulting SVGLayoutAttributes to all RenderSVGInlineText children in the subtree.
        ASSERT(m_layoutAttributes.isEmpty());
        collectLayoutAttributes(this, m_layoutAttributes);
        updateFontInAllDescendants(*this);
        m_layoutAttributesBuilder.buildLayoutAttributesForForSubtree(*this);

        m_needsReordering = true;
        m_needsTextMetricsUpdate = false;
        m_needsPositioningValuesUpdate = false;
        updateCachedBoundariesInParents = true;
    } else if (m_needsPositioningValuesUpdate) {
        // When the x/y/dx/dy/rotate lists change, recompute the layout attributes, and eventually
        // update the on-screen font objects as well in all descendants.
        if (m_needsTextMetricsUpdate)
            updateFontInAllDescendants(*this);
        m_layoutAttributesBuilder.buildLayoutAttributesForForSubtree(*this);
        m_needsReordering = true;
        m_needsTextMetricsUpdate = false;
        m_needsPositioningValuesUpdate = false;
        updateCachedBoundariesInParents = true;
    } else {
        bool isLayoutSizeChanged = false;
        if (auto* legacyRootObject = lineageOfType<LegacyRenderSVGRoot>(*this).first())
            isLayoutSizeChanged = legacyRootObject->isLayoutSizeChanged();
        else if (auto* rootObject = lineageOfType<RenderSVGRoot>(*this).first())
            isLayoutSizeChanged = rootObject->isLayoutSizeChanged();

        if (m_needsTextMetricsUpdate || isLayoutSizeChanged) {
            // If the root layout size changed (eg. window size changes) or the transform to the root
            // context has changed then recompute the on-screen font size.
            updateFontInAllDescendants(*this);

            ASSERT(!m_needsReordering);
            ASSERT(!m_needsPositioningValuesUpdate);
            m_needsTextMetricsUpdate = false;
            updateCachedBoundariesInParents = true;
        }
        m_layoutAttributesBuilder.rebuildMetricsForSubtree(*this);
    }

    checkLayoutAttributesConsistency(this, m_layoutAttributes);

    // Reduced version of RenderBlock::layoutBlock(), which only takes care of SVG text.
    // All if branches that could cause early exit in RenderBlocks layoutBlock() method are turned into assertions.
    ASSERT(!isInline());
    ASSERT(!scrollsOverflow());
    ASSERT(!hasControlClip());
    ASSERT(!multiColumnFlow());
    ASSERT(!positionedObjects());
    ASSERT(!isAnonymousBlock());
    if (!isLayerBasedSVGEngineEnabled()) {
        ASSERT(!simplifiedLayout());
        ASSERT(!m_overflow);
    }

    // FIXME: We need to find a way to only layout the child boxes, if needed.
    auto layoutChanged = everHadLayout() && selfNeedsLayout();
    auto oldBoundaries = objectBoundingBox();

    if (!firstChild()) {
        updatePositionAndOverflow({ });
        setChildrenInline(true);
    }

    ASSERT(childrenInline());

    LayoutUnit repaintLogicalTop;
    LayoutUnit repaintLogicalBottom;
    rebuildFloatingObjectSetFromIntrudingFloats();
    layoutInlineChildren(true, repaintLogicalTop, repaintLogicalBottom);

    // updatePositionAndOverflow() is called by SVGRootInlineBox, after forceLayoutInlineChildren() ran, before this point is reached.
    if (m_needsReordering)
        m_needsReordering = false;

    if (isLayerBasedSVGEngineEnabled()) {
        updateLayerTransform();
        updateCachedBoundariesInParents = false; // No longer needed for LBSE.
        layoutChanged = false; // No longer needed for LBSE.
    } else {
        if (m_needsTransformUpdate) {
            if (previousReferenceBoxRect != transformReferenceBoxRect())
                m_localTransform = textElement().animatedLocalTransform();
            m_needsTransformUpdate = false;
        }
        if (!updateCachedBoundariesInParents)
            updateCachedBoundariesInParents = oldBoundaries != objectBoundingBox();
    }

    // Invalidate all resources of this client if our layout changed.
    if (layoutChanged)
        SVGResourcesCache::clientLayoutChanged(*this);

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents) {
        if (CheckedPtr parent = this->parent())
            parent->invalidateCachedBoundaries();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
}

bool RenderSVGText::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    ASSERT(!document().settings().layerBasedSVGEngineEnabled());

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, usedPointerEvents());
    if (isVisibleToHitTesting(style(), request) || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style().svgStyle().hasStroke() || !hitRules.requireStroke))
            || (hitRules.canHitFill && (style().svgStyle().hasFill() || !hitRules.requireFill))) {
            static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

            SVGVisitedRendererTracking recursionTracking(s_visitedSet);
            if (recursionTracking.isVisiting(*this))
                return false;

            SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

            FloatPoint localPoint = valueOrDefault(localToParentTransform().inverse()).mapPoint(pointInParent);
            if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
                return false;

            HitTestLocation hitTestLocation(LayoutPoint(flooredIntPoint(localPoint)));
            return RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), hitTestAction);
        }
    }

    return false;
}

bool RenderSVGText::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return RenderSVGBlock::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    auto adjustedLocation = accumulatedOffset + location();

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, style().pointerEvents());
    if (isVisibleToHitTesting(style(), request) || !hitRules.requireVisible) {
        if ((hitRules.canHitStroke && (style().svgStyle().hasStroke() || !hitRules.requireStroke))
        || (hitRules.canHitFill && (style().svgStyle().hasFill() || !hitRules.requireFill))) {
            static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

            SVGVisitedRendererTracking recursionTracking(s_visitedSet);
            if (recursionTracking.isVisiting(*this))
                return false;

            SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

            auto localPoint = locationInContainer.point();
            auto coordinateSystemOriginTranslation = nominalSVGLayoutLocation() - adjustedLocation;
            localPoint.move(coordinateSystemOriginTranslation);

            if (!pointInSVGClippingArea(localPoint))
                return false;

            return RenderBlock::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
        }
    }

    return false;
}

void RenderSVGText::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    applySVGTransform(transform, protectedTextElement(), style, boundingBox, std::nullopt, std::nullopt, options);
}

VisiblePosition RenderSVGText::positionForPoint(const LayoutPoint& pointInContents, HitTestSource source, const RenderFragmentContainer* fragment)
{
    auto* rootBox = legacyRootBox();
    if (!rootBox)
        return createVisiblePosition(0, Affinity::Downstream);

    ASSERT(childrenInline());
    LegacyInlineBox* closestBox = downcast<SVGRootInlineBox>(*rootBox).closestLeafChildForPosition(pointInContents);
    if (!closestBox)
        return createVisiblePosition(0, Affinity::Downstream);

    return closestBox->renderer().positionForPoint({ pointInContents.x(), LayoutUnit(closestBox->y()) }, source, fragment);
}

void RenderSVGText::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        OptionSet<PaintPhase> relevantPaintPhases { PaintPhase::Foreground, PaintPhase::ClippingMask, PaintPhase::Mask, PaintPhase::Outline, PaintPhase::SelfOutline };
        if (!shouldPaintSVGRenderer(paintInfo, relevantPaintPhases))
            return;

        if (paintInfo.phase == PaintPhase::ClippingMask) {
            paintSVGClippingMask(paintInfo, objectBoundingBox());
            return;
        }

        auto adjustedPaintOffset = paintOffset + location();
        if (paintInfo.phase == PaintPhase::Mask) {
            paintSVGMask(paintInfo, adjustedPaintOffset);
            return;
        }

        if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
            RenderBlock::paint(paintInfo, paintOffset);
            return;
        }

        ASSERT(paintInfo.phase == PaintPhase::Foreground);
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        auto coordinateSystemOriginTranslation = adjustedPaintOffset - nominalSVGLayoutLocation();
        paintInfo.context().translate(coordinateSystemOriginTranslation.width(), coordinateSystemOriginTranslation.height());

        RenderBlock::paint(paintInfo, paintOffset);
        return;
    }

    if (paintInfo.context().paintingDisabled())
        return;

    if (paintInfo.phase != PaintPhase::ClippingMask && paintInfo.phase != PaintPhase::Mask && paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Outline && paintInfo.phase != PaintPhase::SelfOutline)
        return;

    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    PaintInfo blockInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(blockInfo.context());
    blockInfo.applyTransform(localToParentTransform());
    RenderBlock::paint(blockInfo, LayoutPoint());

    // Paint the outlines, if any
    if (paintInfo.phase == PaintPhase::Foreground) {
        blockInfo.phase = PaintPhase::SelfOutline;
        RenderBlock::paint(blockInfo, LayoutPoint());
    }
}

FloatRect RenderSVGText::strokeBoundingBox() const
{
    FloatRect strokeBoundaries = objectBoundingBox();
    if (!style().svgStyle().hasStroke())
        return strokeBoundaries;

    Ref textElement = this->textElement();
    SVGLengthContext lengthContext(textElement.ptr());
    strokeBoundaries.inflate(lengthContext.valueForLength(style().strokeWidth()));
    return strokeBoundaries;
}

FloatRect RenderSVGText::repaintRectInLocalCoordinates(RepaintRectCalculation repaintRectCalculation) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        auto repaintRect = SVGBoundingBoxComputation::computeRepaintBoundingBox(*this);

        if (const auto* textShadow = style().textShadow())
            textShadow->adjustRectForShadow(repaintRect);

        return repaintRect;
    }

    FloatRect repaintRect = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(*this, repaintRect, repaintRectCalculation);

    if (const ShadowData* textShadow = style().textShadow())
        textShadow->adjustRectForShadow(repaintRect);

    return repaintRect;
}

void RenderSVGText::updatePositionAndOverflow(const FloatRect& boundaries)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        clearOverflow();

        m_objectBoundingBox = boundaries;

        auto boundingRect = enclosingLayoutRect(m_objectBoundingBox);
        setLocation(boundingRect.location());
        setSize(boundingRect.size());

        auto overflowRect = visualOverflowRectEquivalent();
        if (const auto* textShadow = style().textShadow())
            textShadow->adjustRectForShadow(overflowRect);

        addVisualOverflow(overflowRect);
        return;
    }

    auto boundingRect = enclosingLayoutRect(boundaries);
    setLocation(boundingRect.location());
    setSize(boundingRect.size());
    m_objectBoundingBox = boundingRect;
    ASSERT(m_objectBoundingBox == frameRect());
}

void RenderSVGText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    auto needsTransformUpdate = [&]() {
        if (document().settings().layerBasedSVGEngineEnabled())
            return false;
        if (diff != StyleDifference::Layout)
            return false;

        auto& newStyle = style();
        if (!oldStyle)
            return newStyle.affectsTransform();

        return (oldStyle->affectsTransform() != newStyle.affectsTransform()
            || oldStyle->transform() != newStyle.transform()
            || oldStyle->translate() != newStyle.translate()
            || oldStyle->scale() != newStyle.scale()
            || oldStyle->rotate() != newStyle.rotate()
            || oldStyle->offsetPath() != newStyle.offsetPath());
    };

    if (needsTransformUpdate())
        setNeedsTransformUpdate();

    RenderSVGBlock::styleDidChange(diff, oldStyle);
}

}
