/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 - 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include <wtf/Forward.h>

namespace WebCore {

class CSSProperty;
class CSSParserTokenRange;
class CSSValue;
struct CSSParserContext;
struct CSSCustomPropertySyntax;
struct ComputedStyleDependencies;

enum CSSPropertyID : uint16_t;
enum CSSValueID : uint16_t;
enum class CSSWideKeyword : uint8_t;
enum class IsImportant : bool;
enum class StyleRuleType : uint8_t;

namespace CSS {
struct PropertyParserState;
}

namespace Style {
class BuilderState;
class CustomProperty;
}

class CSSPropertyParser {
public:
    // Parses any CSS property or descriptor. If successful, the result will be appended to the `result` Vector and the function will return true, otherwise, the function will return false and the `result` Vector will be unmodified.
    static bool parseValue(CSSPropertyID, IsImportant, CSSParserTokenRange, const CSSParserContext&, Vector<CSSProperty, 256>& result, StyleRuleType);

    // Parses a longhand style property.
    static RefPtr<CSSValue> parseStylePropertyLonghand(CSSPropertyID, const String&, const CSSParserContext&);
    static RefPtr<CSSValue> parseStylePropertyLonghand(CSSPropertyID, CSSParserTokenRange, const CSSParserContext&);

    // Parses a @counter-style descriptor.
    static RefPtr<CSSValue> parseCounterStyleDescriptor(CSSPropertyID, const String&, const CSSParserContext&);

    static RefPtr<const Style::CustomProperty> parseTypedCustomPropertyInitialValue(const AtomString&, const CSSCustomPropertySyntax&, CSSParserTokenRange, Style::BuilderState&, const CSSParserContext&);
    static std::optional<Variant<Ref<const Style::CustomProperty>, CSSWideKeyword>> parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax&, CSSParserTokenRange, Style::BuilderState&, const CSSParserContext&);

    static ComputedStyleDependencies collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax&, CSSParserTokenRange, const CSSParserContext&);
    static bool isValidCustomPropertyValueForSyntax(const CSSCustomPropertySyntax&, CSSParserTokenRange, const CSSParserContext&);

    static std::optional<CSSWideKeyword> parseCSSWideKeyword(CSSParserTokenRange);
};

// MARK: - CSSPropertyID parsing

CSSPropertyID cssPropertyID(StringView);

// MARK: - CSSValueID parsing

WEBCORE_EXPORT CSSValueID cssValueKeywordID(StringView);

// MARK: - Custom property name validation

bool isCustomPropertyName(StringView);

} // namespace WebCore
