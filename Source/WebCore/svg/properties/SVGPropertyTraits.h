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

#include <WebCore/CSSPropertyParserConsumer+Color.h>
#include <WebCore/Color.h>
#include <WebCore/CommonAtomStrings.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/FloatRect.h>
#include <WebCore/QualifiedName.h>
#include <WebCore/SVGParserUtilities.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

class SVGElement;

namespace Style {
struct Color;
}

template<typename PropertyType>
struct SVGPropertyTraits { };

template<>
struct SVGPropertyTraits<bool> {
    static bool initialValue() { return false; }
    static bool fromString(SVGElement&, StringImpl* string) { return equal(string, "true"_s); }
    static std::optional<bool> parse(const QualifiedName&, StringImpl*) { ASSERT_NOT_REACHED(); return initialValue(); }
    static RefPtr<StringImpl> toString(bool type) { return type ? trueAtom() : falseAtom(); }
};

template<>
struct SVGPropertyTraits<Color> {
    static Color initialValue() { return Color(); }
    static Color fromString(SVGElement&, StringImpl*);
    static RefPtr<StringImpl> toString(const Color&);
};

template<>
struct SVGPropertyTraits<unsigned> {
    static unsigned initialValue() { return 0; }
    static std::optional<unsigned> parse(const QualifiedName&, StringImpl*) { ASSERT_NOT_REACHED(); return initialValue(); }
    static RefPtr<StringImpl> toString(unsigned type) { return makeString(type); }
};

template<>
struct SVGPropertyTraits<int> {
    static int initialValue() { return 0; }
    static int fromString(SVGElement&, StringImpl*);
    static std::optional<int> parse(const QualifiedName&, StringImpl*) { ASSERT_NOT_REACHED(); return initialValue(); }
    static RefPtr<StringImpl> toString(int type) { return makeString(type); }
};

template<>
struct SVGPropertyTraits<std::pair<int, int>> {
    static std::pair<int, int> initialValue() { return { }; }
    static std::pair<int, int> fromString(SVGElement&, StringImpl* string)
    {
        return parse(string).value_or(initialValue());
    }
    static std::optional<std::pair<int, int>> parse(StringImpl* string)
    {
        auto result = parseNumberOptionalNumber(string);
        if (!result)
            return std::nullopt;
        return std::make_pair(static_cast<int>(std::round(result->first)), static_cast<int>(std::round(result->second)));
    }
    static RefPtr<StringImpl> toString(std::pair<int, int>) { ASSERT_NOT_REACHED(); return emptyString(); }
};

template<>
struct SVGPropertyTraits<float> {
    static float initialValue() { return 0; }
    static float fromString(SVGElement&, StringImpl* string)
    {
        return parseNumber(string).value_or(0);
    }
    static std::optional<float> parse(StringImpl* string)
    {
        return parseNumber(string);
    }
    static std::optional<float> parse(const QualifiedName&, StringImpl* string)
    {
        return parse(string);
    }
    static RefPtr<StringImpl> toString(float type) { return makeString(type); }
};

template<>
struct SVGPropertyTraits<std::pair<float, float>> {
    static std::pair<float, float> initialValue() { return { }; }
    static std::pair<float, float> fromString(SVGElement&, StringImpl* string)
    {
        return parse(string).value_or(initialValue());
    }
    static std::optional<std::pair<float, float>> parse(StringImpl* string)
    {
        return parseNumberOptionalNumber(string);
    }
    static RefPtr<StringImpl> toString(std::pair<float, float>) { ASSERT_NOT_REACHED(); return emptyString(); }
};

template<>
struct SVGPropertyTraits<FloatPoint> {
    static FloatPoint initialValue() { return FloatPoint(); }
    static FloatPoint fromString(SVGElement&, StringImpl* string)
    {
        return valueOrDefault(parsePoint(string));
    }
    static std::optional<FloatPoint> parse(const QualifiedName&, StringImpl* string)
    {
        return parsePoint(string);
    }
    static RefPtr<StringImpl> toString(const FloatPoint& type)
    {
        return makeString(type.x(), ' ', type.y());
    }
};

template<>
struct SVGPropertyTraits<FloatRect> {
    static FloatRect initialValue() { return FloatRect(); }
    static FloatRect fromString(SVGElement&, StringImpl* string)
    {
        return valueOrDefault(parseRect(string));
    }
    static std::optional<FloatRect> parse(const QualifiedName&, StringImpl* string)
    {
        return parseRect(string);
    }
    static RefPtr<StringImpl> toString(const FloatRect& type)
    {
        return makeString(type.x(), ' ', type.y(), ' ', type.width(), ' ', type.height());
    }
};

template<>
struct SVGPropertyTraits<RefPtr<StringImpl>> {
    static RefPtr<StringImpl> initialValue() { return nullptr; }
    static RefPtr<StringImpl> fromString(SVGElement&, StringImpl* string) { return string; }
    static std::optional<RefPtr<StringImpl>> parse(const QualifiedName&, StringImpl* string) { return string; }
    static RefPtr<StringImpl> toString(StringImpl* string) { return string; }
};

} // namespace WebCore
