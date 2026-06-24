/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 *
 */

#include "config.h"
#include "StyleComputedStyle.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"

#include "ColorBlending.h"
#include "FontCascadeInlines.h"
#include "Pagination.h"
#include "PlatformRenderTheme.h"
#include "RenderBlock.h"
#include "RenderTheme.h"
#include "StyleComputedStyle+ConstructionInlines.h"
#include "StyleCustomPropertyRegistry.h"
#include "StyleLineHeight.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleScaleTransformFunction.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Style {

struct SameSizeAsBorderValue {
    Color m_color;
    float m_width;
    int m_restBits;
};

static_assert(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), "BorderValue should not grow");

IGNORE_CLANG_WARNINGS_BEGIN("unused-private-field")

struct SameSizeAsComputedStyle : CanMakeCheckedPtr<SameSizeAsComputedStyle> {
    WTF_MAKE_TZONE_ALLOCATED(SameSizeAsComputedStyle);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SameSizeAsComputedStyle);
    struct NonInheritedFlags {
        unsigned display : 5;
        unsigned originalDisplay : 5;
        unsigned overflowX : 3;
        unsigned overflowY : 3;
        unsigned clear : 3;
        unsigned position : 3;
        unsigned unicodeBidi : 3;
        unsigned floating : 3;
        bool usesViewportUnits : 1;
        bool usesContainerUnits : 1;
        bool useTreeCountingFunctions : 1;
        bool hasExplicitlyInheritedProperties : 1;
        bool disallowsFastPathInheritance : 1;
        bool firstChildState : 1;
        bool lastChildState : 1;
        bool isLink : 1;
        unsigned pseudoElementType : 5;
        unsigned pseudoBits : 19;
        unsigned textDecorationLine : 5;
    } m_nonInheritedFlags;
    struct InheritedFlags {
        unsigned m_bitfields[2];
    } m_inheritedFlags;
    void* nonInheritedDataRefs[1];
    void* inheritedDataRefs[2];
    void* dataRefSvgStyle;
    HashMap<PseudoElementIdentifier, std::unique_ptr<ComputedStyle>> pseudos;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionCheck;
#endif
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(SameSizeAsComputedStyle);

IGNORE_CLANG_WARNINGS_END

static_assert(sizeof(ComputedStyle) == sizeof(SameSizeAsComputedStyle), "ComputedStyle should stay small");

ComputedStyle::ComputedStyle(ComputedStyle&&) = default;
ComputedStyle& ComputedStyle::operator=(ComputedStyle&&) = default;

SUPPRESS_NODELETE ComputedStyle& ComputedStyle::defaultStyleSingleton()
{
    static NeverDestroyed<ComputedStyle> style { CreateDefaultStyle };
    return style;
}

ComputedStyle ComputedStyle::create()
{
    return clone(defaultStyleSingleton());
}

std::unique_ptr<ComputedStyle> ComputedStyle::createPtr()
{
    return clonePtr(defaultStyleSingleton());
}

std::unique_ptr<ComputedStyle> ComputedStyle::createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry& registry)
{
    return clonePtr(registry.initialValuePrototypeStyle());
}

SUPPRESS_NODELETE ComputedStyle ComputedStyle::clone(const ComputedStyle& style)
{
    return ComputedStyle(style, Clone);
}

ComputedStyle ComputedStyle::cloneIncludingPseudoElements(const ComputedStyle& style)
{
    auto newStyle = ComputedStyle(style, Clone);
    newStyle.copyPseudoElementsFrom(style);
    return newStyle;
}

std::unique_ptr<ComputedStyle> ComputedStyle::clonePtr(const ComputedStyle& style)
{
    return makeUnique<ComputedStyle>(style, Clone);
}

ComputedStyle ComputedStyle::createAnonymousStyleWithDisplay(const ComputedStyle& parentStyle, Style::Display display)
{
    auto newStyle = create();
    newStyle.inheritFrom(parentStyle);
    newStyle.inheritUnicodeBidiFrom(parentStyle);
    newStyle.setDisplay(display);
    return newStyle;
}

