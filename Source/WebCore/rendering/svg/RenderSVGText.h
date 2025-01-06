/*
 * Copyright (C) 2006-2023 Apple Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#pragma once

#include "AffineTransform.h"
#include "RenderSVGBlock.h"
#include "SVGBoundingBoxComputation.h"
#include "SVGTextChunk.h"
#include "SVGTextLayoutAttributesBuilder.h"

namespace WebCore {

namespace InlineIterator {
class InlineBoxIterator;
}

class RenderSVGInlineText;
class SVGRootInlineBox;
class SVGTextElement;
class SVGTextLayoutEngine;

class RenderSVGText final : public RenderSVGBlock {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderSVGText);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSVGText);
public:
    RenderSVGText(SVGTextElement&, RenderStyle&&);
    virtual ~RenderSVGText();

    SVGTextElement& textElement() const;
    Ref<SVGTextElement> protectedTextElement() const;

    bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    void setNeedsPositioningValuesUpdate() { m_needsPositioningValuesUpdate = true; }
    void setNeedsTextMetricsUpdate() { m_needsTextMetricsUpdate = true; }

    // FIXME: [LBSE] Only needed for legacy SVG engine.
    void setNeedsTransformUpdate() override { m_needsTransformUpdate = true; }

    static RenderSVGText* locateRenderSVGTextAncestor(RenderObject&);
    static const RenderSVGText* locateRenderSVGTextAncestor(const RenderObject&);

    bool needsReordering() const { return m_needsReordering; }
    Vector<SVGTextLayoutAttributes*>& layoutAttributes() { return m_layoutAttributes; }

    void subtreeChildWasAdded(RenderObject*);
    void subtreeChildWillBeRemoved(RenderObject*, Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes);
    void subtreeChildWasRemoved(const Vector<SVGTextLayoutAttributes*, 2>& affectedAttributes);
    void subtreeTextDidChange(RenderSVGInlineText*);

    FloatRect objectBoundingBox() const final { return m_objectBoundingBox; }
    FloatRect strokeBoundingBox() const final;
    FloatRect repaintRectInLocalCoordinates(RepaintRectCalculation = RepaintRectCalculation::Fast) const final;

    LayoutRect visualOverflowRectEquivalent() const { return SVGBoundingBoxComputation::computeVisualOverflowRect(*this); }

    void updatePositionAndOverflow(const FloatRect&);

    SVGRootInlineBox* legacyRootBox() const;

private:
    void graphicsElement() const = delete;

    ASCIILiteral renderName() const override { return "RenderSVGText"_s; }

    void paint(PaintInfo&, const LayoutPoint&) override;
    void paintInlineChildren(PaintInfo&, const LayoutPoint&) override;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    bool hitTestInlineChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption>) const final;
    VisiblePosition positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) override;

    bool requiresLayer() const override
    {
        if (document().settings().layerBasedSVGEngineEnabled())
            return true;
        return false;
    }

    void layout() override;

    void computePerCharacterLayoutInformation();
    void layoutCharactersInTextBoxes(const InlineIterator::InlineBoxIterator&, SVGTextLayoutEngine&);
    FloatRect layoutChildBoxes(LegacyInlineFlowBox*, SVGTextFragmentMap&);
    void layoutRootBox(const FloatRect&);
    void reorderValueListsToLogicalOrder();

    void willBeDestroyed() override;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) final;

    // FIXME: [LBSE] Begin code only needed for legacy SVG engine.
    bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction) override;
    const AffineTransform& localToParentTransform() const override { return m_localTransform; }
    AffineTransform localTransform() const override { return m_localTransform; }
    // FIXME: [LBSE] End code only needed for legacy SVG engine.

    bool shouldHandleSubtreeMutations() const;

    bool m_needsReordering : 1 { false };
    bool m_needsPositioningValuesUpdate : 1 { false };
    bool m_needsTransformUpdate : 1 { true }; // FIXME: [LBSE] Only needed for legacy SVG engine.
    bool m_needsTextMetricsUpdate : 1 { false };
    bool m_hasPerformedLayout : 1 { false }; // Needed to distinguish between when we perform a full pass of layout and everHadLayout (which can be set be content visibility for skipped content).
    AffineTransform m_localTransform; // FIXME: [LBSE] Only needed for legacy SVG engine.
    SVGTextLayoutAttributesBuilder m_layoutAttributesBuilder;
    Vector<SVGTextLayoutAttributes*> m_layoutAttributes;
    FloatRect m_objectBoundingBox;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSVGText, isRenderSVGText())
