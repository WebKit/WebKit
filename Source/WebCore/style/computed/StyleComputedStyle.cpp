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
#include "StyleComputedStyle+SettersInlines.h"

#include "FontCascade.h"
#include "Pagination.h"
#include "StyleComputedStyleBase+ConstructionInlines.h"
#include "StyleCustomPropertyRegistry.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleScaleTransformFunction.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(TEXT_AUTOSIZING)
#include <wtf/text/StringHash.h>
#endif

namespace WebCore {
namespace Style {

struct SameSizeAsBorderValue {
    Color m_color;
    float m_width;
    int m_restBits;
};

static_assert(sizeof(BorderValue) == sizeof(SameSizeAsBorderValue), "BorderValue should not grow");

struct SameSizeAsComputedStyle : CanMakeCheckedPtr<SameSizeAsComputedStyle> {
    struct NonInheritedFlags {
        unsigned m_bitfields[3];
    } m_nonInheritedFlags;
    struct InheritedFlags {
        unsigned m_bitfields[2];
    } m_inheritedFlags;
    void* nonInheritedDataRefs[1];
    void* inheritedDataRefs[2];
    HashMap<PseudoElementIdentifier, std::unique_ptr<RenderStyle>> pseudos;
    void* dataRefSvgStyle;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionCheck;
#endif
};

static_assert(sizeof(ComputedStyle) == sizeof(SameSizeAsComputedStyle), "ComputedStyle should stay small");

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

#if ENABLE(TEXT_AUTOSIZING)

static inline unsigned computeFontHash(const FontCascade& font)
{
    // FIXME: Would be better to hash the family name rather than hashing a hash of the family name. Also, should this use FontCascadeDescription::familyNameHash?
    return computeHash(ASCIICaseInsensitiveHash::hash(font.fontDescription().firstFamily()), font.fontDescription().specifiedSize());
}

unsigned ComputedStyle::hashForTextAutosizing() const
{
    // FIXME: Not a very smart hash. Could be improved upon. See <https://bugs.webkit.org/show_bug.cgi?id=121131>.
    unsigned hash = m_nonInheritedData->miscData->usedAppearance;
    hash ^= m_nonInheritedData->rareData->lineClamp.valueForHash();
    hash ^= m_inheritedRareData->overflowWrap;
    hash ^= m_inheritedRareData->nbspMode;
    hash ^= m_inheritedRareData->lineBreak;
    hash ^= m_inheritedData->specifiedLineHeight.valueForHash();
    hash ^= computeFontHash(m_inheritedData->fontData->fontCascade);
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->borderHorizontalSpacing.unresolvedValue());
    hash ^= WTF::FloatHash<float>::hash(m_inheritedData->borderVerticalSpacing.unresolvedValue());
    hash ^= m_inheritedFlags.boxDirection;
    hash ^= m_inheritedFlags.rtlOrdering;
    hash ^= m_nonInheritedFlags.position;
    hash ^= m_nonInheritedFlags.floating;
    hash ^= m_nonInheritedData->miscData->textOverflow;
    hash ^= m_inheritedRareData->textSecurity;
    return hash;
}

bool ComputedStyle::equalForTextAutosizing(const ComputedStyle& other) const
{
    return m_nonInheritedData->miscData->usedAppearance == other.m_nonInheritedData->miscData->usedAppearance
        && m_nonInheritedData->rareData->lineClamp == other.m_nonInheritedData->rareData->lineClamp
        && m_inheritedRareData->textSizeAdjust == other.m_inheritedRareData->textSizeAdjust
        && m_inheritedRareData->overflowWrap == other.m_inheritedRareData->overflowWrap
        && m_inheritedRareData->nbspMode == other.m_inheritedRareData->nbspMode
        && m_inheritedRareData->lineBreak == other.m_inheritedRareData->lineBreak
        && m_inheritedRareData->textSecurity == other.m_inheritedRareData->textSecurity
        && m_inheritedData->specifiedLineHeight == other.m_inheritedData->specifiedLineHeight
        && m_inheritedData->fontData->fontCascade.equalForTextAutoSizing(other.m_inheritedData->fontData->fontCascade)
        && m_inheritedData->borderHorizontalSpacing == other.m_inheritedData->borderHorizontalSpacing
        && m_inheritedData->borderVerticalSpacing == other.m_inheritedData->borderVerticalSpacing
        && m_inheritedFlags.boxDirection == other.m_inheritedFlags.boxDirection
        && m_inheritedFlags.rtlOrdering == other.m_inheritedFlags.rtlOrdering
        && m_nonInheritedFlags.position == other.m_nonInheritedFlags.position
        && m_nonInheritedFlags.floating == other.m_nonInheritedFlags.floating
        && m_nonInheritedData->miscData->textOverflow == other.m_nonInheritedData->miscData->textOverflow;
}

#endif

float ComputedStyle::computedLineHeight() const
{
    return computeLineHeight(lineHeight());
}

float ComputedStyle::computeLineHeight(const LineHeight& lineHeight) const
{
    return WTF::switchOn(lineHeight,
        [&](const CSS::Keyword::Normal&) -> float {
            return metricsOfPrimaryFont().lineSpacing();
        },
        [&](const LineHeight::Fixed& fixed) -> float {
            return evaluate<LayoutUnit>(fixed, usedZoomForLength()).toFloat();
        },
        [&](const LineHeight::Percentage& percentage) -> float {
            return evaluate<LayoutUnit>(percentage, LayoutUnit { computedFontSize() }).toFloat();
        },
        [&](const LineHeight::Calc& calc) -> float {
            return evaluate<LayoutUnit>(calc, LayoutUnit { computedFontSize() }, usedZoomForLength()).toFloat();
        }
    );
}

void ComputedStyle::setPageScaleTransform(float scale)
{
    if (scale == 1)
        return;

    setTransform(Style::Transform { Style::TransformFunction { Style::ScaleTransformFunction::create(scale, scale, Style::TransformFunctionType::Scale) } });
    setTransformOriginX(0_css_px);
    setTransformOriginY(0_css_px);
}

void ComputedStyle::setColumnStylesFromPaginationMode(PaginationMode paginationMode)
{
    if (paginationMode == Pagination::Mode::Unpaginated)
        return;
    
    setColumnFill(ColumnFill::Auto);
    
    switch (paginationMode) {
    case Pagination::Mode::LeftToRightPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::Mode::RightToLeftPaginated:
        setColumnAxis(ColumnAxis::Horizontal);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::Mode::TopToBottomPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        else
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        break;
    case Pagination::Mode::BottomToTopPaginated:
        setColumnAxis(ColumnAxis::Vertical);
        if (writingMode().isHorizontal())
            setColumnProgression(writingMode().isBlockFlipped() ? ColumnProgression::Normal : ColumnProgression::Reverse);
        else
            setColumnProgression(writingMode().isBidiLTR() ? ColumnProgression::Reverse : ColumnProgression::Normal);
        break;
    case Pagination::Mode::Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool ComputedStyle::scrollSnapDataEquivalent(const ComputedStyle& other) const
{
    if (m_nonInheritedData.ptr() == other.m_nonInheritedData.ptr()
        || m_nonInheritedData->rareData.ptr() == other.m_nonInheritedData->rareData.ptr())
        return true;

    return m_nonInheritedData->rareData->scrollMargin == other.m_nonInheritedData->rareData->scrollMargin
        && m_nonInheritedData->rareData->scrollSnapAlign == other.m_nonInheritedData->rareData->scrollSnapAlign
        && m_nonInheritedData->rareData->scrollSnapStop == other.m_nonInheritedData->rareData->scrollSnapStop
        && m_nonInheritedData->rareData->scrollSnapAlign == other.m_nonInheritedData->rareData->scrollSnapAlign;
}

// MARK: - Style adjustment utilities

void ComputedStyle::adjustAnimations()
{
    if (animations().isInitial())
        return;

    ensureAnimations().prepareForUse();
}

void ComputedStyle::adjustTransitions()
{
    if (transitions().isInitial())
        return;

    ensureTransitions().prepareForUse();
}

void ComputedStyle::adjustBackgroundLayers()
{
    if (backgroundLayers().isInitial())
        return;

    ensureBackgroundLayers().prepareForUse();
}

void ComputedStyle::adjustMaskLayers()
{
    if (maskLayers().isInitial())
        return;

    ensureMaskLayers().prepareForUse();
}

void ComputedStyle::adjustScrollTimelines()
{
    auto& names = scrollTimelineNames();
    if (names.isNone() && scrollTimelines().isEmpty())
        return;

    auto& axes = scrollTimelineAxes();
    auto numberOfAxes = axes.size();
    ASSERT(numberOfAxes > 0);

    m_nonInheritedData.access().rareData.access().scrollTimelines = { FixedVector<Ref<ScrollTimeline>>::createWithSizeFromGenerator(names.size(), [&](auto i) {
        return ScrollTimeline::create(names[i].value.value, axes[i % numberOfAxes]);
    }) };
}

void ComputedStyle::adjustViewTimelines()
{
    auto& names = viewTimelineNames();
    if (names.isNone() && viewTimelines().isEmpty())
        return;

    auto& axes = viewTimelineAxes();
    auto numberOfAxes = axes.size();
    ASSERT(numberOfAxes > 0);

    auto& insets = viewTimelineInsets();
    auto numberOfInsets = insets.size();
    ASSERT(numberOfInsets > 0);

    m_nonInheritedData.access().rareData.access().viewTimelines = { FixedVector<Ref<ViewTimeline>>::createWithSizeFromGenerator(names.size(), [&](auto i) {
        return ViewTimeline::create(names[i].value.value, axes[i % numberOfAxes], insets[i % numberOfInsets]);
    }) };
}

} // namespace Style
} // namespace WebCore
