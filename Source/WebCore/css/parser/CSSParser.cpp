/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "config.h"
#include "CSSParser.h"

#include "CSSKeyframeRule.h"
#include "CSSParserFastPaths.h"
#include "CSSParserImpl.h"
#include "CSSParserTokenRange.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSSelectorParser.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "Element.h"
#include "ImmutableStyleProperties.h"
#include "MutableStyleProperties.h"
#include "Page.h"
#include "RenderTheme.h"
#include "Settings.h"
#include "StyleColor.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSParser::CSSParser(const CSSParserContext& context)
    : m_context(context)
{
}

CSSParser::~CSSParser() = default;

void CSSParser::parseSheet(StyleSheetContents& sheet, const String& string)
{
    return CSSParserImpl::parseStyleSheet(string, m_context, sheet);
}

void CSSParser::parseSheetForInspector(const CSSParserContext& context, StyleSheetContents* sheet, const String& string, CSSParserObserver& observer)
{
    return CSSParserImpl::parseStyleSheetForInspector(string, context, sheet, observer);
}

RefPtr<StyleRuleBase> CSSParser::parseRule(const CSSParserContext& context, StyleSheetContents* sheet, const String& string, CSSParserEnum::IsNestedContext isNestedContext)
{
    return CSSParserImpl::parseRule(string, context, sheet, CSSParserImpl::AllowImportRules, isNestedContext);
}

RefPtr<StyleRuleKeyframe> CSSParser::parseKeyframeRule(const String& string)
{
    RefPtr<StyleRuleBase> keyframe = CSSParserImpl::parseRule(string, m_context, nullptr, CSSParserImpl::KeyframeRules);
    return downcast<StyleRuleKeyframe>(keyframe.get());
}

bool CSSParser::parseSupportsCondition(const String& condition)
{
    CSSParserImpl parser(m_context, condition);
    if (!parser.tokenizer())
        return false;
    return CSSSupportsParser::supportsCondition(parser.tokenizer()->tokenRange(), parser, CSSSupportsParser::ForWindowCSS, CSSParserEnum::IsNestedContext::No) == CSSSupportsParser::Supported;
}

static Color color(const RefPtr<CSSValue>& value)
{
    if (!value || !value->isColor())
        return { };
    return value->color();
}

Color CSSParser::parseColor(const String& string, const CSSParserContext& context)
{
    bool strict = !isQuirksModeBehavior(context.mode);
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, strict))
        return *color;
    return color(parseSingleValue(CSSPropertyColor, string, context));
}

Color CSSParser::parseColorWithoutContext(const String& string, bool strict)
{
    if (auto color = CSSParserFastPaths::parseSimpleColor(string, strict))
        return *color;
    // FIXME: Unclear why we want to ignore the boolean argument "strict" and always pass strictCSSParserContext here.
    return color(parseSingleValue(CSSPropertyColor, string, strictCSSParserContext()));
}

Color CSSParser::parseSystemColor(StringView string)
{
    auto keyword = cssValueKeywordID(string);
    if (!StyleColor::isSystemColorKeyword(keyword))
        return { };
    return RenderTheme::singleton().systemColor(keyword, { });
}

std::optional<SRGBA<uint8_t>> CSSParser::parseNamedColor(StringView string)
{
    return CSSParserFastPaths::parseNamedColor(string);
}

std::optional<SRGBA<uint8_t>> CSSParser::parseHexColor(StringView string)
{
    return CSSParserFastPaths::parseHexColor(string);
}

RefPtr<CSSValue> CSSParser::parseSingleValue(CSSPropertyID propertyID, const String& string, const CSSParserContext& context)
{
    if (string.isEmpty())
        return nullptr;
    if (auto value = CSSParserFastPaths::maybeParseValue(propertyID, string, context))
        return value;
    CSSTokenizer tokenizer(string);
    return CSSPropertyParser::parseSingleValue(propertyID, tokenizer.tokenRange(), context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    if (auto value = CSSParserFastPaths::maybeParseValue(propertyID, string, context))
        return declaration.addParsedProperty(CSSProperty(propertyID, WTFMove(value), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
    CSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important);
}

CSSParser::ParseResult CSSParser::parseCustomPropertyValue(MutableStyleProperties& declaration, const AtomString& propertyName, const String& string, bool important, const CSSParserContext& context)
{
    return CSSParserImpl::parseCustomPropertyValue(&declaration, propertyName, string, important, context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important)
{
    return CSSParserImpl::parseValue(&declaration, propertyID, string, important, m_context);
}

std::optional<CSSSelectorList> CSSParser::parseSelector(const String& string, StyleSheetContents* styleSheet, CSSParserEnum::IsNestedContext isNestedContext)
{
    return parseCSSSelector(CSSTokenizer(string).tokenRange(), m_context, styleSheet, isNestedContext);
}

Ref<ImmutableStyleProperties> CSSParser::parseInlineStyleDeclaration(const String& string, const Element* element)
{
    return CSSParserImpl::parseInlineStyleDeclaration(string, element);
}

bool CSSParser::parseDeclaration(MutableStyleProperties& declaration, const String& string)
{
    return CSSParserImpl::parseDeclarationList(&declaration, string, m_context);
}

void CSSParser::parseDeclarationForInspector(const CSSParserContext& context, const String& string, CSSParserObserver& observer)
{
    CSSParserImpl::parseDeclarationListForInspector(string, context, observer);
}

Vector<double> CSSParser::parseKeyframeKeyList(const String& selector)
{
    return CSSParserImpl::parseKeyframeKeyList(selector);
}

}
