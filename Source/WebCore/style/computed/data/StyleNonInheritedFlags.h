/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

enum class Clear : uint8_t;
enum class Float : uint8_t;
enum class Overflow : uint8_t;
enum class PositionType : uint8_t;
enum class PseudoElementType : uint8_t;
enum class UnicodeBidi : uint8_t;

namespace Style {

enum class DisplayType : uint8_t;

constexpr auto PseudoElementTypeBits = 5;
constexpr auto PublicPseudoIDBits = 19;
constexpr auto TextDecorationLineBits = 5;

struct NonInheritedFlags {
    bool operator==(const NonInheritedFlags&) const = default;

    inline void copyNonInheritedFrom(const NonInheritedFlags&);

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const NonInheritedFlags&) const;
#endif

    PREFERRED_TYPE(Style::DisplayType) unsigned display : 5;
    PREFERRED_TYPE(Style::DisplayType) unsigned originalDisplay : 5;
    PREFERRED_TYPE(Overflow) unsigned overflowX : 3;
    PREFERRED_TYPE(Overflow) unsigned overflowY : 3;
    PREFERRED_TYPE(Clear) unsigned clear : 3;
    PREFERRED_TYPE(PositionType) unsigned position : 3;
    PREFERRED_TYPE(UnicodeBidi) unsigned unicodeBidi : 3;
    PREFERRED_TYPE(Float) unsigned floating : 3;

    PREFERRED_TYPE(bool) unsigned usesViewportUnits : 1;
    PREFERRED_TYPE(bool) unsigned usesContainerUnits : 1;
    PREFERRED_TYPE(bool) unsigned useTreeCountingFunctions : 1;
    PREFERRED_TYPE(bool) unsigned hasExplicitlyInheritedProperties : 1; // Explicitly inherits a non-inherited property.
    PREFERRED_TYPE(bool) unsigned disallowsFastPathInheritance : 1;

    // Non-property related state bits.
    PREFERRED_TYPE(bool) unsigned emptyState : 1;
    PREFERRED_TYPE(bool) unsigned firstChildState : 1;
    PREFERRED_TYPE(bool) unsigned lastChildState : 1;
    PREFERRED_TYPE(bool) unsigned isLink : 1;
    PREFERRED_TYPE(PseudoElementType) unsigned pseudoElementType : PseudoElementTypeBits;
    unsigned pseudoBits : PublicPseudoIDBits;
    unsigned textDecorationLine : TextDecorationLineBits; // Text decorations defined *only* by this element. PREFERRED_TYPE elided to avoid header inclusion.

    // If you add more style bits here, you will also need to update NonInheritedFlags::copyNonInheritedFrom().
};

inline void NonInheritedFlags::copyNonInheritedFrom(const NonInheritedFlags& other)
{
    // Only some flags are copied because NonInheritedFlags contains things that are not actually style data.
    display = other.display;
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
