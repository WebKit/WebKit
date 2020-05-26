/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2010, 2015 Apple Inc. All rights reserved.
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

#include "CSSParserContext.h"
#include "CSSRegisteredCustomProperty.h"
#include "CSSValue.h"
#include "WritingMode.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSParserObserver;
class CSSSelectorList;
class CSSValuePool;
class Color;
class Element;
class ImmutableStyleProperties;
class MutableStyleProperties;
class SimpleColor;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleSheetContents;
class RenderStyle;

namespace Style {
class BuilderState;
}

class CSSParser {
public:
    enum class ParseResult {
        Changed,
        Unchanged,
        Error
    };

    WEBCORE_EXPORT explicit CSSParser(const CSSParserContext&);
    WEBCORE_EXPORT ~CSSParser();

    enum class RuleParsing { Normal, Deferred };
    void parseSheet(StyleSheetContents*, const String&, RuleParsing = RuleParsing::Normal);
    
    static RefPtr<StyleRuleBase> parseRule(const CSSParserContext&, StyleSheetContents*, const String&);
    
    RefPtr<StyleRuleKeyframe> parseKeyframeRule(const String&);
    static Vector<double> parseKeyframeKeyList(const String&);
    
    bool parseSupportsCondition(const String&);

    static void parseSheetForInspector(const CSSParserContext&, StyleSheetContents*, const String&, CSSParserObserver&);
    static void parseDeclarationForInspector(const CSSParserContext&, const String&, CSSParserObserver&);

    static ParseResult parseValue(MutableStyleProperties&, CSSPropertyID, const String&, bool important, const CSSParserContext&);
    static ParseResult parseCustomPropertyValue(MutableStyleProperties&, const AtomString& propertyName, const String&, bool important, const CSSParserContext&);
    
    static RefPtr<CSSValue> parseFontFaceDescriptor(CSSPropertyID, const String&, const CSSParserContext&);

    static RefPtr<CSSValue> parseSingleValue(CSSPropertyID, const String&, const CSSParserContext& = strictCSSParserContext());

    WEBCORE_EXPORT bool parseDeclaration(MutableStyleProperties&, const String&);
    static Ref<ImmutableStyleProperties> parseInlineStyleDeclaration(const String&, const Element*);

    void parseSelector(const String&, CSSSelectorList&);

    RefPtr<CSSValue> parseValueWithVariableReferences(CSSPropertyID, const CSSValue&, Style::BuilderState&);

    WEBCORE_EXPORT static Color parseColor(const String&, bool strict = false);
    static Color parseColorWorkerSafe(StringView);
    static Color parseSystemColor(StringView);
    static Optional<SimpleColor> parseNamedColor(StringView);
    static Optional<SimpleColor> parseHexColor(StringView);

private:
    ParseResult parseValue(MutableStyleProperties&, CSSPropertyID, const String&, bool important);

    CSSParserContext m_context;
};

} // namespace WebCore
