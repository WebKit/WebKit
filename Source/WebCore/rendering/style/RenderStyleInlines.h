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

#include <WebCore/AnchorPositionEvaluator.h>
#include <WebCore/Element.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/GraphicsTypes.h>
#include <WebCore/HitTestRequest.h>
#include <WebCore/PositionTryOrder.h>
#include <WebCore/RenderStyle.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleFillLayers.h>
#include <WebCore/StyleFontFamily.h>
#include <WebCore/StyleFontFeatureSettings.h>
#include <WebCore/StyleFontPalette.h>
#include <WebCore/StyleFontSizeAdjust.h>
#include <WebCore/StyleFontStyle.h>
#include <WebCore/StyleFontVariantAlternates.h>
#include <WebCore/StyleFontVariantEastAsian.h>
#include <WebCore/StyleFontVariantLigatures.h>
#include <WebCore/StyleFontVariantNumeric.h>
#include <WebCore/StyleFontVariationSettings.h>
#include <WebCore/StyleFontWeight.h>
#include <WebCore/StyleFontWidth.h>
#include <WebCore/StyleGridTrackSizingDirection.h>
#include <WebCore/StyleTextAlign.h>
#include <WebCore/StyleTextAutospace.h>
#include <WebCore/StyleTextDecorationLine.h>
#include <WebCore/StyleTextSpacingTrim.h>
#include <WebCore/StyleTextTransform.h>
#include <WebCore/StyleWebKitLocale.h>
#include <WebCore/UnicodeBidi.h>
#include <WebCore/ViewTimeline.h>
#include <WebCore/WebAnimationTypes.h>

#if ENABLE(APPLE_PAY)
#include <WebCore/ApplePayButtonPart.h>
#endif

#if HAVE(CORE_MATERIAL)
#include <WebCore/AppleVisualEffect.h>
#endif

#define RENDER_STYLE_INLINES_GENERATED_INCLUDE_TRAP 1
#include <WebCore/RenderStyleInlinesGenerated.h>
#undef RENDER_STYLE_INLINES_GENERATED_INCLUDE_TRAP

namespace WebCore {

using namespace CSS::Literals;

// MARK: - Non-property values

inline bool RenderStyle::effectiveInert() const { return m_rareInheritedData->effectiveInert; }
inline bool RenderStyle::isEffectivelyTransparent() const { return m_rareInheritedData->effectivelyTransparent; }
inline bool RenderStyle::insideDefaultButton() const { return m_rareInheritedData->insideDefaultButton; }
inline bool RenderStyle::insideSubmitButton() const { return m_rareInheritedData->insideSubmitButton; }
inline bool RenderStyle::isInSubtreeWithBlendMode() const { return m_rareInheritedData->isInSubtreeWithBlendMode; }
inline bool RenderStyle::isForceHidden() const { return m_rareInheritedData->isForceHidden; }
inline bool RenderStyle::usesAnchorFunctions() const { return m_nonInheritedData->rareData->usesAnchorFunctions; }
inline EnumSet<BoxAxis> RenderStyle::anchorFunctionScrollCompensatedAxes() const { return EnumSet<BoxAxis>::fromRaw(m_nonInheritedData->rareData->anchorFunctionScrollCompensatedAxes); }
inline bool RenderStyle::isPopoverInvoker() const { return m_nonInheritedData->rareData->isPopoverInvoker; }
inline bool RenderStyle::autoRevealsWhenFound() const { return m_rareInheritedData->autoRevealsWhenFound; }
inline bool RenderStyle::nativeAppearanceDisabled() const { return m_nonInheritedData->rareData->nativeAppearanceDisabled; }
inline OptionSet<EventListenerRegionType> RenderStyle::eventListenerRegionTypes() const { return m_rareInheritedData->eventListenerRegionTypes; }
inline std::optional<PseudoElementType> RenderStyle::pseudoElementType() const { return m_nonInheritedFlags.pseudoElementType ? std::make_optional(static_cast<PseudoElementType>(m_nonInheritedFlags.pseudoElementType - 1)) : std::nullopt; }
inline const AtomString& RenderStyle::pseudoElementNameArgument() const { return m_nonInheritedData->rareData->pseudoElementNameArgument; }
inline bool RenderStyle::hasAnyPublicPseudoStyles() const { return m_nonInheritedFlags.hasAnyPublicPseudoStyles(); }
inline bool RenderStyle::hasAttrContent() const { return m_nonInheritedData->miscData->hasAttrContent; }

// MARK: transform constants

constexpr auto RenderStyle::allTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::TransformOrigin, TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }
constexpr auto RenderStyle::individualTransformOperations() -> OptionSet<TransformOperationOption> { return { TransformOperationOption::Translate, TransformOperationOption::Rotate, TransformOperationOption::Scale, TransformOperationOption::Offset }; }

// MARK: Custom property support

inline const Style::CustomPropertyData& RenderStyle::inheritedCustomProperties() const { return m_rareInheritedData->customProperties.get(); }
inline const Style::CustomPropertyData& RenderStyle::nonInheritedCustomProperties() const { return m_nonInheritedData->rareData->customProperties.get(); }

// MARK: Derived values

inline BoxSizing RenderStyle::boxSizingForAspectRatio() const { return aspectRatio().isAutoAndRatio() ? BoxSizing::ContentBox : boxSizing(); }
inline bool RenderStyle::collapseWhiteSpace() const { return collapseWhiteSpace(whiteSpaceCollapse()); }
inline bool RenderStyle::preserveNewline() const { return preserveNewline(whiteSpaceCollapse()); }
inline bool RenderStyle::preserves3D() const { return usedTransformStyle3D() == TransformStyle3D::Preserve3D; }
inline bool RenderStyle::affectsTransform() const { return hasTransform() || hasOffsetPath() || hasRotate() || hasScale() || hasTranslate(); }
inline CSSPropertyID RenderStyle::usedStrokeColorProperty() const { return hasExplicitlySetStrokeColor() ? CSSPropertyStrokeColor : CSSPropertyWebkitTextStrokeColor; }
// ignore non-standard ::-webkit-scrollbar when standard properties are in use
inline bool RenderStyle::usesStandardScrollbarStyle() const { return scrollbarWidth() != Style::ScrollbarWidth::Auto || !scrollbarColor().isAuto(); }
inline bool RenderStyle::usesLegacyScrollbarStyle() const { return hasPseudoStyle(PseudoElementType::WebKitScrollbar) && !usesStandardScrollbarStyle(); }
inline bool RenderStyle::specifiesColumns() const { return !columnCount().isAuto() || !columnWidth().isAuto() || !hasInlineColumnAxis(); }
inline LayoutBoxExtent RenderStyle::borderImageOutsets() const { return imageOutsets(borderImage()); }
inline LayoutBoxExtent RenderStyle::maskBorderOutsets() const { return imageOutsets(maskBorder()); }
inline bool RenderStyle::autoWrap() const { return textWrapMode() != TextWrapMode::NoWrap; }
inline bool RenderStyle::borderBottomIsTransparent() const { return border().bottom().isTransparent(); }
inline bool RenderStyle::borderLeftIsTransparent() const { return border().left().isTransparent(); }
inline bool RenderStyle::borderRightIsTransparent() const { return border().right().isTransparent(); }
inline bool RenderStyle::borderTopIsTransparent() const { return border().top().isTransparent(); }
inline bool RenderStyle::columnRuleIsTransparent() const { return columnRule().isTransparent(); }

// MARK: Cached used values

inline StyleAppearance RenderStyle::usedAppearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->usedAppearance); }
inline Style::ZIndex RenderStyle::usedZIndex() const { return m_nonInheritedData->boxData->usedZIndex(); }
inline Style::Contain RenderStyle::usedContain() const { return m_nonInheritedData->rareData->usedContain(); }
inline float RenderStyle::usedZoom() const { return m_rareInheritedData->usedZoom; }
inline ContentVisibility RenderStyle::usedContentVisibility() const { return static_cast<ContentVisibility>(m_rareInheritedData->usedContentVisibility); }
inline Style::TouchAction RenderStyle::usedTouchAction() const { return m_rareInheritedData->usedTouchAction; }
#if HAVE(CORE_MATERIAL)
inline AppleVisualEffect RenderStyle::usedAppleVisualEffectForSubtree() const { return static_cast<AppleVisualEffect>(m_rareInheritedData->usedAppleVisualEffectForSubtree); }
#endif
inline float RenderStyle::usedLetterSpacing() const { return fontCascade().letterSpacing(); }
inline float RenderStyle::usedWordSpacing() const { return fontCascade().wordSpacing(); }

// MARK: Derived used values

inline UserModify RenderStyle::usedUserModify() const { return effectiveInert() ? UserModify::ReadOnly : userModify(); }
inline PointerEvents RenderStyle::usedPointerEvents() const { return effectiveInert() ? PointerEvents::None : pointerEvents(); }
inline TransformStyle3D RenderStyle::usedTransformStyle3D() const { return static_cast<bool>(m_nonInheritedData->rareData->transformStyleForcedToFlat) ? TransformStyle3D::Flat : transformStyle3D(); }
inline float RenderStyle::usedPerspective() const { return perspective().usedPerspective(); }
inline Visibility RenderStyle::usedVisibility() const
{
    if (isForceHidden()) [[unlikely]]
        return Visibility::Hidden;
    return static_cast<Visibility>(m_inheritedFlags.visibility);
}

