/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleDifference.h"

#include "InlineTextBoxStyle.h"
#include "RenderStyleConstants.h"
#include "RenderStyle+GettersInlines.h"
#include "StyleExtractor.h"

namespace WebCore {
namespace Style {

class DifferenceFunctions final {
public:
    // MARK: DifferenceResult::Layout

    static bool positionChangeIsMovementOnly(const InsetBox& a, const InsetBox& b, const PreferredSize& width)
    {
        // If any unit types are different, then we can't guarantee
        // that this was just a movement.
        if (!a.left().hasSameType(b.left())
            || !a.right().hasSameType(b.right())
            || !a.top().hasSameType(b.top())
            || !a.bottom().hasSameType(b.bottom()))
            return false;

        // Only one unit can be non-auto in the horizontal direction and
        // in the vertical direction.  Otherwise the adjustment of values
        // is changing the size of the box.
        if (!a.left().isAuto() && !a.right().isAuto())
            return false;
        if (!a.top().isAuto() && !a.bottom().isAuto())
            return false;
        // If our width is auto and left or right is specified then this
        // is not just a movement - we need to resize to our container.
        if ((!a.left().isAuto() || !a.right().isAuto()) && width.isIntrinsicOrLegacyIntrinsicOrAuto())
            return false;

        // One of the units is fixed or percent in both directions and stayed
        // that way in the new style.  Therefore all we are doing is moving.
        return true;
    }

    static bool changeAffectsVisualOverflow(const RenderStyle& a, const RenderStyle& b)
    {
        auto nonInheritedDataChangeAffectsVisualOverflow = [&] {
            if (&a.nonInheritedData() == &b.nonInheritedData())
                return false;

            if (a.nonInheritedData().miscData.ptr() != b.nonInheritedData().miscData.ptr()
                && a.nonInheritedData().miscData->boxShadow != b.nonInheritedData().miscData->boxShadow)
                return true;

            if (a.nonInheritedData().backgroundData.ptr() != b.nonInheritedData().backgroundData.ptr()) {
                auto aHasOutlineInVisualOverflow = a.hasOutlineInVisualOverflow();
                auto bHasOutlineInVisualOverflow = b.hasOutlineInVisualOverflow();
                if (aHasOutlineInVisualOverflow != bHasOutlineInVisualOverflow
                    || (aHasOutlineInVisualOverflow && bHasOutlineInVisualOverflow && a.usedOutlineSize() != b.usedOutlineSize()))
                    return true;
            }

            return false;
        };

        auto textDecorationsDiffer = [&] {
            if (a.inheritedFlags().textDecorationLineInEffect != b.inheritedFlags().textDecorationLineInEffect)
                return true;

            if (&a.nonInheritedData() != &b.nonInheritedData() && a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()) {
                if (a.nonInheritedData().rareData->textDecorationStyle != b.nonInheritedData().rareData->textDecorationStyle
                    || a.nonInheritedData().rareData->textDecorationThickness != b.nonInheritedData().rareData->textDecorationThickness)
                    return true;
            }

            if (&a.inheritedRareData() != &b.inheritedRareData()) {
                if (a.inheritedRareData().textUnderlineOffset != b.inheritedRareData().textUnderlineOffset
                    || a.inheritedRareData().textUnderlinePosition != b.inheritedRareData().textUnderlinePosition)
                        return true;
            }

            return false;
        };

        if (nonInheritedDataChangeAffectsVisualOverflow())
            return true;

        if (&a.inheritedRareData() != &b.inheritedRareData()
            && a.inheritedRareData().textShadow != b.inheritedRareData().textShadow)
            return true;

        if (textDecorationsDiffer()) {
            // Underlines are always drawn outside of their textbox bounds when text-underline-position: under;
            // is specified. We can take an early out here.
            if (isAlignedForUnder(a) || isAlignedForUnder(b))
                return true;

            if (inkOverflowForDecorations(a) != inkOverflowForDecorations(b))
                return true;
        }

        return false;
    }

    static bool svgDataChangeRequiresLayout(const SVGData& a, const SVGData& b)
    {
        // Markers influence layout, as marker boundaries are cached in RenderSVGPath.
        if (a.markerResourceData != b.markerResourceData)
            return true;

        // All text related properties influence layout.
        if (a.inheritedFlags.textAnchor != b.inheritedFlags.textAnchor
            || a.inheritedFlags.glyphOrientationHorizontal != b.inheritedFlags.glyphOrientationHorizontal
            || a.inheritedFlags.glyphOrientationVertical != b.inheritedFlags.glyphOrientationVertical
            || a.nonInheritedFlags.alignmentBaseline != b.nonInheritedFlags.alignmentBaseline
            || a.nonInheritedFlags.dominantBaseline != b.nonInheritedFlags.dominantBaseline)
            return true;

        // Text related properties influence layout.
        if (a.miscData->baselineShift != b.miscData->baselineShift)
            return true;

        // The x and y properties influence layout.
        if (a.layoutData != b.layoutData)
            return true;

        // Some stroke properties influence layout, as the cached stroke boundaries need to be recalculated.
        if (!a.strokeData->stroke.hasSameType(b.strokeData->stroke)
            || a.strokeData->stroke.urlDisregardingType() != b.strokeData->stroke.urlDisregardingType()
            || a.strokeData->strokeDashArray != b.strokeData->strokeDashArray
            || a.strokeData->strokeDashOffset != b.strokeData->strokeDashOffset
            || !a.strokeData->visitedLinkStroke.hasSameType(b.strokeData->visitedLinkStroke)
            || a.strokeData->visitedLinkStroke.urlDisregardingType() != b.strokeData->visitedLinkStroke.urlDisregardingType())
            return true;

        // vector-effect influences layout.
        if (a.nonInheritedFlags.vectorEffect != b.nonInheritedFlags.vectorEffect)
            return true;

        return false;
    }

