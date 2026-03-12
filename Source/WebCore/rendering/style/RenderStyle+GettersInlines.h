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

// MARK: - Non-property getters

inline bool RenderStyle::usesViewportUnits() const
{
    return m_computedStyle.usesViewportUnits();
}

inline bool RenderStyle::usesContainerUnits() const
{
    return m_computedStyle.usesContainerUnits();
}

inline bool RenderStyle::useTreeCountingFunctions() const
{
    return m_computedStyle.useTreeCountingFunctions();
}

inline InsideLink RenderStyle::insideLink() const
{
    return m_computedStyle.insideLink();
}

inline bool RenderStyle::isLink() const
{
    return m_computedStyle.isLink();
}

inline bool RenderStyle::emptyState() const
{
    return m_computedStyle.emptyState();
}

inline bool RenderStyle::firstChildState() const
{
    return m_computedStyle.firstChildState();
}

inline bool RenderStyle::lastChildState() const
{
    return m_computedStyle.lastChildState();
}

inline bool RenderStyle::hasExplicitlyInheritedProperties() const
{
    return m_computedStyle.hasExplicitlyInheritedProperties();
}

inline bool RenderStyle::disallowsFastPathInheritance() const
{
    return m_computedStyle.disallowsFastPathInheritance();
}

inline bool RenderStyle::effectiveInert() const
{
    return m_computedStyle.effectiveInert();
}

inline bool RenderStyle::isEffectivelyTransparent() const
{
    return m_computedStyle.isEffectivelyTransparent();
}

inline bool RenderStyle::insideDefaultButton() const
{
    return m_computedStyle.insideDefaultButton();
}

inline bool RenderStyle::insideSubmitButton() const
{
    return m_computedStyle.insideSubmitButton();
}

inline bool RenderStyle::isForceHidden() const
{
    return m_computedStyle.isForceHidden();
}

inline bool RenderStyle::hasDisplayAffectedByAnimations() const
{
    return m_computedStyle.hasDisplayAffectedByAnimations();
}

inline bool RenderStyle::transformStyleForcedToFlat() const
{
    return m_computedStyle.transformStyleForcedToFlat();
}

inline bool RenderStyle::usesAnchorFunctions() const
{
    return m_computedStyle.usesAnchorFunctions();
}

inline EnumSet<BoxAxis> RenderStyle::anchorFunctionScrollCompensatedAxes() const
{
    return m_computedStyle.anchorFunctionScrollCompensatedAxes();
}

inline bool RenderStyle::isPopoverInvoker() const
{
    return m_computedStyle.isPopoverInvoker();
}

inline bool RenderStyle::autoRevealsWhenFound() const
{
    return m_computedStyle.autoRevealsWhenFound();
}

inline bool RenderStyle::nativeAppearanceDisabled() const
{
    return m_computedStyle.nativeAppearanceDisabled();
}

inline OptionSet<EventListenerRegionType> RenderStyle::eventListenerRegionTypes() const
{
    return m_computedStyle.eventListenerRegionTypes();
}

inline bool RenderStyle::hasAttrContent() const
{
    return m_computedStyle.hasAttrContent();
}

inline std::optional<size_t> RenderStyle::usedPositionOptionIndex() const
{
    return m_computedStyle.usedPositionOptionIndex();
}

inline constexpr Style::Display RenderStyle::originalDisplay() const
{
    return m_computedStyle.originalDisplay();
}

inline StyleAppearance RenderStyle::usedAppearance() const
{
    return m_computedStyle.usedAppearance();
}

inline ContentVisibility RenderStyle::usedContentVisibility() const
{
    return m_computedStyle.usedContentVisibility();
}

inline Style::TouchAction RenderStyle::usedTouchAction() const
{
    return m_computedStyle.usedTouchAction();
}

inline Style::ZIndex RenderStyle::usedZIndex() const
{
    return m_computedStyle.usedZIndex();
}

#if HAVE(CORE_MATERIAL)

inline AppleVisualEffect RenderStyle::usedAppleVisualEffectForSubtree() const
{
    return m_computedStyle.usedAppleVisualEffectForSubtree();
}