ComputedStyle ComputedStyle::createStyleInheritingFromPseudoStyle(const ComputedStyle& pseudoStyle)
{
    ASSERT(pseudoStyle.pseudoElementType() == PseudoElementType::Before
        || pseudoStyle.pseudoElementType() == PseudoElementType::After
        || pseudoStyle.pseudoElementType() == PseudoElementType::Checkmark
        || pseudoStyle.pseudoElementType() == PseudoElementType::PickerIcon);

    auto style = create();
    style.inheritFrom(pseudoStyle);
    return style;
}

SUPPRESS_NODELETE ComputedStyle ComputedStyle::replace(ComputedStyle&& newStyle)
{
    return ComputedStyle { *this, WTF::move(newStyle) };
}

void ComputedStyle::copyPseudoElementsFrom(const ComputedStyle& other)
{
    for (auto& [key, pseudoElementStyle] : other.pseudoElementStyles()) {
        if (!pseudoElementStyle) {
            ASSERT_NOT_REACHED();
            continue;
        }
        addPseudoElementStyle(makeUnique<ComputedStyle>(cloneIncludingPseudoElements(*pseudoElementStyle)));
    }
}


void ComputedStyle::inheritFrom(const ComputedStyle& inheritParent)
{
    m_inheritedRareData = inheritParent.m_inheritedRareData;
    m_inheritedData = inheritParent.m_inheritedData;
    m_inheritedFlags = inheritParent.m_inheritedFlags;

    if (m_svgData != inheritParent.m_svgData)
        m_svgData.access().inheritFrom(inheritParent.m_svgData.get());
}

void ComputedStyle::inheritIgnoringCustomPropertiesFrom(const ComputedStyle& inheritParent)
{
    auto oldCustomProperties = m_inheritedRareData->customProperties;
    inheritFrom(inheritParent);
    if (oldCustomProperties != m_inheritedRareData->customProperties)
        m_inheritedRareData.access().customProperties = oldCustomProperties;
}

void ComputedStyle::inheritUnicodeBidiFrom(const ComputedStyle& inheritParent)
{
    m_nonInheritedFlags.unicodeBidi = inheritParent.m_nonInheritedFlags.unicodeBidi;
}

void ComputedStyle::fastPathInheritFrom(const ComputedStyle& inheritParent)
{
    ASSERT(!disallowsFastPathInheritance());

    // FIXME: Use this mechanism for other properties too, like variables.
    m_inheritedFlags.visibility = inheritParent.m_inheritedFlags.visibility;
    m_inheritedFlags.hasExplicitlySetColor = inheritParent.m_inheritedFlags.hasExplicitlySetColor;

    if (m_inheritedData.ptr() != inheritParent.m_inheritedData.ptr()) {
        if (m_inheritedData->nonFastPathInheritedEqual(*inheritParent.m_inheritedData)) {
            m_inheritedData = inheritParent.m_inheritedData;
            return;
        }
        m_inheritedData.access().fastPathInheritFrom(*inheritParent.m_inheritedData);
    }
}

void ComputedStyle::copyNonInheritedFrom(const ComputedStyle& other)
{
    m_nonInheritedData = other.m_nonInheritedData;
    m_nonInheritedFlags.copyNonInheritedFrom(other.m_nonInheritedFlags);

    if (m_svgData != other.m_svgData)
        m_svgData.access().copyNonInheritedFrom(other.m_svgData.get());

    ASSERT(zoom() == initialZoom());
}

void ComputedStyle::copyContentFrom(const ComputedStyle& other)
{
    if (!other.m_nonInheritedData->miscData->content.isData())
        return;
    m_nonInheritedData.access().miscData.access().content = other.m_nonInheritedData->miscData->content;
}

void ComputedStyle::copyPseudoElementBitsFrom(const ComputedStyle& other)
{
    m_nonInheritedFlags.pseudoBits = other.m_nonInheritedFlags.pseudoBits;
}

bool ComputedStyle::operator==(const ComputedStyle& other) const
{
    // compare everything except the pseudoStyle pointer
    return m_inheritedFlags == other.m_inheritedFlags
        && m_nonInheritedFlags == other.m_nonInheritedFlags
        && m_nonInheritedData == other.m_nonInheritedData
        && m_inheritedRareData == other.m_inheritedRareData
        && m_inheritedData == other.m_inheritedData
        && m_svgData == other.m_svgData;
}