    static bool miscDataChangeRequiresLayout(const NonInheritedMiscData& a, const NonInheritedMiscData& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        ASSERT(&a != &b);

        if (a.usedAppearance != b.usedAppearance
            || a.textOverflow != b.textOverflow)
            return true;

        if (a.deprecatedFlexibleBox != b.deprecatedFlexibleBox)
            return true;

        if (a.flexibleBox != b.flexibleBox)
            return true;

        if (a.order != b.order
            || a.alignContent != b.alignContent
            || a.alignItems != b.alignItems
            || a.alignSelf != b.alignSelf
            || a.justifyContent != b.justifyContent
            || a.justifyItems != b.justifyItems
            || a.justifySelf != b.justifySelf)
            return true;

        if (a.multiCol != b.multiCol)
            return true;

        if (a.transform.ptr() != b.transform.ptr()) {
            if (a.transform->hasTransform() != b.transform->hasTransform())
                return true;
            if (*a.transform != *b.transform) {
                changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Transform);
                // Don't return; keep looking for another change
            }
        }

        if (a.opacity.isOpaque() != b.opacity.isOpaque()) {
            // FIXME: We would like to use SimplifiedLayout here, but we can't quite do that yet.
            // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
            // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
            // In addition we need to solve the floating object issue when layers come and go. Right now
            // a full layout is necessary to keep floating object lists sane.
            return true;
        }

        if (a.hasFilters() != b.hasFilters())
            return true;

        if (a.aspectRatio != b.aspectRatio)
            return true;

        return false;
    }

    static bool rareDataChangeRequiresLayout(const NonInheritedRareData& a, const NonInheritedRareData& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        ASSERT(&a != &b);

        if (a.lineClamp != b.lineClamp || a.initialLetter != b.initialLetter)
            return true;

        if (a.shapeMargin != b.shapeMargin)
            return true;

        if (a.columnGap != b.columnGap || a.rowGap != b.rowGap)
            return true;

        if (a.boxReflect != b.boxReflect)
            return true;

        // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
        if (a.usedCounterDirectives != b.usedCounterDirectives)
            return true;

        if (a.scale != b.scale || a.rotate != b.rotate || a.translate != b.translate)
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Transform);

        if (a.offsetPath != b.offsetPath
            || a.offsetPosition != b.offsetPosition
            || a.offsetDistance != b.offsetDistance
            || a.offsetAnchor != b.offsetAnchor
            || a.offsetRotate != b.offsetRotate)
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Transform);

        if (a.grid != b.grid
            || a.gridItem != b.gridItem)
            return true;

