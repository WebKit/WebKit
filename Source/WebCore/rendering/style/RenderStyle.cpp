/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderStyle.h"

#include "AutosizeStatus.h"
#include "CSSCustomPropertyValue.h"
#include "ColorBlending.h"
#include "FontCascade.h"
#include "FontSelector.h"
#include "Logging.h"
#include "Pagination.h"
#include "RenderBlock.h"
#include "RenderElement.h"
#include "RenderStyleBase+ConstructionInlines.h"
#include "RenderStyle+SettersInlines.h"
#include "RenderTheme.h"
#include "StyleCustomPropertyRegistry.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleTransformResolver.h"
#include "StyleTreeResolver.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(TEXT_AUTOSIZING)
#include <wtf/text/StringHash.h>
#endif

namespace WebCore {

RenderStyle::RenderStyle(RenderStyle&&) = default;
RenderStyle& RenderStyle::operator=(RenderStyle&&) = default;

inline RenderStyle::RenderStyle(CreateDefaultStyleTag tag)
    : RenderStyleProperties { tag }
{
}

inline RenderStyle::RenderStyle(const RenderStyle& other, CloneTag tag)
    : RenderStyleProperties { other, tag }
{
}

inline RenderStyle::RenderStyle(RenderStyle& a, RenderStyle&& b)
    : RenderStyleProperties { a, WTF::move(b) }
{
}

RenderStyle& RenderStyle::defaultStyleSingleton()
{
    static NeverDestroyed<RenderStyle> style { CreateDefaultStyle };
    return style;
}

RenderStyle RenderStyle::create()
{
    return clone(defaultStyleSingleton());
}

std::unique_ptr<RenderStyle> RenderStyle::createPtr()
{
    return clonePtr(defaultStyleSingleton());
}

std::unique_ptr<RenderStyle> RenderStyle::createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry& registry)
{
    return clonePtr(registry.initialValuePrototypeStyle());
}

RenderStyle RenderStyle::clone(const RenderStyle& style)
{
    return RenderStyle(style, Clone);
}

RenderStyle RenderStyle::cloneIncludingPseudoElements(const RenderStyle& style)
{
    auto newStyle = RenderStyle(style, Clone);
    newStyle.copyPseudoElementsFrom(style);
    return newStyle;
}

std::unique_ptr<RenderStyle> RenderStyle::clonePtr(const RenderStyle& style)
{
    return makeUnique<RenderStyle>(style, Clone);
}

RenderStyle RenderStyle::createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType display)
{
    auto newStyle = create();
    newStyle.inheritFrom(parentStyle);
    newStyle.inheritUnicodeBidiFrom(parentStyle);
    newStyle.setDisplay(display);
    return newStyle;
}

RenderStyle RenderStyle::createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle)
{
    ASSERT(pseudoStyle.pseudoElementType() == PseudoElementType::Before || pseudoStyle.pseudoElementType() == PseudoElementType::After);

    auto style = create();
    style.inheritFrom(pseudoStyle);
    return style;
}

RenderStyle RenderStyle::replace(RenderStyle&& newStyle)
{
    return RenderStyle { *this, WTF::move(newStyle) };
}

void RenderStyle::copyPseudoElementsFrom(const RenderStyle& other)
{
    for (auto& [key, pseudoElementStyle] : other.m_computedStyle.cachedPseudoStyles()) {
        if (!pseudoElementStyle) {
            ASSERT_NOT_REACHED();
            continue;
        }
        addCachedPseudoStyle(makeUnique<RenderStyle>(cloneIncludingPseudoElements(*pseudoElementStyle)));
    }
}

// MARK: - Specific style change queries

bool RenderStyle::scrollAnchoringSuppressionStyleDidChange(const RenderStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there are any style changes that should result in an scroll anchoring suppression
    if (!other)
        return false;

    if (m_computedStyle.m_nonInheritedData->boxData.ptr() != other->m_computedStyle.m_nonInheritedData->boxData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& boxData = m_computedStyle.m_nonInheritedData->boxData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherBoxData = other->m_computedStyle.m_nonInheritedData->boxData.get();
        if (boxData.width != otherBoxData.width
            || boxData.minWidth != otherBoxData.minWidth
            || boxData.maxWidth != otherBoxData.maxWidth
            || boxData.height != otherBoxData.height
            || boxData.minHeight != otherBoxData.minHeight
            || boxData.maxHeight != otherBoxData.maxHeight)
            return true;
    }

    if (overflowAnchor() != other->overflowAnchor() && overflowAnchor() == OverflowAnchor::None)
        return true;

    if (position() != other->position())
        return true;

    if (m_computedStyle.m_nonInheritedData->surroundData.ptr() && other->m_computedStyle.m_nonInheritedData->surroundData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& surroundData = m_computedStyle.m_nonInheritedData->surroundData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherSurroundData = other->m_computedStyle.m_nonInheritedData->surroundData.get();
        if (surroundData.margin != otherSurroundData.margin)
            return true;

        if (surroundData.padding != otherSurroundData.padding)
            return true;

        if (position() != PositionType::Static) {
            if (surroundData.inset != otherSurroundData.inset)
                return true;
        }
    }

    if (hasTransformRelatedProperty() != other->hasTransformRelatedProperty() || transform() != other->transform())
        return true;

    return false;
}