bool ComputedStyle::inheritedEqual(const ComputedStyle& other) const
{
    return m_inheritedFlags == other.m_inheritedFlags
        && m_inheritedData == other.m_inheritedData
        && (m_svgData.ptr() == other.m_svgData.ptr() || m_svgData->inheritedEqual(other.m_svgData))
        && m_inheritedRareData == other.m_inheritedRareData;
}

bool ComputedStyle::nonInheritedEqual(const ComputedStyle& other) const
{
    return m_nonInheritedFlags == other.m_nonInheritedFlags
        && m_nonInheritedData == other.m_nonInheritedData
        && (m_svgData.ptr() == other.m_svgData.ptr() || m_svgData->nonInheritedEqual(other.m_svgData));
}

bool ComputedStyle::fastPathInheritedEqual(const ComputedStyle& other) const
{
    if (m_inheritedFlags.visibility != other.m_inheritedFlags.visibility)
        return false;
    if (m_inheritedFlags.hasExplicitlySetColor != other.m_inheritedFlags.hasExplicitlySetColor)
        return false;
    if (m_inheritedData.ptr() == other.m_inheritedData.ptr())
        return true;
    return m_inheritedData->fastPathInheritedEqual(*other.m_inheritedData);
}

bool ComputedStyle::nonFastPathInheritedEqual(const ComputedStyle& other) const
{
    auto withoutFastPathFlags = [](auto flags) {
        flags.visibility = { };
        flags.hasExplicitlySetColor = { };
        return flags;
    };
    if (withoutFastPathFlags(m_inheritedFlags) != withoutFastPathFlags(other.m_inheritedFlags))
        return false;
    if (m_inheritedData.ptr() != other.m_inheritedData.ptr() && !m_inheritedData->nonFastPathInheritedEqual(*other.m_inheritedData))
        return false;
    if (m_inheritedRareData != other.m_inheritedRareData)
        return false;
    if (m_svgData.ptr() != other.m_svgData.ptr() && !m_svgData->inheritedEqual(other.m_svgData))
        return false;
    return true;
}

bool ComputedStyle::descendantAffectingNonInheritedPropertiesEqual(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->miscData.ptr() == other.m_nonInheritedData->miscData.ptr())
        return true;

    if (m_nonInheritedData->miscData->alignItems != other.m_nonInheritedData->miscData->alignItems)
        return false;

    if (m_nonInheritedData->miscData->justifyItems != other.m_nonInheritedData->miscData->justifyItems)
        return false;

    if (m_nonInheritedData->miscData->usedAppearance != other.m_nonInheritedData->miscData->usedAppearance)
        return false;

    return true;
}

bool ComputedStyle::borderAndBackgroundEqual(const ComputedStyle& other) const
{
    return border() == other.border()
        && backgroundLayers() == other.backgroundLayers()
        && backgroundColor() == other.backgroundColor();
}

float ComputedStyle::computedLineHeight() const
{
    return evaluate<float>(lineHeight(), LineHeightEvaluationContext { computedFontSize(), metricsOfPrimaryFont().lineSpacing() }, usedZoomForLength());
}

bool ComputedStyle::scrollSnapDataEquivalent(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollMargin == other.m_nonInheritedData->rareData->scrollMargin
        && m_nonInheritedData->rareData->scrollSnapAlign == other.m_nonInheritedData->rareData->scrollSnapAlign
        && m_nonInheritedData->rareData->scrollSnapStop == other.m_nonInheritedData->rareData->scrollSnapStop;
}



// MARK: - Specific style change queries