#endif

#if ENABLE(TEXT_AUTOSIZING)

inline AutosizeStatus RenderStyle::autosizeStatus() const
{
    return m_computedStyle.autosizeStatus();
}

#endif // ENABLE(TEXT_AUTOSIZING)

// MARK: - Pseudo element/style

inline bool RenderStyle::hasAnyPublicPseudoStyles() const
{
    return m_computedStyle.hasAnyPublicPseudoStyles();
}

inline bool RenderStyle::hasPseudoStyle(PseudoElementType pseudo) const
{
    return m_computedStyle.hasPseudoStyle(pseudo);
}

inline std::optional<PseudoElementType> RenderStyle::pseudoElementType() const
{
    return m_computedStyle.pseudoElementType();
}

inline const AtomString& RenderStyle::pseudoElementNameArgument() const
{
    return m_computedStyle.pseudoElementNameArgument();
}

inline std::optional<Style::PseudoElementIdentifier> RenderStyle::pseudoElementIdentifier() const
{
    return m_computedStyle.pseudoElementIdentifier();
}

inline RenderStyle* RenderStyle::getCachedPseudoStyle(const Style::PseudoElementIdentifier& pseudoElementIdentifier) const
{
    return m_computedStyle.getCachedPseudoStyle(pseudoElementIdentifier);
}

// MARK: - Custom properties

inline const Style::CustomPropertyData& RenderStyle::inheritedCustomProperties() const
{
    return m_computedStyle.inheritedCustomProperties();
}

inline const Style::CustomPropertyData& RenderStyle::nonInheritedCustomProperties() const
{
    return m_computedStyle.nonInheritedCustomProperties();
}

inline const Style::CustomProperty* RenderStyle::customPropertyValue(const AtomString& property) const
{
    return m_computedStyle.customPropertyValue(property);
}

inline bool RenderStyle::customPropertyValueEqual(const RenderStyle& other, const AtomString& property) const
{
    return m_computedStyle.customPropertyValueEqual(other.m_computedStyle, property);
}

inline bool RenderStyle::customPropertiesEqual(const RenderStyle& other) const
{
    return m_computedStyle.customPropertiesEqual(other.m_computedStyle);
}

inline void RenderStyle::deduplicateCustomProperties(const RenderStyle& other)
{
    m_computedStyle.deduplicateCustomProperties(other.m_computedStyle);
}

// MARK: - Custom paint

inline void RenderStyle::addCustomPaintWatchProperty(const AtomString& property)
{
    m_computedStyle.addCustomPaintWatchProperty(property);
}

// MARK: - Zoom

inline bool RenderStyle::evaluationTimeZoomEnabled() const
{
    return m_computedStyle.evaluationTimeZoomEnabled();
}

inline bool RenderStyle::useSVGZoomRulesForLength() const
{
    return m_computedStyle.useSVGZoomRulesForLength();
}

inline float RenderStyle::usedZoom() const
{
    return m_computedStyle.usedZoom();
}

inline Style::ZoomFactor RenderStyle::usedZoomForLength() const
{
    return m_computedStyle.usedZoomForLength();
}

// MARK: - Fonts

inline const FontCascade& RenderStyle::fontCascade() const
{
    return m_computedStyle.fontCascade();
}

inline FontCascade& RenderStyle::mutableFontCascadeWithoutUpdate()
{
    return m_computedStyle.mutableFontCascadeWithoutUpdate();
}

inline void RenderStyle::setFontCascade(FontCascade&& fontCascade)
{
    m_computedStyle.setFontCascade(WTF::move(fontCascade));
}

inline const FontCascadeDescription& RenderStyle::fontDescription() const
{
    return m_computedStyle.fontDescription();
}

inline FontCascadeDescription& RenderStyle::mutableFontDescriptionWithoutUpdate()
{
    return m_computedStyle.mutableFontDescriptionWithoutUpdate();
}

inline void RenderStyle::setFontDescription(FontCascadeDescription&& description)
{
    m_computedStyle.setFontDescription(WTF::move(description));
}