inline bool RenderStyle::NonInheritedFlags::hasPseudoStyle(PseudoElementType pseudo) const
{
    ASSERT(allPublicPseudoElementTypes.contains(pseudo));
    return EnumSet<PseudoElementType>::fromRaw(pseudoBits).contains(pseudo);
}

inline bool RenderStyle::NonInheritedFlags::hasAnyPublicPseudoStyles() const
{
    return !!pseudoBits;
}

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

    return roundForImpreciseConversion<int>(value / zoomFactor);
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

inline bool RenderStyle::columnSpanEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->miscData.ptr() == other.m_nonInheritedData->miscData.ptr()
        || m_nonInheritedData->miscData->multiCol.ptr() == other.m_nonInheritedData->miscData->multiCol.ptr())
        return true;

    return m_nonInheritedData->miscData->multiCol->columnSpan == other.m_nonInheritedData->miscData->multiCol->columnSpan;
}

inline bool RenderStyle::borderIsEquivalentForPainting(const RenderStyle& other) const
{
    bool colorDiffers = color() != other.color();

    if (!colorDiffers
        && (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->surroundData.ptr() == other.m_nonInheritedData->surroundData.ptr()
        || m_nonInheritedData->surroundData->border == other.m_nonInheritedData->surroundData->border))
        return true;

    return border().isEquivalentForPainting(other.border(), colorDiffers);
}

inline bool RenderStyle::containerTypeAndNamesEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return containerType() == other.containerType() && containerNames() == other.containerNames();
}