bool ComputedStyle::scrollAnchoringSuppressionStyleDidChange(const ComputedStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there are any style changes that should result in an scroll anchoring suppression
    if (!other)
        return false;

    if (m_nonInheritedData->boxData.ptr() != other->m_nonInheritedData->boxData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& boxData = m_nonInheritedData->boxData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherBoxData = other->m_nonInheritedData->boxData.get();
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

    if (m_nonInheritedData->surroundData.ptr() != other->m_nonInheritedData->surroundData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& surroundData = m_nonInheritedData->surroundData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherSurroundData = other->m_nonInheritedData->surroundData.get();
        if (surroundData.margin != otherSurroundData.margin)
            return true;

        if (surroundData.padding != otherSurroundData.padding)
            return true;

        if (position() != PositionType::Static) {
            if (surroundData.inset != otherSurroundData.inset)
                return true;
        }
    }

    if (m_nonInheritedData->miscData.ptr() != other->m_nonInheritedData->miscData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& miscData = m_nonInheritedData->miscData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherMiscData = other->m_nonInheritedData->miscData.get();
        if (miscData.transform != otherMiscData.transform)
            return true;
    }

    // The spec doesn't list `translate`, `rotate`, `scale` but test them here.
    // https://github.com/w3c/csswg-drafts/issues/13489
    if (m_nonInheritedData->rareData.ptr() != other->m_nonInheritedData->rareData.ptr()) {
        SUPPRESS_UNCOUNTED_LOCAL auto& rareData = m_nonInheritedData->rareData.get();
        SUPPRESS_UNCOUNTED_LOCAL auto& otherRareData = other->m_nonInheritedData->rareData.get();
        if (rareData.translate != otherRareData.translate
            || rareData.rotate != otherRareData.rotate
            || rareData.scale != otherRareData.scale) {
            return true;
        }
    }
    return false;
}

bool ComputedStyle::outOfFlowPositionStyleDidChange(const ComputedStyle* other) const
{
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    // Determine if there is a style change that causes an element to become or stop
    // being absolutely or fixed positioned
    return other && hasOutOfFlowPosition() != other->hasOutOfFlowPosition();
}

// MARK: - Used Values

const WTF::String& ComputedStyle::hyphenString() const
{
    ASSERT(hyphens() != Hyphens::None);

    return WTF::switchOn(hyphenateCharacter(),
        [&](const CSS::Keyword::Auto&) -> const WTF::String& {
            // FIXME: This should depend on locale.
            static MainThreadNeverDestroyed<const WTF::String> hyphenMinusString(span(hyphenMinus));
            static MainThreadNeverDestroyed<const WTF::String> hyphenString(span(hyphen));

            return protect(fontCascade().primaryFont())->glyphForCharacter(hyphen) ? hyphenString : hyphenMinusString;
        },
        [](const String& string) -> const WTF::String& {
            return string.value;
        }
    );
}

float ComputedStyle::usedStrokeWidth(const IntSize& viewportSize) const
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

WebCore::Color ComputedStyle::usedStrokeColor() const
{
    return hasExplicitlySetStrokeColor() ? visitedDependentStrokeColor() : visitedDependentTextStrokeColor();
}

WebCore::Color ComputedStyle::usedStrokeColorApplyingColorFilter() const
{
    return hasExplicitlySetStrokeColor() ? visitedDependentStrokeColorApplyingColorFilter() : visitedDependentTextStrokeColorApplyingColorFilter();
}

Style::Contain ComputedStyle::usedContain() const
{
    auto result = contain();

    if (containerType().hasSize())
        result.add({ Style::ContainValue::Style, Style::ContainValue::Size });
    else if (containerType().hasInlineSize())
        result.add({ Style::ContainValue::Style, Style::ContainValue::InlineSize });

    return result;
}

UsedClear ComputedStyle::usedClear(const RenderElement& renderer)
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

UsedFloat ComputedStyle::usedFloat(const RenderElement& renderer)
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

UserSelect ComputedStyle::usedUserSelect() const
{
    if (effectiveInert())
        return UserSelect::None;

    auto value = userSelect();
    if (userModify() != UserModify::ReadOnly && userDrag() != UserDrag::Element)
        return value == UserSelect::None ? UserSelect::Text : value;

    return value;
}

WebCore::Color ComputedStyle::usedScrollbarThumbColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> WebCore::Color {
            return { };
        },
        [&](const auto& parts) -> WebCore::Color {
            Style::ColorResolver colorResolver { *this };
            if (!appleColorFilter().isNone())
                return colorResolver.colorResolvingCurrentColorApplyingColorFilter(parts.thumb);
            return colorResolver.colorResolvingCurrentColor(parts.thumb);
        }
    );
}

WebCore::Color ComputedStyle::usedScrollbarTrackColor() const
{
    return WTF::switchOn(scrollbarColor(),
        [&](const CSS::Keyword::Auto&) -> WebCore::Color {
            return { };
        },
        [&](const auto& parts) -> WebCore::Color {
            Style::ColorResolver colorResolver { *this };
            if (!appleColorFilter().isNone())
                return colorResolver.colorResolvingCurrentColorApplyingColorFilter(parts.track);
            return colorResolver.colorResolvingCurrentColor(parts.track);
        }
    );
}