bool RenderStyle::outOfFlowPositionStyleDidChange(const RenderStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there is a style change that causes an element to become or stop
    // being absolutely or fixed positioned
    return other && hasOutOfFlowPosition() != other->hasOutOfFlowPosition();
}

#if ENABLE(TEXT_AUTOSIZING)

// MARK: - Text Autosizing

bool RenderStyle::isIdempotentTextAutosizingCandidate() const
{
    return isIdempotentTextAutosizingCandidate(OptionSet<AutosizeStatus::Fields>::fromRaw(m_computedStyle.m_inheritedFlags.autosizeStatus));
}

bool RenderStyle::isIdempotentTextAutosizingCandidate(AutosizeStatus status) const
{
    // Refer to <rdar://problem/51826266> for more information regarding how this function was generated.
    auto fields = status.fields();

    if (fields.contains(AutosizeStatus::Fields::AvoidSubtree))
        return false;

    constexpr float smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 5;
    constexpr float largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText = 25;

    if (fields.contains(AutosizeStatus::Fields::FixedHeight)) {
        if (fields.contains(AutosizeStatus::Fields::FixedWidth)) {
            if (whiteSpaceCollapse() == WhiteSpaceCollapse::Collapse && textWrapMode() == TextWrapMode::NoWrap) {
                if (width().isFixed())
                    return false;
                if (auto fixedHeight = height().tryFixed(); fixedHeight && specifiedLineHeight().isFixed()) {
                    if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed()) {
                        float specifiedSize = specifiedFontSize();
                        if (fixedHeight->resolveZoom(usedZoomForLength()) == specifiedSize && fixedSpecifiedLineHeight->resolveZoom(usedZoomForLength()) == specifiedSize)
                            return false;
                    }
                }
                return true;
            }
            if (fields.contains(AutosizeStatus::Fields::Floating)) {
                if (auto fixedHeight = height().tryFixed(); specifiedLineHeight().isFixed() && fixedHeight) {
                    if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed()) {
                        float specifiedSize = specifiedFontSize();
                        if (fixedSpecifiedLineHeight->resolveZoom(Style::ZoomFactor { 1.0f }) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText
                            && fixedHeight->resolveZoom(usedZoomForLength()) - specifiedSize > smallMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
                            return true;
                    }
                }
                return false;
            }
            if (fields.contains(AutosizeStatus::Fields::OverflowXHidden))
                return false;
            return true;
        }
        if (fields.contains(AutosizeStatus::Fields::OverflowXHidden)) {
            if (fields.contains(AutosizeStatus::Fields::Floating))
                return false;
            return true;
        }
        return true;
    }

    if (width().isFixed()) {
        if (breakWords())
            return true;
        return false;
    }

    if (textSizeAdjust().isPercentage() && textSizeAdjust().percentage() == 100) {
        if (fields.contains(AutosizeStatus::Fields::Floating))
            return true;
        if (fields.contains(AutosizeStatus::Fields::FixedWidth))
            return true;
        if (auto fixedSpecifiedLineHeight = specifiedLineHeight().tryFixed(); fixedSpecifiedLineHeight && fixedSpecifiedLineHeight->resolveZoom(usedZoomForLength()) - specifiedFontSize() > largeMinimumDifferenceThresholdBetweenLineHeightAndSpecifiedFontSizeForBoostingText)
            return true;
        return false;
    }

    if (hasBackgroundImage() && backgroundLayers().usedFirst().repeat() == FillRepeat::NoRepeat)
        return false;

    return true;
}

#endif // ENABLE(TEXT_AUTOSIZING)

// MARK: - Used Values

String RenderStyle::altFromContent() const
{
    if (auto* contentData = content().tryData())
        return contentData->altText.value_or(nullString());
    return { };
}

const AtomString& RenderStyle::hyphenString() const
{
    ASSERT(hyphens() != Hyphens::None);

    return WTF::switchOn(hyphenateCharacter(),
        [&](const CSS::Keyword::Auto&) -> const AtomString& {
            // FIXME: This should depend on locale.
            static MainThreadNeverDestroyed<const AtomString> hyphenMinusString(span(hyphenMinus));
            static MainThreadNeverDestroyed<const AtomString> hyphenString(span(hyphen));

            return fontCascade().primaryFont()->glyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
        },
        [](const AtomString& string) -> const AtomString& {
            return string;
        }
    );
}

