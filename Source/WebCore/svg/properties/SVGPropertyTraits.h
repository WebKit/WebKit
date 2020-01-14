/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "CSSParser.h"
#include "Color.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "QualifiedName.h"
#include "SVGParserUtilities.h"

namespace WebCore {

template<typename PropertyType>
struct SVGPropertyTraits { };

template<>
struct SVGPropertyTraits<bool> {
    static bool initialValue() { return false; }
    static bool fromString(const String& string) { return string == "true"; }
    static Optional<bool> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(bool type) { return type ? "true" : "false"; }
};

template<>
struct SVGPropertyTraits<Color> {
    static Color initialValue() { return Color(); }
    static Color fromString(const String& string) { return CSSParser::parseColor(string.stripWhiteSpace()); }
    static Optional<Color> parse(const QualifiedName&, const String& string)
    {
        Color color = CSSParser::parseColor(string.stripWhiteSpace());
        if (!color.isValid())
            return WTF::nullopt;
        return color;
    }
    static String toString(const Color& type) { return type.serialized(); }
};

template<>
struct SVGPropertyTraits<unsigned> {
    static unsigned initialValue() { return 0; }
    static Optional<unsigned> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(unsigned type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<int> {
    static int initialValue() { return 0; }
    static int fromString(const String&string) { return string.toIntStrict(); }
    static Optional<int> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(int type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<std::pair<int, int>> {
    static std::pair<int, int> initialValue() { return { }; }
    static std::pair<int, int> fromString(const String& string)
    {
        float firstNumber = 0, secondNumber = 0;
        if (!parseNumberOptionalNumber(string, firstNumber, secondNumber))
            return { };
        return std::make_pair(static_cast<int>(roundf(firstNumber)), static_cast<int>(roundf(secondNumber)));
    }
    static Optional<std::pair<int, int>> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(std::pair<int, int>) { ASSERT_NOT_REACHED(); return emptyString(); }
};

template<>
struct SVGPropertyTraits<float> {
    static float initialValue() { return 0; }
    static float fromString(const String& string)
    {
        float number = 0;
        if (!parseNumberFromString(string, number))
            return 0;
        return number;
    }
    static Optional<float> parse(const QualifiedName&, const String& string)
    {
        float number;
        if (!parseNumberFromString(string, number))
            return WTF::nullopt;
        return number;
    }
    static String toString(float type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<std::pair<float, float>> {
    static std::pair<float, float> initialValue() { return { }; }
    static std::pair<float, float> fromString(const String& string)
    {
        float firstNumber = 0, secondNumber = 0;
        if (!parseNumberOptionalNumber(string, firstNumber, secondNumber))
            return { };
        return std::make_pair(firstNumber, secondNumber);
    }
    static Optional<std::pair<float, float>> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(std::pair<float, float>) { ASSERT_NOT_REACHED(); return emptyString(); }
};

template<>
struct SVGPropertyTraits<FloatPoint> {
    static FloatPoint initialValue() { return FloatPoint(); }
    static FloatPoint fromString(const String& string)
    {
        FloatPoint point;
        if (!parsePoint(string, point))
            return { };
        return point;
    }
    static Optional<FloatPoint> parse(const QualifiedName&, const String& string)
    {
        FloatPoint point;
        if (!parsePoint(string, point))
            return WTF::nullopt;
        return point;
    }
    static String toString(const FloatPoint& type)
    {
        return makeString(type.x(), ' ', type.y());
    }
};

template<>
struct SVGPropertyTraits<FloatRect> {
    static FloatRect initialValue() { return FloatRect(); }
    static FloatRect fromString(const String& string)
    {
        FloatRect rect;
        if (!parseRect(string, rect))
            return { };
        return rect;
    }
    static Optional<FloatRect> parse(const QualifiedName&, const String& string)
    {
        FloatRect rect;
        if (!parseRect(string, rect))
            return WTF::nullopt;
        return rect;
    }
    static String toString(const FloatRect& type)
    {
        return makeString(type.x(), ' ', type.y(), ' ', type.width(), ' ', type.height());
    }
};

template<>
struct SVGPropertyTraits<String> {
    static String initialValue() { return String(); }
    static String fromString(const String& string) { return string; }
    static Optional<String> parse(const QualifiedName&, const String& string) { return string; }
    static String toString(const String& string) { return string; }
};

template<typename EnumType>
struct SVGIDLEnumLimits {
    // Specialize this function for a particular enumeration to limit the values that are exposed through the DOM.
    static unsigned highestExposedEnumValue() { return SVGPropertyTraits<EnumType>::highestEnumValue(); }
};

} // namespace WebCore
