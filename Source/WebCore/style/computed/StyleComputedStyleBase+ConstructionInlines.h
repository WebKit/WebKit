/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include <WebCore/StyleComputedStyleBase.h>
#include <WebCore/StyleComputedStyle+InitialInlines.h>

namespace WebCore {
namespace Style {

inline ComputedStyleBase::ComputedStyleBase(ComputedStyleBase&&) = default;
inline ComputedStyleBase& ComputedStyleBase::operator=(ComputedStyleBase&&) = default;

inline ComputedStyleBase::ComputedStyleBase(CreateDefaultStyleTag)
    : m_nonInheritedData(NonInheritedData::create())
    , m_inheritedRareData(InheritedRareData::create())
    , m_inheritedData(InheritedData::create())
    , m_svgData(SVGData::create())
{
    m_inheritedFlags.writingMode = WritingMode(ComputedStyle::initialWritingMode(), ComputedStyle::initialDirection(), ComputedStyle::initialTextOrientation()).toData();
    m_inheritedFlags.emptyCells = static_cast<unsigned>(ComputedStyle::initialEmptyCells());
    m_inheritedFlags.captionSide = static_cast<unsigned>(ComputedStyle::initialCaptionSide());
    m_inheritedFlags.listStylePosition = static_cast<unsigned>(ComputedStyle::initialListStylePosition());
    m_inheritedFlags.visibility = static_cast<unsigned>(ComputedStyle::initialVisibility());
    m_inheritedFlags.textAlign = static_cast<unsigned>(ComputedStyle::initialTextAlign());
    m_inheritedFlags.textTransform = ComputedStyle::initialTextTransform().toRaw();
    m_inheritedFlags.textDecorationLineInEffect = ComputedStyle::initialTextDecorationLine().toRaw();
    m_inheritedFlags.cursorType = static_cast<unsigned>(ComputedStyle::initialCursor().predefined);
#if ENABLE(CURSOR_VISIBILITY)
    m_inheritedFlags.cursorVisibility = static_cast<unsigned>(ComputedStyle::initialCursorVisibility());
#endif
    m_inheritedFlags.whiteSpaceCollapse = static_cast<unsigned>(ComputedStyle::initialWhiteSpaceCollapse());
    m_inheritedFlags.textWrapMode = static_cast<unsigned>(ComputedStyle::initialTextWrapMode());
    m_inheritedFlags.textWrapStyle = static_cast<unsigned>(ComputedStyle::initialTextWrapStyle());
    m_inheritedFlags.borderCollapse = static_cast<unsigned>(ComputedStyle::initialBorderCollapse());
    m_inheritedFlags.rtlOrdering = static_cast<unsigned>(ComputedStyle::initialRTLOrdering());
    m_inheritedFlags.boxDirection = static_cast<unsigned>(ComputedStyle::initialBoxDirection());
    m_inheritedFlags.printColorAdjust = static_cast<unsigned>(ComputedStyle::initialPrintColorAdjust());
    m_inheritedFlags.pointerEvents = static_cast<unsigned>(ComputedStyle::initialPointerEvents());
    m_inheritedFlags.insideLink = static_cast<unsigned>(InsideLink::NotInside);
    m_inheritedFlags.isZoomed = 0;
#if ENABLE(TEXT_AUTOSIZING)
    m_inheritedFlags.autosizeStatus = 0;
#endif

    m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(ComputedStyle::initialDisplay());
    m_nonInheritedFlags.originalDisplay = static_cast<unsigned>(ComputedStyle::initialDisplay());
    m_nonInheritedFlags.overflowX = static_cast<unsigned>(ComputedStyle::initialOverflowX());
    m_nonInheritedFlags.overflowY = static_cast<unsigned>(ComputedStyle::initialOverflowY());
    m_nonInheritedFlags.clear = static_cast<unsigned>(ComputedStyle::initialClear());
    m_nonInheritedFlags.position = static_cast<unsigned>(ComputedStyle::initialPosition());
    m_nonInheritedFlags.unicodeBidi = static_cast<unsigned>(ComputedStyle::initialUnicodeBidi());
    m_nonInheritedFlags.floating = static_cast<unsigned>(ComputedStyle::initialFloating());
    m_nonInheritedFlags.textDecorationLine = ComputedStyle::initialTextDecorationLine().toRaw();
    m_nonInheritedFlags.usesViewportUnits = false;
    m_nonInheritedFlags.usesContainerUnits = false;
    m_nonInheritedFlags.useTreeCountingFunctions = false;
    m_nonInheritedFlags.hasExplicitlyInheritedProperties = false;
    m_nonInheritedFlags.disallowsFastPathInheritance = false;
    m_nonInheritedFlags.emptyState = false;
    m_nonInheritedFlags.firstChildState = false;
    m_nonInheritedFlags.lastChildState = false;
    m_nonInheritedFlags.isLink = false;
    m_nonInheritedFlags.pseudoElementType = 0;
    m_nonInheritedFlags.pseudoBits = 0;

    static_assert((sizeof(InheritedFlags) <= 8), "InheritedFlags does not grow");
    static_assert((sizeof(NonInheritedFlags) <= 12), "NonInheritedFlags does not grow");
}

inline ComputedStyleBase::ComputedStyleBase(const ComputedStyleBase& other, CloneTag)
    : m_nonInheritedFlags(other.m_nonInheritedFlags)
    , m_inheritedFlags(other.m_inheritedFlags)
    , m_nonInheritedData(other.m_nonInheritedData)
    , m_inheritedRareData(other.m_inheritedRareData)
    , m_inheritedData(other.m_inheritedData)
    , m_svgData(other.m_svgData)
{
}

inline ComputedStyleBase::ComputedStyleBase(ComputedStyleBase& a, ComputedStyleBase&& b)
    : m_nonInheritedFlags(std::exchange(a.m_nonInheritedFlags, b.m_nonInheritedFlags))
    , m_inheritedFlags(std::exchange(a.m_inheritedFlags, b.m_inheritedFlags))
    , m_nonInheritedData(a.m_nonInheritedData.replace(WTF::move(b.m_nonInheritedData)))
    , m_inheritedRareData(a.m_inheritedRareData.replace(WTF::move(b.m_inheritedRareData)))
    , m_inheritedData(a.m_inheritedData.replace(WTF::move(b.m_inheritedData)))
    , m_svgData(a.m_svgData.replace(WTF::move(b.m_svgData)))
    , m_cachedPseudoStyles(std::exchange(a.m_cachedPseudoStyles, WTF::move(b.m_cachedPseudoStyles)))
{
}

inline void ComputedStyleBase::NonInheritedFlags::copyNonInheritedFrom(const NonInheritedFlags& other)
{
    // Only some flags are copied because NonInheritedFlags contains things that are not actually style data.
    effectiveDisplay = other.effectiveDisplay;
    originalDisplay = other.originalDisplay;
    overflowX = other.overflowX;
    overflowY = other.overflowY;
    clear = other.clear;
    position = other.position;
    unicodeBidi = other.unicodeBidi;
    floating = other.floating;
    textDecorationLine = other.textDecorationLine;
    usesViewportUnits = other.usesViewportUnits;
    usesContainerUnits = other.usesContainerUnits;
    useTreeCountingFunctions = other.useTreeCountingFunctions;
    hasExplicitlyInheritedProperties = other.hasExplicitlyInheritedProperties;
    disallowsFastPathInheritance = other.disallowsFastPathInheritance;
}

} // namespace Style
} // namespace WebCore
