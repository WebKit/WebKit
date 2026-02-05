/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/RenderStyle.h>

#define RENDER_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP 1
#include <WebCore/RenderStyleProperties+GettersInlines.h>
#undef RENDER_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP

#include <WebCore/StylePrimitiveNumericTypes+Rounding.h>

namespace WebCore {

// MARK: - Comparisons

inline bool RenderStyle::operator==(const RenderStyle& other) const
{
    return m_computedStyle == other.m_computedStyle;
}

inline bool RenderStyle::inheritedEqual(const RenderStyle& other) const
{
    return m_computedStyle.inheritedEqual(other.m_computedStyle);
}

inline bool RenderStyle::nonInheritedEqual(const RenderStyle& other) const
{
    return m_computedStyle.nonInheritedEqual(other.m_computedStyle);
}

inline bool RenderStyle::fastPathInheritedEqual(const RenderStyle& other) const
{
    return m_computedStyle.fastPathInheritedEqual(other.m_computedStyle);
}

inline bool RenderStyle::nonFastPathInheritedEqual(const RenderStyle& other) const
{
    return m_computedStyle.nonFastPathInheritedEqual(other.m_computedStyle);
}

inline bool RenderStyle::descendantAffectingNonInheritedPropertiesEqual(const RenderStyle& other) const
{
    return m_computedStyle.descendantAffectingNonInheritedPropertiesEqual(other.m_computedStyle);
}

inline bool RenderStyle::borderAndBackgroundEqual(const RenderStyle& other) const
{
    return m_computedStyle.borderAndBackgroundEqual(other.m_computedStyle);
}

inline bool RenderStyle::containerTypeAndNamesEqual(const RenderStyle& other) const
{
    return m_computedStyle.containerTypeAndNamesEqual(other.m_computedStyle);
}

inline bool RenderStyle::columnSpanEqual(const RenderStyle& other) const
{
    return m_computedStyle.columnSpanEqual(other.m_computedStyle);
}

inline bool RenderStyle::scrollPaddingEqual(const RenderStyle& other) const
{
    return m_computedStyle.scrollPaddingEqual(other.m_computedStyle);
}

inline bool RenderStyle::fontCascadeEqual(const RenderStyle& other) const
{
    return m_computedStyle.fontCascadeEqual(other.m_computedStyle);
}

inline bool RenderStyle::scrollSnapDataEquivalent(const RenderStyle& other) const
{
    return m_computedStyle.scrollSnapDataEquivalent(other.m_computedStyle);
}

// MARK: Derived values

inline bool RenderStyle::collapseWhiteSpace() const
{
    return collapseWhiteSpace(whiteSpaceCollapse());
}

inline bool RenderStyle::preserveNewline() const
{
    return preserveNewline(whiteSpaceCollapse());
}

inline bool RenderStyle::preserves3D() const
{
    return usedTransformStyle3D() == TransformStyle3D::Preserve3D;
}

inline bool RenderStyle::affectsTransform() const
{
    return hasTransform() || hasOffsetPath() || hasRotate() || hasScale() || hasTranslate();
}

// ignore non-standard ::-webkit-scrollbar when standard properties are in use
inline bool RenderStyle::usesStandardScrollbarStyle() const
{
    return scrollbarWidth() != Style::ScrollbarWidth::Auto || !scrollbarColor().isAuto();
}

inline bool RenderStyle::usesLegacyScrollbarStyle() const
{
    return hasPseudoStyle(PseudoElementType::WebKitScrollbar) && !usesStandardScrollbarStyle();
}

inline bool RenderStyle::shouldPlaceVerticalScrollbarOnLeft() const
{
    return !writingMode().isAnyLeftToRight();
}

inline bool RenderStyle::specifiesColumns() const
{
    return !columnCount().isAuto() || !columnWidth().isAuto() || !hasInlineColumnAxis();
}

inline bool RenderStyle::autoWrap() const
{
    return textWrapMode() != TextWrapMode::NoWrap;
}

inline bool RenderStyle::hasExplicitlySetBorderRadius() const
{
    return hasExplicitlySetBorderBottomLeftRadius()
        || hasExplicitlySetBorderBottomRightRadius()
        || hasExplicitlySetBorderTopLeftRadius()
        || hasExplicitlySetBorderTopRightRadius();
}

inline float RenderStyle::computedLineHeight() const
{
    return m_computedStyle.computedLineHeight();
}

inline float RenderStyle::computeLineHeight(const Style::LineHeight& lineHeight) const
{
    return m_computedStyle.computeLineHeight(lineHeight);
}

// MARK: - Derived used values

inline UserModify RenderStyle::usedUserModify() const
{
    return effectiveInert() ? UserModify::ReadOnly : userModify();
}

inline PointerEvents RenderStyle::usedPointerEvents() const
{
    return effectiveInert() ? PointerEvents::None : pointerEvents();
}

inline TransformStyle3D RenderStyle::usedTransformStyle3D() const
{
    return transformStyleForcedToFlat() ? TransformStyle3D::Flat : transformStyle3D();
}

inline float RenderStyle::usedPerspective() const
{
    return perspective().usedPerspective();
}

inline Visibility RenderStyle::usedVisibility() const
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

inline decltype(auto) RenderStyle::usedBorderWidths() const
{
    return RectEdgesView<true, BorderData, UsedBorderWidthsAccessor, Style::LineWidth> {
        .data = border()
    };
}

inline Style::LineWidth RenderStyle::usedBorderBottomWidth() const
{
    return usedBorderWidths().bottom();
}

inline Style::LineWidth RenderStyle::usedBorderLeftWidth() const
{
    return usedBorderWidths().left();
}

inline Style::LineWidth RenderStyle::usedBorderRightWidth() const
{
    return usedBorderWidths().right();
}

inline Style::LineWidth RenderStyle::usedBorderTopWidth() const
{
    return usedBorderWidths().top();
}

inline Style::LineWidth RenderStyle::usedBorderWidthStart(WritingMode writingMode) const
{
    return usedBorderWidths().start(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthStart() const
{
    return usedBorderWidthStart(writingMode());
}

inline Style::LineWidth RenderStyle::usedBorderWidthEnd(WritingMode writingMode) const
{
    return usedBorderWidths().end(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthEnd() const
{
    return usedBorderWidthEnd(writingMode());
}

inline Style::LineWidth RenderStyle::usedBorderWidthBefore(WritingMode writingMode) const
{
    return usedBorderWidths().before(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthBefore() const
{
    return usedBorderWidthBefore(writingMode());
}

inline Style::LineWidth RenderStyle::usedBorderWidthAfter(WritingMode writingMode) const
{
    return usedBorderWidths().after(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthAfter() const
{
    return usedBorderWidthAfter(writingMode());
}

inline Style::LineWidth RenderStyle::usedBorderWidthLogicalLeft(WritingMode writingMode) const
{
    return usedBorderWidths().logicalLeft(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthLogicalLeft() const
{
    return usedBorderWidthLogicalLeft(writingMode());
}

inline Style::LineWidth RenderStyle::usedBorderWidthLogicalRight(WritingMode writingMode) const
{
    return usedBorderWidths().logicalRight(writingMode);
}

inline Style::LineWidth RenderStyle::usedBorderWidthLogicalRight() const
{
    return usedBorderWidthLogicalRight(writingMode());
}

// MARK: - Other Predicates

inline bool RenderStyle::breakOnlyAfterWhiteSpace() const
{
    return whiteSpaceCollapse() == WhiteSpaceCollapse::Preserve || whiteSpaceCollapse() == WhiteSpaceCollapse::PreserveBreaks || whiteSpaceCollapse() == WhiteSpaceCollapse::BreakSpaces || lineBreak() == LineBreak::AfterWhiteSpace;
}

inline bool RenderStyle::breakWords() const
{
    return wordBreak() == WordBreak::BreakWord || overflowWrap() == OverflowWrap::BreakWord || overflowWrap() == OverflowWrap::Anywhere;
}

constexpr bool RenderStyle::collapseWhiteSpace(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Collapse || mode == WhiteSpaceCollapse::PreserveBreaks;
}

inline bool RenderStyle::hasInlineColumnAxis() const
{
    auto axis = columnAxis();
    return axis == ColumnAxis::Auto || writingMode().isHorizontal() == (axis == ColumnAxis::Horizontal);
}

inline bool RenderStyle::isCollapsibleWhiteSpace(char16_t character) const
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

constexpr bool RenderStyle::isDisplayBlockType(DisplayType display)
{
    return display == DisplayType::Block
        || display == DisplayType::Box
        || display == DisplayType::Flex
        || display == DisplayType::FlowRoot
        || display == DisplayType::Grid
        || display == DisplayType::GridLanes
        || display == DisplayType::ListItem
        || display == DisplayType::Table
        || display == DisplayType::RubyBlock;
}

constexpr bool RenderStyle::isDisplayInlineType(DisplayType display)
{
    return display == DisplayType::Inline
        || display == DisplayType::InlineBlock
        || display == DisplayType::InlineBox
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || display == DisplayType::InlineGridLanes
        || display == DisplayType::InlineTable
        || display == DisplayType::Ruby
        || display == DisplayType::RubyBase
        || display == DisplayType::RubyAnnotation;
}

constexpr bool RenderStyle::isDisplayRegionType() const
{
    return display() == DisplayType::Block
        || display() == DisplayType::InlineBlock
        || display() == DisplayType::TableCell
        || display() == DisplayType::TableCaption
        || display() == DisplayType::ListItem;
}

constexpr bool RenderStyle::isDisplayTableOrTablePart(DisplayType display)
{
    return display == DisplayType::Table
        || display == DisplayType::InlineTable
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

constexpr bool RenderStyle::isInternalTableBox(DisplayType display)
{
    // https://drafts.csswg.org/css-display-3/#layout-specific-display
    return display == DisplayType::TableCell
        || display == DisplayType::TableRowGroup
        || display == DisplayType::TableHeaderGroup
        || display == DisplayType::TableFooterGroup
        || display == DisplayType::TableRow
        || display == DisplayType::TableColumnGroup
        || display == DisplayType::TableColumn;
}

constexpr bool RenderStyle::isRubyContainerOrInternalRubyBox(DisplayType display)
{
    return display == DisplayType::Ruby
        || display == DisplayType::RubyAnnotation
        || display == DisplayType::RubyBase;
}

constexpr bool RenderStyle::doesDisplayGenerateBlockContainer() const
{
    auto display = this->display();
    return (display == DisplayType::Block
        || display == DisplayType::InlineBlock
        || display == DisplayType::FlowRoot
        || display == DisplayType::ListItem
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption);
}

constexpr bool RenderStyle::preserveNewline(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Preserve || mode == WhiteSpaceCollapse::PreserveBreaks || mode == WhiteSpaceCollapse::BreakSpaces;
}

inline float adjustFloatForAbsoluteZoom(float value, const RenderStyle& style)
{
    return value / style.usedZoom();
}

inline int adjustForAbsoluteZoom(int value, const RenderStyle& style)
{
    double zoomFactor = style.usedZoom();
    if (zoomFactor == 1)
        return value;
    // Needed because resolveAsLength<int> truncates (rather than rounds) when scaling up.
    if (zoomFactor > 1) {
        if (value < 0)
            value--;
        else
            value++;
    }

    return Style::roundForImpreciseConversion<int>(value / zoomFactor);
}

inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize size, const RenderStyle& style)
{
    auto zoom = style.usedZoom();
    return { size.width() / zoom, size.height() / zoom };
}

inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit value, const RenderStyle& style)
{
    return LayoutUnit(value / style.usedZoom());
}

inline float applyZoom(float value, const RenderStyle& style)
{
    return value * style.usedZoom();
}

constexpr BorderStyle collapsedBorderStyle(BorderStyle style)
{
    if (style == BorderStyle::Outset)
        return BorderStyle::Groove;
    if (style == BorderStyle::Inset)
        return BorderStyle::Ridge;
    return style;
}

inline bool RenderStyle::isInterCharacterRubyPosition() const
{
    auto rubyPosition = this->rubyPosition();
    return rubyPosition == RubyPosition::InterCharacter || rubyPosition == RubyPosition::LegacyInterCharacter;
}

inline bool generatesBox(const RenderStyle& style)
{
    return style.display() != DisplayType::None && style.display() != DisplayType::Contents;
}

inline bool isNonVisibleOverflow(Overflow overflow)
{
    return overflow == Overflow::Hidden || overflow == Overflow::Scroll || overflow == Overflow::Clip;
}

inline bool pseudoElementRendererIsNeeded(const RenderStyle* style)
{
    return style && style->display() != DisplayType::None && style->content().isData();
}

inline bool isVisibleToHitTesting(const RenderStyle& style, const HitTestRequest& request)
{
    return (request.userTriggered() ? style.usedVisibility() : style.visibility()) == Visibility::Visible;
}

inline bool shouldApplyLayoutContainment(const RenderStyle& style, const Element& element)
{
    // content-visibility hidden and auto turns on layout containment.
    auto hasContainment = style.usedContain().contains(Style::ContainValue::Layout)
        || style.contentVisibility() == ContentVisibility::Hidden
        || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element layout containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(&style)))
        return false;
    return true;
}

inline bool shouldApplySizeContainment(const RenderStyle& style, const Element& element)
{
    auto hasContainment = style.usedContain().contains(Style::ContainValue::Size)
        || style.contentVisibility() == ContentVisibility::Hidden
        || (style.contentVisibility() == ContentVisibility::Auto && !element.isRelevantToUser());
    if (!hasContainment)
        return false;
    // Giving an element size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(&style)))
        return false;
    return true;
}

inline bool shouldApplyInlineSizeContainment(const RenderStyle& style, const Element& element)
{
    if (!style.usedContain().contains(Style::ContainValue::InlineSize))
        return false;
    // Giving an element inline-size containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its inner display type is table
    //   if its principal box is an internal table box
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.display() == DisplayType::Table || style.display() == DisplayType::InlineTable)
        return false;
    if (style.isInternalTableBox())
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(&style)))
        return false;
    return true;
}

inline bool shouldApplyStyleContainment(const RenderStyle& style, const Element&)
{
    // content-visibility hidden and auto turns on style containment.
    return style.usedContain().contains(Style::ContainValue::Style)
        || style.contentVisibility() == ContentVisibility::Hidden
        || style.contentVisibility() == ContentVisibility::Auto;
}

inline bool shouldApplyPaintContainment(const RenderStyle& style, const Element& element)
{
    // content-visibility hidden and auto turns on paint containment.
    auto hasContainment = style.usedContain().contains(Style::ContainValue::Paint)
        || style.contentVisibility() == ContentVisibility::Hidden
        || style.contentVisibility() == ContentVisibility::Auto;
    if (!hasContainment)
        return false;
    // Giving an element paint containment has no effect if any of the following are true:
    //   if the element does not generate a principal box (as is the case with display: contents or display: none)
    //   if its principal box is an internal table box other than table-cell
    //   if its principal box is an internal ruby box or a non-atomic inline-level box
    if (style.display() == DisplayType::None || style.display() == DisplayType::Contents)
        return false;
    if (style.isInternalTableBox() && style.display() != DisplayType::TableCell)
        return false;
    if (style.isRubyContainerOrInternalRubyBox() || (style.display() == DisplayType::Inline && !element.isReplaced(&style)))
        return false;
    return true;
}

inline bool isSkippedContentRoot(const RenderStyle& style, const Element& element)
{
    if (!shouldApplySizeContainment(style, element))
        return false;

    switch (style.contentVisibility()) {
    case ContentVisibility::Visible:
        return false;
    case ContentVisibility::Hidden:
        return true;
    case ContentVisibility::Auto:
        return !element.isRelevantToUser();
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

// MARK: has*() functions

inline bool RenderStyle::hasAnimations() const
{
    return !animations().isInitial();
}

inline bool RenderStyle::hasAnimationsOrTransitions() const
{
    return hasAnimations() || hasTransitions();
}

// FIXME: Rename this function.
inline bool RenderStyle::hasAppearance() const
{
    return appearance() != StyleAppearance::None && appearance() != StyleAppearance::Base;
}

#if HAVE(CORE_MATERIAL)
inline bool RenderStyle::hasAppleVisualEffect() const
{
    return appleVisualEffect() != AppleVisualEffect::None;
}

inline bool RenderStyle::hasAppleVisualEffectRequiringBackdropFilter() const
{
    return appleVisualEffectNeedsBackdrop(appleVisualEffect());
}

#endif
inline bool RenderStyle::hasAspectRatio() const
{
    return aspectRatio().hasRatio();
}

inline bool RenderStyle::hasAutoLeftAndRight() const
{
    return left().isAuto() && right().isAuto();
}

inline bool RenderStyle::hasAutoLengthContainIntrinsicSize() const
{
    return containIntrinsicWidth().hasAuto() || containIntrinsicHeight().hasAuto();
}

inline bool RenderStyle::hasAutoTopAndBottom() const
{
    return top().isAuto() && bottom().isAuto();
}

inline bool RenderStyle::hasBackdropFilter() const
{
    return !backdropFilter().isNone();
}

inline bool RenderStyle::hasBackground() const
{
    return visitedDependentBackgroundColor().isVisible() || hasBackgroundImage();
}

inline bool RenderStyle::hasBackgroundImage() const
{
    return Style::hasImageInAnyLayer(backgroundLayers());
}

inline bool RenderStyle::hasBlendMode() const
{
    return blendMode() != BlendMode::Normal;
}

inline bool RenderStyle::hasBorder() const
{
    return border().hasBorder();
}

inline bool RenderStyle::hasBorderImage() const
{
    return border().hasBorderImage();
}

inline bool RenderStyle::hasBorderImageOutsets() const
{
    return !borderImageSource().isNone() && !borderImageOutset().isZero();
}

inline bool RenderStyle::hasBorderRadius() const
{
    return border().hasBorderRadius();
}

inline bool RenderStyle::hasBoxReflect() const
{
    return !boxReflect().isNone();
}

inline bool RenderStyle::hasBoxShadow() const
{
    return !boxShadow().isNone();
}

inline bool RenderStyle::hasClip() const
{
    return !clip().isAuto();
}

inline bool RenderStyle::hasClipPath() const
{
    return !clipPath().isNone();
}

inline bool RenderStyle::hasContent() const
{
    return content().isData();
}

inline bool RenderStyle::hasFill() const
{
    return !fill().isNone();
}

inline bool RenderStyle::hasFilter() const
{
    return !filter().isNone();
}

inline bool RenderStyle::hasInFlowPosition() const
{
    return position() == PositionType::Relative || position() == PositionType::Sticky;
}

inline bool RenderStyle::hasIsolation() const
{
    return isolation() != Isolation::Auto;
}

inline bool RenderStyle::hasMarkers() const
{
    return !markerStart().isNone() || !markerMid().isNone() || !markerEnd().isNone();
}

inline bool RenderStyle::hasMask() const
{
    return Style::hasImageInAnyLayer(maskLayers()) || !maskBorderSource().isNone();
}

inline bool RenderStyle::hasOffsetPath() const
{
    return !WTF::holdsAlternative<CSS::Keyword::None>(offsetPath());
}

inline bool RenderStyle::hasOpacity() const
{
    return !opacity().isOpaque();
}

inline bool RenderStyle::hasOutline() const
{
    return outlineStyle() != OutlineStyle::None && usedOutlineWidth().isPositive();
}

inline bool RenderStyle::hasOutlineInVisualOverflow() const
{
    return hasOutline() && usedOutlineSize() > 0;
}

inline bool RenderStyle::hasOutOfFlowPosition() const
{
    return position() == PositionType::Absolute || position() == PositionType::Fixed;
}

inline bool RenderStyle::hasPerspective() const
{
    return !perspective().isNone();
}

inline bool RenderStyle::hasPositionedMask() const
{
    return Style::hasImageInAnyLayer(maskLayers());
}

inline bool RenderStyle::hasRotate() const
{
    return !rotate().isNone();
}

inline bool RenderStyle::hasScale() const
{
    return !scale().isNone();
}

inline bool RenderStyle::hasScrollTimelines() const
{
    return !scrollTimelines().isEmpty() || !scrollTimelineNames().isNone();
}

inline bool RenderStyle::hasSnapPosition() const
{
    return !scrollSnapAlign().isNone();
}

inline bool RenderStyle::hasStaticBlockPosition(bool horizontal) const
{
    return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight();
}

inline bool RenderStyle::hasStaticInlinePosition(bool horizontal) const
{
    return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom();
}

inline bool RenderStyle::hasStroke() const
{
    return !stroke().isNone();
}

inline bool RenderStyle::hasTextCombine() const
{
    return textCombine() != TextCombine::None;
}

inline bool RenderStyle::hasTextShadow() const
{
    return !textShadow().isNone();
}

inline bool RenderStyle::hasTransform() const
{
    return !transform().isNone() || hasOffsetPath();
}

inline bool RenderStyle::hasTransformRelatedProperty() const
{
    return hasTransform() || hasRotate() || hasScale() || hasTranslate() || transformStyle3D() == TransformStyle3D::Preserve3D || hasPerspective();
}

inline bool RenderStyle::hasTransitions() const
{
    return !transitions().isInitial();
}

inline bool RenderStyle::hasTranslate() const
{
    return !translate().isNone();
}

inline bool RenderStyle::hasUsedAppearance() const
{
    return usedAppearance() != StyleAppearance::None && usedAppearance() != StyleAppearance::Base;
}

inline bool RenderStyle::hasUsedContentNone() const
{
    return content().isNone() || (content().isNormal() && (pseudoElementType() == PseudoElementType::Before || pseudoElementType() == PseudoElementType::After));
}

inline bool RenderStyle::hasViewportConstrainedPosition() const
{
    return position() == PositionType::Fixed || position() == PositionType::Sticky;
}

inline bool RenderStyle::hasViewTimelines() const
{
    return !viewTimelines().isEmpty() || !viewTimelineNames().isNone();
}

inline bool RenderStyle::hasVisibleBorder() const
{
    return border().hasVisibleBorder();
}

inline bool RenderStyle::hasVisibleBorderDecoration() const
{
    return hasVisibleBorder() || hasBorderImage();
}

inline bool RenderStyle::hasPositiveStrokeWidth() const
{
    if (!hasExplicitlySetStrokeWidth())
        return textStrokeWidth().isPositive();
    return strokeWidth().isPossiblyPositive();
}

// MARK: is*() functions

inline bool RenderStyle::isColumnFlexDirection() const
{
    return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse;
}

inline bool RenderStyle::isRowFlexDirection() const
{
    return flexDirection() == FlexDirection::Row || flexDirection() == FlexDirection::RowReverse;
}

constexpr bool RenderStyle::isDisplayBlockLevel() const
{
    return isDisplayBlockType(display());
}

constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox(DisplayType display)
{
    return display == DisplayType::Box || display == DisplayType::InlineBox;
}

constexpr bool RenderStyle::isDisplayFlexibleBox(DisplayType display)
{
    return display == DisplayType::Flex || display == DisplayType::InlineFlex;
}

constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox() const
{
    return isDisplayDeprecatedFlexibleBox(display());
}

constexpr bool RenderStyle::isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox() const
{
    return isDisplayFlexibleOrGridFormattingContextBox() || isDisplayDeprecatedFlexibleBox();
}

constexpr bool RenderStyle::isDisplayFlexibleOrGridFormattingContextBox() const
{
    return isDisplayFlexibleOrGridFormattingContextBox(display());
}

constexpr bool RenderStyle::isDisplayFlexibleOrGridFormattingContextBox(DisplayType display)
{
    return isDisplayFlexibleBox(display) || isDisplayGridFormattingContextBox(display);
}

constexpr bool RenderStyle::isDisplayGridFormattingContextBox(DisplayType display)
{
    return isDisplayGridBox(display) || isDisplayGridLanesBox(display);
}

constexpr bool RenderStyle::isDisplayGridBox(DisplayType display)
{
    return display == DisplayType::Grid || display == DisplayType::InlineGrid;
}

constexpr bool RenderStyle::isDisplayGridLanesBox(DisplayType display)
{
    return display == DisplayType::GridLanes || display == DisplayType::InlineGridLanes;
}

constexpr bool RenderStyle::isDisplayInlineType() const
{
    return isDisplayInlineType(display());
}

constexpr bool RenderStyle::isDisplayListItemType(DisplayType display)
{
    return display == DisplayType::ListItem;
}

constexpr bool RenderStyle::isDisplayTableOrTablePart() const
{
    return isDisplayTableOrTablePart(display());
}

constexpr bool RenderStyle::isInternalTableBox() const
{
    return isInternalTableBox(display());
}

constexpr bool RenderStyle::isRubyContainerOrInternalRubyBox() const
{
    return isRubyContainerOrInternalRubyBox(display());
}

inline bool RenderStyle::isFixedTableLayout() const
{
    return tableLayout() == TableLayoutType::Fixed 
        && (logicalWidth().isSpecified() 
            || logicalWidth().isFitContent() 
            || logicalWidth().isFillAvailable() 
            || logicalWidth().isMinContent());
}

inline bool RenderStyle::isFloating() const
{
    return floating() != Float::None;
}

constexpr bool RenderStyle::isOriginalDisplayBlockType() const
{
    return isDisplayBlockType(originalDisplay());
}

constexpr bool RenderStyle::isOriginalDisplayInlineType() const
{
    return isDisplayInlineType(originalDisplay());
}

constexpr bool RenderStyle::isOriginalDisplayListItemType() const
{
    return isDisplayListItemType(originalDisplay());
}

inline bool RenderStyle::isOverflowVisible() const
{
    return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible;
}

inline bool RenderStyle::isReverseFlexDirection() const
{
    return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse;
}

inline bool RenderStyle::isSkippedRootOrSkippedContent() const
{
    return usedContentVisibility() != ContentVisibility::Visible;
}

// MARK: - Logical getters

// MARK: logical inset value aliases

inline const Style::InsetEdge& RenderStyle::logicalTop() const
{
    return insetBefore();
}

inline const Style::InsetEdge& RenderStyle::logicalRight() const
{
    return insetLogicalRight();
}

inline const Style::InsetEdge& RenderStyle::logicalBottom() const
{
    return insetAfter();
}

inline const Style::InsetEdge& RenderStyle::logicalLeft() const
{
    return insetLogicalLeft();
}

// MARK: logical aggregate border values

inline const BorderValue& RenderStyle::borderBefore() const
{
    return borderBefore(writingMode());
}

inline const BorderValue& RenderStyle::borderAfter() const
{
    return borderAfter(writingMode());
}

inline const BorderValue& RenderStyle::borderStart() const
{
    return borderStart(writingMode());
}

inline const BorderValue& RenderStyle::borderEnd() const
{
    return borderEnd(writingMode());
}

// MARK: logical aspect-ratio values

inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalHeight() const
{
    return writingMode().isHorizontal() ? aspectRatio().height() : aspectRatio().width();
}

inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalWidth() const
{
    return writingMode().isHorizontal() ? aspectRatio().width() : aspectRatio().height();
}

inline double RenderStyle::logicalAspectRatio() const
{
    auto ratio = this->aspectRatio().tryRatio();
    ASSERT(ratio);

    if (writingMode().isHorizontal())
        return ratio->numerator.value / ratio->denominator.value;
    return ratio->denominator.value / ratio->numerator.value;
}

inline BoxSizing RenderStyle::boxSizingForAspectRatio() const
{
    return aspectRatio().isAutoAndRatio() ? BoxSizing::ContentBox : boxSizing();
}


// MARK: logical grid values

inline const Style::GapGutter& RenderStyle::gap(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? columnGap() : rowGap();
}

inline const Style::GridTrackSizes& RenderStyle::gridAutoList(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridAutoColumns() : gridAutoRows();
}

inline const Style::GridPosition& RenderStyle::gridItemEnd(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnEnd() : gridItemRowEnd();
}

inline const Style::GridPosition& RenderStyle::gridItemStart(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnStart() : gridItemRowStart();
}

inline const Style::GridTemplateList& RenderStyle::gridTemplateList(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? gridTemplateColumns() : gridTemplateRows();
}

} // namespace WebCore
