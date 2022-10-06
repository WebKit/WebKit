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
#include "RectEdges.h"
#include "RenderLayerModelObject.h"

namespace WebCore {

// Modes for some of the line-related functions.
enum LinePositionMode { PositionOnContainingLine, PositionOfInteriorLineBoxes };
enum LineDirectionMode { HorizontalLine, VerticalLine };

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
    FullScreenChanged,
    ModelChanged
};

class BorderEdge;
class ImageBuffer;
class RenderTextFragment;
class StickyPositionViewportConstraints;
class TransformationMatrix;

namespace InlineIterator {
class InlineBoxIterator;
};

enum class BoxSideFlag : uint8_t;
using BoxSideSet = OptionSet<BoxSideFlag>;
using BorderEdges = RectEdges<BorderEdge>;

// This class is the base for all objects that adhere to the CSS box model as described
// at http://www.w3.org/TR/CSS21/box.html

class RenderBoxModelObject : public RenderLayerModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderBoxModelObject);
public:
    virtual ~RenderBoxModelObject();
    
    LayoutSize relativePositionOffset() const;
    LayoutSize relativePositionLogicalOffset() const { return style().isHorizontalWritingMode() ? relativePositionOffset() : relativePositionOffset().transposedSize(); }

    FloatRect constrainingRectForStickyPosition() const;
    std::pair<const RenderBox&, const RenderLayer*> enclosingClippingBoxForStickyPosition() const;
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

    bool requiresLayer() const override { return isDocumentElementRenderer() || isPositioned() || createsGroup() || hasTransformRelatedProperty() || hasHiddenBackface() || hasReflection(); }

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

    virtual LayoutUnit borderTop() const { return LayoutUnit(style().borderTopWidth()); }
    virtual LayoutUnit borderBottom() const { return LayoutUnit(style().borderBottomWidth()); }
    virtual LayoutUnit borderLeft() const { return LayoutUnit(style().borderLeftWidth()); }
    virtual LayoutUnit borderRight() const { return LayoutUnit(style().borderRightWidth()); }
    virtual LayoutUnit horizontalBorderExtent() const { return borderLeft() + borderRight(); }
    virtual LayoutUnit verticalBorderExtent() const { return borderTop() + borderBottom(); }
    virtual LayoutUnit borderBefore() const { return LayoutUnit(style().borderBeforeWidth()); }
    virtual LayoutUnit borderAfter() const { return LayoutUnit(style().borderAfterWidth()); }
    virtual LayoutUnit borderStart() const { return LayoutUnit(style().borderStartWidth()); }
    virtual LayoutUnit borderEnd() const { return LayoutUnit(style().borderEndWidth()); }

    LayoutUnit borderAndPaddingStart() const { return borderStart() + paddingStart(); }
    LayoutUnit borderAndPaddingBefore() const { return borderBefore() + paddingBefore(); }
    LayoutUnit borderAndPaddingAfter() const { return borderAfter() + paddingAfter(); }

    LayoutUnit marginAndBorderAndPaddingStart() const { return marginStart() + borderStart() + paddingStart(); }
    LayoutUnit marginAndBorderAndPaddingEnd() const { return marginEnd() + borderEnd() + paddingEnd(); }
    LayoutUnit marginAndBorderAndPaddingBefore() const { return marginBefore() + borderBefore() + paddingBefore(); }
    LayoutUnit marginAndBorderAndPaddingAfter() const { return marginAfter() + borderAfter() + paddingAfter(); }

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

    // Overridden by subclasses to determine line height and baseline position.
    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;
    virtual LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const = 0;

    void mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode>, TransformState&) const override;

    void setSelectionState(HighlightState) override;

    bool canHaveBoxInfoInFragment() const { return !isFloating() && !isReplacedOrInlineBlock() && !isInline() && !isTableCell() && isRenderBlock() && !isRenderSVGBlock(); }

    void contentChanged(ContentChangeType);
    bool hasAcceleratedCompositing() const;

    RenderBoxModelObject* continuation() const;
    WEBCORE_EXPORT RenderInline* inlineContinuation() const;

    static void forRendererAndContinuations(RenderBoxModelObject&, const std::function<void(RenderBoxModelObject&)>&);

    void insertIntoContinuationChainAfter(RenderBoxModelObject&);
    void removeFromContinuationChain();

    bool hasRunningAcceleratedAnimations() const;

    virtual std::optional<LayoutUnit> overridingContainingBlockContentWidth() const { ASSERT_NOT_REACHED(); return -1_lu; }
    virtual std::optional<LayoutUnit> overridingContainingBlockContentHeight() const { ASSERT_NOT_REACHED(); return -1_lu; }
    virtual bool hasOverridingContainingBlockContentWidth() const { return false; }
    virtual bool hasOverridingContainingBlockContentHeight() const { return false; }

    void applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const override;

protected:
    RenderBoxModelObject(Element&, RenderStyle&&, BaseTypeFlags);
    RenderBoxModelObject(Document&, RenderStyle&&, BaseTypeFlags);

    void willBeDestroyed() override;

    LayoutPoint adjustedPositionRelativeToOffsetParent(const LayoutPoint&) const;

    bool hasVisibleBoxDecorationStyle() const;
    bool borderObscuresBackgroundEdge(const FloatSize& contextScale) const;
    bool borderObscuresBackground() const;

    bool hasAutoHeightOrContainingBlockWithAutoHeight() const;

public:
    bool fixedBackgroundPaintsInLocalCoordinates() const;
    InterpolationQuality chooseInterpolationQuality(GraphicsContext&, Image&, const void*, const LayoutSize&) const;
    DecodingMode decodingModeForImageDraw(const Image&, const PaintInfo&) const;

    void paintMaskForTextFillBox(ImageBuffer*, const FloatRect&, const InlineIterator::InlineBoxIterator&, const LayoutRect&);

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
    WEBCORE_EXPORT LayoutUnit computedCSSPadding(const Length&) const;
    virtual void absoluteQuadsIgnoringContinuation(const FloatRect&, Vector<FloatQuad>&, bool* /*wasFixed*/) const { ASSERT_NOT_REACHED(); }
    void collectAbsoluteQuadsForContinuation(Vector<FloatQuad>& quads, bool* wasFixed) const;

private:
    ContinuationChainNode& ensureContinuationChainNode();
    
    virtual LayoutRect frameRectForStickyPositioning() const = 0;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderBoxModelObject, isBoxModelObject())
