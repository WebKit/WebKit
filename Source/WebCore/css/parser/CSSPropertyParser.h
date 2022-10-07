/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 - 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "CSSParserTokenRange.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSPropertyParserWorkerSafe.h"
#include "StyleRuleType.h"
#include <wtf/text/StringView.h>

namespace WebCore {

class CSSCustomPropertyValue;
class CSSProperty;
class StylePropertyShorthand;

namespace Style {
class BuilderState;
}
    
// Inputs: PropertyID, isImportant bool, CSSParserTokenRange.
// Outputs: Vector of CSSProperties

class CSSPropertyParser {
    WTF_MAKE_NONCOPYABLE(CSSPropertyParser);
public:
    static bool parseValue(CSSPropertyID, bool important,
        const CSSParserTokenRange&, const CSSParserContext&,
        Vector<CSSProperty, 256>&, StyleRuleType);

    // Parses a non-shorthand CSS property
    static RefPtr<CSSValue> parseSingleValue(CSSPropertyID, const CSSParserTokenRange&, const CSSParserContext&);
    static bool canParseTypedCustomPropertyValue(const String& syntax, const CSSParserTokenRange&, const CSSParserContext&);
    static RefPtr<CSSCustomPropertyValue> parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const CSSParserTokenRange&, const Style::BuilderState&, const CSSParserContext&);
    static void collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies, const CSSParserTokenRange&, const CSSParserContext&);

    static RefPtr<CSSValue> parseCounterStyleDescriptor(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);

private:
    CSSPropertyParser(const CSSParserTokenRange&, const CSSParserContext&, Vector<CSSProperty, 256>*, bool consumeWhitespace = true);

    // FIXME: Rename once the CSSParserValue-based parseValue is removed
    bool parseValueStart(CSSPropertyID, bool important);
    bool consumeCSSWideKeyword(CSSPropertyID, bool important);
    RefPtr<CSSValue> parseSingleValue(CSSPropertyID, CSSPropertyID = CSSPropertyInvalid);
    bool canParseTypedCustomPropertyValue(const String& syntax);
    RefPtr<CSSCustomPropertyValue> parseTypedCustomPropertyValue(const AtomString& name, const String& syntax, const Style::BuilderState&);
    void collectParsedCustomPropertyValueDependencies(const String& syntax, bool isRoot, HashSet<CSSPropertyID>& dependencies);

    bool inQuirksMode() const { return m_context.mode == HTMLQuirksMode; }

    bool parseViewportDescriptor(CSSPropertyID propId, bool important);
    bool parseFontFaceDescriptor(CSSPropertyID);
    bool parseFontPaletteValuesDescriptor(CSSPropertyID);
    bool parseCounterStyleDescriptor(CSSPropertyID, const CSSParserContext&);
    bool parseKeyframeDescriptor(CSSPropertyID, bool important);

    void addProperty(CSSPropertyID, CSSPropertyID, Ref<CSSValue>&&, bool important, bool implicit = false);
    void addPropertyWithImplicitDefault(CSSPropertyID, CSSPropertyID, RefPtr<CSSValue>&&, Ref<CSSValue>&& implicitDefault, bool important);
    void addExpandedPropertyForValue(CSSPropertyID propId, Ref<CSSValue>&&, bool);

    bool consumeBorder(RefPtr<CSSValue>& width, RefPtr<CSSValue>& style, RefPtr<CSSValue>& color);

    bool parseShorthand(CSSPropertyID, bool important);
    bool consumeShorthandGreedily(const StylePropertyShorthand&, bool important);
    bool consume2ValueShorthand(const StylePropertyShorthand&, bool important);
    bool consume4ValueShorthand(const StylePropertyShorthand&, bool important);

    // Legacy parsing allows <string>s for animation-name
    bool consumeAnimationShorthand(const StylePropertyShorthand&, bool important);
    bool consumeBackgroundShorthand(const StylePropertyShorthand&, bool important);
    bool consumeOverflowShorthand(bool important);

    bool consumeColumns(bool important);

    bool consumeGridItemPositionShorthand(CSSPropertyID, bool important);
    bool consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID, bool important);
    bool consumeGridTemplateShorthand(CSSPropertyID, bool important);
    bool consumeGridShorthand(bool important);
    bool consumeGridAreaShorthand(bool important);

    bool consumePlaceContentShorthand(bool important);
    bool consumePlaceItemsShorthand(bool important);
    bool consumePlaceSelfShorthand(bool important);

    bool consumeFont(bool important);
    bool consumeTextDecorationSkip(bool important);
    bool consumeFontVariantShorthand(bool important);
    bool consumeFontSynthesis(bool important);
    bool consumeSystemFont(bool important);

    bool consumeBorderSpacing(bool important);

    // CSS3 Parsing Routines (for properties specific to CSS3)
    bool consumeBorderImage(CSSPropertyID, bool important);

    bool consumeFlex(bool important);

    bool consumeLegacyBreakProperty(CSSPropertyID, bool important);
    bool consumeLegacyTextOrientation(bool important);

    bool consumeTransformOrigin(bool important);
    bool consumePerspectiveOrigin(bool important);
    bool consumePrefixedPerspective(bool important);
    bool consumeOffset(bool important);
    bool consumeListStyleShorthand(bool important);

    bool consumeOverscrollBehaviorShorthand(bool important);

    bool consumeContainerShorthand(bool important);
    bool consumeContainIntrinsicSizeShorthand(bool important);

private:
    // Inputs:
    CSSParserTokenRange m_range;
    const CSSParserContext& m_context;

    // Outputs:
    Vector<CSSProperty, 256>* m_parsedProperties;
};

CSSPropertyID cssPropertyID(StringView);
WEBCORE_EXPORT CSSValueID cssValueKeywordID(StringView);
bool isCustomPropertyName(const String&);

} // namespace WebCore
