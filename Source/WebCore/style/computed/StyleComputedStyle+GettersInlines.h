/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleComputedStyle.h>

#define COMPUTED_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP 1
#include <WebCore/StyleComputedStyleProperties+GettersInlines.h>
#undef COMPUTED_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP

namespace WebCore {
namespace Style {

inline bool ComputedStyle::columnSpanEqual(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->miscData.ptr() == other.m_nonInheritedData->miscData.ptr()
        || m_nonInheritedData->miscData->multiCol.ptr() == other.m_nonInheritedData->miscData->multiCol.ptr())
        return true;

    return m_nonInheritedData->miscData->multiCol->columnSpan == other.m_nonInheritedData->miscData->multiCol->columnSpan;
}

inline bool ComputedStyle::containerTypeAndNamesEqual(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return containerType() == other.containerType() && containerNames() == other.containerNames();
}

inline bool ComputedStyle::scrollPaddingEqual(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollPadding == other.m_nonInheritedData->rareData->scrollPadding;
}

inline bool ComputedStyle::fontCascadeEqual(const ComputedStyle& other) const
{
    return m_inheritedData.ptr() == other.m_inheritedData.ptr()
        || m_inheritedData->fontData.ptr() == other.m_inheritedData->fontData.ptr()
        || m_inheritedData->fontData->fontCascade == other.m_inheritedData->fontData->fontCascade;
}

// MARK: - Derived values

inline bool ComputedStyle::collapseWhiteSpace() const
{
    return collapseWhiteSpace(whiteSpaceCollapse());
}

inline bool ComputedStyle::preserveNewline() const
{
    return preserveNewline(whiteSpaceCollapse());
}

inline bool ComputedStyle::affectsTransform() const
{
    return !transform().isNone()
        || !offsetPath().isNone()
        || !offsetPath().isNone()
        || !rotate().isNone()
        || !scale().isNone()
        || !translate().isNone();
}

// ignore non-standard ::-webkit-scrollbar when standard properties are in use
inline bool ComputedStyle::usesStandardScrollbarStyle() const
{
    return scrollbarWidth() != Style::ScrollbarWidth::Auto || !scrollbarColor().isAuto();
}

inline bool ComputedStyle::usesLegacyScrollbarStyle() const
{
    return hasPseudoStyle(PseudoElementType::WebKitScrollbar) && !usesStandardScrollbarStyle();
}

inline bool ComputedStyle::shouldPlaceVerticalScrollbarOnLeft() const
{
    return !writingMode().isAnyLeftToRight();
}

inline bool ComputedStyle::specifiesColumns() const
{
    return !columnCount().isAuto() || !columnWidth().isAuto() || !hasInlineColumnAxis();
}

inline bool ComputedStyle::hasExplicitlySetBorderRadius() const
{
    return hasExplicitlySetBorderBottomLeftRadius()
        || hasExplicitlySetBorderBottomRightRadius()
        || hasExplicitlySetBorderTopLeftRadius()
        || hasExplicitlySetBorderTopRightRadius();
}

// MARK: - Derived used values

inline UserModify ComputedStyle::usedUserModify() const
{
    return effectiveInert() ? UserModify::ReadOnly : userModify();
}

inline PointerEvents ComputedStyle::usedPointerEvents() const
{
    return effectiveInert() ? PointerEvents::None : pointerEvents();
}

inline TransformStyle3D ComputedStyle::usedTransformStyle3D() const
{
    return transformStyleForcedToFlat() ? TransformStyle3D::Flat : transformStyle3D();
}

inline float ComputedStyle::usedPerspective() const
{
    return perspective().usedPerspective();
}

inline Visibility ComputedStyle::usedVisibility() const
{
    if (isForceHidden()) [[unlikely]]
        return Visibility::Hidden;
    return visibility();
}

template<BoxSide side> struct UsedBorderWidthsAccessor {
    static Style::LineWidth get(const BorderData& data)
    {
        using namespace CSS::Literals;

        if (!data.edges[side].hasVisibleStyle())
            return 0_css_px;
        if (data.borderImage->borderImage.borderImageWidth.overridesBorderWidths()) {
            if (auto fixedBorderWidthValue = data.borderImage->borderImage.borderImageWidth.values[side].tryFixed())
                return Style::LineWidth { fixedBorderWidthValue->unresolvedValue() };
        }
        return data.edges[side].width;
    }
};

inline decltype(auto) ComputedStyle::usedBorderWidths() const
{
    return RectEdgesView<true, BorderData, UsedBorderWidthsAccessor, Style::LineWidth> {
        .data = border()
    };
}

inline Style::LineWidth ComputedStyle::usedBorderBottomWidth() const
{
    return usedBorderWidths().bottom();
}

inline Style::LineWidth ComputedStyle::usedBorderLeftWidth() const
{
    return usedBorderWidths().left();
}

inline Style::LineWidth ComputedStyle::usedBorderRightWidth() const
{
    return usedBorderWidths().right();
}

inline Style::LineWidth ComputedStyle::usedBorderTopWidth() const
{
    return usedBorderWidths().top();
}

inline Style::LineWidth ComputedStyle::usedBorderWidthStart(WritingMode writingMode) const
{
    return usedBorderWidths().start(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthStart() const
{
    return usedBorderWidthStart(writingMode());
}

inline Style::LineWidth ComputedStyle::usedBorderWidthEnd(WritingMode writingMode) const
{
    return usedBorderWidths().end(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthEnd() const
{
    return usedBorderWidthEnd(writingMode());
}

inline Style::LineWidth ComputedStyle::usedBorderWidthBefore(WritingMode writingMode) const
{
    return usedBorderWidths().before(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthBefore() const
{
    return usedBorderWidthBefore(writingMode());
}

inline Style::LineWidth ComputedStyle::usedBorderWidthAfter(WritingMode writingMode) const
{
    return usedBorderWidths().after(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthAfter() const
{
    return usedBorderWidthAfter(writingMode());
}

inline Style::LineWidth ComputedStyle::usedBorderWidthLogicalLeft(WritingMode writingMode) const
{
    return usedBorderWidths().logicalLeft(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthLogicalLeft() const
{
    return usedBorderWidthLogicalLeft(writingMode());
}

inline Style::LineWidth ComputedStyle::usedBorderWidthLogicalRight(WritingMode writingMode) const
{
    return usedBorderWidths().logicalRight(writingMode);
}

inline Style::LineWidth ComputedStyle::usedBorderWidthLogicalRight() const
{
    return usedBorderWidthLogicalRight(writingMode());
}

// MARK: - Other Predicates

inline bool ComputedStyle::breakOnlyAfterWhiteSpace() const
{
    return whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve || whiteSpaceCollapse() == WhiteSpaceCollapse::PreserveBreaks || whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool ComputedStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::BreakWord || overflowWrap() == OverflowWrap::Anywhere;
}

constexpr bool ComputedStyle::collapseWhiteSpace(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Collapse || mode == WhiteSpaceCollapse::PreserveBreaks;
}

inline bool ComputedStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || writingMode().isHorizontal() == (axis == ColumnAxis::Horizontal);
}

inline bool ComputedStyle::isCollapsibleWhiteSpace(char16_t character) const
{
    switch (character) {
    case ' ':
    case '\t':
        return collapseWhiteSpace();
    case '\n':
        return !preserveNewline();
    default:
        return false;
    }
}


constexpr bool ComputedStyle::preserveNewline(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Preserve || mode == WhiteSpaceCollapse::PreserveBreaks || mode == WhiteSpaceCollapse::BreakSpaces;
}

inline bool ComputedStyle::isInterCharacterRubyPosition() const
{
    auto rubyPosition = this->rubyPosition();
    return rubyPosition == RubyPosition::InterCharacter || rubyPosition == RubyPosition::LegacyInterCharacter;
}

// MARK: has*() functions

inline bool ComputedStyle::hasBackground() const
{
    return visitedDependentBackgroundColor().isVisible()
        || Style::hasImageInAnyLayer(backgroundLayers());
}

inline bool ComputedStyle::hasBorderImageOutsets() const
{
    return !borderImageSource().isNone() && !borderImageOutset().isZero();
}

inline bool ComputedStyle::hasInFlowPosition() const
{
    return position() == PositionType::Relative || position() == PositionType::Sticky;
}

inline bool ComputedStyle::hasMarkers() const
{
    return !markerStart().isNone() || !markerMid().isNone() || !markerEnd().isNone();
}

inline bool ComputedStyle::hasMask() const
{
    return Style::hasImageInAnyLayer(maskLayers()) || !maskBorderSource().isNone();
}

inline bool ComputedStyle::hasOutline() const
{
    return outlineStyle() != OutlineStyle::None && usedOutlineWidth().isPositive();
}

inline bool ComputedStyle::hasOutlineInVisualOverflow() const
{
    return hasOutline() && usedOutlineSize() > 0;
}

inline bool ComputedStyle::hasOutOfFlowPosition() const
{
    return position() == PositionType::Absolute || position() == PositionType::Fixed;
}

inline bool ComputedStyle::hasPositionedMask() const
{
    return Style::hasImageInAnyLayer(maskLayers());
}

inline bool ComputedStyle::hasStaticBlockPosition(bool horizontal) const
{
    return horizontal
        ? (top().isAuto() && bottom().isAuto())
        : (left().isAuto() && right().isAuto());
}

inline bool ComputedStyle::hasStaticInlinePosition(bool horizontal) const
{
    return horizontal
        ? (left().isAuto() && right().isAuto())
        : (top().isAuto() && bottom().isAuto());
}

inline bool ComputedStyle::hasTransformRelatedProperty() const
{
    return !transform().isNone()
        || !offsetPath().isNone()
        || !rotate().isNone()
        || !scale().isNone()
        || !translate().isNone()
        || transformStyle3D() == TransformStyle3D::Preserve3D
        || !perspective().isNone();
}

inline bool ComputedStyle::has3DTransformOperation() const
{
    return transform().has3DOperation()
        || translate().is3DOperation()
        || scale().is3DOperation()
        || rotate().is3DOperation();
}

inline bool ComputedStyle::hasUsedAppearance() const
{
    return usedAppearance() != StyleAppearance::None && usedAppearance() != StyleAppearance::Base;
}

inline bool ComputedStyle::hasUsedContentNone() const
{
    return content().isNone() || (content().isNormal() && (pseudoElementType() == PseudoElementType::Before || pseudoElementType() == PseudoElementType::After));
}

inline bool ComputedStyle::hasViewportConstrainedPosition() const
{
    return position() == PositionType::Fixed || position() == PositionType::Sticky;
}

inline bool ComputedStyle::hasPositiveStrokeWidth() const
{
    if (!hasExplicitlySetStrokeWidth())
        return textStrokeWidth().isPositive();
    return strokeWidth().isPossiblyPositive();
}

// MARK: is*() functions

inline bool ComputedStyle::isColumnFlexDirection() const
{
    return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse;
}

inline bool ComputedStyle::isRowFlexDirection() const
{
    return flexDirection() == FlexDirection::Row || flexDirection() == FlexDirection::RowReverse;
}

inline bool ComputedStyle::isFixedTableLayout() const
{
    return tableLayout() == TableLayoutType::Fixed
        && (logicalWidth().isSpecified()
            || logicalWidth().isFitContent()
            || logicalWidth().isStretch()
            || logicalWidth().isMinContent());
}

inline bool ComputedStyle::isOverflowVisible() const
{
    return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible;
}

inline bool ComputedStyle::isReverseFlexDirection() const
{
    return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse;
}

inline bool ComputedStyle::isSkippedRootOrSkippedContent() const
{
    return usedContentVisibility() != ContentVisibility::Visible;
}

// MARK: - Logical getters

// MARK: logical inset value aliases

inline const Style::InsetEdge& ComputedStyle::logicalTop() const
{
    return insetBefore();
}

inline const Style::InsetEdge& ComputedStyle::logicalRight() const
{
    return insetLogicalRight();
}

inline const Style::InsetEdge& ComputedStyle::logicalBottom() const
{
    return insetAfter();
}

inline const Style::InsetEdge& ComputedStyle::logicalLeft() const
{
    return insetLogicalLeft();
}

// MARK: logical aggregate border values

inline const BorderValue& ComputedStyle::borderBefore() const
{
    return borderBefore(writingMode());
}

inline const BorderValue& ComputedStyle::borderAfter() const
{
    return borderAfter(writingMode());
}

inline const BorderValue& ComputedStyle::borderStart() const
{
    return borderStart(writingMode());
}

inline const BorderValue& ComputedStyle::borderEnd() const
{
    return borderEnd(writingMode());
}

// MARK: logical aspect-ratio values

inline Style::Number<CSS::Nonnegative> ComputedStyle::aspectRatioLogicalHeight() const
{
    return writingMode().isHorizontal() ? aspectRatio().height() : aspectRatio().width();
}

inline Style::Number<CSS::Nonnegative> ComputedStyle::aspectRatioLogicalWidth() const
{
    return writingMode().isHorizontal() ? aspectRatio().width() : aspectRatio().height();
}

inline double ComputedStyle::logicalAspectRatio() const
{
    auto ratio = this->aspectRatio().tryRatio();
    ASSERT(ratio);

    if (writingMode().isHorizontal())
        return ratio->numerator.value / ratio->denominator.value;
    return ratio->denominator.value / ratio->numerator.value;
}

inline BoxSizing ComputedStyle::boxSizingForAspectRatio() const
{
    return aspectRatio().isAutoAndRatio() ? BoxSizing::ContentBox : boxSizing();
}


// MARK: logical grid values

inline const Style::GapGutter& ComputedStyle::gap(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? columnGap() : rowGap();
}

inline const Style::GridTrackSizes& ComputedStyle::gridAutoList(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridAutoColumns() : gridAutoRows();
}

inline const Style::GridPosition& ComputedStyle::gridItemEnd(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnEnd() : gridItemRowEnd();
}

inline const Style::GridPosition& ComputedStyle::gridItemStart(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnStart() : gridItemRowStart();
}

inline const Style::GridTemplateList& ComputedStyle::gridTemplateList(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridTemplateColumns() : gridTemplateRows();
}

} // namespace Style
} // namespace WebCore