inline bool RenderStyle::setFontDescriptionWithoutUpdate(FontCascadeDescription&& description)
{
    return m_computedStyle.setFontDescriptionWithoutUpdate(WTF::move(description));
}

inline const FontMetrics& RenderStyle::metricsOfPrimaryFont() const
{
    return m_computedStyle.metricsOfPrimaryFont();
}

inline std::pair<FontOrientation, NonCJKGlyphOrientation> RenderStyle::fontAndGlyphOrientation()
{
    return m_computedStyle.fontAndGlyphOrientation();
}

inline Style::WebkitLocale RenderStyle::computedLocale() const
{
    return m_computedStyle.computedLocale();
}

inline float RenderStyle::computedFontSize() const
{
    return m_computedStyle.computedFontSize();
}

inline const Style::LineHeight& RenderStyle::specifiedLineHeight() const
{
    return m_computedStyle.specifiedLineHeight();
}

inline void RenderStyle::synchronizeLetterSpacingWithFontCascade()
{
    m_computedStyle.synchronizeLetterSpacingWithFontCascade();
}

inline void RenderStyle::synchronizeLetterSpacingWithFontCascadeWithoutUpdate()
{
    m_computedStyle.synchronizeLetterSpacingWithFontCascadeWithoutUpdate();
}

inline void RenderStyle::synchronizeWordSpacingWithFontCascade()
{
    m_computedStyle.synchronizeWordSpacingWithFontCascade();
}

inline void RenderStyle::synchronizeWordSpacingWithFontCascadeWithoutUpdate()
{
    m_computedStyle.synchronizeWordSpacingWithFontCascadeWithoutUpdate();
}

inline float RenderStyle::usedLetterSpacing() const
{
    return m_computedStyle.usedLetterSpacing();
}

inline float RenderStyle::usedWordSpacing() const
{
    return m_computedStyle.usedWordSpacing();
}

// MARK: Used Counter Directives

inline const CounterDirectiveMap& RenderStyle::usedCounterDirectives() const
{
    return m_computedStyle.usedCounterDirectives();
}

// MARK: - Aggregates

inline const Style::InsetBox& RenderStyle::insetBox() const
{
    return m_computedStyle.insetBox();
}

inline const Style::MarginBox& RenderStyle::marginBox() const
{
    return m_computedStyle.marginBox();
}

inline const Style::PaddingBox& RenderStyle::paddingBox() const
{
    return m_computedStyle.paddingBox();
}

inline const Style::ScrollMarginBox& RenderStyle::scrollMarginBox() const
{
    return m_computedStyle.scrollMarginBox();
}

inline const Style::ScrollPaddingBox& RenderStyle::scrollPaddingBox() const
{
    return m_computedStyle.scrollPaddingBox();
}

inline const Style::ScrollTimelines& RenderStyle::scrollTimelines() const
{
    return m_computedStyle.scrollTimelines();
}

inline const Style::ViewTimelines& RenderStyle::viewTimelines() const
{
    return m_computedStyle.viewTimelines();
}

inline const Style::Animations& RenderStyle::animations() const
{
    return m_computedStyle.animations();
}

inline const Style::Transitions& RenderStyle::transitions() const
{
    return m_computedStyle.transitions();
}

inline const Style::BackgroundLayers& RenderStyle::backgroundLayers() const
{
    return m_computedStyle.backgroundLayers();
}

inline const Style::MaskLayers& RenderStyle::maskLayers() const
{
    return m_computedStyle.maskLayers();
}

inline const Style::MaskBorder& RenderStyle::maskBorder() const
{
    return m_computedStyle.maskBorder();
}

inline const Style::BorderImage& RenderStyle::borderImage() const
{
    return m_computedStyle.borderImage();
}

inline const Style::TransformOrigin& RenderStyle::transformOrigin() const
{
    return m_computedStyle.transformOrigin();
}

inline const Style::PerspectiveOrigin& RenderStyle::perspectiveOrigin() const
{
    return m_computedStyle.perspectiveOrigin();
}