float RenderStyle::usedStrokeWidth(const IntSize& viewportSize) const
{
    // Use the stroke-width and stroke-color value combination only if stroke-color has been explicitly specified.
    // Since there will be no visible stroke when stroke-color is not specified (transparent by default), we fall
    // back to the legacy Webkit text stroke combination in that case.
    if (!hasExplicitlySetStrokeColor())
        return Style::evaluate<float>(textStrokeWidth(), usedZoomForLength());

    return WTF::switchOn(strokeWidth(),
        [&](const Style::StrokeWidth::Fixed& fixedStrokeWidth) -> float {
            return Style::evaluate<float>(fixedStrokeWidth, usedZoomForLength());
        },
        [&](const Style::StrokeWidth::Percentage& percentageStrokeWidth) -> float {
            // According to the spec, https://drafts.fxtf.org/paint/#stroke-width, the percentage is relative to the scaled viewport size.
            // The scaled viewport size is the geometric mean of the viewport width and height.
            return percentageStrokeWidth.value * (viewportSize.width() + viewportSize.height()) / 200.0f;
        },
        [&](const Style::StrokeWidth::Calc& calcStrokeWidth) -> float {
            // FIXME: It is almost certainly wrong that calc and percentage are being handled differently - https://bugs.webkit.org/show_bug.cgi?id=296482
            return Style::evaluate<float>(calcStrokeWidth, viewportSize.width(), usedZoomForLength());
        }
    );
}

Color RenderStyle::usedStrokeColor() const
{
    return hasExplicitlySetStrokeColor() ? visitedDependentStrokeColor() : visitedDependentTextStrokeColor();
}

Color RenderStyle::usedStrokeColorApplyingColorFilter() const
{
    return hasExplicitlySetStrokeColor() ? visitedDependentStrokeColorApplyingColorFilter() : visitedDependentTextStrokeColorApplyingColorFilter();
}

Style::Contain RenderStyle::usedContain() const
{
    auto result = contain();

    switch (containerType()) {
    case ContainerType::Normal:
        break;
    case ContainerType::Size:
        result.add({ Style::ContainValue::Style, Style::ContainValue::Size });
        break;
    case ContainerType::InlineSize:
        result.add({ Style::ContainValue::Style, Style::ContainValue::InlineSize });
        break;
    };

    return result;
}

UsedClear RenderStyle::usedClear(const RenderElement& renderer)
{
    auto computedClear = renderer.style().clear();
    auto writingMode = renderer.containingBlock()->writingMode();
    switch (computedClear) {
    case Clear::None:
        return UsedClear::None;
    case Clear::Both:
        return UsedClear::Both;
    case Clear::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Left : UsedClear::Right;
    case Clear::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedClear::Right : UsedClear::Left;
    case Clear::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Left : UsedClear::Right;
    case Clear::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedClear::Right : UsedClear::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UsedFloat RenderStyle::usedFloat(const RenderElement& renderer)
{
    auto computedFloat = renderer.style().floating();
    auto writingMode = renderer.containingBlock()->writingMode();
    switch (computedFloat) {
    case Float::None:
        return UsedFloat::None;
    case Float::Left:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Left : UsedFloat::Right;
    case Float::Right:
        return writingMode.isLogicalLeftLineLeft() ? UsedFloat::Right : UsedFloat::Left;
    case Float::InlineStart:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Left : UsedFloat::Right;
    case Float::InlineEnd:
        return writingMode.isLogicalLeftInlineStart() ? UsedFloat::Right : UsedFloat::Left;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

UserSelect RenderStyle::usedUserSelect() const
{
    if (effectiveInert())
        return UserSelect::None;

    auto value = userSelect();
    if (userModify() != UserModify::ReadOnly && userDrag() != UserDrag::Element)
        return value == UserSelect::None ? UserSelect::Text : value;

    return value;
}

Color RenderStyle::usedScrollbarThumbColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const auto& parts) -> Color {
            Style::ColorResolver colorResolver { *this };
            if (!appleColorFilter().isNone())
                return colorResolver.colorResolvingCurrentColorApplyingColorFilter(parts.thumb);
            return colorResolver.colorResolvingCurrentColor(parts.thumb);
        }
    );
}

Color RenderStyle::usedScrollbarTrackColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const auto& parts) -> Color {
            Style::ColorResolver colorResolver { *this };
            if (!appleColorFilter().isNone())
                return colorResolver.colorResolvingCurrentColorApplyingColorFilter(parts.track);
            return colorResolver.colorResolvingCurrentColor(parts.track);
        }
    );
}

