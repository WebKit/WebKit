/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include "AXObjectCache.h"
#include "FloatQuad.h"
#include "Font.h"
#include "FontCascadeInlines.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorSVGTextBox.h"
#include "InlineWalker.h"
#include "LayoutIntegrationLineLayout.h"
#include "LayoutRepainter.h"
#include "LegacyRenderSVGResource.h"
#include "LegacyRenderSVGRoot.h"
#include "PointerEventsHitRules.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderSVGBlockInlines.h"
#include "RenderSVGInline.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGModelObject.h"
#include "RenderSVGRoot.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLengthList.h"
#include "SVGRenderingContext.h"
#include "SVGResourcesCache.h"
#include "SVGTextBoxPainter.h"
#include "SVGTextElement.h"
#include "SVGTextFragment.h"
#include "SVGTextLayoutAlgorithm.h"
#include "SVGURIReference.h"
#include "SVGVisitedRendererTracking.h"
#include "Settings.h"
#include "StyleTextShadow.h"
#include "StyleTransformResolver.h"
#include "TransformState.h"
#include "VisiblePosition.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGText);

RenderSVGText::RenderSVGText(SVGTextElement& element, Style::ComputedStyle&& style)
    : RenderSVGBlock(Type::SVGText, element, WTF::move(style))
{
    ASSERT(isRenderSVGText());
}

RenderSVGText::~RenderSVGText() = default;

SVGTextElement& RenderSVGText::textElement() const
{
    return downcast<SVGTextElement>(RenderSVGBlock::graphicsElement());
}

bool RenderSVGText::isChildAllowed(const RenderObject& child, const Style::ComputedStyle&) const
{
    auto isEmptySVGInlineText = [](const RenderObject* object) {
        const auto svgInlineText = dynamicDowncast<RenderSVGInlineText>(object);
        return svgInlineText && svgInlineText->hasEmptyText();
    };

    return child.isInline() && !isEmptySVGInlineText(&child);
}

RenderSVGText* RenderSVGText::locateRenderSVGTextAncestor(RenderObject& start)
{
    return lineageOfType<RenderSVGText>(start).first();
}

const RenderSVGText* RenderSVGText::locateRenderSVGTextAncestor(const RenderObject& start)
{
    return lineageOfType<RenderSVGText>(start).first();
}

static inline void updateFontInAllDescendants(RenderSVGText& text)
{
    for (RenderObject* descendant = &text; descendant; descendant = descendant->nextInPreOrder(&text)) {
        if (CheckedPtr text = dynamicDowncast<RenderSVGInlineText>(*descendant))
            text->updateScaledFont();
    }
}

AffineTransform RenderSVGText::computeLocalTransform() const
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    TransformationMatrix transform;
    applyTransform(transform, style(), transformReferenceBoxRect(style()), Style::TransformResolver::allTransformOperations);
    return transform.toAffineTransform();
}

void RenderSVGText::updateLocalTransform()
{
    m_localTransform = computeLocalTransform();
}

