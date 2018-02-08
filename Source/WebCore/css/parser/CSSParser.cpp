/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSVariableData.h"
#include "CSSVariableReferenceValue.h"
#include "Document.h"
#include "Element.h"
#include "RenderTheme.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleColor.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>


namespace WebCore {
using namespace WTF;

const CSSParserContext& strictCSSParserContext()
{
    static NeverDestroyed<CSSParserContext> strictContext(HTMLStandardMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
    , cssGridLayoutEnabled(RuntimeEnabledFeatures::sharedFeatures().isCSSGridLayoutEnabled())
{
#if PLATFORM(IOS)
    // FIXME: Force the site specific quirk below to work on iOS. Investigating other site specific quirks
    // to see if we can enable the preference all together is to be handled by:
    // <rdar://problem/8493309> Investigate Enabling Site Specific Quirks in MobileSafari and UIWebView
    needsSiteSpecificQuirks = true;
#endif
}

CSSParserContext::CSSParserContext(Document& document, const URL& sheetBaseURL, const String& charset)
    : baseURL(sheetBaseURL.isNull() ? document.baseURL() : sheetBaseURL)
    , charset(charset)
    , mode(document.inQuirksMode() ? HTMLQuirksMode : HTMLStandardMode)
    , isHTMLDocument(document.isHTMLDocument())
    , cssGridLayoutEnabled(document.isCSSGridLayoutEnabled())
    , hasDocumentSecurityOrigin(sheetBaseURL.isNull() || document.securityOrigin().canRequest(baseURL))
{
    
    needsSiteSpecificQuirks = document.settings().needsSiteSpecificQuirks();
    enforcesCSSMIMETypeInNoQuirksMode = document.settings().enforceCSSMIMETypeInNoQuirksMode();
    useLegacyBackgroundSizeShorthandBehavior = document.settings().useLegacyBackgroundSizeShorthandBehavior();
#if ENABLE(TEXT_AUTOSIZING)
    textAutosizingEnabled = document.settings().textAutosizingEnabled();
#endif
    springTimingFunctionEnabled = document.settings().springTimingFunctionEnabled();
    constantPropertiesEnabled = document.settings().constantPropertiesEnabled();
    conicGradientsEnabled = document.settings().conicGradientsEnabled();
    deferredCSSParserEnabled = document.settings().deferredCSSParserEnabled();
    allowNewLinesClamp = document.settings().appleMailLinesClampEnabled();
    
#if PLATFORM(IOS)
    // FIXME: Force the site specific quirk below to work on iOS. Investigating other site specific quirks
    // to see if we can enable the preference all together is to be handled by:
    // <rdar://problem/8493309> Investigate Enabling Site Specific Quirks in MobileSafari and UIWebView
    needsSiteSpecificQuirks = true;
#endif
}

bool operator==(const CSSParserContext& a, const CSSParserContext& b)
{
    return a.baseURL == b.baseURL
        && a.charset == b.charset
        && a.mode == b.mode
        && a.isHTMLDocument == b.isHTMLDocument
        && a.cssGridLayoutEnabled == b.cssGridLayoutEnabled
        && a.needsSiteSpecificQuirks == b.needsSiteSpecificQuirks
        && a.enforcesCSSMIMETypeInNoQuirksMode == b.enforcesCSSMIMETypeInNoQuirksMode
        && a.useLegacyBackgroundSizeShorthandBehavior == b.useLegacyBackgroundSizeShorthandBehavior
        && a.springTimingFunctionEnabled == b.springTimingFunctionEnabled
        && a.constantPropertiesEnabled == b.constantPropertiesEnabled
        && a.conicGradientsEnabled == b.conicGradientsEnabled
        && a.deferredCSSParserEnabled == b.deferredCSSParserEnabled
        && a.hasDocumentSecurityOrigin == b.hasDocumentSecurityOrigin;
}

CSSParser::CSSParser(const CSSParserContext& context)
    : m_context(context)
{
}

CSSParser::~CSSParser() = default;

void CSSParser::parseSheet(StyleSheetContents* sheet, const String& string, RuleParsing ruleParsing)
{
    return CSSParserImpl::parseStyleSheet(string, m_context, sheet, ruleParsing);
}

void CSSParser::parseSheetForInspector(const CSSParserContext& context, StyleSheetContents* sheet, const String& string, CSSParserObserver& observer)
{
    return CSSParserImpl::parseStyleSheetForInspector(string, context, sheet, observer);
}

RefPtr<StyleRuleBase> CSSParser::parseRule(const CSSParserContext& context, StyleSheetContents* sheet, const String& string)
{
    return CSSParserImpl::parseRule(string, context, sheet, CSSParserImpl::AllowImportRules);
}

RefPtr<StyleRuleKeyframe> CSSParser::parseKeyframeRule(const String& string)
{
    RefPtr<StyleRuleBase> keyframe = CSSParserImpl::parseRule(string, m_context, nullptr, CSSParserImpl::KeyframeRules);
    return downcast<StyleRuleKeyframe>(keyframe.get());
}

bool CSSParser::parseSupportsCondition(const String& condition)
{
    CSSParserImpl parser(m_context, condition);
    return CSSSupportsParser::supportsCondition(parser.tokenizer()->tokenRange(), parser, CSSSupportsParser::ForWindowCSS) == CSSSupportsParser::Supported;
}

Color CSSParser::parseColor(const String& string, bool strict)
{
    if (string.isEmpty())
        return Color();
    
    // Try named colors first.
    Color namedColor { string };
    if (namedColor.isValid())
        return namedColor;
    
    // Try the fast path to parse hex and rgb.
    RefPtr<CSSValue> value = CSSParserFastPaths::parseColor(string, strict ? HTMLStandardMode : HTMLQuirksMode);
    
    // If that fails, try the full parser.
    if (!value)
        value = parseSingleValue(CSSPropertyColor, string, strictCSSParserContext());
    if (!value || !value->isPrimitiveValue())
        return Color();
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    if (!primitiveValue.isRGBColor())
        return Color();
    return primitiveValue.color();
}

Color CSSParser::parseSystemColor(const String& string)
{
    CSSValueID id = cssValueKeywordID(string);
    if (!StyleColor::isSystemColor(id))
        return Color();
    
    return RenderTheme::singleton().systemColor(id);
}

RefPtr<CSSValue> CSSParser::parseSingleValue(CSSPropertyID propertyID, const String& string, const CSSParserContext& context)
{
    if (string.isEmpty())
        return nullptr;
    if (RefPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(propertyID, string, context.mode))
        return value;
    CSSTokenizer tokenizer(string);
    return CSSPropertyParser::parseSingleValue(propertyID, tokenizer.tokenRange(), context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    RefPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(propertyID, string, context.mode);
    if (value)
        return declaration.addParsedProperty(CSSProperty(propertyID, WTFMove(value), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;

    CSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important);
}

CSSParser::ParseResult CSSParser::parseCustomPropertyValue(MutableStyleProperties& declaration, const AtomicString& propertyName, const String& string, bool important, const CSSParserContext& context)
{
    return CSSParserImpl::parseCustomPropertyValue(&declaration, propertyName, string, important, context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important)
{
    return CSSParserImpl::parseValue(&declaration, propertyID, string, important, m_context);
}

void CSSParser::parseSelector(const String& string, CSSSelectorList& selectorList)
{
    CSSTokenizer tokenizer(string);
    selectorList = CSSSelectorParser::parseSelector(tokenizer.tokenRange(), m_context, nullptr);
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

RefPtr<CSSValue> CSSParser::parseValueWithVariableReferences(CSSPropertyID propID, const CSSValue& value, const CustomPropertyValueMap& customProperties, TextDirection direction, WritingMode writingMode)
{
    if (value.isPendingSubstitutionValue()) {
        // FIXME: Should have a resolvedShorthands cache to stop this from being done
        // over and over for each longhand value.
        const CSSPendingSubstitutionValue& pendingSubstitution = downcast<CSSPendingSubstitutionValue>(value);
        CSSPropertyID shorthandID = pendingSubstitution.shorthandPropertyId();
        if (CSSProperty::isDirectionAwareProperty(shorthandID))
            shorthandID = CSSProperty::resolveDirectionAwareProperty(shorthandID, direction, writingMode);
        CSSVariableReferenceValue* shorthandValue = pendingSubstitution.shorthandValue();
        const CSSVariableData* variableData = shorthandValue->variableDataValue();
        ASSERT(variableData);
        
        Vector<CSSParserToken> resolvedTokens;
        if (!variableData->resolveTokenRange(customProperties, variableData->tokens(), resolvedTokens))
            return nullptr;
        
        ParsedPropertyVector parsedProperties;
        if (!CSSPropertyParser::parseValue(shorthandID, false, resolvedTokens, m_context, parsedProperties, StyleRule::Style))
            return nullptr;
        
        for (auto& property : parsedProperties) {
            if (property.id() == propID)
                return property.value();
        }
        
        return nullptr;
    }

    if (value.isVariableReferenceValue()) {
        const CSSVariableReferenceValue& valueWithReferences = downcast<CSSVariableReferenceValue>(value);
        const CSSVariableData* variableData = valueWithReferences.variableDataValue();
        ASSERT(variableData);
        
        Vector<CSSParserToken> resolvedTokens;
        if (!variableData->resolveTokenRange(customProperties, variableData->tokens(), resolvedTokens))
            return nullptr;
        
        return CSSPropertyParser::parseSingleValue(propID, resolvedTokens, m_context);
    }
    
    return nullptr;
}

std::unique_ptr<Vector<double>> CSSParser::parseKeyframeKeyList(const String& selector)
{
    return CSSParserImpl::parseKeyframeKeyList(selector);
}

RefPtr<CSSValue> CSSParser::parseFontFaceDescriptor(CSSPropertyID propertyID, const String& propertyValue, const CSSParserContext& context)
{
    StringBuilder builder;
    builder.appendLiteral("@font-face { ");
    builder.append(getPropertyNameString(propertyID));
    builder.appendLiteral(" : ");
    builder.append(propertyValue);
    builder.appendLiteral("; }");
    RefPtr<StyleRuleBase> rule = parseRule(context, nullptr, builder.toString());
    if (!rule || !rule->isFontFaceRule())
        return nullptr;
    return downcast<StyleRuleFontFace>(*rule.get()).properties().getPropertyCSSValue(propertyID);
}

}