WebCore::Color ComputedStyle::usedAccentColor(OptionSet<StyleColorOptions> styleColorOptions) const
{
    return WTF::switchOn(accentColor(),
        [](const CSS::Keyword::Auto&) -> WebCore::Color {
            return { };
        },
        [&](const Color& color) -> WebCore::Color {
            ColorResolver colorResolver { *this };

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

Style::LineWidth ComputedStyle::usedColumnRuleWidth() const
{
    if (!isVisibleBorderStyle(columnRuleStyle()))
        return 0_css_px;
    return columnRuleWidth();
}

Style::Length<> ComputedStyle::usedOutlineOffset() const
{
    auto& outline = this->outline();
    if (outline.outlineOffset.isInternalInset())
        return Style::Length<> { -Style::evaluate<float>(usedOutlineWidth(), Style::ZoomNeeded { }) };
    return *outline.outlineOffset.tryLength();
}

Style::LineWidth ComputedStyle::usedOutlineWidth() const
{
    auto& outline = this->outline();
    if (static_cast<OutlineStyle>(outline.outlineStyle) == OutlineStyle::None)
        return 0_css_px;
    if (static_cast<OutlineStyle>(outline.outlineStyle) == OutlineStyle::Auto)
        return Style::LineWidth { RenderTheme::singleton().platformFocusRingWidth() * usedZoom() };
    return outline.outlineWidth;
}

float ComputedStyle::usedOutlineSize() const
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

LayoutBoxExtent ComputedStyle::imageOutsets(const Style::BorderImage& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(usedBorderTopWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(usedBorderRightWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(usedBorderBottomWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(usedBorderLeftWidth(), Style::ZoomNeeded { })),
    };
}

LayoutBoxExtent ComputedStyle::imageOutsets(const Style::MaskBorder& image) const
{
    return {
        computeOutset(image.outset().values.top(), Style::evaluate<LayoutUnit>(usedBorderTopWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.right(), Style::evaluate<LayoutUnit>(usedBorderRightWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.bottom(), Style::evaluate<LayoutUnit>(usedBorderBottomWidth(), Style::ZoomNeeded { })),
        computeOutset(image.outset().values.left(), Style::evaluate<LayoutUnit>(usedBorderLeftWidth(), Style::ZoomNeeded { })),
    };
}

LayoutBoxExtent ComputedStyle::borderImageOutsets() const
{
    return imageOutsets(borderImage());
}

LayoutBoxExtent ComputedStyle::maskBorderOutsets() const
{
    return imageOutsets(maskBorder());
}

// MARK: - Logical

const BorderValue& ComputedStyle::borderBefore(const WritingMode writingMode) const
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

const BorderValue& ComputedStyle::borderAfter(const WritingMode writingMode) const
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

const BorderValue& ComputedStyle::borderStart(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderLeft() : borderRight();
    return writingMode.isInlineTopToBottom() ? borderTop() : borderBottom();
}

const BorderValue& ComputedStyle::borderEnd(const WritingMode writingMode) const
{
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderRight() : borderLeft();
    return writingMode.isInlineTopToBottom() ? borderBottom() : borderTop();
}

TextAlign textAlign(const ComputedStyle& style)
{
    return style.textAlign();
}

FontWeight fontWeight(const ComputedStyle& style)
{
    return style.fontWeight();
}

FontStyle fontStyle(const ComputedStyle& style)
{
    return style.fontStyle();
}

TextDecorationLine textDecorationLineInEffect(const ComputedStyle& style)
{
    return style.textDecorationLineInEffect();
}

const FontCascade& fontCascade(const ComputedStyle& style)
{
    return style.fontCascade();
}

SpeakAs speakAs(const ComputedStyle& style)
{
    return style.speakAs();
}

const VerticalAlign& verticalAlign(const ComputedStyle& style)
{
    return style.verticalAlign();
}

const TextShadows& textShadow(const ComputedStyle& style)
{
    return style.textShadow();
}

bool effectiveInert(const ComputedStyle& style)
{
    return style.effectiveInert();
}

} // namespace Style
} // namespace WebCore