void RenderSVGText::layout()
{
    auto isLayerBasedSVGEngineEnabled = [&]() {
        return document().settings().layerBasedSVGEngineEnabled();
    };

    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    auto checkForRepaintOverride = !isLayerBasedSVGEngineEnabled() ? std::make_optional(SVGRenderSupport::checkForSVGRepaintDuringLayout(*this)) : std::nullopt;
    LayoutRepainter repainter(*this, checkForRepaintOverride);

    bool updateCachedBoundariesInParents = false;
    auto previousReferenceBoxRect = transformReferenceBoxRect();

    // We update the transform now because updateScaledFont() needs it, but we do it a second time at the end of the layout,
    // since the transform reference box may change because of the font change.
    if (!isLayerBasedSVGEngineEnabled() && m_needsTransformUpdate) {
        m_localTransform = protect(textElement())->animatedLocalTransform();
        updateCachedBoundariesInParents = true;
    }

    auto isLayoutSizeChanged = [&] {
        if (auto* legacyRootObject = lineageOfType<LegacyRenderSVGRoot>(*this).first())
            return legacyRootObject->isLayoutSizeChanged();
        if (auto* rootObject = lineageOfType<RenderSVGRoot>(*this).first())
            return rootObject->isLayoutSizeChanged();
        return false;
    };

    if (!m_hasPerformedLayout || m_needsTextMetricsUpdate || isLayoutSizeChanged() || m_needsTransformUpdate) {
        // Text renderers remeasure if the on-screen font changed.
        updateFontInAllDescendants(*this);
        updateCachedBoundariesInParents = true;
    }
    if (m_needsPositioningValuesUpdate)
        updateCachedBoundariesInParents = true;
    m_needsTextMetricsUpdate = false;
    m_needsPositioningValuesUpdate = false;

    // Reduced version of RenderBlock::layoutBlock(), which only takes care of SVG text.
    // All if branches that could cause early exit in RenderBlocks layoutBlock() method are turned into assertions.
    ASSERT(!isInline());
    ASSERT(!scrollsOverflow());
    ASSERT(!hasControlClip());
    ASSERT(!multiColumnFlow());
    ASSERT(!outOfFlowBoxes());
    ASSERT(!isAnonymousBlock());
    if (!isLayerBasedSVGEngineEnabled()) {
        ASSERT(!simplifiedLayout());
        ASSERT(!m_overflow);
    }

    // FIXME: We need to find a way to only layout the child boxes, if needed.
    auto layoutChanged = m_hasPerformedLayout && selfNeedsLayout();
    auto oldBoundaries = objectBoundingBox();

    if (!firstChild()) {
        updatePositionAndOverflow({ });
        setChildrenInline(true);
    }

    ASSERT(childrenInline());

    rebuildFloatingObjectSetFromIntrudingFloats();
    if (!layoutInlineChildrenWithoutLineLayout()) {
        LayoutUnit repaintLogicalTop;
        LayoutUnit repaintLogicalBottom;
        layoutInlineChildren(RelayoutChildren::Yes, logicalHeight(), repaintLogicalTop, repaintLogicalBottom);
    }

    computePerCharacterLayoutInformation();

    if (isLayerBasedSVGEngineEnabled()) {
        updateLayerTransform();
        // Non-layered text caches its transform in m_localTransform (read via localTransform()
        // by getCTM()/getScreenCTM(), hit-testing and the transform-recursion paint path),
        // mirroring RenderSVGModelObject::updateLocalTransform(). Layered text uses its RenderLayer.
        if (!hasLayer())
            updateLocalTransform();
        updateCachedBoundariesInParents = false; // No longer needed for LBSE.
        layoutChanged = false; // No longer needed for LBSE.
    } else {
        if (m_needsTransformUpdate) {
            if (previousReferenceBoxRect != transformReferenceBoxRect())
                m_localTransform = protect(textElement())->animatedLocalTransform();
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
        if (auto* parent = this->parent())
            parent->invalidateCachedBoundaries();
    }

    repainter.repaintAfterLayout();
    clearNeedsLayout();
    m_hasPerformedLayout = true;
}

bool RenderSVGText::layoutInlineChildrenWithoutLineLayout()
{
    // Empty content without a line needs no line layout.
    if (!firstChild() && !hasLineIfEmpty())
        return false;

    computeAndSetLineLayoutPath();

    auto& inlineLayout = ensureInlineLayout();
    if (!inlineLayout.layoutSVGText())
        return false;

    CheckedPtr cache = document().existingAXObjectCache();
    for (auto walker = InlineWalker(*this); !walker.atEnd(); walker.advance()) {
        auto& renderer = *walker.current();
        ASSERT(renderer.isInlineBox() || is<RenderText>(renderer));
        renderer.clearNeedsLayout();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        if (cache)
            cache->onTextRunsChanged(renderer);
#endif
    }

    // Only content without a contentful line keeps this height. updatePositionAndOverflow() sets the geometry otherwise.
    auto contentLogicalHeight = [&] -> LayoutUnit {
        if (inlineLayout.hasContentfulInlineLine())
            return inlineLayout.contentLogicalHeight();
        if (hasLineIfEmpty())
            return lineHeight();
        return { };
    };
    setLogicalHeight(borderAndPaddingLogicalHeight() + contentLogicalHeight());

    // Makes LayoutRepainter issue a full repaint.
    setNeedsLayout(MarkingBehavior::MarkOnlyThis);

    if (cache)
        cache->onLaidOutInlineContent(*this);
    return true;
}

void RenderSVGText::computePerCharacterLayoutInformation()
{
    auto hasSVGContent = inlineLayout() && inlineLayout()->hasContentfulInlineLine();
    if (!hasSVGContent)
        return;

    CheckedRef lineLayout = *inlineLayout();
    auto [boxes, fragmentsForBoxes] = lineLayout->resetSVGTextFragments();
    ASSERT(!boxes.isEmpty() && boxes[0].isRootInlineBox());

    SVGTextLayoutAlgorithm { *layoutBox(), boxes, fragmentsForBoxes }.layout();
    updatePositionAndOverflow(lineLayout->applySVGTextFragments());
}

bool RenderSVGText::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    ASSERT(!document().settings().layerBasedSVGEngineEnabled());

    // <text> has no background to hit-test; only the painted glyphs in the foreground phase are hittable.
    if (hitTestAction != HitTestAction::Foreground)
        return false;

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    FloatPoint localPoint = valueOrDefault(localToParentTransform().inverse()).mapPoint(pointInParent);
    if (!SVGRenderSupport::pointInClippingArea(*this, localPoint))
        return false;

    HitTestLocation hitTestLocation { localPoint };

    // Descend unconditionally: hitTestInlineChildren enforces per-tspan fill/stroke/visibility,
    // so <text fill=none>/visibility=hidden must not gate out differently-styled descendants.
    if (RenderBlock::nodeAtPoint(request, result, hitTestLocation, LayoutPoint(), hitTestAction))
        return true;

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, usedPointerEvents());
    if (hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint)) {
        updateHitTestResult(result, LayoutPoint { localPoint });
        if (result.addNodeToListBasedTestResult(protect(nodeForHitTest()).get(), request, hitTestLocation) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

bool RenderSVGText::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!document().settings().layerBasedSVGEngineEnabled())
        return RenderSVGBlock::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);

    auto adjustedLocation = accumulatedOffset + location();

    // SVG text is hit per glyph in the foreground phase only, like RenderSVGShape.
    if (hitTestAction != HitTestAction::Foreground)
        return false;

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*this))
        return false;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *this);

    // The inline text fragments are positioned in this element's SVG coordinate system, so
    // the query point and the offset that places the line boxes must both be expressed there.
    // coordinateSystemOriginTranslation maps the caller's space into it, and is non-zero for
    // a text layer whose origin differs from the SVG coordinate system. Shift both the
    // location and the offset by it.
    auto coordinateSystemOriginTranslation = nominalSVGLayoutLocation() - adjustedLocation;
    HitTestLocation localLocation(locationInContainer, coordinateSystemOriginTranslation);

    // Use the unrounded point so hits on fractional-size glyphs are not lost to flooring.
    auto localPoint = localLocation.transformedPoint();
    if (!pointInSVGClippingArea(localPoint))
        return false;

    // Descend unconditionally: hitTestInlineChildren enforces per-tspan fill/stroke/visibility,
    // so <text fill=none>/visibility=hidden must not gate out differently-styled descendants.
    if (RenderBlock::nodeAtPoint(request, result, localLocation, accumulatedOffset + coordinateSystemOriginTranslation, hitTestAction))
        return true;

    PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, usedPointerEvents());
    if (hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint)) {
        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (result.addNodeToListBasedTestResult(protect(nodeForHitTest()).get(), request, locationInContainer, objectBoundingBox()) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

bool RenderSVGText::hitTestInlineChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction)
{
    auto hitTestInlineBoxes = [&] {
        for (auto boxIterator = InlineIterator::lastBoxFor(*this); boxIterator; --boxIterator) {
            if (!boxIterator->isSVGText())
                continue;
            auto textBox = downcast<InlineIterator::SVGTextBox>(*boxIterator);

            PointerEventsHitRules hitRules(PointerEventsHitRules::HitTestingTargetType::SVGText, request, usedPointerEvents());

            auto& renderer = textBox.renderer();
            if (!request.isVisibleForStyle(renderer.usedStyle()) && hitRules.requireVisible)
                continue;

            bool hitsStroke = hitRules.canHitStroke && (!renderer.style().stroke().isNone() || !hitRules.requireStroke);
            bool hitsFill = hitRules.canHitFill && (!renderer.style().fill().isNone() || !hitRules.requireFill);
            if (!hitsStroke && !hitsFill)
                continue;

            auto rect = textBox.calculateBoundariesIncludingSVGTransform();
            if (!locationInContainer.intersects(rect))
                continue;

            float scalingFactor = renderer.scalingFactor();
            ASSERT(scalingFactor);

            float baseline = renderer.scaledFont().metricsOfPrimaryFont().ascent() / scalingFactor;

            AffineTransform fragmentTransform;
            for (auto& fragment : textBox.textFragments()) {
                FloatQuad fragmentQuad(FloatRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height));
                fragment.buildFragmentTransform(fragmentTransform);
                if (!fragmentTransform.isIdentity())
                    fragmentQuad = fragmentTransform.mapQuad(fragmentQuad);

                if (!fragmentQuad.containsPoint(locationInContainer.transformedPoint()))
                    continue;

                renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                if (result.addNodeToListBasedTestResult(protect(renderer.nodeForHitTest()).get(), request, locationInContainer, rect) == HitTestProgress::Stop)
                    return true;
            }
        }
        return false;
    };


    if (hitTestInlineBoxes()) {
        updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        return true;
    }

    return false;
}

