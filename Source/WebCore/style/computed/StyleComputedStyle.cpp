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

} // namespace Style
} // namespace WebCore