        if (a.willChange != b.willChange) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::WillChange);
            // Don't return; keep looking for another change
        }

        if (a.breakBefore != b.breakBefore || a.breakAfter != b.breakAfter || a.breakInside != b.breakInside)
            return true;

        if (a.isolation != b.isolation) {
            // Ideally this would trigger a cheaper layout that just updates layer z-order trees (webkit.org/b/190088).
            return true;
        }

        if (a.backdropFilter->backdropFilter.isNone() != b.backdropFilter->backdropFilter.isNone())
            return true;

    #if HAVE(CORE_MATERIAL)
        if (a.appleVisualEffect != b.appleVisualEffect)
            return true;
    #endif

        if (a.inputSecurity != b.inputSecurity)
            return true;

        if (a.usedContain().contains(ContainValue::Size) != b.usedContain().contains(ContainValue::Size)
            || a.usedContain().contains(ContainValue::InlineSize) != b.usedContain().contains(ContainValue::InlineSize)
            || a.usedContain().contains(ContainValue::Layout) != b.usedContain().contains(ContainValue::Layout))
            return true;

        // content-visibility:hidden turns on contain:size which requires relayout.
        if ((static_cast<ContentVisibility>(a.contentVisibility) == ContentVisibility::Hidden) != (static_cast<ContentVisibility>(b.contentVisibility) == ContentVisibility::Hidden))
            return true;

        if (a.scrollPadding != b.scrollPadding)
            return true;

        if (a.scrollSnapType != b.scrollSnapType)
            return true;

        if (a.containIntrinsicWidth != b.containIntrinsicWidth || a.containIntrinsicHeight != b.containIntrinsicHeight)
            return true;

        if (a.marginTrim != b.marginTrim)
            return true;

        if (a.scrollbarGutter != b.scrollbarGutter)
            return true;

        if (a.scrollbarWidth != b.scrollbarWidth)
            return true;

        if (a.textBoxTrim != b.textBoxTrim)
            return true;

        if (a.maxLines != b.maxLines)
            return true;

        if (a.overflowContinue != b.overflowContinue)
            return true;

        // CSS Anchor Positioning.
        if (a.anchorScope != b.anchorScope || a.positionArea != b.positionArea)
            return true;

        if (a.fieldSizing != b.fieldSizing)
            return true;

        return false;
    }

    static bool rareInheritedDataChangeRequiresLayout(const InheritedRareData& a, const InheritedRareData& b)
    {
        ASSERT(&a != &b);

        if (a.textIndent != b.textIndent
            || a.textAlignLast != b.textAlignLast
            || a.textJustify != b.textJustify
            || a.textBoxEdge != b.textBoxEdge
            || a.lineFitEdge != b.lineFitEdge
            || a.usedZoom != b.usedZoom
            || a.textZoom != b.textZoom
    #if ENABLE(TEXT_AUTOSIZING)
            || a.textSizeAdjust != b.textSizeAdjust
    #endif
            || a.wordBreak != b.wordBreak
            || a.overflowWrap != b.overflowWrap
            || a.nbspMode != b.nbspMode
            || a.lineBreak != b.lineBreak
            || a.textSecurity != b.textSecurity
            || a.hyphens != b.hyphens
            || a.hyphenateLimitBefore != b.hyphenateLimitBefore
            || a.hyphenateLimitAfter != b.hyphenateLimitAfter
            || a.hyphenateCharacter != b.hyphenateCharacter
            || a.rubyPosition != b.rubyPosition
            || a.rubyAlign != b.rubyAlign
            || a.rubyOverhang != b.rubyOverhang
            || a.textCombine != b.textCombine
            || a.textEmphasisStyle != b.textEmphasisStyle
            || a.textEmphasisPosition != b.textEmphasisPosition
            || a.tabSize != b.tabSize
            || a.lineBoxContain != b.lineBoxContain
            || a.lineGrid != b.lineGrid
            || a.imageOrientation != b.imageOrientation
            || a.lineSnap != b.lineSnap
            || a.lineAlign != b.lineAlign
            || a.hangingPunctuation != b.hangingPunctuation
            || a.usedContentVisibility != b.usedContentVisibility
    #if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
            || a.overflowScrolling != b.overflowScrolling
    #endif
            || a.listStyleType != b.listStyleType
            || a.listStyleImage != b.listStyleImage
            || a.blockEllipsis != b.blockEllipsis)
            return true;

        if (a.textStrokeWidth != b.textStrokeWidth)
            return true;

        // These properties affect the cached stroke bounding box rects.
        if (a.capStyle != b.capStyle
            || a.joinStyle != b.joinStyle
            || a.strokeWidth != b.strokeWidth
            || a.strokeMiterLimit != b.strokeMiterLimit)
            return true;

        if (a.quotes != b.quotes)
            return true;

        return false;
    }

    static bool changeRequiresLayout(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        if (&a.svgData() != &b.svgData() && svgDataChangeRequiresLayout(a.svgData(), b.svgData()))
            return true;

        if (&a.nonInheritedData() != &b.nonInheritedData()) {
            if (a.nonInheritedData().boxData.ptr() != b.nonInheritedData().boxData.ptr()) {
                if (a.nonInheritedData().boxData->width != b.nonInheritedData().boxData->width
                    || a.nonInheritedData().boxData->minWidth != b.nonInheritedData().boxData->minWidth
                    || a.nonInheritedData().boxData->maxWidth != b.nonInheritedData().boxData->maxWidth
                    || a.nonInheritedData().boxData->height != b.nonInheritedData().boxData->height
                    || a.nonInheritedData().boxData->minHeight != b.nonInheritedData().boxData->minHeight
                    || a.nonInheritedData().boxData->maxHeight != b.nonInheritedData().boxData->maxHeight)
                    return true;

                if (a.nonInheritedData().boxData->verticalAlign != b.nonInheritedData().boxData->verticalAlign)
                    return true;

                if (a.nonInheritedData().boxData->boxSizing != b.nonInheritedData().boxData->boxSizing)
                    return true;

                if (a.nonInheritedData().boxData->hasAutoUsedZIndex != b.nonInheritedData().boxData->hasAutoUsedZIndex)
                    return true;
            }

            if (a.nonInheritedData().surroundData.ptr() != b.nonInheritedData().surroundData.ptr()) {
                if (a.nonInheritedData().surroundData->margin != b.nonInheritedData().surroundData->margin)
                    return true;

                if (a.nonInheritedData().surroundData->padding != b.nonInheritedData().surroundData->padding)
                    return true;

                // If our border widths change, then we need to layout. Other changes to borders only necessitate a repaint.
                if (a.usedBorderLeftWidth() != b.usedBorderLeftWidth()
                    || a.usedBorderTopWidth() != b.usedBorderTopWidth()
                    || a.usedBorderBottomWidth() != b.usedBorderBottomWidth()
                    || a.usedBorderRightWidth() != b.usedBorderRightWidth())
                    return true;

                if (a.position() != PositionType::Static) {
                    if (a.nonInheritedData().surroundData->inset != b.nonInheritedData().surroundData->inset) {
                        // FIXME: We would like to use SimplifiedLayout for relative positioning, but we can't quite do that yet.
                        // We need to make sure SimplifiedLayout can operate correctly on RenderInlines (we will need
                        // to add a selfNeedsSimplifiedLayout bit in order to not get confused and taint every line).
                        if (a.position() != PositionType::Absolute)
                            return true;

                        // Optimize for the case where a positioned layer is moving but not changing size.
                        if (!positionChangeIsMovementOnly(a.nonInheritedData().surroundData->inset, b.nonInheritedData().surroundData->inset, a.nonInheritedData().boxData->width))
                            return true;
                    }
                }
            }
        }

        // FIXME: We should add an optimized form of layout that just recomputes visual overflow.
        if (changeAffectsVisualOverflow(a, b))
            return true;

        if (&a.nonInheritedData() != &b.nonInheritedData()) {
            if (a.nonInheritedData().miscData.ptr() != b.nonInheritedData().miscData.ptr()
                && miscDataChangeRequiresLayout(*a.nonInheritedData().miscData, *b.nonInheritedData().miscData, changedContextSensitiveProperties))
                return true;

            if (a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()
                && rareDataChangeRequiresLayout(*a.nonInheritedData().rareData, *b.nonInheritedData().rareData, changedContextSensitiveProperties))
                return true;
        }

        if (&a.inheritedRareData() != &b.inheritedRareData()
            && rareInheritedDataChangeRequiresLayout(a.inheritedRareData(), b.inheritedRareData()))
            return true;

        if (&a.inheritedData() != &b.inheritedData()) {
            if (a.inheritedData().lineHeight != b.inheritedData().lineHeight
    #if ENABLE(TEXT_AUTOSIZING)
                || a.inheritedData().specifiedLineHeight != b.inheritedData().specifiedLineHeight
    #endif
                || a.inheritedData().borderHorizontalSpacing != b.inheritedData().borderHorizontalSpacing
                || a.inheritedData().borderVerticalSpacing != b.inheritedData().borderVerticalSpacing)
                return true;

            if (a.inheritedData().fontData != b.inheritedData().fontData)
                return true;
        }

        if (a.inheritedFlags().boxDirection != b.inheritedFlags().boxDirection
            || a.inheritedFlags().rtlOrdering != b.inheritedFlags().rtlOrdering
            || a.nonInheritedFlags().position != b.nonInheritedFlags().position
            || a.nonInheritedFlags().floating != b.nonInheritedFlags().floating
            || a.nonInheritedFlags().originalDisplay != b.nonInheritedFlags().originalDisplay)
            return true;

        if (static_cast<DisplayType>(a.nonInheritedFlags().effectiveDisplay) >= DisplayType::Table) {
            if (a.inheritedFlags().borderCollapse != b.inheritedFlags().borderCollapse
                || a.inheritedFlags().emptyCells != b.inheritedFlags().emptyCells
                || a.inheritedFlags().captionSide != b.inheritedFlags().captionSide
                || a.tableLayout() != b.tableLayout())
                return true;

            // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
            // does not, so these style differences can be width differences.
            if (a.inheritedFlags().borderCollapse
                && ((a.borderTopStyle() == BorderStyle::Hidden && b.borderTopStyle() == BorderStyle::None)
                    || (a.borderTopStyle() == BorderStyle::None && b.borderTopStyle() == BorderStyle::Hidden)
                    || (a.borderBottomStyle() == BorderStyle::Hidden && b.borderBottomStyle() == BorderStyle::None)
                    || (a.borderBottomStyle() == BorderStyle::None && b.borderBottomStyle() == BorderStyle::Hidden)
                    || (a.borderLeftStyle() == BorderStyle::Hidden && b.borderLeftStyle() == BorderStyle::None)
                    || (a.borderLeftStyle() == BorderStyle::None && b.borderLeftStyle() == BorderStyle::Hidden)
                    || (a.borderRightStyle() == BorderStyle::Hidden && b.borderRightStyle() == BorderStyle::None)
                    || (a.borderRightStyle() == BorderStyle::None && b.borderRightStyle() == BorderStyle::Hidden)))
                return true;
        }

        if (static_cast<DisplayType>(a.nonInheritedFlags().effectiveDisplay) == DisplayType::ListItem) {
            if (a.inheritedFlags().listStylePosition != b.inheritedFlags().listStylePosition || a.inheritedRareData().listStyleType != b.inheritedRareData().listStyleType)
                return true;
        }

        if (a.inheritedFlags().textAlign != b.inheritedFlags().textAlign
            || a.inheritedFlags().textTransform != b.inheritedFlags().textTransform
            || a.inheritedFlags().whiteSpaceCollapse != b.inheritedFlags().whiteSpaceCollapse
            || a.inheritedFlags().textWrapMode != b.inheritedFlags().textWrapMode
            || a.inheritedFlags().textWrapStyle != b.inheritedFlags().textWrapStyle
            || a.nonInheritedFlags().clear != b.nonInheritedFlags().clear
            || a.nonInheritedFlags().unicodeBidi != b.nonInheritedFlags().unicodeBidi)
            return true;

        if (a.writingMode() != b.writingMode())
            return true;

        // Overflow returns a layout hint.
        if (a.nonInheritedFlags().overflowX != b.nonInheritedFlags().overflowX
            || a.nonInheritedFlags().overflowY != b.nonInheritedFlags().overflowY)
            return true;

        if ((a.usedVisibility() == Visibility::Collapse) != (b.usedVisibility() == Visibility::Collapse))
            return true;

        bool aHasFirstLineStyle = a.hasPseudoStyle(PseudoElementType::FirstLine);
        if (aHasFirstLineStyle != b.hasPseudoStyle(PseudoElementType::FirstLine))
            return true;

        if (aHasFirstLineStyle) {
            auto* aFirstLineStyle = a.getCachedPseudoStyle({ PseudoElementType::FirstLine });
            if (!aFirstLineStyle)
                return true;
            auto* bFirstLineStyle = b.getCachedPseudoStyle({ PseudoElementType::FirstLine });
            if (!bFirstLineStyle)
                return true;
            // FIXME: Not all first line style changes actually need layout.
            if (*aFirstLineStyle != *bFirstLineStyle)
                return true;
        }

        return false;
    }

    // MARK: DifferenceResult::LayoutOutOfFlowMovementOnly

    static bool changeRequiresOutOfFlowMovementLayoutOnly(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>&)
    {
        if (a.position() != PositionType::Absolute)
            return false;

        // Optimize for the case where a out-of-flow box is moving but not changing size.
        return a.nonInheritedData().surroundData->inset != b.nonInheritedData().surroundData->inset
            && positionChangeIsMovementOnly(a.nonInheritedData().surroundData->inset, b.nonInheritedData().surroundData->inset, a.nonInheritedData().boxData->width);
    }

    // MARK: DifferenceResult::RepaintLayer

    static bool miscDataChangeRequiresLayerRepaint(const NonInheritedMiscData& a, const NonInheritedMiscData& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        if (a.opacity != b.opacity) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Opacity);
            // Don't return true; keep looking for another change.
        }

        if (a.filter != b.filter) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Filter);
            // Don't return true; keep looking for another change.
        }

        // FIXME: In SVG this needs to trigger a layout.
        if (a.mask != b.mask)
            return true;

        return false;
    }

    static bool rareDataChangeRequiresLayerRepaint(const NonInheritedRareData& a, const NonInheritedRareData& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        if (a.effectiveBlendMode != b.effectiveBlendMode)
            return true;

        if (a.backdropFilter != b.backdropFilter) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Filter);
            // Don't return true; keep looking for another change.
        }

        // FIXME: In SVG this needs to trigger a layout.
        if (a.maskBorder != b.maskBorder)
            return true;

        return false;
    }

    static bool changeRequiresLayerRepaint(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        // Resolver has ensured that zIndex is non-auto only if it's applicable.

        if (&a.nonInheritedData() != &b.nonInheritedData()) {
            if (a.nonInheritedData().boxData.ptr() != b.nonInheritedData().boxData.ptr()) {
                if (a.nonInheritedData().boxData->usedZIndex() != b.nonInheritedData().boxData->usedZIndex())
                    return true;
            }

            if (a.position() != PositionType::Static) {
                if (a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()) {
                    if (a.nonInheritedData().rareData->clip != b.nonInheritedData().rareData->clip) {
                        changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::ClipRect);
                        return true;
                    }
                }
            }

            if (a.nonInheritedData().miscData.ptr() != b.nonInheritedData().miscData.ptr()
                && miscDataChangeRequiresLayerRepaint(*a.nonInheritedData().miscData, *b.nonInheritedData().miscData, changedContextSensitiveProperties))
                return true;

            if (a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()
                && rareDataChangeRequiresLayerRepaint(*a.nonInheritedData().rareData, *b.nonInheritedData().rareData, changedContextSensitiveProperties))
                return true;
        }

        if (&a.inheritedRareData() != &b.inheritedRareData()
            && a.inheritedRareData().dynamicRangeLimit != b.inheritedRareData().dynamicRangeLimit) {
            return true;
        }

    #if HAVE(CORE_MATERIAL)
        if (&a.inheritedRareData() != &b.inheritedRareData()
            && a.inheritedRareData().usedAppleVisualEffectForSubtree != b.inheritedRareData().usedAppleVisualEffectForSubtree) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::Filter);
            // Don't return true; keep looking for another change.
        }
    #endif

        bool currentColorDiffers = a.inheritedData().color != b.inheritedData().color;
        if (currentColorDiffers) {
            if (a.filter().hasFilterThatRequiresRepaintForCurrentColorChange() || a.backdropFilter().hasFilterThatRequiresRepaintForCurrentColorChange())
                return true;
        }

        return false;
    }

    // MARK: DifferenceResult::Repaint

    static bool requiresPainting(const RenderStyle& style)
    {
        if (style.usedVisibility() == Visibility::Hidden)
            return false;
        if (style.opacity().isTransparent())
            return false;
        return true;
    }

    static bool isEquivalentForPainting(const BackgroundData& a, const BackgroundData& b, bool currentColorDiffers)
    {
        if (&a == &b) {
            ASSERT(currentColorDiffers);
            return !a.containsCurrentColor();
        }

        if (a.background != b.background || a.backgroundColor != b.backgroundColor)
            return false;
        if (currentColorDiffers && a.backgroundColor.containsCurrentColor())
            return false;
        if (!a.outline.isVisible() && !b.outline.isVisible())
            return true;
        if (currentColorDiffers && a.outline.outlineColor.containsCurrentColor())
            return false;
        return a.outline == b.outline;
    }

    static bool isEquivalentForPainting(const BorderData& a, const BorderData& b, bool currentColorDiffers)
    {
        if (&a == &b) {
            ASSERT(currentColorDiffers);
            return !a.containsCurrentColor();
        }

        if (a != b)
            return false;

        if (!currentColorDiffers)
            return true;

        return !a.containsCurrentColor();
    }

    static bool colorChangeRequiresRepaint(const Color& a, const Color& b, bool currentColorDiffers)
    {
        if (a != b)
            return true;

        if (a.containsCurrentColor()) {
            ASSERT(b.containsCurrentColor());
            return currentColorDiffers;
        }

        return false;
    }

    static bool svgDataChangeRequiresRepaint(const SVGData& a, const SVGData& b, bool currentColorDiffers)
    {
        if (&a == &b) {
            ASSERT(currentColorDiffers);
            return containsCurrentColor(a.strokeData->stroke)
                || containsCurrentColor(a.strokeData->visitedLinkStroke)
                || containsCurrentColor(a.miscData->floodColor)
                || containsCurrentColor(a.miscData->lightingColor)
                || containsCurrentColor(a.fillData->fill); // FIXME: Should this be checking fillData->visitedLinkFill as well?
        }

        if (a.strokeData->strokeOpacity != b.strokeData->strokeOpacity
            || colorChangeRequiresRepaint(a.strokeData->stroke.colorDisregardingType(), b.strokeData->stroke.colorDisregardingType(), currentColorDiffers)
            || colorChangeRequiresRepaint(a.strokeData->visitedLinkStroke.colorDisregardingType(), b.strokeData->visitedLinkStroke.colorDisregardingType(), currentColorDiffers))
            return true;

        // Painting related properties only need repaints.
        if (colorChangeRequiresRepaint(a.miscData->floodColor, b.miscData->floodColor, currentColorDiffers)
            || a.miscData->floodOpacity != b.miscData->floodOpacity
            || colorChangeRequiresRepaint(a.miscData->lightingColor, b.miscData->lightingColor, currentColorDiffers))
            return true;

        // If fill data changes, we just need to repaint. Fill boundaries are not influenced by this, only by the Path, that RenderSVGPath contains.
        if (!a.fillData->fill.hasSameType(b.fillData->fill)
            || colorChangeRequiresRepaint(a.fillData->fill.colorDisregardingType(), b.fillData->fill.colorDisregardingType(), currentColorDiffers)
            || a.fillData->fill.urlDisregardingType() != b.fillData->fill.urlDisregardingType()
            || a.fillData->fillOpacity != b.fillData->fillOpacity)
            return true;

        // If gradient stops change, we just need to repaint. Style updates are already handled through RenderSVGGradientStop.
        if (a.stopData != b.stopData)
            return true;

        // Changes of these flags only cause repaints.
        if (a.inheritedFlags.shapeRendering != b.inheritedFlags.shapeRendering
            || a.inheritedFlags.clipRule != b.inheritedFlags.clipRule
            || a.inheritedFlags.fillRule != b.inheritedFlags.fillRule
            || a.inheritedFlags.colorInterpolation != b.inheritedFlags.colorInterpolation
            || a.inheritedFlags.colorInterpolationFilters != b.inheritedFlags.colorInterpolationFilters)
            return true;

        if (a.nonInheritedFlags.bufferedRendering != b.nonInheritedFlags.bufferedRendering)
            return true;

        if (a.nonInheritedFlags.maskType != b.nonInheritedFlags.maskType)
            return true;

        return false;
    }

    static bool miscDataChangeRequiresRepaint(const NonInheritedMiscData& a, const NonInheritedMiscData& b, OptionSet<DifferenceContextSensitiveProperty>&)
    {
        if (a.userDrag != b.userDrag
            || a.objectFit != b.objectFit
            || a.objectPosition != b.objectPosition)
            return true;

        return false;
    }

    static bool rareDataChangeRequiresRepaint(const NonInheritedRareData& a, const NonInheritedRareData& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        if (a.shapeOutside != b.shapeOutside)
            return true;

        // FIXME: this should probably be moved to changeRequiresLayerRepaint().
        if (a.clipPath != b.clipPath) {
            changedContextSensitiveProperties.add(DifferenceContextSensitiveProperty::ClipPath);
            // Don't return true; keep looking for another change.
        }

        if (a.textDecorationStyle != b.textDecorationStyle || a.textDecorationColor != b.textDecorationColor || a.textDecorationThickness != b.textDecorationThickness)
            return true;

        return false;
    }

    static bool rareInheritedDataChangeRequiresRepaint(const InheritedRareData& a, const InheritedRareData& b)
    {
        return a.effectiveInert != b.effectiveInert
            || a.userModify != b.userModify
            || a.userSelect != b.userSelect
            || a.appleColorFilter != b.appleColorFilter
            || a.imageRendering != b.imageRendering
            || a.accentColor != b.accentColor
            || a.insideDefaultButton != b.insideDefaultButton
            || a.insideSubmitButton != b.insideSubmitButton
    #if ENABLE(DARK_MODE_CSS)
            || a.colorScheme != b.colorScheme
    #endif
        ;
    }

    inline static bool changedCustomPaintWatchedProperty(const RenderStyle& a, const NonInheritedRareData& aData, const RenderStyle& b, const NonInheritedRareData& bData)
    {
        auto& propertiesA = aData.customPaintWatchedProperties;
        auto& propertiesB = bData.customPaintWatchedProperties;

        if (!propertiesA.isEmpty() || !propertiesB.isEmpty()) [[unlikely]] {
            // FIXME: We should not need to use Extractor here.
            Extractor extractor((Element*)nullptr);
            auto& pool = CSSValuePool::singleton();

            for (auto& watchPropertiesMap : { propertiesA, propertiesB }) {
                for (auto& name : watchPropertiesMap) {
                    if (isCustomPropertyName(name)) {
                        RefPtr valueA = a.customPropertyValue(name);
                        RefPtr valueB = b.customPropertyValue(name);

                        if (valueA != valueB && (!valueA || !valueB || *valueA != *valueB))
                            return true;
                    } else if (auto propertyID = cssPropertyID(name)) {
                        auto valueA = extractor.propertyValueInStyle(a, propertyID, pool);
                        auto valueB = extractor.propertyValueInStyle(b, propertyID, pool);

                        if (valueA != valueB && (!valueA || !valueB || *valueA != *valueB))
                            return true;
                    }
                }
            }
        }

        return false;
    }

    static bool changeRequiresRepaint(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>& changedContextSensitiveProperties)
    {
        bool currentColorDiffers = a.inheritedData().color != b.inheritedData().color;

        if (currentColorDiffers || &a.svgData() != &b.svgData()) {
            if (svgDataChangeRequiresRepaint(a.svgData(), b.svgData(), currentColorDiffers))
                return true;
        }

        if (!requiresPainting(a) && !requiresPainting(b))
            return false;

        if (a.usedVisibility() != b.usedVisibility()
            || a.inheritedFlags().printColorAdjust != b.inheritedFlags().printColorAdjust
            || a.inheritedFlags().insideLink != b.inheritedFlags().insideLink)
            return true;


        if (currentColorDiffers || &a.nonInheritedData() != &b.nonInheritedData()) {
            if (currentColorDiffers || a.nonInheritedData().backgroundData.ptr() != b.nonInheritedData().backgroundData.ptr()) {
                if (!isEquivalentForPainting(*a.nonInheritedData().backgroundData, *b.nonInheritedData().backgroundData, currentColorDiffers))
                    return true;
            }

            if (currentColorDiffers || a.nonInheritedData().surroundData.ptr() != b.nonInheritedData().surroundData.ptr()) {
                if (!isEquivalentForPainting(a.nonInheritedData().surroundData->border, b.nonInheritedData().surroundData->border, currentColorDiffers))
                    return true;
            }
        }

        if (&a.nonInheritedData() != &b.nonInheritedData()) {
            if (a.nonInheritedData().miscData.ptr() != b.nonInheritedData().miscData.ptr()
                && miscDataChangeRequiresRepaint(*a.nonInheritedData().miscData, *b.nonInheritedData().miscData, changedContextSensitiveProperties))
                return true;

            if (a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()
                && rareDataChangeRequiresRepaint(*a.nonInheritedData().rareData, *b.nonInheritedData().rareData, changedContextSensitiveProperties))
                return true;
        }

        if (&a.inheritedRareData() != &b.inheritedRareData()
            && rareInheritedDataChangeRequiresRepaint(a.inheritedRareData(), b.inheritedRareData()))
            return true;

        if (changedCustomPaintWatchedProperty(a, *a.nonInheritedData().rareData, b, *b.nonInheritedData().rareData))
            return true;

        return false;
    }

    // MARK: DifferenceResult::RepaintIfText

    static bool changeRequiresRepaintIfText(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>&)
    {
        // FIXME: Does this code need to consider currentColorDiffers? webkit.org/b/266833
        if (a.inheritedData().color != b.inheritedData().color)
            return true;

        // Note that we may reach this function with mutated text-decoration values (e.g. thickness), when visual overflow recompute is not required.
        // see `changeAffectsVisualOverflow`
        if (a.inheritedFlags().textDecorationLineInEffect != b.inheritedFlags().textDecorationLineInEffect
            || a.nonInheritedFlags().textDecorationLine != b.nonInheritedFlags().textDecorationLine)
            return true;

        if (&a.inheritedRareData() != &b.inheritedRareData()) {
            if (a.inheritedRareData().textDecorationSkipInk != b.inheritedRareData().textDecorationSkipInk
                || a.inheritedRareData().textFillColor != b.inheritedRareData().textFillColor
                || a.inheritedRareData().textStrokeColor != b.inheritedRareData().textStrokeColor
                || a.inheritedRareData().textEmphasisColor != b.inheritedRareData().textEmphasisColor
                || a.inheritedRareData().textEmphasisStyle != b.inheritedRareData().textEmphasisStyle
                || a.inheritedRareData().strokeColor != b.inheritedRareData().strokeColor
                || a.inheritedRareData().caretColor != b.inheritedRareData().caretColor
                || a.inheritedRareData().textUnderlineOffset != b.inheritedRareData().textUnderlineOffset)
                return true;
        }

        return false;
    }

    // MARK: DifferenceResult::RecompositeLayer

    static bool changeRequiresRecompositeLayer(const RenderStyle& a, const RenderStyle& b, OptionSet<DifferenceContextSensitiveProperty>&)
    {
        if (a.inheritedFlags().pointerEvents != b.inheritedFlags().pointerEvents)
            return true;

        if (&a.nonInheritedData() != &b.nonInheritedData() && a.nonInheritedData().rareData.ptr() != b.nonInheritedData().rareData.ptr()) {
            if (a.usedTransformStyle3D() != b.usedTransformStyle3D()
                || a.nonInheritedData().rareData->backfaceVisibility != b.nonInheritedData().rareData->backfaceVisibility
                || a.nonInheritedData().rareData->perspective != b.nonInheritedData().rareData->perspective
                || a.nonInheritedData().rareData->perspectiveOrigin != b.nonInheritedData().rareData->perspectiveOrigin
                || a.nonInheritedData().rareData->overscrollBehaviorX != b.nonInheritedData().rareData->overscrollBehaviorX
                || a.nonInheritedData().rareData->overscrollBehaviorY != b.nonInheritedData().rareData->overscrollBehaviorY)
                return true;
        }

        if (&a.inheritedRareData() != &b.inheritedRareData() && a.inheritedRareData().effectiveInert != b.inheritedRareData().effectiveInert)
            return true;

        return false;
    }

    // MARK: - Root Functions

    static bool differenceRequiresLayerRepaint(const RenderStyle& a, const RenderStyle& b, bool isComposited)
    {
        auto changedContextSensitiveProperties = OptionSet<DifferenceContextSensitiveProperty>();

        if (changeRequiresRepaint(a, b, changedContextSensitiveProperties))
            return true;

        if (isComposited && changeRequiresLayerRepaint(a, b, changedContextSensitiveProperties))
            return changedContextSensitiveProperties.contains(DifferenceContextSensitiveProperty::ClipRect);

        return false;
    }

    static bool borderIsEquivalentForPainting(const RenderStyle& a, const RenderStyle& b)
    {
        bool colorDiffers = a.color() != b.color();

        if (!colorDiffers
            && (&a.nonInheritedData() == &b.nonInheritedData()
            || a.nonInheritedData().surroundData.ptr() == b.nonInheritedData().surroundData.ptr()
            || a.nonInheritedData().surroundData->border == b.nonInheritedData().surroundData->border))
            return true;

        return isEquivalentForPainting(a.border(), b.border(), colorDiffers);
    }

    static Difference difference(const RenderStyle& a, const RenderStyle& b)
    {
        auto changedContextSensitiveProperties = OptionSet<DifferenceContextSensitiveProperty>();

        if (changeRequiresLayout(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::Layout, changedContextSensitiveProperties };

        if (changeRequiresOutOfFlowMovementLayoutOnly(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::LayoutOutOfFlowMovementOnly, changedContextSensitiveProperties };

        if (changeRequiresLayerRepaint(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::RepaintLayer, changedContextSensitiveProperties };

        if (changeRequiresRepaint(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::Repaint, changedContextSensitiveProperties };

        if (changeRequiresRepaintIfText(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::RepaintIfText, changedContextSensitiveProperties };

        // FIXME: RecompositeLayer should also behave as a priority bit (e.g when the style change requires layout, we know that
        // the content also needs repaint and it will eventually get repainted,
        // but a repaint type of change (e.g. color change) does not necessarily trigger recomposition).
        if (changeRequiresRecompositeLayer(a, b, changedContextSensitiveProperties))
            return { DifferenceResult::RecompositeLayer, changedContextSensitiveProperties };

        // Cursors are not checked, since they will be set appropriately in response to mouse events,
        // so they don't need to cause any repaint or layout.

        // Animations don't need to be checked either.  We always set the new style on the RenderObject, so we will get a chance to fire off
        // the resulting transition properly.
        return { DifferenceResult::Equal, changedContextSensitiveProperties };
    }

    // MARK: - Logging

#if !LOG_DISABLED
    static void dumpDifferences(TextStream& ts, const RenderStyle& a, const RenderStyle& b)
    {
        a.nonInheritedData().dumpDifferences(ts, b.nonInheritedData());
        a.nonInheritedFlags().dumpDifferences(ts, b.nonInheritedFlags());

        a.inheritedRareData().dumpDifferences(ts, b.inheritedRareData());
        a.inheritedData().dumpDifferences(ts, b.inheritedData());
        a.inheritedFlags().dumpDifferences(ts, b.inheritedFlags());

        a.svgData().dumpDifferences(ts, b.svgData());
    }
#endif
};

// MARK: - Exported Functions

Difference difference(const RenderStyle& a, const RenderStyle& b)
{
    return DifferenceFunctions::difference(a, b);
}

bool differenceRequiresLayerRepaint(const RenderStyle& a, const RenderStyle& b, bool isComposited)
{
    return DifferenceFunctions::differenceRequiresLayerRepaint(a, b, isComposited);
}

bool borderIsEquivalentForPainting(const RenderStyle& a, const RenderStyle& b)
{
    return DifferenceFunctions::borderIsEquivalentForPainting(a, b);
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, Difference value)
{
    return ts << "style diff [" << value.result << "] (context sensitive changes " << value.contextSensitiveProperties << ")";
}

TextStream& operator<<(TextStream& ts, DifferenceResult value)
{
    switch (value) {
    case DifferenceResult::Equal: ts << "equal"_s; break;
    case DifferenceResult::RecompositeLayer: ts << "recomposite layer"_s; break;
    case DifferenceResult::Repaint: ts << "repaint"_s; break;
    case DifferenceResult::RepaintIfText: ts << "repaint if text"_s; break;
    case DifferenceResult::RepaintLayer: ts << "repaint layer"_s; break;
    case DifferenceResult::LayoutOutOfFlowMovementOnly: ts << "layout positioned movement only"_s; break;
    case DifferenceResult::Overflow: ts << "overflow"_s; break;
    case DifferenceResult::OverflowAndOutOfFlowMovement: ts << "overflow and positioned movement"_s; break;
    case DifferenceResult::Layout: ts << "layout"_s; break;
    case DifferenceResult::NewStyle: ts << "new style"_s; break;
    }
    return ts;
}


TextStream& operator<<(TextStream& ts, DifferenceContextSensitiveProperty value)
{
    switch (value) {
    case DifferenceContextSensitiveProperty::Transform: ts << "transform"_s; break;
    case DifferenceContextSensitiveProperty::Opacity: ts << "opacity"_s; break;
    case DifferenceContextSensitiveProperty::Filter: ts << "filter"_s; break;
    case DifferenceContextSensitiveProperty::ClipRect: ts << "clipRect"_s; break;
    case DifferenceContextSensitiveProperty::ClipPath: ts << "clipPath"_s; break;
    case DifferenceContextSensitiveProperty::WillChange: ts << "willChange"_s; break;
    }
    return ts;
}

#if !LOG_DISABLED
void dumpDifferences(TextStream& ts, const RenderStyle& a, const RenderStyle& b)
{
    return DifferenceFunctions::dumpDifferences(ts, a, b);
}

#endif

} // namespace Style
} // namespace WebCore
