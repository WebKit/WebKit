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

#include "CSSCustomPropertySyntax.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSProperty.h"
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
    static bool parseValue(CSSPropertyID, IsImportant, const CSSParserTokenRange&, const CSSParserContext&, Vector<CSSProperty, 256>&, StyleRuleType);

    // Parses a non-shorthand CSS property
    static RefPtr<CSSValue> parseSingleValue(CSSPropertyID, const CSSParserTokenRange&, const CSSParserContext&);

    static RefPtr<CSSCustomPropertyValue> parseTypedCustomPropertyInitialValue(const AtomString&, const CSSCustomPropertySyntax&, CSSParserTokenRange, Style::BuilderState&, const CSSParserContext&);
    static RefPtr<CSSCustomPropertyValue> parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax&, const CSSParserTokenRange&, Style::BuilderState&, const CSSParserContext&);
    static ComputedStyleDependencies collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax&, const CSSParserTokenRange&, const CSSParserContext&);
    static bool isValidCustomPropertyValueForSyntax(const CSSCustomPropertySyntax&, CSSParserTokenRange, const CSSParserContext&);

    static RefPtr<CSSValue> parseCounterStyleDescriptor(CSSPropertyID, CSSParserTokenRange&, const CSSParserContext&);

private:
    CSSPropertyParser(const CSSParserTokenRange&, const CSSParserContext&, Vector<CSSProperty, 256>*, bool consumeWhitespace = true);

    // FIXME: Rename once the CSSParserValue-based parseValue is removed
    bool parseValueStart(CSSPropertyID, IsImportant);
    bool consumeCSSWideKeyword(CSSPropertyID, IsImportant);
    RefPtr<CSSValue> parseSingleValue(CSSPropertyID, CSSPropertyID = CSSPropertyInvalid);
    
    std::pair<RefPtr<CSSValue>, CSSCustomPropertySyntax::Type> consumeCustomPropertyValueWithSyntax(const CSSCustomPropertySyntax&);
    RefPtr<CSSCustomPropertyValue> parseTypedCustomPropertyValue(const AtomString& name, const CSSCustomPropertySyntax&, Style::BuilderState&);
    ComputedStyleDependencies collectParsedCustomPropertyValueDependencies(const CSSCustomPropertySyntax&);

    bool inQuirksMode() const { return m_context.mode == HTMLQuirksMode; }

    // @font-face descriptors.
    bool parseFontFaceDescriptor(CSSPropertyID);
    bool parseFontFaceDescriptorShorthand(CSSPropertyID);

    // @font-palette-values descriptors.
    bool parseFontPaletteValuesDescriptor(CSSPropertyID);

    // @counter-style descriptors.
    bool parseCounterStyleDescriptor(CSSPropertyID);
    
    // @keyframe descriptors.
    bool parseKeyframeDescriptor(CSSPropertyID, IsImportant);

    // @page descriptors.
    bool parsePageDescriptor(CSSPropertyID, IsImportant);

    // @property descriptors.
    bool parsePropertyDescriptor(CSSPropertyID);

    // @view-transition descriptors.
    bool parseViewTransitionDescriptor(CSSPropertyID);

    // @position-try descriptors.
    bool parsePositionTryDescriptor(CSSPropertyID, IsImportant);

    void addProperty(CSSPropertyID longhand, CSSPropertyID shorthand, RefPtr<CSSValue>&&, IsImportant, IsImplicit = IsImplicit::No);
    void addExpandedProperty(CSSPropertyID shorthand, RefPtr<CSSValue>&&, IsImportant, IsImplicit = IsImplicit::No);

    // Shorthand Parsing.

    bool parseShorthand(CSSPropertyID, IsImportant);
    bool consumeShorthandGreedily(const StylePropertyShorthand&, IsImportant);
    bool consume2ValueShorthand(const StylePropertyShorthand&, IsImportant);
    bool consume4ValueShorthand(const StylePropertyShorthand&, IsImportant);

    bool consumeBorderShorthand(CSSPropertyID widthProperty, CSSPropertyID styleProperty, CSSPropertyID colorProperty, IsImportant);

    // Legacy parsing allows <string>s for animation-name
    bool consumeAnimationShorthand(const StylePropertyShorthand&, IsImportant);
    bool consumeBackgroundShorthand(const StylePropertyShorthand&, IsImportant);
    bool consumeOverflowShorthand(IsImportant);

    bool consumeColumns(IsImportant);

    bool consumeGridItemPositionShorthand(CSSPropertyID, IsImportant);
    bool consumeGridTemplateRowsAndAreasAndColumns(CSSPropertyID, IsImportant);
    bool consumeGridTemplateShorthand(CSSPropertyID, IsImportant);
    bool consumeGridShorthand(IsImportant);
    bool consumeGridAreaShorthand(IsImportant);

    bool consumeAlignShorthand(const StylePropertyShorthand&, IsImportant);

    bool consumeBlockStepShorthand(IsImportant);

    bool consumeFont(IsImportant);
    bool consumeTextDecorationSkip(IsImportant);
    bool consumeFontVariantShorthand(IsImportant);
    bool consumeFontSynthesis(IsImportant);

    bool consumeBorderSpacing(IsImportant);

    // CSS3 Parsing Routines (for properties specific to CSS3)
    bool consumeBorderImage(CSSPropertyID, IsImportant);
    bool consumeBorderRadius(CSSPropertyID, IsImportant);

    bool consumeFlex(IsImportant);

    bool consumeLegacyBreakProperty(CSSPropertyID, IsImportant);
    bool consumeLegacyTextOrientation(IsImportant);

    bool consumeTransformOrigin(IsImportant);
    bool consumePerspectiveOrigin(IsImportant);
    bool consumePrefixedPerspective(IsImportant);
    bool consumeOffset(IsImportant);
    bool consumeListStyleShorthand(IsImportant);

    bool consumeOverscrollBehaviorShorthand(IsImportant);

    bool consumeContainerShorthand(IsImportant);
    bool consumeContainIntrinsicSizeShorthand(IsImportant);

    bool consumeAnimationRangeShorthand(IsImportant);
    bool consumeScrollTimelineShorthand(IsImportant);
    bool consumeViewTimelineShorthand(IsImportant);

    bool consumeLineClampShorthand(IsImportant);

    bool consumeTextBoxShorthand(IsImportant);

    bool consumeTextWrapShorthand(IsImportant);
    bool consumeWhiteSpaceShorthand(IsImportant);

private:
    // Inputs:
    CSSParserTokenRange m_range;
    const CSSParserContext& m_context;

    // Outputs:
    Vector<CSSProperty, 256>* m_parsedProperties;
};

CSSPropertyID cssPropertyID(StringView);
WEBCORE_EXPORT CSSValueID cssValueKeywordID(StringView);
bool isCustomPropertyName(StringView);

bool isInitialValueForLonghand(CSSPropertyID, const CSSValue&);
ASCIILiteral initialValueTextForLonghand(CSSPropertyID);
CSSValueID initialValueIDForLonghand(CSSPropertyID); // Returns CSSPropertyInvalid if not a keyword.

} // namespace WebCore
