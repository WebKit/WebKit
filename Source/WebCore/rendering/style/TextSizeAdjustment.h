/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(TEXT_AUTOSIZING)

#include <wtf/OptionSet.h>

namespace WebCore {

class RenderStyle;

enum TextSizeAdjustmentType { AutoTextSizeAdjustment = -1, NoTextSizeAdjustment = -2 };

class TextSizeAdjustment {
public:
    constexpr TextSizeAdjustment() : m_value(AutoTextSizeAdjustment) { }
    constexpr TextSizeAdjustment(float value) : m_value(value) { }

    constexpr float percentage() const { return m_value; }
    constexpr float multiplier() const { return m_value / 100; }

    constexpr bool isAuto() const { return m_value == AutoTextSizeAdjustment; }
    constexpr bool isNone() const { return m_value == NoTextSizeAdjustment; }
    constexpr bool isPercentage() const { return m_value >= 0; }

    constexpr bool operator==(const TextSizeAdjustment& anAdjustment) const { return m_value == anAdjustment.m_value; }

private:
    float m_value;
};

class AutosizeStatus {
public:
    enum class Fields : uint8_t {
        AvoidSubtree = 1 << 0,
        FixedHeight = 1 << 1,
        FixedWidth = 1 << 2,
        Floating = 1 << 3,
        OverflowXHidden = 1 << 4,
        // Adding new values requires giving RenderStyle::InheritedFlags::autosizeStatus additional bits.
    };

    constexpr AutosizeStatus(OptionSet<Fields>);
    constexpr OptionSet<Fields> fields() const { return m_fields; }

    constexpr bool contains(Fields) const;

    constexpr bool operator==(const AutosizeStatus& other) const { return fields() == other.fields(); }

    static float idempotentTextSize(float specifiedSize, float pageScale);
    static AutosizeStatus computeStatus(const RenderStyle&);
    static void updateStatus(RenderStyle&);

    static bool probablyContainsASmallFixedNumberOfLines(const RenderStyle&);

private:
    OptionSet<Fields> m_fields;
};

constexpr AutosizeStatus::AutosizeStatus(OptionSet<Fields> fields)
    : m_fields(fields)
{
}

constexpr bool AutosizeStatus::contains(Fields fields) const
{
    return m_fields.contains(fields);
}

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