inline bool RenderStyle::scrollPaddingEqual(const RenderStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollPadding == other.m_nonInheritedData->rareData->scrollPadding;
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

inline bool RenderStyle::fontCascadeEqual(const RenderStyle& other) const
{
    return m_inheritedData.ptr() == other.m_inheritedData.ptr()
        || m_inheritedData->fontData.ptr() == other.m_inheritedData->fontData.ptr()
        || m_inheritedData->fontData->fontCascade == other.m_inheritedData->fontData->fontCascade;
}

inline bool RenderStyle::evaluationTimeZoomEnabled() const
{
    return m_rareInheritedData->evaluationTimeZoomEnabled;
}

inline float RenderStyle::deviceScaleFactor() const
{
    return m_rareInheritedData->deviceScaleFactor;
}

inline bool RenderStyle::useSVGZoomRulesForLength() const
{
    return m_nonInheritedData->rareData->useSVGZoomRulesForLength;
}

inline Style::ZoomFactor RenderStyle::usedZoomForLength() const
{
    if (useSVGZoomRulesForLength())
        return Style::ZoomFactor(1.0f, deviceScaleFactor());

    if (evaluationTimeZoomEnabled())
        return Style::ZoomFactor(usedZoom(), deviceScaleFactor());

    return Style::ZoomFactor(1.0f, deviceScaleFactor());
}


// MARK: has*() functions

inline bool RenderStyle::hasAnimations() const { return !animations().isInitial(); }
inline bool RenderStyle::hasAnimationsOrTransitions() const { return hasAnimations() || hasTransitions(); }
// FIXME: Rename this function.
inline bool RenderStyle::hasAppearance() const { return appearance() != StyleAppearance::None && appearance() != StyleAppearance::Base; }
inline bool RenderStyle::hasAppleColorFilter() const { return !appleColorFilter().isNone(); }
#if HAVE(CORE_MATERIAL)
inline bool RenderStyle::hasAppleVisualEffect() const { return appleVisualEffect() != AppleVisualEffect::None; }
inline bool RenderStyle::hasAppleVisualEffectRequiringBackdropFilter() const { return appleVisualEffectNeedsBackdrop(appleVisualEffect()); }
#endif
inline bool RenderStyle::hasAspectRatio() const { return aspectRatio().hasRatio(); }
inline bool RenderStyle::hasAutoLeftAndRight() const { return left().isAuto() && right().isAuto(); }
inline bool RenderStyle::hasAutoLengthContainIntrinsicSize() const { return containIntrinsicWidth().hasAuto() || containIntrinsicHeight().hasAuto(); }
inline bool RenderStyle::hasAutoTopAndBottom() const { return top().isAuto() && bottom().isAuto(); }
inline bool RenderStyle::hasBackdropFilter() const { return !backdropFilter().isNone(); }
inline bool RenderStyle::hasBackground() const { return visitedDependentColor(CSSPropertyBackgroundColor).isVisible() || hasBackgroundImage(); }
inline bool RenderStyle::hasBackgroundImage() const { return Style::hasImageInAnyLayer(backgroundLayers()); }
inline bool RenderStyle::hasBlendMode() const { return blendMode() != BlendMode::Normal; }
inline bool RenderStyle::hasBorder() const { return border().hasBorder(); }
inline bool RenderStyle::hasBorderImage() const { return border().hasBorderImage(); }
inline bool RenderStyle::hasBorderImageOutsets() const { return borderImage().hasSource() && !borderImage().outset().isZero(); }
inline bool RenderStyle::hasBorderRadius() const { return border().hasBorderRadius(); }
inline bool RenderStyle::hasBoxReflect() const { return !boxReflect().isNone(); }
inline bool RenderStyle::hasBoxShadow() const { return !boxShadow().isNone(); }
inline bool RenderStyle::hasClip() const { return !clip().isAuto(); }
inline bool RenderStyle::hasClipPath() const { return !clipPath().isNone(); }
inline bool RenderStyle::hasContent() const { return content().isData(); }
inline bool RenderStyle::hasDisplayAffectedByAnimations() const { return m_nonInheritedData->miscData->hasDisplayAffectedByAnimations; }
inline bool RenderStyle::hasFill() const { return !fill().isNone(); }
inline bool RenderStyle::hasFilter() const { return !filter().isNone(); }
inline bool RenderStyle::hasInFlowPosition() const { return position() == PositionType::Relative || position() == PositionType::Sticky; }
inline bool RenderStyle::hasIsolation() const { return isolation() != Isolation::Auto; }
inline bool RenderStyle::hasMarkers() const { return !markerStart().isNone() || !markerMid().isNone() || !markerEnd().isNone(); }
inline bool RenderStyle::hasMask() const { return Style::hasImageInAnyLayer(maskLayers()) || maskBorder().hasSource(); }
inline bool RenderStyle::hasOffsetPath() const { return !WTF::holdsAlternative<CSS::Keyword::None>(offsetPath()); }
inline bool RenderStyle::hasOpacity() const { return !opacity().isOpaque(); }
inline bool RenderStyle::hasOutline() const { return outlineStyle() != OutlineStyle::None && outlineWidth().isPositive(); }
inline bool RenderStyle::hasOutlineInVisualOverflow() const { return hasOutline() && outlineSize() > 0; }
inline bool RenderStyle::hasOutOfFlowPosition() const { return position() == PositionType::Absolute || position() == PositionType::Fixed; }
inline bool RenderStyle::hasPerspective() const { return !perspective().isNone(); }
inline bool RenderStyle::hasPositionedMask() const { return Style::hasImageInAnyLayer(maskLayers()); }
inline bool RenderStyle::hasPseudoStyle(PseudoElementType pseudo) const { return m_nonInheritedFlags.hasPseudoStyle(pseudo); }
inline bool RenderStyle::hasRotate() const { return !rotate().isNone(); }
inline bool RenderStyle::hasScale() const { return !scale().isNone(); }
inline bool RenderStyle::hasScrollTimelines() const { return m_nonInheritedData->rareData->hasScrollTimelines(); }
inline bool RenderStyle::hasSnapPosition() const { return !scrollSnapAlign().isNone(); }
inline bool RenderStyle::hasStaticBlockPosition(bool horizontal) const { return horizontal ? hasAutoTopAndBottom() : hasAutoLeftAndRight(); }
inline bool RenderStyle::hasStaticInlinePosition(bool horizontal) const { return horizontal ? hasAutoLeftAndRight() : hasAutoTopAndBottom(); }
inline bool RenderStyle::hasStroke() const { return !stroke().isNone(); }
inline bool RenderStyle::hasTextCombine() const { return textCombine() != TextCombine::None; }
inline bool RenderStyle::hasTextShadow() const { return !textShadow().isNone(); }
inline bool RenderStyle::hasTransform() const { return !transform().isNone() || hasOffsetPath(); }
inline bool RenderStyle::hasTransformRelatedProperty() const { return hasTransform() || hasRotate() || hasScale() || hasTranslate() || transformStyle3D() == TransformStyle3D::Preserve3D || hasPerspective(); }
inline bool RenderStyle::hasTransitions() const { return !transitions().isInitial(); }
inline bool RenderStyle::hasTranslate() const { return !translate().isNone(); }
inline bool RenderStyle::hasUsedAppearance() const { return usedAppearance() != StyleAppearance::None && usedAppearance() != StyleAppearance::Base; }
inline bool RenderStyle::hasUsedContentNone() const { return content().isNone() || (content().isNormal() && (pseudoElementType() == PseudoElementType::Before || pseudoElementType() == PseudoElementType::After)); }
inline bool RenderStyle::hasViewportConstrainedPosition() const { return position() == PositionType::Fixed || position() == PositionType::Sticky; }
inline bool RenderStyle::hasViewTimelines() const { return m_nonInheritedData->rareData->hasViewTimelines(); }
inline bool RenderStyle::hasVisibleBorder() const { return border().hasVisibleBorder(); }
inline bool RenderStyle::hasVisibleBorderDecoration() const { return hasVisibleBorder() || hasBorderImage(); }

// MARK: is*() functions

inline bool RenderStyle::isColumnFlexDirection() const { return flexDirection() == FlexDirection::Column || flexDirection() == FlexDirection::ColumnReverse; }
inline bool RenderStyle::isRowFlexDirection() const { return flexDirection() == FlexDirection::Row || flexDirection() == FlexDirection::RowReverse; }
constexpr bool RenderStyle::isDisplayBlockLevel() const { return isDisplayBlockType(display()); }
constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox(DisplayType display) { return display == DisplayType::Box || display == DisplayType::InlineBox; }
constexpr bool RenderStyle::isDisplayFlexibleBox(DisplayType display) { return display == DisplayType::Flex || display == DisplayType::InlineFlex; }
constexpr bool RenderStyle::isDisplayDeprecatedFlexibleBox() const { return isDisplayDeprecatedFlexibleBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox() const { return isDisplayFlexibleOrGridFormattingContextBox() || isDisplayDeprecatedFlexibleBox(); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridFormattingContextBox() const { return isDisplayFlexibleOrGridFormattingContextBox(display()); }
constexpr bool RenderStyle::isDisplayFlexibleOrGridFormattingContextBox(DisplayType display) { return isDisplayFlexibleBox(display) || isDisplayGridFormattingContextBox(display); }
constexpr bool RenderStyle::isDisplayGridFormattingContextBox(DisplayType display) { return isDisplayGridBox(display) || isDisplayGridLanesBox(display); }
constexpr bool RenderStyle::isDisplayGridBox(DisplayType display) { return display == DisplayType::Grid || display == DisplayType::InlineGrid; }
constexpr bool RenderStyle::isDisplayGridLanesBox(DisplayType display) { return display == DisplayType::GridLanes || display == DisplayType::InlineGridLanes; }
constexpr bool RenderStyle::isDisplayInlineType() const { return isDisplayInlineType(display()); }
constexpr bool RenderStyle::isDisplayListItemType(DisplayType display) { return display == DisplayType::ListItem; }
constexpr bool RenderStyle::isDisplayTableOrTablePart() const { return isDisplayTableOrTablePart(display()); }
constexpr bool RenderStyle::isInternalTableBox() const { return isInternalTableBox(display()); }
constexpr bool RenderStyle::isRubyContainerOrInternalRubyBox() const { return isRubyContainerOrInternalRubyBox(display()); }
inline bool RenderStyle::isFixedTableLayout() const { return tableLayout() == TableLayoutType::Fixed && (logicalWidth().isSpecified() || logicalWidth().isFitContent() || logicalWidth().isFillAvailable() || logicalWidth().isMinContent()); }
inline bool RenderStyle::isFloating() const { return floating() != Float::None; }
constexpr bool RenderStyle::isOriginalDisplayBlockType() const { return isDisplayBlockType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayInlineType() const { return isDisplayInlineType(originalDisplay()); }
constexpr bool RenderStyle::isOriginalDisplayListItemType() const { return isDisplayListItemType(originalDisplay()); }
inline bool RenderStyle::isOverflowVisible() const { return overflowX() == Overflow::Visible || overflowY() == Overflow::Visible; }
inline bool RenderStyle::isReverseFlexDirection() const { return flexDirection() == FlexDirection::RowReverse || flexDirection() == FlexDirection::ColumnReverse; }
inline bool RenderStyle::isSkippedRootOrSkippedContent() const { return usedContentVisibility() != ContentVisibility::Visible; }

// MARK: - Logical getters

// MARK: sizing logical
inline const Style::PreferredSize& RenderStyle::logicalHeight() const { return logicalHeight(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? height() : width(); }
inline const Style::MaximumSize& RenderStyle::logicalMaxHeight() const { return logicalMaxHeight(writingMode()); }
inline const Style::MaximumSize& RenderStyle::logicalMaxHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? maxHeight() : maxWidth(); }
inline const Style::MaximumSize& RenderStyle::logicalMaxWidth() const { return logicalMaxWidth(writingMode()); }
inline const Style::MaximumSize& RenderStyle::logicalMaxWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? maxWidth() : maxHeight(); }
inline const Style::MinimumSize& RenderStyle::logicalMinHeight() const { return logicalMinHeight(writingMode()); }
inline const Style::MinimumSize& RenderStyle::logicalMinHeight(const WritingMode writingMode) const { return writingMode.isHorizontal() ? minHeight() : minWidth(); }
inline const Style::MinimumSize& RenderStyle::logicalMinWidth() const { return logicalMinWidth(writingMode()); }
inline const Style::MinimumSize& RenderStyle::logicalMinWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? minWidth() : minHeight(); }
inline const Style::PreferredSize& RenderStyle::logicalWidth() const { return logicalWidth(writingMode()); }
inline const Style::PreferredSize& RenderStyle::logicalWidth(const WritingMode writingMode) const { return writingMode.isHorizontal() ? width() : height(); }

// MARK: inset logical
inline const Style::InsetEdge& RenderStyle::logicalTop() const { return insetBox().before(writingMode()); }
inline const Style::InsetEdge& RenderStyle::logicalRight() const { return insetBox().logicalRight(writingMode()); }
inline const Style::InsetEdge& RenderStyle::logicalBottom() const { return insetBox().after(writingMode()); }
inline const Style::InsetEdge& RenderStyle::logicalLeft() const { return insetBox().logicalLeft(writingMode()); }

// MARK: margin logical
inline const Style::MarginEdge& RenderStyle::marginAfter() const { return marginAfter(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginAfter(const WritingMode writingMode) const { return marginBox().after(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginBefore() const { return marginBefore(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginBefore(const WritingMode writingMode) const { return marginBox().before(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginEnd() const { return marginEnd(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginEnd(const WritingMode writingMode) const { return marginBox().end(writingMode); }
inline const Style::MarginEdge& RenderStyle::marginStart() const { return marginStart(writingMode()); }
inline const Style::MarginEdge& RenderStyle::marginStart(const WritingMode writingMode) const { return marginBox().start(writingMode); }

// MARK: padding logical
inline const Style::PaddingEdge& RenderStyle::paddingAfter() const { return paddingAfter(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingAfter(const WritingMode writingMode) const { return paddingBox().after(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingBefore() const { return paddingBefore(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingBefore(const WritingMode writingMode) const { return paddingBox().before(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingEnd() const { return paddingEnd(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingEnd(const WritingMode writingMode) const { return paddingBox().end(writingMode); }
inline const Style::PaddingEdge& RenderStyle::paddingStart() const { return paddingStart(writingMode()); }
inline const Style::PaddingEdge& RenderStyle::paddingStart(const WritingMode writingMode) const { return paddingBox().start(writingMode); }

// MARK: grid logical
inline const Style::GapGutter& RenderStyle::gap(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? columnGap() : rowGap(); }
inline const Style::GridTrackSizes& RenderStyle::gridAutoList(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridAutoColumns() : gridAutoRows(); }
inline const Style::GridPosition& RenderStyle::gridItemEnd(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnEnd() : gridItemRowEnd(); }
inline const Style::GridPosition& RenderStyle::gridItemStart(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridItemColumnStart() : gridItemRowStart(); }
inline const Style::GridTemplateList& RenderStyle::gridTemplateList(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? gridTemplateColumns() : gridTemplateRows(); }

// MARK: contain-intrinsic-* logical
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicLogicalHeight() const { return writingMode().isHorizontal() ? containIntrinsicHeight() : containIntrinsicWidth(); }
inline const Style::ContainIntrinsicSize& RenderStyle::containIntrinsicLogicalWidth() const { return writingMode().isHorizontal() ? containIntrinsicWidth() : containIntrinsicHeight(); }

// MARK: aspect-ratio logical
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalHeight() const { return writingMode().isHorizontal() ? aspectRatio().height() : aspectRatio().width(); }
inline Style::Number<CSS::Nonnegative> RenderStyle::aspectRatioLogicalWidth() const { return writingMode().isHorizontal() ? aspectRatio().width() : aspectRatio().height(); }
inline double RenderStyle::logicalAspectRatio() const
{
    auto ratio = this->aspectRatio().tryRatio();
    ASSERT(ratio);

    if (writingMode().isHorizontal())
        return ratio->numerator.value / ratio->denominator.value;
    return ratio->denominator.value / ratio->numerator.value;
}

// MARK: border-width logical
inline Style::LineWidth RenderStyle::borderAfterWidth() const { return borderAfterWidth(writingMode()); }
inline Style::LineWidth RenderStyle::borderBeforeWidth() const { return borderBeforeWidth(writingMode()); }
inline Style::LineWidth RenderStyle::borderEndWidth() const { return borderEndWidth(writingMode()); }
inline Style::LineWidth RenderStyle::borderStartWidth() const { return borderStartWidth(writingMode()); }

// MARK: - Aggregate getters

inline const Style::InsetBox& RenderStyle::insetBox() const { return m_nonInheritedData->surroundData->inset; }
inline const Style::MarginBox& RenderStyle::marginBox() const { return m_nonInheritedData->surroundData->margin; }
inline const Style::PaddingBox& RenderStyle::paddingBox() const { return m_nonInheritedData->surroundData->padding; }
inline const Style::ScrollMarginBox& RenderStyle::scrollMarginBox() const { return m_nonInheritedData->rareData->scrollMargin; }
inline const Style::ScrollPaddingBox& RenderStyle::scrollPaddingBox() const { return m_nonInheritedData->rareData->scrollPadding; }
inline const Style::ScrollTimelines& RenderStyle::scrollTimelines() const { return m_nonInheritedData->rareData->scrollTimelines; }
inline const Style::ViewTimelines& RenderStyle::viewTimelines() const { return m_nonInheritedData->rareData->viewTimelines; }
inline const Style::Animations& RenderStyle::animations() const { return m_nonInheritedData->miscData->animations; }
inline const Style::Transitions& RenderStyle::transitions() const { return m_nonInheritedData->miscData->transitions; }
inline const Style::BackgroundLayers& RenderStyle::backgroundLayers() const { return m_nonInheritedData->backgroundData->background; }
inline const Style::MaskLayers& RenderStyle::maskLayers() const { return m_nonInheritedData->miscData->mask; }
inline const Style::MaskBorder& RenderStyle::maskBorder() const { return m_nonInheritedData->rareData->maskBorder; }
inline const Style::BorderImage& RenderStyle::borderImage() const { return border().image(); }
inline const Style::TransformOrigin& RenderStyle::transformOrigin() const { return m_nonInheritedData->miscData->transform->origin; }
inline const Style::PerspectiveOrigin& RenderStyle::perspectiveOrigin() const { return m_nonInheritedData->rareData->perspectiveOrigin; }
inline const OutlineValue& RenderStyle::outline() const { return m_nonInheritedData->backgroundData->outline; }
inline const BorderData& RenderStyle::border() const { return m_nonInheritedData->surroundData->border; }
inline Style::LineWidthBox RenderStyle::borderWidth() const { return border().borderWidth(); }
inline const Style::BorderRadius& RenderStyle::borderRadii() const { return border().radii(); }
inline const BorderValue& RenderStyle::borderBottom() const { return border().bottom(); }
inline const BorderValue& RenderStyle::borderLeft() const { return border().left(); }
inline const BorderValue& RenderStyle::borderRight() const { return border().right(); }
inline const BorderValue& RenderStyle::borderTop() const { return border().top(); }
inline const BorderValue& RenderStyle::columnRule() const { return m_nonInheritedData->miscData->multiCol->columnRule; }
inline const FontCascade& RenderStyle::fontCascade() const { return m_inheritedData->fontData->fontCascade; }
inline bool RenderStyle::hasExplicitlySetBorderRadius() const { return hasExplicitlySetBorderBottomLeftRadius() || hasExplicitlySetBorderBottomRightRadius() || hasExplicitlySetBorderTopLeftRadius() || hasExplicitlySetBorderTopRightRadius(); }
inline bool RenderStyle::hasExplicitlySetPadding() const { return hasExplicitlySetPaddingBottom() || hasExplicitlySetPaddingLeft() || hasExplicitlySetPaddingRight() || hasExplicitlySetPaddingTop(); }

// MARK: - Property initial values

constexpr Style::AlignContent RenderStyle::initialAlignContent() { return CSS::Keyword::Normal { }; }
constexpr Style::AlignItems RenderStyle::initialAlignItems() { return CSS::Keyword::Normal { }; }
constexpr Style::AlignSelf RenderStyle::initialAlignSelf() { return CSS::Keyword::Auto { }; }
inline Style::AnchorNames RenderStyle::initialAnchorNames() { return CSS::Keyword::None { }; }
inline Style::NameScope RenderStyle::initialAnchorScope() { return CSS::Keyword::None { }; }
inline Style::Animations RenderStyle::initialAnimations() { return CSS::Keyword::None { }; }
constexpr StyleAppearance RenderStyle::initialAppearance() { return StyleAppearance::None; }
#if HAVE(CORE_MATERIAL)
constexpr AppleVisualEffect RenderStyle::initialAppleVisualEffect() { return AppleVisualEffect::None; }
#endif
inline Style::AppleColorFilter RenderStyle::initialAppleColorFilter() { return CSS::Keyword::None { }; }
inline Style::AspectRatio RenderStyle::initialAspectRatio() { return CSS::Keyword::Auto { }; }
constexpr BackfaceVisibility RenderStyle::initialBackfaceVisibility() { return BackfaceVisibility::Visible; }
inline Style::Color RenderStyle::initialBackgroundColor() { return Color::transparentBlack; }
inline Style::BackgroundLayers RenderStyle::initialBackgroundLayers() { return CSS::Keyword::None { }; }
inline Style::BlockEllipsis RenderStyle::initialBlockEllipsis() { return CSS::Keyword::None { }; }
constexpr BlockStepAlign RenderStyle::initialBlockStepAlign() { return BlockStepAlign::Auto; }
constexpr BlockStepInsert RenderStyle::initialBlockStepInsert() { return BlockStepInsert::MarginBox; }
constexpr BlockStepRound RenderStyle::initialBlockStepRound() { return BlockStepRound::Up; }
inline Style::BlockStepSize RenderStyle::initialBlockStepSize() { return CSS::Keyword::None { }; }
constexpr BorderCollapse RenderStyle::initialBorderCollapse() { return BorderCollapse::Separate; }
constexpr Style::WebkitBorderSpacing RenderStyle::initialBorderHorizontalSpacing() { return 0_css_px; }
inline Style::BorderImage RenderStyle::initialBorderImage() { return Style::BorderImage { }; }
inline Style::BorderImageSource RenderStyle::initialBorderImageSource() { return CSS::Keyword::None { }; }
inline Style::BorderRadiusValue RenderStyle::initialBorderRadius() { return { 0_css_px, 0_css_px }; }
constexpr BorderStyle RenderStyle::initialBorderStyle() { return BorderStyle::None; }
constexpr Style::WebkitBorderSpacing RenderStyle::initialBorderVerticalSpacing() { return 0_css_px; }
constexpr Style::LineWidth RenderStyle::initialBorderWidth() { return Style::LineWidth { 3.0f }; }
constexpr BoxAlignment RenderStyle::initialBoxAlign() { return BoxAlignment::Stretch; }
constexpr BoxDecorationBreak RenderStyle::initialBoxDecorationBreak() { return BoxDecorationBreak::Slice; }
constexpr BoxDirection RenderStyle::initialBoxDirection() { return BoxDirection::Normal; }
constexpr Style::WebkitBoxFlex RenderStyle::initialBoxFlex() { return 0; }
constexpr Style::WebkitBoxFlexGroup RenderStyle::initialBoxFlexGroup() { return 1; }
constexpr BoxLines RenderStyle::initialBoxLines() { return BoxLines::Single; }
constexpr Style::WebkitBoxOrdinalGroup RenderStyle::initialBoxOrdinalGroup() { return 1; }
constexpr BoxOrient RenderStyle::initialBoxOrient() { return BoxOrient::Horizontal; }
constexpr BoxPack RenderStyle::initialBoxPack() { return BoxPack::Start; }
inline Style::BoxShadows RenderStyle::initialBoxShadow() { return CSS::Keyword::None { }; }
constexpr BoxSizing RenderStyle::initialBoxSizing() { return BoxSizing::ContentBox; }
inline Style::WebkitBoxReflect RenderStyle::initialBoxReflect() { return CSS::Keyword::None { }; }
constexpr BreakBetween RenderStyle::initialBreakBetween() { return BreakBetween::Auto; }
constexpr BreakInside RenderStyle::initialBreakInside() { return BreakInside::Auto; }
constexpr LineCap RenderStyle::initialCapStyle() { return LineCap::Butt; }
constexpr CaptionSide RenderStyle::initialCaptionSide() { return CaptionSide::Top; }
constexpr Clear RenderStyle::initialClear() { return Clear::None; }
inline Style::Clip RenderStyle::initialClip() { return CSS::Keyword::Auto { }; }
inline Style::ClipPath RenderStyle::initialClipPath() { return CSS::Keyword::None { }; }
inline Color RenderStyle::initialColor() { return Color::black; }
constexpr ColumnAxis RenderStyle::initialColumnAxis() { return ColumnAxis::Auto; }
constexpr Style::ColumnCount RenderStyle::initialColumnCount() { return CSS::Keyword::Auto { }; }
constexpr ColumnFill RenderStyle::initialColumnFill() { return ColumnFill::Balance; }
inline Style::GapGutter RenderStyle::initialColumnGap() { return CSS::Keyword::Normal { }; }
inline Style::ItemTolerance RenderStyle::initialItemTolerance() { return CSS::Keyword::Normal { }; }
constexpr ColumnProgression RenderStyle::initialColumnProgression() { return ColumnProgression::Normal; }
constexpr Style::LineWidth RenderStyle::initialColumnRuleWidth() { return Style::LineWidth { 3.0f }; }
constexpr ColumnSpan RenderStyle::initialColumnSpan() { return ColumnSpan::None; }
constexpr Style::ColumnWidth RenderStyle::initialColumnWidth() { return CSS::Keyword::Auto { }; }
inline Style::ContainIntrinsicSize RenderStyle::initialContainIntrinsicHeight() { return CSS::Keyword::None { }; }
inline Style::ContainIntrinsicSize RenderStyle::initialContainIntrinsicWidth() { return CSS::Keyword::None { }; }
inline Style::ContainerNames RenderStyle::initialContainerNames() { return CSS::Keyword::None { }; }
constexpr ContainerType RenderStyle::initialContainerType() { return ContainerType::Normal; }
constexpr Style::Contain RenderStyle::initialContain() { return CSS::Keyword::None { }; }
inline Style::Content RenderStyle::initialContent() { return CSS::Keyword::Normal { }; }
constexpr ContentVisibility RenderStyle::initialContentVisibility() { return ContentVisibility::Visible; }
constexpr Style::CornerShapeValue RenderStyle::initialCornerShapeValue() { return Style::CornerShapeValue::round(); }
inline Style::Cursor RenderStyle::initialCursor() { return CSS::Keyword::Auto { }; }
constexpr TextDirection RenderStyle::initialDirection() { return TextDirection::LTR; }
constexpr DisplayType RenderStyle::initialDisplay() { return DisplayType::Inline; }
constexpr EmptyCell RenderStyle::initialEmptyCells() { return EmptyCell::Show; }
constexpr FieldSizing RenderStyle::initialFieldSizing() { return FieldSizing::Fixed; }
inline Style::Filter RenderStyle::initialFilter() { return CSS::Keyword::None { }; }
inline Style::FlexBasis RenderStyle::initialFlexBasis() { return CSS::Keyword::Auto { }; }
constexpr FlexDirection RenderStyle::initialFlexDirection() { return FlexDirection::Row; }
constexpr Style::FlexGrow RenderStyle::initialFlexGrow() { return 0_css_number; }
constexpr Style::FlexShrink RenderStyle::initialFlexShrink() { return 1_css_number; }
constexpr FlexWrap RenderStyle::initialFlexWrap() { return FlexWrap::NoWrap; }
constexpr Float RenderStyle::initialFloating() { return Float::None; }
constexpr FontOpticalSizing RenderStyle::initialFontOpticalSizing() { return FontOpticalSizing::Enabled; }
inline Style::FontFeatureSettings RenderStyle::initialFontFeatureSettings() { return CSS::Keyword::Normal { }; }
inline Style::FontVariationSettings RenderStyle::initialFontVariationSettings() { return CSS::Keyword::Normal { }; }
inline Style::FontPalette RenderStyle::initialFontPalette() { return CSS::Keyword::Normal { }; }
inline Style::FontSizeAdjust RenderStyle::initialFontSizeAdjust() { return CSS::Keyword::None { }; }
inline Style::FontStyle RenderStyle::initialFontStyle() { return CSS::Keyword::Normal { }; }
inline Style::FontWeight RenderStyle::initialFontWeight() { return CSS::Keyword::Normal { }; }
inline Style::FontWidth RenderStyle::initialFontWidth() { return CSS::Keyword::Normal { }; }
constexpr Kerning RenderStyle::initialFontKerning() { return Kerning::Auto; }
constexpr FontSmoothingMode RenderStyle::initialFontSmoothing() { return FontSmoothingMode::AutoSmoothing; }
constexpr FontSynthesisLonghandValue RenderStyle::initialFontSynthesisSmallCaps() { return FontSynthesisLonghandValue::Auto; }
constexpr FontSynthesisLonghandValue RenderStyle::initialFontSynthesisStyle() { return FontSynthesisLonghandValue::Auto; }
constexpr FontSynthesisLonghandValue RenderStyle::initialFontSynthesisWeight() { return FontSynthesisLonghandValue::Auto; }
inline Style::FontVariantAlternates RenderStyle::initialFontVariantAlternates() { return CSS::Keyword::Normal { }; }
constexpr FontVariantCaps RenderStyle::initialFontVariantCaps() { return FontVariantCaps::Normal; }
constexpr Style::FontVariantEastAsian RenderStyle::initialFontVariantEastAsian() { return CSS::Keyword::Normal { }; }
constexpr FontVariantEmoji RenderStyle::initialFontVariantEmoji() { return FontVariantEmoji::Normal; }
constexpr Style::FontVariantLigatures RenderStyle::initialFontVariantLigatures() { return CSS::Keyword::Normal { }; }
constexpr Style::FontVariantNumeric RenderStyle::initialFontVariantNumeric() { return CSS::Keyword::Normal { }; }
constexpr FontVariantPosition RenderStyle::initialFontVariantPosition() { return FontVariantPosition::Normal; }
inline Style::WebkitLocale RenderStyle::initialLocale() { return CSS::Keyword::Auto { }; }
constexpr Style::TextAutospace RenderStyle::initialTextAutospace() { return CSS::Keyword::NoAutospace { }; }
constexpr TextRenderingMode RenderStyle::initialTextRendering() { return TextRenderingMode::AutoTextRendering; }
constexpr Style::TextSpacingTrim RenderStyle::initialTextSpacingTrim() { return CSS::Keyword::SpaceAll { }; }
inline Style::GridTrackSizes RenderStyle::initialGridAutoColumns() { return CSS::Keyword::Auto { }; }
constexpr Style::GridAutoFlow RenderStyle::initialGridAutoFlow() { return CSS::Keyword::Normal { }; }
inline Style::GridTrackSizes RenderStyle::initialGridAutoRows() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemColumnEnd() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemColumnStart() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemRowEnd() { return CSS::Keyword::Auto { }; }
inline Style::GridPosition RenderStyle::initialGridItemRowStart() { return CSS::Keyword::Auto { }; }
inline Style::GridTemplateList RenderStyle::initialGridTemplateColumns() { return CSS::Keyword::None { }; }
inline Style::GridTemplateList RenderStyle::initialGridTemplateRows() { return CSS::Keyword::None { }; }
inline Style::GridTemplateAreas RenderStyle::initialGridTemplateAreas() { return CSS::Keyword::None { }; }
constexpr Style::HangingPunctuation RenderStyle::initialHangingPunctuation() { return CSS::Keyword::None { }; }
constexpr Style::HyphenateLimitEdge RenderStyle::initialHyphenateLimitAfter() { return CSS::Keyword::Auto { }; }
constexpr Style::HyphenateLimitEdge RenderStyle::initialHyphenateLimitBefore() { return CSS::Keyword::Auto { }; }
constexpr Style::HyphenateLimitLines RenderStyle::initialHyphenateLimitLines() { return CSS::Keyword::NoLimit { }; }
inline Style::HyphenateCharacter RenderStyle::initialHyphenateCharacter() { return CSS::Keyword::Auto { }; }
constexpr Hyphens RenderStyle::initialHyphens() { return Hyphens::Manual; }
constexpr Style::ImageOrientation RenderStyle::initialImageOrientation() { return Style::ImageOrientation::FromImage; }
constexpr ImageRendering RenderStyle::initialImageRendering() { return ImageRendering::Auto; }
inline Style::InsetEdge RenderStyle::initialInset() { return CSS::Keyword::Auto { }; }
constexpr Style::WebkitInitialLetter RenderStyle::initialInitialLetter() { return CSS::Keyword::Normal { }; }
constexpr InputSecurity RenderStyle::initialInputSecurity() { return InputSecurity::Auto; }
constexpr LineJoin RenderStyle::initialJoinStyle() { return LineJoin::Miter; }
constexpr Style::JustifyContent RenderStyle::initialJustifyContent() { return CSS::Keyword::Normal { }; }
constexpr Style::JustifyItems RenderStyle::initialJustifyItems() { return CSS::Keyword::Legacy { }; }
constexpr Style::JustifySelf RenderStyle::initialJustifySelf() { return CSS::Keyword::Auto { }; }
inline Style::LetterSpacing RenderStyle::initialLetterSpacing() { return CSS::Keyword::Normal { }; }
constexpr LineAlign RenderStyle::initialLineAlign() { return LineAlign::None; }
constexpr Style::WebkitLineBoxContain RenderStyle::initialLineBoxContain() { return { Style::WebkitLineBoxContainValue::Block, Style::WebkitLineBoxContainValue::Inline, Style::WebkitLineBoxContainValue::Replaced }; }
constexpr LineBreak RenderStyle::initialLineBreak() { return LineBreak::Auto; }
constexpr Style::WebkitLineClamp RenderStyle::initialLineClamp() { return CSS::Keyword::None { }; }
inline Style::WebkitLineGrid RenderStyle::initialLineGrid() { return CSS::Keyword::None { }; }
inline Style::LineHeight RenderStyle::initialLineHeight() { return CSS::Keyword::Normal { }; }
constexpr LineSnap RenderStyle::initialLineSnap() { return LineSnap::None; }
inline Style::ImageOrNone RenderStyle::initialListStyleImage() { return CSS::Keyword::None { }; }
constexpr ListStylePosition RenderStyle::initialListStylePosition() { return ListStylePosition::Outside; }
inline Style::ListStyleType RenderStyle::initialListStyleType() { return CSS::Keyword::Disc { }; }
inline Style::MarginEdge RenderStyle::initialMargin() { return 0_css_px; }
constexpr Style::MarginTrim RenderStyle::initialMarginTrim() { return CSS::Keyword::None { }; }
constexpr MarqueeBehavior RenderStyle::initialMarqueeBehavior() { return MarqueeBehavior::Scroll; }
constexpr MarqueeDirection RenderStyle::initialMarqueeDirection() { return MarqueeDirection::Auto; }
inline Style::WebkitMarqueeIncrement RenderStyle::initialMarqueeIncrement() { return 6_css_px; }
constexpr Style::WebkitMarqueeRepetition RenderStyle::initialMarqueeRepetition() { return CSS::Keyword::Infinite { }; }
constexpr Style::WebkitMarqueeSpeed RenderStyle::initialMarqueeSpeed() { return 85_css_ms; }
inline Style::MaskBorder RenderStyle::initialMaskBorder() { return Style::MaskBorder { }; }
inline Style::MaskBorderSource RenderStyle::initialMaskBorderSource() { return CSS::Keyword::None { }; }
inline Style::MaskLayers RenderStyle::initialMaskLayers() { return CSS::Keyword::None { }; }
constexpr Style::MathDepth RenderStyle::initialMathDepth() { return 0_css_integer; }
constexpr MathShift RenderStyle::initialMathShift() { return MathShift::Normal; }
constexpr MathStyle RenderStyle::initialMathStyle() { return MathStyle::Normal; }
constexpr Style::MaximumLines RenderStyle::initialMaxLines() { return CSS::Keyword::None { }; }
inline Style::MaximumSize RenderStyle::initialMaxSize() { return CSS::Keyword::None { }; }
inline Style::MinimumSize RenderStyle::initialMinSize() { return CSS::Keyword::Auto { }; }
constexpr NBSPMode RenderStyle::initialNBSPMode() { return NBSPMode::Normal; }
constexpr ObjectFit RenderStyle::initialObjectFit() { return ObjectFit::Fill; }
inline Style::ObjectPosition RenderStyle::initialObjectPosition() { return { 50_css_percentage, 50_css_percentage }; }
inline Style::OffsetAnchor RenderStyle::initialOffsetAnchor() { return CSS::Keyword::Auto { }; }
inline Style::OffsetDistance RenderStyle::initialOffsetDistance() { return 0_css_px; }
inline Style::OffsetPath RenderStyle::initialOffsetPath() { return CSS::Keyword::None { }; }
inline Style::OffsetPosition RenderStyle::initialOffsetPosition() { return CSS::Keyword::Normal { }; }
constexpr Style::OffsetRotate RenderStyle::initialOffsetRotate() { return CSS::Keyword::Auto { }; }
constexpr Style::Opacity RenderStyle::initialOpacity() { return 1_css_number; }
constexpr Style::Order RenderStyle::initialOrder() { return 0_css_integer; }
constexpr Style::Orphans RenderStyle::initialOrphans() { return CSS::Keyword::Auto { }; }
constexpr OverflowAnchor RenderStyle::initialOverflowAnchor() { return OverflowAnchor::Auto; }
inline OverflowContinue RenderStyle::initialOverflowContinue() { return OverflowContinue::Auto; }
constexpr Style::Length<> RenderStyle::initialOutlineOffset() { return 0_css_px; }
constexpr OutlineStyle RenderStyle::initialOutlineStyle() { return OutlineStyle::None; }
constexpr Style::LineWidth RenderStyle::initialOutlineWidth() { return Style::LineWidth { 3.0f }; }
constexpr OverflowWrap RenderStyle::initialOverflowWrap() { return OverflowWrap::Normal; }
constexpr Overflow RenderStyle::initialOverflowX() { return Overflow::Visible; }
constexpr Overflow RenderStyle::initialOverflowY() { return Overflow::Visible; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorX() { return OverscrollBehavior::Auto; }
constexpr OverscrollBehavior RenderStyle::initialOverscrollBehaviorY() { return OverscrollBehavior::Auto; }
inline Style::PaddingEdge RenderStyle::initialPadding() { return 0_css_px; }
inline Style::PageSize RenderStyle::initialPageSize() { return CSS::Keyword::Auto { }; }
constexpr Style::SVGPaintOrder RenderStyle::initialPaintOrder() { return CSS::Keyword::Normal { }; }
inline Style::Perspective RenderStyle::initialPerspective() { return CSS::Keyword::None { }; }
inline Style::PerspectiveOrigin RenderStyle::initialPerspectiveOrigin() { return { initialPerspectiveOriginX(), initialPerspectiveOriginY() }; }
inline Style::PerspectiveOriginX RenderStyle::initialPerspectiveOriginX() { return 50_css_percentage; }
inline Style::PerspectiveOriginY RenderStyle::initialPerspectiveOriginY() { return 50_css_percentage; }
constexpr PointerEvents RenderStyle::initialPointerEvents() { return PointerEvents::Auto; }
constexpr PositionType RenderStyle::initialPosition() { return PositionType::Static; }
inline Style::PositionAnchor RenderStyle::initialPositionAnchor() { return CSS::Keyword::Auto { }; }
inline Style::PositionArea RenderStyle::initialPositionArea() { return CSS::Keyword::None { }; }
inline Style::PositionTryFallbacks RenderStyle::initialPositionTryFallbacks() { return CSS::Keyword::None { }; }
constexpr Style::PositionTryOrder RenderStyle::initialPositionTryOrder() { return Style::PositionTryOrder::Normal; }
constexpr Style::PositionVisibility RenderStyle::initialPositionVisibility() { return Style::PositionVisibilityValue::AnchorsVisible; }
constexpr PrintColorAdjust RenderStyle::initialPrintColorAdjust() { return PrintColorAdjust::Economy; }
inline Style::Quotes RenderStyle::initialQuotes() { return CSS::Keyword::Auto { }; }
constexpr Order RenderStyle::initialRTLOrdering() { return Order::Logical; }
constexpr Style::Resize RenderStyle::initialResize() { return Style::Resize::None; }
inline Style::GapGutter RenderStyle::initialRowGap() { return CSS::Keyword::Normal { }; }
constexpr RubyPosition RenderStyle::initialRubyPosition() { return RubyPosition::Over; }
constexpr RubyAlign RenderStyle::initialRubyAlign() { return RubyAlign::SpaceAround; }
constexpr RubyOverhang RenderStyle::initialRubyOverhang() { return RubyOverhang::Auto; }
constexpr Style::ScrollBehavior RenderStyle::initialScrollBehavior() { return Style::ScrollBehavior::Auto; }
inline Style::ScrollMarginEdge RenderStyle::initialScrollMargin() { return 0_css_px; }
inline Style::ScrollPaddingEdge RenderStyle::initialScrollPadding() { return CSS::Keyword::Auto { }; }
constexpr Style::ScrollSnapAlign RenderStyle::initialScrollSnapAlign() { return CSS::Keyword::None { }; }
constexpr ScrollSnapStop RenderStyle::initialScrollSnapStop() { return ScrollSnapStop::Normal; }
constexpr Style::ScrollSnapType RenderStyle::initialScrollSnapType() { return CSS::Keyword::None { }; }
inline Style::ProgressTimelineAxes RenderStyle::initialScrollTimelineAxes() { return CSS::Keyword::Block { }; }
inline Style::ProgressTimelineNames RenderStyle::initialScrollTimelineNames() { return CSS::Keyword::None { }; }
inline Style::ScrollbarColor RenderStyle::initialScrollbarColor() { return CSS::Keyword::Auto { }; }
constexpr Style::ScrollbarGutter RenderStyle::initialScrollbarGutter() { return CSS::Keyword::Auto { }; }
constexpr Style::ScrollbarWidth RenderStyle::initialScrollbarWidth() { return Style::ScrollbarWidth::Auto; }
constexpr Style::ShapeImageThreshold RenderStyle::initialShapeImageThreshold() { return 0_css_number; }
inline Style::ShapeMargin RenderStyle::initialShapeMargin() { return 0_css_px; }
inline Style::ShapeOutside RenderStyle::initialShapeOutside() { return CSS::Keyword::None { }; }
inline Style::PreferredSize RenderStyle::initialSize() { return CSS::Keyword::Auto { }; }
constexpr Style::SpeakAs RenderStyle::initialSpeakAs() { return CSS::Keyword::Normal { }; }
constexpr Style::ZIndex RenderStyle::initialSpecifiedZIndex() { return CSS::Keyword::Auto { }; }
inline Style::Color RenderStyle::initialStrokeColor() { return { Color::transparentBlack }; }
constexpr Style::StrokeMiterlimit RenderStyle::initialStrokeMiterLimit() { return 4_css_number; }
inline Style::StrokeWidth RenderStyle::initialStrokeWidth() { return 1_css_px; }
constexpr Style::TabSize RenderStyle::initialTabSize() { return 8_css_number; }
constexpr TableLayoutType RenderStyle::initialTableLayout() { return TableLayoutType::Auto; }
constexpr Style::TextAlign RenderStyle::initialTextAlign() { return Style::TextAlign::Start; }
constexpr Style::TextAlignLast RenderStyle::initialTextAlignLast() { return Style::TextAlignLast::Auto; }
constexpr TextBoxTrim RenderStyle::initialTextBoxTrim() { return TextBoxTrim::None; }
constexpr Style::TextBoxEdge RenderStyle::initialTextBoxEdge() { return CSS::Keyword::Auto { }; }
constexpr Style::LineFitEdge RenderStyle::initialLineFitEdge() { return CSS::Keyword::Leading { }; }
constexpr TextCombine RenderStyle::initialTextCombine() { return TextCombine::None; }
inline Style::Color RenderStyle::initialTextDecorationColor() { return CSS::Keyword::Currentcolor { }; }
constexpr Style::TextDecorationLine RenderStyle::initialTextDecorationLine() { return CSS::Keyword::None { }; }
constexpr Style::TextDecorationLine RenderStyle::initialTextDecorationLineInEffect() { return initialTextDecorationLine(); }
constexpr TextDecorationSkipInk RenderStyle::initialTextDecorationSkipInk() { return TextDecorationSkipInk::Auto; }
constexpr TextDecorationStyle RenderStyle::initialTextDecorationStyle() { return TextDecorationStyle::Solid; }
inline Style::TextDecorationThickness RenderStyle::initialTextDecorationThickness() { return CSS::Keyword::Auto { }; }
inline Style::Color RenderStyle::initialTextEmphasisColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::TextEmphasisStyle RenderStyle::initialTextEmphasisStyle() { return CSS::Keyword::None { }; }
constexpr Style::TextEmphasisPosition RenderStyle::initialTextEmphasisPosition() { return { Style::TextEmphasisPositionValue::Over, Style::TextEmphasisPositionValue::Right }; }
inline Style::Color RenderStyle::initialTextFillColor() { return CSS::Keyword::Currentcolor { }; }
constexpr TextGroupAlign RenderStyle::initialTextGroupAlign() { return TextGroupAlign::None; }
inline Style::TextIndent RenderStyle::initialTextIndent() { return 0_css_px; }
constexpr TextJustify RenderStyle::initialTextJustify() { return TextJustify::Auto; }
constexpr TextOrientation RenderStyle::initialTextOrientation() { return TextOrientation::Mixed; }
constexpr TextOverflow RenderStyle::initialTextOverflow() { return TextOverflow::Clip; }
constexpr TextSecurity RenderStyle::initialTextSecurity() { return TextSecurity::None; }
inline Style::TextShadows RenderStyle::initialTextShadow() { return CSS::Keyword::None { }; }
inline Style::Color RenderStyle::initialTextStrokeColor() { return CSS::Keyword::Currentcolor { }; }
constexpr Style::WebkitTextStrokeWidth RenderStyle::initialTextStrokeWidth() { return 0_css_px; }
constexpr Style::TextTransform RenderStyle::initialTextTransform() { return CSS::Keyword::None { }; }
inline Style::TextUnderlineOffset RenderStyle::initialTextUnderlineOffset() { return CSS::Keyword::Auto { }; }
constexpr Style::TextUnderlinePosition RenderStyle::initialTextUnderlinePosition() { return CSS::Keyword::Auto { }; }
constexpr TextWrapMode RenderStyle::initialTextWrapMode() { return TextWrapMode::Wrap; }
constexpr TextWrapStyle RenderStyle::initialTextWrapStyle() { return TextWrapStyle::Auto; }
constexpr TextZoom RenderStyle::initialTextZoom() { return TextZoom::Normal; }
constexpr Style::TouchAction RenderStyle::initialTouchAction() { return CSS::Keyword::Auto { }; }
inline Style::Transform RenderStyle::initialTransform() { return CSS::Keyword::None { }; }
constexpr TransformBox RenderStyle::initialTransformBox() { return TransformBox::ViewBox; }
inline Style::Transitions RenderStyle::initialTransitions() { return CSS::Keyword::All { }; }
inline Style::Rotate RenderStyle::initialRotate() { return CSS::Keyword::None { }; }
inline Style::Scale RenderStyle::initialScale() { return CSS::Keyword::None { }; }
inline Style::Translate RenderStyle::initialTranslate() { return CSS::Keyword::None { }; }
inline Style::TransformOrigin RenderStyle::initialTransformOrigin() { return { initialTransformOriginX(), initialTransformOriginY(), initialTransformOriginZ() }; }
inline Style::TransformOriginX RenderStyle::initialTransformOriginX() { return 50_css_percentage; }
inline Style::TransformOriginY RenderStyle::initialTransformOriginY() { return 50_css_percentage; }
inline Style::TransformOriginZ RenderStyle::initialTransformOriginZ() { return 0_css_px; }
constexpr TransformStyle3D RenderStyle::initialTransformStyle3D() { return TransformStyle3D::Flat; }
constexpr UnicodeBidi RenderStyle::initialUnicodeBidi() { return UnicodeBidi::Normal; }
constexpr Style::ZIndex RenderStyle::initialUsedZIndex() { return CSS::Keyword::Auto { }; }
constexpr UserDrag RenderStyle::initialUserDrag() { return UserDrag::Auto; }
constexpr UserModify RenderStyle::initialUserModify() { return UserModify::ReadOnly; }
constexpr UserSelect RenderStyle::initialUserSelect() { return UserSelect::Text; }
inline Style::VerticalAlign RenderStyle::initialVerticalAlign() { return CSS::Keyword::Baseline { }; }
inline Style::ProgressTimelineAxes RenderStyle::initialViewTimelineAxes() { return CSS::Keyword::Block { }; }
inline Style::ViewTimelineInsets RenderStyle::initialViewTimelineInsets() { return CSS::Keyword::Auto { }; }
inline Style::ProgressTimelineNames RenderStyle::initialViewTimelineNames() { return CSS::Keyword::None { }; }
inline Style::ViewTransitionClasses RenderStyle::initialViewTransitionClasses() { return CSS::Keyword::None { }; }
inline Style::ViewTransitionName RenderStyle::initialViewTransitionName() { return CSS::Keyword::None { }; }
constexpr Visibility RenderStyle::initialVisibility() { return Visibility::Visible; }
inline Style::NameScope RenderStyle::initialTimelineScope() { return CSS::Keyword::None { }; }
constexpr WhiteSpaceCollapse RenderStyle::initialWhiteSpaceCollapse() { return WhiteSpaceCollapse::Collapse; }
constexpr Style::Widows RenderStyle::initialWidows() { return CSS::Keyword::Auto { }; }
inline Style::WillChange RenderStyle::initialWillChange() { return CSS::Keyword::Auto { }; }
constexpr WordBreak RenderStyle::initialWordBreak() { return WordBreak::Normal; }
inline Style::WordSpacing RenderStyle::initialWordSpacing() { return CSS::Keyword::Normal { }; }
constexpr StyleWritingMode RenderStyle::initialWritingMode() { return StyleWritingMode::HorizontalTb; }
inline Style::Color RenderStyle::initialBorderBottomColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::Color RenderStyle::initialBorderLeftColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::Color RenderStyle::initialBorderRightColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::Color RenderStyle::initialBorderTopColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::Color RenderStyle::initialColumnRuleColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::Color RenderStyle::initialOutlineColor() { return CSS::Keyword::Currentcolor { }; }
inline Style::AccentColor RenderStyle::initialAccentColor() { return CSS::Keyword::Auto { }; }
inline Style::SVGCenterCoordinateComponent RenderStyle::initialCx() { return 0_css_px; }
inline Style::SVGCenterCoordinateComponent RenderStyle::initialCy() { return 0_css_px; }
inline Style::SVGPathData RenderStyle::initialD() { return CSS::Keyword::None { }; }
inline Style::SVGRadius RenderStyle::initialR() { return 0_css_px; }
inline Style::SVGRadiusComponent RenderStyle::initialRx() { return CSS::Keyword::Auto { }; }
inline Style::SVGRadiusComponent RenderStyle::initialRy() { return CSS::Keyword::Auto { }; }
inline Style::SVGCoordinateComponent RenderStyle::initialX() { return 0_css_px; }
inline Style::SVGCoordinateComponent RenderStyle::initialY() { return 0_css_px; }
inline Style::SVGStrokeDasharray RenderStyle::initialStrokeDashArray() { return CSS::Keyword::None { }; }
inline Style::SVGStrokeDashoffset RenderStyle::initialStrokeDashOffset() { return 0_css_px; }
constexpr Style::Opacity RenderStyle::initialFillOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialStrokeOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialStopOpacity() { return 1_css_number; }
constexpr Style::Opacity RenderStyle::initialFloodOpacity() { return 1_css_number; }
constexpr AlignmentBaseline RenderStyle::initialAlignmentBaseline() { return AlignmentBaseline::Baseline; }
constexpr DominantBaseline RenderStyle::initialDominantBaseline() { return DominantBaseline::Auto; }
constexpr VectorEffect RenderStyle::initialVectorEffect() { return VectorEffect::None; }
constexpr BufferedRendering RenderStyle::initialBufferedRendering() { return BufferedRendering::Auto; }
constexpr WindRule RenderStyle::initialClipRule() { return WindRule::NonZero; }
constexpr ColorInterpolation RenderStyle::initialColorInterpolation() { return ColorInterpolation::SRGB; }
constexpr ColorInterpolation RenderStyle::initialColorInterpolationFilters() { return ColorInterpolation::LinearRGB; }
constexpr WindRule RenderStyle::initialFillRule() { return WindRule::NonZero; }
constexpr ShapeRendering RenderStyle::initialShapeRendering() { return ShapeRendering::Auto; }
constexpr TextAnchor RenderStyle::initialTextAnchor() { return TextAnchor::Start; }
constexpr Style::SVGGlyphOrientationHorizontal RenderStyle::initialGlyphOrientationHorizontal() { return Style::SVGGlyphOrientationHorizontal::Degrees0; }
constexpr Style::SVGGlyphOrientationVertical RenderStyle::initialGlyphOrientationVertical() { return Style::SVGGlyphOrientationVertical::Auto; }
inline Style::SVGPaint RenderStyle::initialFill() { return Style::Color { Color::black }; }
inline Style::SVGPaint RenderStyle::initialStroke() { return CSS::Keyword::None { }; }
inline Style::Color RenderStyle::initialStopColor() { return Color::black; }
inline Style::Color RenderStyle::initialFloodColor() { return Color::black; }
inline Style::Color RenderStyle::initialLightingColor() { return Color::white; }
inline Style::SVGMarkerResource RenderStyle::initialMarkerStart() { return CSS::Keyword::None { }; }
inline Style::SVGMarkerResource RenderStyle::initialMarkerMid() { return CSS::Keyword::None { }; }
inline Style::SVGMarkerResource RenderStyle::initialMarkerEnd() { return CSS::Keyword::None { }; }
constexpr MaskType RenderStyle::initialMaskType() { return MaskType::Luminance; }
inline Style::SVGBaselineShift RenderStyle::initialBaselineShift() { return CSS::Keyword::Baseline { }; }
constexpr BlendMode RenderStyle::initialBlendMode() { return BlendMode::Normal; }
constexpr Isolation RenderStyle::initialIsolation() { return Isolation::Auto; }
inline Style::Filter RenderStyle::initialBackdropFilter() { return CSS::Keyword::None { }; }
inline Style::DynamicRangeLimit RenderStyle::initialDynamicRangeLimit() { return CSS::Keyword::NoLimit { }; }
#if ENABLE(APPLE_PAY)
constexpr ApplePayButtonStyle RenderStyle::initialApplePayButtonStyle() { return ApplePayButtonStyle::Black; }
constexpr ApplePayButtonType RenderStyle::initialApplePayButtonType() { return ApplePayButtonType::Plain; }
#endif
#if ENABLE(CURSOR_VISIBILITY)
constexpr CursorVisibility RenderStyle::initialCursorVisibility() { return CursorVisibility::Auto; }
#endif
#if ENABLE(DARK_MODE_CSS)
inline Style::ColorScheme RenderStyle::initialColorScheme() { return Style::ColorScheme { .schemes = { }, .only = { } }; }
#endif
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
constexpr Style::WebkitOverflowScrolling RenderStyle::initialOverflowScrolling() { return Style::WebkitOverflowScrolling::Auto; }
#endif
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
constexpr Style::WebkitTouchCallout RenderStyle::initialTouchCallout() { return Style::WebkitTouchCallout::Default; }
#endif
#if ENABLE(TEXT_AUTOSIZING)
inline Style::LineHeight RenderStyle::initialSpecifiedLineHeight() { return CSS::Keyword::Normal { }; }
constexpr Style::TextSizeAdjust RenderStyle::initialTextSizeAdjust() { return CSS::Keyword::Auto { }; }
#endif

// MARK: - Property getters

inline TextDirection RenderStyle::computedDirection() const { return writingMode().computedTextDirection(); }
inline StyleWritingMode RenderStyle::computedWritingMode() const { return writingMode().computedWritingMode(); }
inline Style::LineWidth RenderStyle::borderBottomWidth() const { return border().borderBottomWidth(); }
inline Style::LineWidth RenderStyle::borderLeftWidth() const { return border().borderLeftWidth(); }
inline Style::LineWidth RenderStyle::borderRightWidth() const { return border().borderRightWidth(); }
inline Style::LineWidth RenderStyle::borderTopWidth() const { return border().borderTopWidth(); }
inline Style::LineWidth RenderStyle::columnRuleWidth() const { return m_nonInheritedData->miscData->multiCol->columnRuleWidth(); }

// FIXME: - Below are property getters that are not yet generated

// FIXME: Support properties that set more than one value when set.
inline StyleAppearance RenderStyle::appearance() const { return static_cast<StyleAppearance>(m_nonInheritedData->miscData->appearance); }
inline BlendMode RenderStyle::blendMode() const { return static_cast<BlendMode>(m_nonInheritedData->rareData->effectiveBlendMode); }
inline float RenderStyle::zoom() const { return m_nonInheritedData->rareData->zoom; }

// FIXME: Add a type that encapsulates both caretColor() and hasAutoCaretColor().
inline const Style::Color& RenderStyle::caretColor() const { return m_rareInheritedData->caretColor; }
inline bool RenderStyle::hasAutoCaretColor() const { return m_rareInheritedData->hasAutoCaretColor; }
inline const Style::Color& RenderStyle::visitedLinkCaretColor() const { return m_rareInheritedData->visitedLinkCaretColor; }
inline bool RenderStyle::hasVisitedLinkAutoCaretColor() const { return m_rareInheritedData->hasVisitedLinkAutoCaretColor; }

// FIXME: Support generating properties that have their storage spread out
inline Style::Cursor RenderStyle::cursor() const { return { m_rareInheritedData->cursorImages, cursorType() }; }
inline Style::ZIndex RenderStyle::specifiedZIndex() const { return m_nonInheritedData->boxData->specifiedZIndex(); }

// FIXME: Support descriptors
inline const Style::PageSize& RenderStyle::pageSize() const { return m_nonInheritedData->rareData->pageSize; }

// FIXME: Support generating getter and setter with different names (or rename computedLetterSpacing() to letterSpacing() and computedWordSpacing() to wordSpacing())
inline const Style::LetterSpacing& RenderStyle::computedLetterSpacing() const { return m_inheritedData->fontData->letterSpacing; }
inline const Style::WordSpacing& RenderStyle::computedWordSpacing() const { return m_inheritedData->fontData->wordSpacing; }

// FIXME: Support generated font-property getters
inline Style::FontFamilies RenderStyle::fontFamily() const { return { fontDescription().families(), fontDescription().isSpecifiedFont() }; }
inline Style::FontPalette RenderStyle::fontPalette() const { return fontDescription().fontPalette(); }
inline Style::FontSizeAdjust RenderStyle::fontSizeAdjust() const { return fontDescription().fontSizeAdjust(); }
inline Style::FontStyle RenderStyle::fontStyle() const { return { fontDescription().fontStyleSlope(), fontDescription().fontStyleAxis() }; }
inline FontOpticalSizing RenderStyle::fontOpticalSizing() const { return fontDescription().opticalSizing(); }
inline Style::FontFeatureSettings RenderStyle::fontFeatureSettings() const { return fontDescription().featureSettings(); }
inline Style::FontVariationSettings RenderStyle::fontVariationSettings() const { return fontDescription().variationSettings(); }
inline Style::FontWeight RenderStyle::fontWeight() const { return fontDescription().weight(); }
inline Style::FontWidth RenderStyle::fontWidth() const { return fontDescription().width(); }
inline Kerning RenderStyle::fontKerning() const { return fontDescription().kerning(); }
inline FontSmoothingMode RenderStyle::fontSmoothing() const { return fontDescription().fontSmoothing(); }
inline FontSynthesisLonghandValue RenderStyle::fontSynthesisSmallCaps() const { return fontDescription().fontSynthesisSmallCaps(); }
inline FontSynthesisLonghandValue RenderStyle::fontSynthesisStyle() const { return fontDescription().fontSynthesisStyle(); }
inline FontSynthesisLonghandValue RenderStyle::fontSynthesisWeight() const { return fontDescription().fontSynthesisWeight(); }
inline Style::FontVariantAlternates RenderStyle::fontVariantAlternates() const { return fontDescription().variantAlternates(); }
inline FontVariantCaps RenderStyle::fontVariantCaps() const { return fontDescription().variantCaps(); }
inline Style::FontVariantEastAsian RenderStyle::fontVariantEastAsian() const { return fontDescription().variantEastAsian(); }
inline FontVariantEmoji RenderStyle::fontVariantEmoji() const { return fontDescription().variantEmoji(); }
inline Style::FontVariantLigatures RenderStyle::fontVariantLigatures() const { return fontDescription().variantLigatures(); }
inline Style::FontVariantNumeric RenderStyle::fontVariantNumeric() const { return fontDescription().variantNumeric(); }
inline FontVariantPosition RenderStyle::fontVariantPosition() const { return fontDescription().variantPosition(); }
inline TextRenderingMode RenderStyle::textRendering() const { return fontDescription().textRenderingMode(); }
inline Style::TextAutospace RenderStyle::textAutospace() const { return fontDescription().textAutospace(); }
inline Style::TextSpacingTrim RenderStyle::textSpacingTrim() const { return fontDescription().textSpacingTrim(); }
inline Style::WebkitLocale RenderStyle::locale() const { return fontDescription().specifiedLocale(); }
inline Style::WebkitLocale RenderStyle::computedLocale() const { return fontDescription().computedLocale(); }

} // namespace WebCore