void RenderSVGText::applyTransform(TransformationMatrix& transform, const Style::ComputedStyle& style, const FloatRect& boundingBox, OptionSet<Style::TransformResolverOption> options) const
{
    ASSERT(document().settings().layerBasedSVGEngineEnabled());
    applySVGTransform(transform, protect(textElement()), style, boundingBox, std::nullopt, std::nullopt, options);
}

PositionWithAffinity RenderSVGText::positionForPoint(const LayoutPoint& pointInContents, HitTestSource source, const RenderFragmentContainer* fragment)
{
    InlineIterator::BoxIterator closestBox;
    InlineIterator::BoxIterator lastBox;
    for (auto& box : InlineIterator::boxesFor(*this)) {
        auto* textBox = dynamicDowncast<InlineIterator::SVGTextBox>(box);
        if (!textBox)
            continue;
        lastBox = *textBox;
        auto rect = textBox->visualRectIgnoringBlockDirection();
        if (pointInContents.y() < rect.y())
            continue;
        if (pointInContents.y() > rect.maxY())
            continue;
        closestBox = *textBox;
        if (pointInContents.x() < rect.maxX())
            break;
    }

    if (!closestBox)
        closestBox = lastBox;

    if (!closestBox)
        return createPositionWithAffinity(0, Affinity::Downstream);

    return const_cast<RenderObject&>(closestBox->renderer()).positionForPoint({ pointInContents.x(), LayoutUnit(closestBox->visualRectIgnoringBlockDirection().y()) }, source, fragment);
}

