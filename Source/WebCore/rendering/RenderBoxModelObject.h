/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#pragma once

#include "FontBaseline.h"
#include "LayoutRect.h"
#include "RenderLayerModelObject.h"

namespace WebCore {

// Modes for some of the line-related functions.
enum LinePositionMode { PositionOnContainingLine, PositionOfInteriorLineBoxes };
enum LineDirectionMode { HorizontalLine, VerticalLine };
typedef unsigned BorderEdgeFlags;

enum BackgroundBleedAvoidance {
    BackgroundBleedNone,
    BackgroundBleedShrinkBackground,
    BackgroundBleedUseTransparencyLayer,
    BackgroundBleedBackgroundOverBorder
};

enum BaseBackgroundColorUsage {
    BaseBackgroundColorUse,
    BaseBackgroundColorOnly,
    BaseBackgroundColorSkip
};

enum ContentChangeType {
    ImageChanged,
    MaskImageChanged,
    BackgroundImageChanged,
    CanvasChanged,
    CanvasPixelsChanged,
    VideoChanged,
    FullScreenChanged
};

class BorderEdge;
class ImageBuffer;
class InlineFlowBox;
class KeyframeList;
class RenderTextFragment;
class StickyPositionViewportConstraints;

class BackgroundImageGeometry {
public:
    BackgroundImageGeometry(const LayoutRect& destinationRect, const LayoutSize& tile, const LayoutSize& phase, const LayoutSize& space, bool fixedAttachment)
        : m_destRect(destinationRect)
        , m_destOrigin(m_destRect.location())
        , m_tileSize(tile)
        , m_phase(phase)
        , m_space(space)
        , m_hasNonLocalGeometry(fixedAttachment)
    {
    }

    LayoutRect destRect() const { return m_destRect; }
    LayoutSize phase() const { return m_phase; }
    LayoutSize tileSize() const { return m_tileSize; }
    LayoutSize spaceSize() const { return m_space; }
    bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

    LayoutSize relativePhase() const
    {
        LayoutSize phase = m_phase;
        phase += m_destRect.location() - m_destOrigin;
        return phase;
    }