inline const BorderData& RenderStyle::border() const
{
    return m_computedStyle.border();
}

inline const Style::BorderRadius& RenderStyle::borderRadii() const
{
    return m_computedStyle.borderRadii();
}

inline const BorderValue& RenderStyle::borderBottom() const
{
    return m_computedStyle.borderBottom();
}

inline const BorderValue& RenderStyle::borderLeft() const
{
    return m_computedStyle.borderLeft();
}

inline const BorderValue& RenderStyle::borderRight() const
{
    return m_computedStyle.borderRight();
}

inline const BorderValue& RenderStyle::borderTop() const
{
    return m_computedStyle.borderTop();
}

// MARK: - Properties/descriptors that are not yet generated

inline CursorType RenderStyle::cursorType() const
{
    return m_computedStyle.cursorType();
}

// FIXME: Support descriptors

inline const Style::PageSize& RenderStyle::pageSize() const
{
    return m_computedStyle.pageSize();
}

// MARK: - Derived values

inline bool RenderStyle::collapseWhiteSpace() const
{
    return collapseWhiteSpace(whiteSpaceCollapse());
}

inline bool RenderStyle::preserveNewline() const
{
    return preserveNewline(whiteSpaceCollapse());
}

inline bool RenderStyle::affectsTransform() const
{
    return !transform().isNone()
        || !offsetPath().isNone()
        || !offsetPath().isNone()
        || !rotate().isNone()
        || !scale().isNone()
        || !translate().isNone();
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


constexpr bool RenderStyle::preserveNewline(WhiteSpaceCollapse mode)
{
    return mode == WhiteSpaceCollapse::Preserve || mode == WhiteSpaceCollapse::PreserveBreaks || mode == WhiteSpaceCollapse::BreakSpaces;
}

inline bool RenderStyle::isInterCharacterRubyPosition() const
{
    auto rubyPosition = this->rubyPosition();
    return rubyPosition == RubyPosition::InterCharacter || rubyPosition == RubyPosition::LegacyInterCharacter;
}

// MARK: has*() functions

inline bool RenderStyle::hasBackground() const
{
    return visitedDependentBackgroundColor().isVisible()
        || Style::hasImageInAnyLayer(backgroundLayers());
}

inline bool RenderStyle::hasBorderImageOutsets() const
{
    return !borderImageSource().isNone() && !borderImageOutset().isZero();
}

inline bool RenderStyle::hasInFlowPosition() const
{
    return position() == PositionType::Relative || position() == PositionType::Sticky;
}

inline bool RenderStyle::hasMarkers() const
{
    return !markerStart().isNone() || !markerMid().isNone() || !markerEnd().isNone();
}

inline bool RenderStyle::hasMask() const
{
    return Style::hasImageInAnyLayer(maskLayers()) || !maskBorderSource().isNone();
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

inline bool RenderStyle::hasPositionedMask() const
{
    return Style::hasImageInAnyLayer(maskLayers());
}

inline bool RenderStyle::hasStaticBlockPosition(bool horizontal) const
{
    return horizontal
        ? (top().isAuto() && bottom().isAuto())
        : (left().isAuto() && right().isAuto());
}

inline bool RenderStyle::hasStaticInlinePosition(bool horizontal) const
{
    return horizontal
        ? (left().isAuto() && right().isAuto())
        : (top().isAuto() && bottom().isAuto());
}

inline bool RenderStyle::hasTransformRelatedProperty() const
{
    return !transform().isNone()
        || !offsetPath().isNone()
        || !rotate().isNone()
        || !scale().isNone()
        || !translate().isNone()
        || transformStyle3D() == TransformStyle3D::Preserve3D
        || !perspective().isNone();
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

inline bool RenderStyle::isFixedTableLayout() const
{
    return tableLayout() == TableLayoutType::Fixed 
        && (logicalWidth().isSpecified() 
            || logicalWidth().isFitContent() 
            || logicalWidth().isFillAvailable() 
            || logicalWidth().isMinContent());
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