bool RenderSVGText::requiresLayer() const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (document().settings().layerBasedSVGEngineForceLayerCreationEnabled())
            return true;
        return requiresLayerForSVGIntrinsicReasons();
    }
    return false;
}

void RenderSVGText::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (paintInfo.phase == PaintPhase::EventRegion) {
            paintSVGEventRegion(paintInfo, paintOffset);
            return;
        }

        OptionSet<PaintPhase> relevantPaintPhases { PaintPhase::Foreground, PaintPhase::ClippingMask, PaintPhase::Mask, PaintPhase::Outline, PaintPhase::SelfOutline };
        if (!shouldPaintSVGContainerRenderer(paintInfo, relevantPaintPhases))
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

        GraphicsContextStateSaver stateSaver(paintInfo.context());

        if (paintInfo.phase == PaintPhase::Outline || paintInfo.phase == PaintPhase::SelfOutline) {
            paintInfo.context().resetClip();
            RenderBlock::paint(paintInfo, paintOffset);
            return;
        }

        ASSERT(paintInfo.phase == PaintPhase::Foreground);

        auto coordinateSystemOriginTranslation = adjustedPaintOffset - nominalSVGLayoutLocation();
        paintInfo.context().translate(coordinateSystemOriginTranslation.width(), coordinateSystemOriginTranslation.height());

        RenderBlock::paint(paintInfo, paintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhase::EventRegion) {
        if (usedStyle().visibility() == UsedVisibility::Hidden || m_objectBoundingBox.isEmpty())
            return;

        paintInfo.eventRegionContext()->unite(FloatRoundedRect(strokeBoundingBox()), *this, style(), false);
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

void RenderSVGText::paintInlineChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!paintInfo.context().paintingDisabled());

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection)
        return;

    bool isPrinting = document().printing();
    bool hasSelection = !isPrinting && selectionState() != RenderObject::HighlightState::None;
    bool shouldPaintSelectionHighlight = !(paintInfo.paintBehavior.contains(PaintBehavior::SkipSelectionHighlight));

    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.updateSubtreePaintRootForChildren(this);

    auto boxes = InlineIterator::boxesFor(*this);

    if (hasSelection && shouldPaintSelectionHighlight) {
        for (auto& box : boxes) {
            if (auto* textBox = dynamicDowncast<InlineIterator::SVGTextBox>(box)) {
                SVGTextBoxPainter painter(textBox->inlineContent(), textBox->boxIndex(), paintInfo, paintOffset);
                painter.paintSelectionBackground();
            }
        }
    }

    Vector<SVGRenderingContext> contextStack;

    for (auto box = boxes.begin(); box != boxes.end();) {
        while (!contextStack.isEmpty() && contextStack.last().renderer() != box->renderer().parent())
            contextStack.removeLast();

        if (box->isSVGText()) {
            SVGTextBoxPainter painter(box->inlineContent(), box->boxIndex(), paintInfo, paintOffset);
            painter.paint();
        } else {
            auto* renderer = dynamicDowncast<RenderElement>(box->renderer());
            contextStack.append({ const_cast<RenderElement&>(*renderer), paintInfo, SVGRenderingContext::SaveGraphicsContext });

            // FIXME: find out if we can skip children too in LBSE case.
            if (!document().settings().layerBasedSVGEngineEnabled()) {
                if (!contextStack.last().isRenderingPrepared() || renderer->hasSelfPaintingLayer()) {
                    box.traverseLineRightwardOnLineSkippingChildren();
                    continue;
                }
            }
        }

        box.traverseLineRightwardOnLine();
    }

    while (!contextStack.isEmpty())
        contextStack.removeLast();
}