Color RenderStyle::usedAccentColor(OptionSet<StyleColorOptions> styleColorOptions) const
{
    return WTF::switchOn(accentColor(),
        [](const CSS::Keyword::Auto&) -> Color {
            return { };
        },
        [&](const Style::Color& color) -> Color {
            Style::ColorResolver colorResolver { *this };

            auto resolvedAccentColor = colorResolver.colorResolvingCurrentColor(color);

            if (!resolvedAccentColor.isOpaque()) {
                auto computedCanvasColor = RenderTheme::singleton().systemColor(CSSValueCanvas, styleColorOptions);
                resolvedAccentColor = blendSourceOver(computedCanvasColor, resolvedAccentColor);
            }

            if (!appleColorFilter().isNone())
                return colorResolver.colorApplyingColorFilter(resolvedAccentColor);
            return resolvedAccentColor;
        }
    );
}

Style::LineWidth RenderStyle::usedColumnRuleWidth() const
{
    if (!isVisibleBorderStyle(m_computedStyle.columnRuleStyle()))
        return 0_css_px;
    return m_computedStyle.columnRuleWidth();
}

Style::Length<> RenderStyle::usedOutlineOffset() const
{
    auto& outline = m_computedStyle.outline();
    if (static_cast<OutlineStyle>(outline.outlineStyle) == OutlineStyle::Auto)
        return Style::Length<> { Style::evaluate<float>(outline.outlineOffset, Style::ZoomNeeded { }) + RenderTheme::platformFocusRingOffset(Style::evaluate<float>(outline.outlineWidth, Style::ZoomNeeded { })) };
    return outline.outlineOffset;
}

Style::LineWidth RenderStyle::usedOutlineWidth() const
{
    auto& outline = m_computedStyle.outline();
    if (static_cast<OutlineStyle>(outline.outlineStyle) == OutlineStyle::None)
        return 0_css_px;
    if (static_cast<OutlineStyle>(outline.outlineStyle) == OutlineStyle::Auto)
        return Style::LineWidth { std::max(Style::evaluate<float>(outline.outlineWidth, Style::ZoomNeeded { }), RenderTheme::platformFocusRingWidth()) };
    return outline.outlineWidth;
}

float RenderStyle::usedOutlineSize() const
{
    return std::max(0.0f, Style::evaluate<float>(usedOutlineWidth(), Style::ZoomNeeded { }) + Style::evaluate<float>(usedOutlineOffset(), Style::ZoomNeeded { }));
}

// MARK: - Derived Values

template<typename OutsetValue>
static LayoutUnit computeOutset(const OutsetValue& outsetValue, LayoutUnit borderWidth)
{
    return WTF::switchOn(outsetValue,
        [&](const typename OutsetValue::Number& number) {
            return LayoutUnit(number.value * borderWidth);
        },
        [&](const typename OutsetValue::Length& length) {
            return LayoutUnit(length.resolveZoom(Style::ZoomNeeded { }));
        }
    );
}

LayoutBoxExtent RenderStyle::imageOutsets(const Style::BorderImage& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(usedBorderTopWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(usedBorderRightWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(usedBorderBottomWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(usedBorderLeftWidth(), Style::ZoomNeeded { })),
    };
}

LayoutBoxExtent RenderStyle::imageOutsets(const Style::MaskBorder& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(usedBorderTopWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(usedBorderRightWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(usedBorderBottomWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(usedBorderLeftWidth(), Style::ZoomNeeded { })),
    };
}

LayoutBoxExtent RenderStyle::borderImageOutsets() const
{
    return imageOutsets(borderImage());
}

LayoutBoxExtent RenderStyle::maskBorderOutsets() const
{
    return imageOutsets(maskBorder());
}

// MARK: - Logical

const BorderValue& RenderStyle::borderBefore(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderTop();
    case FlowDirection::BottomToTop:
        return borderBottom();
    case FlowDirection::LeftToRight:
        return borderLeft();
    case FlowDirection::RightToLeft:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

const BorderValue& RenderStyle::borderAfter(const WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
        return borderBottom();
    case FlowDirection::BottomToTop:
        return borderTop();
    case FlowDirection::LeftToRight:
        return borderRight();
    case FlowDirection::RightToLeft:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderBottom();
}

const BorderValue& RenderStyle::borderStart(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderLeft() : borderRight();
    return writingMode.isInlineTopToBottom() ? borderTop() : borderBottom();
}

const BorderValue& RenderStyle::borderEnd(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderRight() : borderLeft();
    return writingMode.isInlineTopToBottom() ? borderBottom() : borderTop();
}

} // namespace WebCore