    void clip(const LayoutRect& clipRect) { m_destRect.intersect(clipRect); }

private:
    LayoutRect m_destRect;
    LayoutPoint m_destOrigin;
    LayoutSize m_tileSize;
    LayoutSize m_phase;
    LayoutSize m_space;
    bool m_hasNonLocalGeometry; // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
};

// This class is the base for all objects that adhere to the CSS box model as described
// at http://www.w3.org/TR/CSS21/box.html

class RenderBoxModelObject : public RenderLayerModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderBoxModelObject);
public:
    virtual ~RenderBoxModelObject();
    
    LayoutSize relativePositionOffset() const;
    LayoutSize relativePositionLogicalOffset() const { return style().isHorizontalWritingMode() ? relativePositionOffset() : relativePositionOffset().transposedSize(); }

    FloatRect constrainingRectForStickyPosition() const;
    void computeStickyPositionConstraints(StickyPositionViewportConstraints&, const FloatRect& constrainingRect) const;
    LayoutSize stickyPositionOffset() const;
    LayoutSize stickyPositionLogicalOffset() const { return style().isHorizontalWritingMode() ? stickyPositionOffset() : stickyPositionOffset().transposedSize(); }

    LayoutSize offsetForInFlowPosition() const;

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (RenderFlow)
    // to return the remaining width on a given line (and the height of a single line).
    virtual LayoutUnit offsetLeft() const;
    virtual LayoutUnit offsetTop() const;
    virtual LayoutUnit offsetWidth() const = 0;
    virtual LayoutUnit offsetHeight() const = 0;

    void updateFromStyle() override;

    bool requiresLayer() const override { return isDocumentElementRenderer() || isPositioned() || createsGroup() || hasClipPath() || hasTransformRelatedProperty() || hasHiddenBackface() || hasReflection(); }

    // This will work on inlines to return the bounding box of all of the lines' border boxes.
    virtual LayoutRect borderBoundingBox() const = 0;

    // These return the CSS computed padding values.
    LayoutUnit computedCSSPaddingTop() const { return computedCSSPadding(style().paddingTop()); }
    LayoutUnit computedCSSPaddingBottom() const { return computedCSSPadding(style().paddingBottom()); }
    LayoutUnit computedCSSPaddingLeft() const { return computedCSSPadding(style().paddingLeft()); }
    LayoutUnit computedCSSPaddingRight() const { return computedCSSPadding(style().paddingRight()); }
    LayoutUnit computedCSSPaddingBefore() const { return computedCSSPadding(style().paddingBefore()); }
    LayoutUnit computedCSSPaddingAfter() const { return computedCSSPadding(style().paddingAfter()); }
    LayoutUnit computedCSSPaddingStart() const { return computedCSSPadding(style().paddingStart()); }
    LayoutUnit computedCSSPaddingEnd() const { return computedCSSPadding(style().paddingEnd()); }

    // These functions are used during layout. Table cells and the MathML
    // code override them to include some extra intrinsic padding.
    virtual LayoutUnit paddingTop() const { return computedCSSPaddingTop(); }
    virtual LayoutUnit paddingBottom() const { return computedCSSPaddingBottom(); }
    virtual LayoutUnit paddingLeft() const { return computedCSSPaddingLeft(); }
    virtual LayoutUnit paddingRight() const { return computedCSSPaddingRight(); }
    virtual LayoutUnit paddingBefore() const { return computedCSSPaddingBefore(); }
    virtual LayoutUnit paddingAfter() const { return computedCSSPaddingAfter(); }
    virtual LayoutUnit paddingStart() const { return computedCSSPaddingStart(); }
    virtual LayoutUnit paddingEnd() const { return computedCSSPaddingEnd(); }

    virtual LayoutUnit borderTop() const { return style().borderTopWidth(); }
    virtual LayoutUnit borderBottom() const { return style().borderBottomWidth(); }
    virtual LayoutUnit borderLeft() const { return style().borderLeftWidth(); }
    virtual LayoutUnit borderRight() const { return style().borderRightWidth(); }
    virtual LayoutUnit horizontalBorderExtent() const { return borderLeft() + borderRight(); }
    virtual LayoutUnit verticalBorderExtent() const { return borderTop() + borderBottom(); }
    virtual LayoutUnit borderBefore() const { return style().borderBeforeWidth(); }
    virtual LayoutUnit borderAfter() const { return style().borderAfterWidth(); }
    virtual LayoutUnit borderStart() const { return style().borderStartWidth(); }
    virtual LayoutUnit borderEnd() const { return style().borderEndWidth(); }

    LayoutUnit borderAndPaddingStart() const { return borderStart() + paddingStart(); }
    LayoutUnit borderAndPaddingBefore() const { return borderBefore() + paddingBefore(); }
    LayoutUnit borderAndPaddingAfter() const { return borderAfter() + paddingAfter(); }

    LayoutUnit verticalBorderAndPaddingExtent() const { return borderTop() + borderBottom() + paddingTop() + paddingBottom(); }
    LayoutUnit horizontalBorderAndPaddingExtent() const { return borderLeft() + borderRight() + paddingLeft() + paddingRight(); }
    LayoutUnit borderAndPaddingLogicalHeight() const { return borderAndPaddingBefore() + borderAndPaddingAfter(); }
    LayoutUnit borderAndPaddingLogicalWidth() const { return borderStart() + borderEnd() + paddingStart() + paddingEnd(); }
    LayoutUnit borderAndPaddingLogicalLeft() const { return style().isHorizontalWritingMode() ? borderLeft() + paddingLeft() : borderTop() + paddingTop(); }

    LayoutUnit borderLogicalLeft() const { return style().isHorizontalWritingMode() ? borderLeft() : borderTop(); }
    LayoutUnit borderLogicalRight() const { return style().isHorizontalWritingMode() ? borderRight() : borderBottom(); }
    LayoutUnit borderLogicalWidth() const { return borderStart() + borderEnd(); }
    LayoutUnit borderLogicalHeight() const { return borderBefore() + borderAfter(); }

    LayoutUnit paddingLogicalLeft() const { return style().isHorizontalWritingMode() ? paddingLeft() : paddingTop(); }
    LayoutUnit paddingLogicalRight() const { return style().isHorizontalWritingMode() ? paddingRight() : paddingBottom(); }
    LayoutUnit paddingLogicalWidth() const { return paddingStart() + paddingEnd(); }
    LayoutUnit paddingLogicalHeight() const { return paddingBefore() + paddingAfter(); }

    virtual LayoutUnit marginTop() const = 0;
    virtual LayoutUnit marginBottom() const = 0;
    virtual LayoutUnit marginLeft() const = 0;
    virtual LayoutUnit marginRight() const = 0;
    virtual LayoutUnit marginBefore(const RenderStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginAfter(const RenderStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginStart(const RenderStyle* otherStyle = nullptr) const = 0;
    virtual LayoutUnit marginEnd(const RenderStyle* otherStyle = nullptr) const = 0;
    LayoutUnit verticalMarginExtent() const { return marginTop() + marginBottom(); }
    LayoutUnit horizontalMarginExtent() const { return marginLeft() + marginRight(); }
    LayoutUnit marginLogicalHeight() const { return marginBefore() + marginAfter(); }
    LayoutUnit marginLogicalWidth() const { return marginStart() + marginEnd(); }

    bool hasInlineDirectionBordersPaddingOrMargin() const { return hasInlineDirectionBordersOrPadding() || marginStart()|| marginEnd(); }
    bool hasInlineDirectionBordersOrPadding() const { return borderStart() || borderEnd() || paddingStart()|| paddingEnd(); }

    virtual LayoutUnit containingBlockLogicalWidthForContent() const;

    void paintBorder(const PaintInfo&, const LayoutRect&, const RenderStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    bool paintNinePieceImage(GraphicsContext&, const LayoutRect&, const RenderStyle&, const NinePieceImage&, CompositeOperator = CompositeSourceOver);
    void paintBoxShadow(const PaintInfo&, const LayoutRect&, const RenderStyle&, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    void paintFillLayerExtended(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox* = nullptr, const LayoutSize& = LayoutSize(), CompositeOperator = CompositeSourceOver, RenderElement* backgroundObject = nullptr, BaseBackgroundColorUsage = BaseBackgroundColorUse);

    virtual bool boxShadowShouldBeAppliedToBackground(const LayoutPoint& absolutePaintPostion, BackgroundBleedAvoidance, InlineFlowBox* = nullptr) const;

    // Overridden by subclasses to determine line height and baseline position.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;

    void mapAbsoluteToLocalPoint(MapCoordinatesFlags, TransformState&) const override;

    void setSelectionState(SelectionState) override;

    bool canHaveBoxInfoInFragment() const { return !isFloating() && !isReplaced() && !isInline() && !isTableCell() && isRenderBlock() && !isRenderSVGBlock(); }

    void getGeometryForBackgroundImage(const RenderLayerModelObject* paintContainer, const LayoutPoint& paintOffset, FloatRect& destRect, FloatSize& phase, FloatSize& tileSize) const;
    void contentChanged(ContentChangeType);
    bool hasAcceleratedCompositing() const;

    bool startTransition(double, CSSPropertyID, const RenderStyle* fromStyle, const RenderStyle* toStyle);
    void transitionPaused(double timeOffset, CSSPropertyID);
    void transitionFinished(CSSPropertyID);

    bool startAnimation(double timeOffset, const Animation*, const KeyframeList& keyframes);
    void animationPaused(double timeOffset, const String& name);
    void animationSeeked(double timeOffset, const String& name);
    void animationFinished(const String& name);

    void suspendAnimations(MonotonicTime = MonotonicTime());

    RenderBoxModelObject* continuation() const;
    WEBCORE_EXPORT RenderInline* inlineContinuation() const;
    
    void insertIntoContinuationChainAfter(RenderBoxModelObject&);
    void removeFromContinuationChain();

    virtual LayoutRect paintRectToClipOutFromBorder(const LayoutRect&) { return LayoutRect(); };

    bool hasRunningAcceleratedAnimations() const;
    
protected:
    RenderBoxModelObject(Element&, RenderStyle&&, BaseTypeFlags);
    RenderBoxModelObject(Document&, RenderStyle&&, BaseTypeFlags);

    void willBeDestroyed() override;

    LayoutPoint adjustedPositionRelativeToOffsetParent(const LayoutPoint&) const;

    bool hasVisibleBoxDecorationStyle() const;
    BackgroundImageGeometry calculateBackgroundImageGeometry(const RenderLayerModelObject* paintContainer, const FillLayer&, const LayoutPoint& paintOffset,
        const LayoutRect& paintRect, RenderElement* = nullptr) const;
    bool borderObscuresBackgroundEdge(const FloatSize& contextScale) const;
    bool borderObscuresBackground() const;
    RoundedRect backgroundRoundedRectAdjustedForBleedAvoidance(const GraphicsContext&, const LayoutRect&, BackgroundBleedAvoidance, InlineFlowBox*, const LayoutSize&, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;
    LayoutRect borderInnerRectAdjustedForBleedAvoidance(const GraphicsContext&, const LayoutRect&, BackgroundBleedAvoidance) const;

    InterpolationQuality chooseInterpolationQuality(GraphicsContext&, Image&, const void*, const LayoutSize&);

    LayoutRect localCaretRectForEmptyElement(LayoutUnit width, LayoutUnit textIndentOffset);

    static bool shouldAntialiasLines(GraphicsContext&);

    static void clipRoundedInnerRect(GraphicsContext&, const FloatRect&, const FloatRoundedRect& clipRect);

    bool hasAutoHeightOrContainingBlockWithAutoHeight() const;

    DecodingMode decodingModeForImageDraw(const Image&, const PaintInfo&) const;

public:
    // For RenderBlocks and RenderInlines with m_style->styleType() == PseudoId::FirstLetter, this tracks their remaining text fragments
    RenderTextFragment* firstLetterRemainingText() const;
    void setFirstLetterRemainingText(RenderTextFragment&);
    void clearFirstLetterRemainingText();

    enum ScaleByEffectiveZoomOrNot { ScaleByEffectiveZoom, DoNotScaleByEffectiveZoom };
    LayoutSize calculateImageIntrinsicDimensions(StyleImage*, const LayoutSize& scaledPositioningAreaSize, ScaleByEffectiveZoomOrNot) const;

    RenderBlock* containingBlockForAutoHeightDetection(Length logicalHeight) const;

    struct ContinuationChainNode {
        WeakPtr<RenderBoxModelObject> renderer;
        ContinuationChainNode* previous { nullptr };
        ContinuationChainNode* next { nullptr };

        ContinuationChainNode(RenderBoxModelObject&);
        ~ContinuationChainNode();

        void insertAfter(ContinuationChainNode&);

        WTF_MAKE_FAST_ALLOCATED;
    };

    ContinuationChainNode* continuationChainNode() const;

protected:
    LayoutUnit computedCSSPadding(const Length&) const;

private:
    ContinuationChainNode& ensureContinuationChainNode();
    
    virtual LayoutRect frameRectForStickyPositioning() const = 0;

    LayoutSize calculateFillTileSize(const FillLayer&, const LayoutSize& scaledPositioningAreaSize) const;

    RoundedRect getBackgroundRoundedRect(const LayoutRect&, InlineFlowBox*, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const;
    
    bool fixedBackgroundPaintsInLocalCoordinates() const;

    void clipBorderSidePolygon(GraphicsContext&, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                               BoxSide, bool firstEdgeMatches, bool secondEdgeMatches);

    void paintOneBorderSide(GraphicsContext&, const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const LayoutRect& sideRect, BoxSide, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge[],
        const Path*, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor = nullptr);
    void paintTranslucentBorderSides(GraphicsContext&, const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
        const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false);
    void paintBorderSides(GraphicsContext&, const RenderStyle&, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
        const IntPoint& innerBorderAdjustment, const BorderEdge[], BorderEdgeFlags, BackgroundBleedAvoidance,
        bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias = false, const Color* overrideColor = nullptr);
    void drawBoxSideFromPath(GraphicsContext&, const LayoutRect&, const Path&, const BorderEdge[],
        float thickness, float drawThickness, BoxSide, const RenderStyle&,
        Color, BorderStyle, BackgroundBleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge);
    void paintMaskForTextFillBox(ImageBuffer*, const IntRect&, InlineFlowBox*, const LayoutRect&);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBoxModelObject, isBoxModelObject())