FloatRect RenderSVGText::strokeBoundingBox() const
{
    FloatRect strokeBoundaries = objectBoundingBox();
    if (style().stroke().isNone())
        return strokeBoundaries;

    Ref textElement = this->textElement();
    SVGLengthContext lengthContext(textElement.ptr());
    auto strokeWidth = lengthContext.valueForLength(style().strokeWidth(), style().usedZoomForLength());
    if (strokeWidth <= 0)
        return strokeBoundaries;

    strokeBoundaries.inflate(strokeInflation(strokeWidth));
    return strokeBoundaries;
}

float RenderSVGText::strokeInflation(float strokeWidth) const
{
    // LBSE tracks non scaling strokes in the subtree, to invalidate cached boxes on ancestor transform
    // changes, so without the flag the stroke width applies as is. The legacy engine relayouts the text
    // on those changes and keeps no such flag: it always has to inspect the runs.
    if (document().settings().layerBasedSVGEngineEnabled() && !mayHaveNonScalingStrokeInSubtree())
        return strokeWidth;

    // vector-effect is not inherited, so a run may or may not use a non scaling stroke. SVGTextBoxPainter
    // decides per run, using the style of the run's parent renderer: do the same, and keep the widest inflation.
    auto zoomAndDeviceScale = style().usedZoom() * document().deviceScaleFactor();
    std::optional<float> inflation;
    for (const RenderObject* descendant = this; descendant; descendant = descendant->nextInPreOrder(this)) {
        CheckedPtr inlineText = dynamicDowncast<RenderSVGInlineText>(*descendant);
        if (!inlineText)
            continue;

        auto runInflation = strokeWidth;
        if (CheckedPtr runRenderer = inlineText->parent(); runRenderer && runRenderer->style().vectorEffect() == VectorEffect::NonScalingStroke) {
            if (auto scalingFactor = RenderSVGInlineText::computeScalingFactorForRenderer(*runRenderer); scalingFactor > 0)
                runInflation = strokeWidth * zoomAndDeviceScale / scalingFactor;
        }

        inflation = std::max(inflation.value_or(runInflation), runInflation);
    }

    return inflation.value_or(strokeWidth);
}

void RenderSVGText::updateFromStyle()
{
    RenderSVGBlock::updateFromStyle();

    if (style().vectorEffect() == VectorEffect::NonScalingStroke && document().settings().layerBasedSVGEngineEnabled())
        setMayHaveNonScalingStrokeInSubtreeIncludingAncestors();
}

void RenderSVGText::invalidateNonScalingStrokeCaches()
{
    if (!mayHaveNonScalingStrokeInSubtree() || !m_hasPerformedLayout)
        return;

    invalidateCachedVisualOverflowRect();
    updateVisualOverflow();
}

FloatRect RenderSVGText::decoratedBoundingBox() const
{
    return SVGBoundingBoxComputation::computeDecoratedBoundingBox(*this);
}

FloatRect RenderSVGText::repaintRectInLocalCoordinates(RepaintRectCalculation repaintRectCalculation) const
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        auto repaintRect = SVGBoundingBoxComputation::computeRepaintBoundingBox(*this);

        if (auto& textShadow = style().textShadow(); !textShadow.isNone())
            Style::adjustRectForShadow(repaintRect, textShadow, style().usedZoomForLength());

        return repaintRect;
    }

    auto repaintRect = strokeBoundingBox();
    SVGRenderSupport::intersectRepaintRectWithResources(*this, repaintRect, repaintRectCalculation);

    if (auto& textShadow = style().textShadow(); !textShadow.isNone())
        Style::adjustRectForShadow(repaintRect, textShadow, style().usedZoomForLength());

    return repaintRect;
}

void RenderSVGText::updatePositionAndOverflow(const FloatRect& boundaries)
{
    if (document().settings().layerBasedSVGEngineEnabled()) {
        m_objectBoundingBox = boundaries;
        m_cachedVisualOverflowRect = std::nullopt;

        setLocation(SVGContainerLayout::layoutLocationRelativeToContainer(*this));
        setBorderBoxSize(enclosingLayoutRect(m_objectBoundingBox).size());

        updateVisualOverflow();
        return;
    }

    auto boundingRect = enclosingLayoutRect(boundaries);
    setLocation(boundingRect.location());
    setBorderBoxSize(boundingRect.size());
    m_objectBoundingBox = boundingRect;
    ASSERT(m_objectBoundingBox == borderBoxRectInContainer());
}

void RenderSVGText::updateVisualOverflow()
{
    clearOverflow();

    auto overflowRect = visualOverflowRectEquivalent();
    if (auto& textShadow = style().textShadow(); !textShadow.isNone())
        Style::adjustRectForShadow(overflowRect, textShadow, style().usedZoomForLength());

    addVisualOverflow(overflowRect);
}

void RenderSVGText::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    auto needsTransformUpdate = [&]() {
        if (document().settings().layerBasedSVGEngineEnabled())
            return false;
        if (diff != Style::DifferenceResult::Layout)
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

bool RenderSVGText::isObjectBoundingBoxValid() const
{
    // If we don't have any line boxes, then consider the bbox invalid.
    return !!InlineIterator::firstRootInlineBoxFor(*this);
}

}
