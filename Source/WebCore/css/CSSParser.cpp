/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2015 Apple Inc. All rights reserved.
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

#include "CSSAnimationTriggerScrollValue.h"
#include "CSSAspectRatioValue.h"
#include "CSSBasicShapes.h"
#include "CSSBorderImage.h"
#include "CSSCanvasValue.h"
#include "CSSContentDistributionValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSCursorImageValue.h"
#include "CSSCustomPropertyValue.h"
#include "CSSFilterImageValue.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSFontFeatureValue.h"
#include "CSSFontValue.h"
#include "CSSFunctionValue.h"
#include "CSSGradientValue.h"
#include "CSSImageValue.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSKeyframeRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLineBoxContainValue.h"
#include "CSSMediaRule.h"
#include "CSSNamedImageValue.h"
#include "CSSPageRule.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertySourceData.h"
#include "CSSReflectValue.h"
#include "CSSRevertValue.h"
#include "CSSSelector.h"
#include "CSSShadowValue.h"
#include "CSSStyleSheet.h"
#include "CSSTimingFunctionValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSUnsetValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CSSVariableDependentValue.h"
#include "Counter.h"
#include "Document.h"
#include "FloatConversion.h"
#include "GridCoordinate.h"
#include "HTMLParserIdioms.h"
#include "HashTools.h"
#include "MediaList.h"
#include "MediaQueryExp.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "Pair.h"
#include "Rect.h"
#include "RenderTheme.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGParserUtilities.h"
#include "SVGPathByteStream.h"
#include "SVGPathUtilities.h"
#include "SelectorChecker.h"
#include "SelectorCheckerTestFunctions.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "TextEncoding.h"
#include "WebKitCSSFilterValue.h"
#include "WebKitCSSRegionRule.h"
#include "WebKitCSSTransformValue.h"
#include <JavaScriptCore/Profile.h>
#include <bitset>
#include <limits.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringImpl.h>

#if ENABLE(CSS_GRID_LAYOUT)
#include "CSSGridLineNamesValue.h"
#include "CSSGridTemplateAreasValue.h"
#endif

#if ENABLE(CSS_IMAGE_SET)
#include "CSSImageSetValue.h"
#endif

#if ENABLE(CSS_SCROLL_SNAP)
#include "LengthRepeat.h"
#endif

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#define YYDEBUG 0

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse(WebCore::CSSParser*);

using namespace WTF;

namespace {

enum PropertyType {
    PropertyExplicit,
    PropertyImplicit
};

class ImplicitScope {
    WTF_MAKE_NONCOPYABLE(ImplicitScope);
public:
    ImplicitScope(WebCore::CSSParser& parser, PropertyType propertyType)
        : m_parser(parser)
    {
        m_parser.m_implicitShorthand = propertyType == PropertyImplicit;
    }

    ~ImplicitScope()
    {
        m_parser.m_implicitShorthand = false;
    }

private:
    WebCore::CSSParser& m_parser;
};

} // namespace

namespace WebCore {

static const unsigned INVALID_NUM_PARSED_PROPERTIES = UINT_MAX;
static const double MAX_SCALE = 1000000;

template<unsigned length> bool equalLettersIgnoringASCIICase(const CSSParserValue& value, const char (&lowercaseLetters)[length])
{
    ASSERT(value.unit == CSSPrimitiveValue::CSS_IDENT || value.unit == CSSPrimitiveValue::CSS_STRING);
    return equalLettersIgnoringASCIICase(value.string, lowercaseLetters);
}

static bool hasPrefix(const char* string, unsigned length, const char* prefix)
{
    for (unsigned i = 0; i < length; ++i) {
        if (!prefix[i])
            return true;
        if (string[i] != prefix[i])
            return false;
    }
    return false;
}

template<typename... Args>
static Ref<CSSPrimitiveValue> createPrimitiveValuePair(Args&&... args)
{
    return CSSValuePool::singleton().createValue(Pair::create(std::forward<Args>(args)...));
}

class AnimationParseContext {
public:
    AnimationParseContext()
        : m_animationPropertyKeywordAllowed(true)
        , m_firstAnimationCommitted(false)
        , m_hasSeenAnimationPropertyKeyword(false)
    {
    }

    void commitFirstAnimation()
    {
        m_firstAnimationCommitted = true;
    }

    bool hasCommittedFirstAnimation() const
    {
        return m_firstAnimationCommitted;
    }

    void commitAnimationPropertyKeyword()
    {
        m_animationPropertyKeywordAllowed = false;
    }

    bool animationPropertyKeywordAllowed() const
    {
        return m_animationPropertyKeywordAllowed;
    }

    bool hasSeenAnimationPropertyKeyword() const
    {
        return m_hasSeenAnimationPropertyKeyword;
    }

    void sawAnimationPropertyKeyword()
    {
        m_hasSeenAnimationPropertyKeyword = true;
    }

private:
    bool m_animationPropertyKeywordAllowed;
    bool m_firstAnimationCommitted;
    bool m_hasSeenAnimationPropertyKeyword;
};

const CSSParserContext& strictCSSParserContext()
{
    static NeverDestroyed<CSSParserContext> strictContext(CSSStrictMode);
    return strictContext;
}

CSSParserContext::CSSParserContext(CSSParserMode mode, const URL& baseURL)
    : baseURL(baseURL)
    , mode(mode)
    , isHTMLDocument(false)
    , isCSSRegionsEnabled(RuntimeEnabledFeatures::sharedFeatures().cssRegionsEnabled())
    , isCSSCompositingEnabled(RuntimeEnabledFeatures::sharedFeatures().cssCompositingEnabled())
    , needsSiteSpecificQuirks(false)
    , enforcesCSSMIMETypeInNoQuirksMode(true)
    , useLegacyBackgroundSizeShorthandBehavior(false)
{
#if PLATFORM(IOS)
    // FIXME: Force the site specific quirk below to work on iOS. Investigating other site specific quirks
    // to see if we can enable the preference all together is to be handled by:
    // <rdar://problem/8493309> Investigate Enabling Site Specific Quirks in MobileSafari and UIWebView
    needsSiteSpecificQuirks = true;
#endif
}

CSSParserContext::CSSParserContext(Document& document, const URL& baseURL, const String& charset)
    : baseURL(baseURL.isNull() ? document.baseURL() : baseURL)
    , charset(charset)
    , mode(document.inQuirksMode() ? CSSQuirksMode : CSSStrictMode)
    , isHTMLDocument(document.isHTMLDocument())
    , isCSSRegionsEnabled(document.cssRegionsEnabled())
    , isCSSCompositingEnabled(document.cssCompositingEnabled())
    , needsSiteSpecificQuirks(document.settings() ? document.settings()->needsSiteSpecificQuirks() : false)
    , enforcesCSSMIMETypeInNoQuirksMode(!document.settings() || document.settings()->enforceCSSMIMETypeInNoQuirksMode())
    , useLegacyBackgroundSizeShorthandBehavior(document.settings() ? document.settings()->useLegacyBackgroundSizeShorthandBehavior() : false)
{
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
        && a.isCSSRegionsEnabled == b.isCSSRegionsEnabled
        && a.isCSSCompositingEnabled == b.isCSSCompositingEnabled
        && a.needsSiteSpecificQuirks == b.needsSiteSpecificQuirks
        && a.enforcesCSSMIMETypeInNoQuirksMode == b.enforcesCSSMIMETypeInNoQuirksMode
        && a.useLegacyBackgroundSizeShorthandBehavior == b.useLegacyBackgroundSizeShorthandBehavior;
}

CSSParser::CSSParser(const CSSParserContext& context)
    : m_context(context)
    , m_important(false)
    , m_id(CSSPropertyInvalid)
    , m_styleSheet(nullptr)
    , m_supportsCondition(false)
    , m_selectorListForParseSelector(nullptr)
    , m_numParsedPropertiesBeforeMarginBox(INVALID_NUM_PARSED_PROPERTIES)
    , m_inParseShorthand(0)
    , m_currentShorthand(CSSPropertyInvalid)
    , m_implicitShorthand(false)
    , m_hadSyntacticallyValidCSSRule(false)
    , m_logErrors(false)
    , m_ignoreErrorsInDeclaration(false)
    , m_defaultNamespace(starAtom)
    , m_parsedTextPrefixLength(0)
    , m_nestedSelectorLevel(0)
    , m_propertyRange(UINT_MAX, UINT_MAX)
    , m_ruleSourceDataResult(nullptr)
    , m_parsingMode(NormalMode)
    , m_is8BitSource(false)
    , m_currentCharacter8(nullptr)
    , m_currentCharacter16(nullptr)
    , m_length(0)
    , m_token(0)
    , m_lineNumber(0)
    , m_tokenStartLineNumber(0)
    , m_tokenStartColumnNumber(0)
    , m_lastSelectorLineNumber(0)
    , m_columnOffsetForLine(0)
    , m_sheetStartLineNumber(0)
    , m_sheetStartColumnNumber(0)
    , m_allowImportRules(true)
    , m_allowNamespaceDeclarations(true)
#if ENABLE(CSS_DEVICE_ADAPTATION)
    , m_inViewport(false)
#endif
{
#if YYDEBUG > 0
    cssyydebug = 1;
#endif
    m_tokenStart.ptr8 = nullptr;
}

CSSParser::~CSSParser()
{
    clearProperties();
}

template<typename CharacterType> ALWAYS_INLINE static void convertToASCIILowercaseInPlace(CharacterType* characters, unsigned length)
{
    for (unsigned i = 0; i < length; ++i)
        characters[i] = toASCIILower(characters[i]);
}

void CSSParserString::convertToASCIILowercaseInPlace()
{
    if (is8Bit())
        WebCore::convertToASCIILowercaseInPlace(characters8(), length());
    else
        WebCore::convertToASCIILowercaseInPlace(characters16(), length());
}

void CSSParser::setupParser(const char* prefix, unsigned prefixLength, StringView string, const char* suffix, unsigned suffixLength)
{
    m_parsedTextPrefixLength = prefixLength;
    unsigned stringLength = string.length();
    unsigned length = stringLength + m_parsedTextPrefixLength + suffixLength + 1;
    m_length = length;

    if (!stringLength || string.is8Bit()) {
        m_dataStart8 = std::make_unique<LChar[]>(length);
        for (unsigned i = 0; i < m_parsedTextPrefixLength; ++i)
            m_dataStart8[i] = prefix[i];

        if (stringLength)
            memcpy(m_dataStart8.get() + m_parsedTextPrefixLength, string.characters8(), stringLength * sizeof(LChar));

        unsigned start = m_parsedTextPrefixLength + stringLength;
        unsigned end = start + suffixLength;
        for (unsigned i = start; i < end; i++)
            m_dataStart8[i] = suffix[i - start];

        m_dataStart8[length - 1] = '\0';

        m_is8BitSource = true;
        m_currentCharacter8 = m_dataStart8.get();
        m_currentCharacter16 = nullptr;
        setTokenStart<LChar>(m_currentCharacter8);
        m_lexFunc = &CSSParser::realLex<LChar>;
        return;
    }

    m_dataStart16 = std::make_unique<UChar[]>(length);
    for (unsigned i = 0; i < m_parsedTextPrefixLength; ++i)
        m_dataStart16[i] = prefix[i];

    ASSERT(stringLength);
    memcpy(m_dataStart16.get() + m_parsedTextPrefixLength, string.characters16(), stringLength * sizeof(UChar));

    unsigned start = m_parsedTextPrefixLength + stringLength;
    unsigned end = start + suffixLength;
    for (unsigned i = start; i < end; i++)
        m_dataStart16[i] = suffix[i - start];

    m_dataStart16[length - 1] = '\0';

    m_is8BitSource = false;
    m_currentCharacter8 = nullptr;
    m_currentCharacter16 = m_dataStart16.get();
    setTokenStart<UChar>(m_currentCharacter16);
    m_lexFunc = &CSSParser::realLex<UChar>;
}

void CSSParser::parseSheet(StyleSheetContents* sheet, const String& string, const TextPosition& textPosition, RuleSourceDataList* ruleSourceDataResult, bool logErrors)
{
    setStyleSheet(sheet);
    m_defaultNamespace = starAtom; // Reset the default namespace.
    if (ruleSourceDataResult)
        m_currentRuleDataStack = std::make_unique<RuleSourceDataList>();
    m_ruleSourceDataResult = ruleSourceDataResult;

    m_logErrors = logErrors && sheet->singleOwnerDocument() && !sheet->baseURL().isEmpty() && sheet->singleOwnerDocument()->page();
    m_ignoreErrorsInDeclaration = false;
    m_sheetStartLineNumber = textPosition.m_line.zeroBasedInt();
    m_sheetStartColumnNumber = textPosition.m_column.zeroBasedInt();
    m_lineNumber = m_sheetStartLineNumber;
    m_columnOffsetForLine = 0;
    setupParser("", string, "");
    cssyyparse(this);
    sheet->shrinkToFit();
    m_currentRuleDataStack.reset();
    m_ruleSourceDataResult = nullptr;
    m_rule = nullptr;
    m_ignoreErrorsInDeclaration = false;
    m_logErrors = false;
}

RefPtr<StyleRuleBase> CSSParser::parseRule(StyleSheetContents* sheet, const String& string)
{
    setStyleSheet(sheet);
    m_allowNamespaceDeclarations = false;
    setupParser("@-webkit-rule{", string, "} ");
    cssyyparse(this);
    return m_rule;
}

RefPtr<StyleKeyframe> CSSParser::parseKeyframeRule(StyleSheetContents* sheet, const String& string)
{
    setStyleSheet(sheet);
    setupParser("@-webkit-keyframe-rule{ ", string, "} ");
    cssyyparse(this);
    return m_keyframe;
}

bool CSSParser::parseSupportsCondition(const String& string)
{
    m_supportsCondition = false;
    // can't use { because tokenizer state switches from supports to initial state when it sees { token.
    // instead insert one " " (which is WHITESPACE in CSSGrammar.y)
    setupParser("@-webkit-supports-condition ", string, "} ");
    cssyyparse(this);
    return m_supportsCondition;
}

static inline bool isColorPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        return false;
    }
}

bool CSSParser::isValidSystemColorValue(CSSValueID valueID)
{
    return valueID >= CSSValueAqua && valueID <= CSSValueAppleSystemYellow;
}

static bool validPrimitiveValueColor(CSSValueID valueID, bool strict = false)
{
    return (valueID == CSSValueWebkitText || valueID == CSSValueCurrentcolor || valueID == CSSValueMenu
        || CSSParser::isValidSystemColorValue(valueID) || valueID == CSSValueAlpha
        || (valueID >= CSSValueWebkitFocusRingColor && valueID < CSSValueWebkitText && !strict));
}

static CSSParser::ParseResult parseColorValue(MutableStyleProperties* declaration, CSSPropertyID propertyId, const String& string, bool important, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool strict = isStrictParserMode(cssParserMode);
    if (!isColorPropertyID(propertyId))
        return CSSParser::ParseResult::Error;

    CSSParserString cssString;
    cssString.init(string);
    CSSValueID valueID = cssValueKeywordID(cssString);
    if (validPrimitiveValueColor(valueID, strict)) {
        RefPtr<CSSValue> value = CSSValuePool::singleton().createIdentifierValue(valueID);
        return declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
    }
    RGBA32 color;
    if (!CSSParser::fastParseColor(color, string, strict && string[0] != '#'))
        return CSSParser::ParseResult::Error;

    RefPtr<CSSValue> value = CSSValuePool::singleton().createColorValue(color);
    return declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

static inline bool isSimpleLengthPropertyID(CSSPropertyID propertyId, bool& acceptsNegativeNumbers)
{
    switch (propertyId) {
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridColumnGap:
    case CSSPropertyWebkitGridRowGap:
#endif
        acceptsNegativeNumbers = false;
        return true;
#if ENABLE(CSS_SHAPES)
    case CSSPropertyWebkitShapeMargin:
        acceptsNegativeNumbers = false;
        return RuntimeEnabledFeatures::sharedFeatures().cssShapesEnabled();
#endif
    case CSSPropertyBottom:
    case CSSPropertyCx:
    case CSSPropertyCy:
    case CSSPropertyLeft:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyR:
    case CSSPropertyRx:
    case CSSPropertyRy:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyX:
    case CSSPropertyY:
        acceptsNegativeNumbers = true;
        return true;
    default:
        return false;
    }
}

template <typename CharacterType>
static inline bool parseSimpleLength(const CharacterType* characters, unsigned& length, CSSPrimitiveValue::UnitTypes& unit, double& number)
{
    if (length > 2 && (characters[length - 2] | 0x20) == 'p' && (characters[length - 1] | 0x20) == 'x') {
        length -= 2;
        unit = CSSPrimitiveValue::CSS_PX;
    } else if (length > 1 && characters[length - 1] == '%') {
        length -= 1;
        unit = CSSPrimitiveValue::CSS_PERCENTAGE;
    }

    // We rely on charactersToDouble for validation as well. The function
    // will set "ok" to "false" if the entire passed-in character range does
    // not represent a double.
    bool ok;
    number = charactersToDouble(characters, length, &ok);
    return ok;
}

static CSSParser::ParseResult parseSimpleLengthValue(MutableStyleProperties* declaration, CSSPropertyID propertyId, const String& string, bool important, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool acceptsNegativeNumbers;
    if (!isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
        return CSSParser::ParseResult::Error;

    unsigned length = string.length();
    double number;
    CSSPrimitiveValue::UnitTypes unit = CSSPrimitiveValue::CSS_NUMBER;

    if (string.is8Bit()) {
        if (!parseSimpleLength(string.characters8(), length, unit, number))
            return CSSParser::ParseResult::Error;
    } else {
        if (!parseSimpleLength(string.characters16(), length, unit, number))
            return CSSParser::ParseResult::Error;
    }

    if (unit == CSSPrimitiveValue::CSS_NUMBER) {
        if (number && isStrictParserMode(cssParserMode))
            return CSSParser::ParseResult::Error;
        unit = CSSPrimitiveValue::CSS_PX;
    }
    if (number < 0 && !acceptsNegativeNumbers)
        return CSSParser::ParseResult::Error;
    if (std::isinf(number))
        return CSSParser::ParseResult::Error;

    RefPtr<CSSValue> value = CSSValuePool::singleton().createValue(number, unit);
    return declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

static inline bool isValidKeywordPropertyAndValue(CSSPropertyID propertyId, int valueID, const CSSParserContext& parserContext, StyleSheetContents* styleSheetContents)
{
    if (!valueID)
        return false;

    switch (propertyId) {
    case CSSPropertyBorderCollapse: // collapse | separate | inherit
        if (valueID == CSSValueCollapse || valueID == CSSValueSeparate)
            return true;
        break;
    case CSSPropertyBorderTopStyle: // <border-style> | inherit
    case CSSPropertyBorderRightStyle: // Defined as: none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle: // solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyColumnRuleStyle:
        if (valueID >= CSSValueNone && valueID <= CSSValueDouble)
            return true;
        break;
    case CSSPropertyBoxSizing:
         if (valueID == CSSValueBorderBox || valueID == CSSValueContentBox)
             return true;
         break;
    case CSSPropertyCaptionSide: // top | bottom | left | right | inherit
        if (valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueTop || valueID == CSSValueBottom)
            return true;
        break;
    case CSSPropertyClear: // none | left | right | both | inherit
        if (valueID == CSSValueNone || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueBoth)
            return true;
        break;
    case CSSPropertyDirection: // ltr | rtl | inherit
        if (valueID == CSSValueLtr || valueID == CSSValueRtl)
            return true;
        break;
    case CSSPropertyDisplay:
        // inline | block | list-item | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | -webkit-box | -webkit-inline-box | none | inherit
        // flex | -webkit-flex | inline-flex | -webkit-inline-flex | -webkit-grid | -webkit-inline-grid
        if ((valueID >= CSSValueInline && valueID <= CSSValueWebkitInlineFlex) || valueID == CSSValueNone)
            return true;
#if ENABLE(CSS_GRID_LAYOUT)
        if (valueID == CSSValueWebkitGrid || valueID == CSSValueWebkitInlineGrid)
            return true;
#endif
        break;

    case CSSPropertyEmptyCells: // show | hide | inherit
        if (valueID == CSSValueShow || valueID == CSSValueHide)
            return true;
        break;
    case CSSPropertyFloat: // left | right | none | center (for buggy CSS, maps to none)
        if (valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueNone || valueID == CSSValueCenter)
            return true;
        break;
    case CSSPropertyFontStyle: // normal | italic | oblique | inherit
        if (valueID == CSSValueNormal || valueID == CSSValueItalic || valueID == CSSValueOblique)
            return true;
        break;
    case CSSPropertyImageRendering: // auto | optimizeSpeed | optimizeQuality | -webkit-crisp-edges | -webkit-optimize-contrast | crisp-edges | pixelated
        // optimizeSpeed and optimizeQuality are deprecated; a user agent must accept them as valid values but must treat them as having the same behavior as pixelated and auto respectively.
        if (valueID == CSSValueAuto || valueID == CSSValueOptimizespeed || valueID == CSSValueOptimizequality
            || valueID == CSSValueWebkitCrispEdges || valueID == CSSValueWebkitOptimizeContrast || valueID == CSSValueCrispEdges || valueID == CSSValuePixelated)
            return true;
        break;
    case CSSPropertyListStylePosition: // inside | outside | inherit
        if (valueID == CSSValueInside || valueID == CSSValueOutside)
            return true;
        break;
    case CSSPropertyListStyleType:
        // See section CSS_PROP_LIST_STYLE_TYPE of file CSSValueKeywords.in
        // for the list of supported list-style-types.
        if ((valueID >= CSSValueDisc && valueID <= CSSValueKatakanaIroha) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyObjectFit:
        if (valueID == CSSValueFill || valueID == CSSValueContain || valueID == CSSValueCover || valueID == CSSValueNone || valueID == CSSValueScaleDown)
            return true;
        break;
    case CSSPropertyOutlineStyle: // (<border-style> except hidden) | auto | inherit
        if (valueID == CSSValueAuto || valueID == CSSValueNone || (valueID >= CSSValueInset && valueID <= CSSValueDouble))
            return true;
        break;
    case CSSPropertyOverflowWrap: // normal | break-word
    case CSSPropertyWordWrap:
        if (valueID == CSSValueNormal || valueID == CSSValueBreakWord)
            return true;
        break;
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyTouchAction: // auto | manipulation
        if (valueID == CSSValueAuto || valueID == CSSValueManipulation)
            return true;
        break;
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPropertyWebkitScrollSnapType: // none | mandatory | proximity
        if (valueID == CSSValueNone || valueID == CSSValueMandatory || valueID == CSSValueProximity)
            return true;
        break;
#endif
    case CSSPropertyOverflowX: // visible | hidden | scroll | auto | marquee | overlay | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay || valueID == CSSValueWebkitMarquee)
            return true;
        break;
    case CSSPropertyOverflowY: // visible | hidden | scroll | auto | marquee | overlay | inherit | -webkit-paged-x | -webkit-paged-y
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay || valueID == CSSValueWebkitMarquee || valueID == CSSValueWebkitPagedX || valueID == CSSValueWebkitPagedY)
            return true;
        break;
    case CSSPropertyPageBreakAfter: // auto | always | avoid | left | right | inherit
    case CSSPropertyPageBreakBefore:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        if (valueID == CSSValueAuto || valueID == CSSValueAlways || valueID == CSSValueAvoid || valueID == CSSValueLeft || valueID == CSSValueRight)
            return true;
        break;
    case CSSPropertyPageBreakInside: // avoid | auto | inherit
    case CSSPropertyWebkitColumnBreakInside:
        if (valueID == CSSValueAuto || valueID == CSSValueAvoid)
            return true;
        break;
    case CSSPropertyPointerEvents:
        // none | visiblePainted | visibleFill | visibleStroke | visible |
        // painted | fill | stroke | auto | all | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueNone || valueID == CSSValueAll || valueID == CSSValueAuto || (valueID >= CSSValueVisiblepainted && valueID <= CSSValueStroke))
            return true;
        break;
    case CSSPropertyPosition: // static | relative | absolute | fixed | sticky | inherit
        if (valueID == CSSValueStatic || valueID == CSSValueRelative || valueID == CSSValueAbsolute || valueID == CSSValueFixed || valueID == CSSValueWebkitSticky)
            return true;
        break;
    case CSSPropertyResize: // none | both | horizontal | vertical | auto
        if (valueID == CSSValueNone || valueID == CSSValueBoth || valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueAuto)
            return true;
        break;
    case CSSPropertySpeak: // none | normal | spell-out | digits | literal-punctuation | no-punctuation | inherit
        if (valueID == CSSValueNone || valueID == CSSValueNormal || valueID == CSSValueSpellOut || valueID == CSSValueDigits || valueID == CSSValueLiteralPunctuation || valueID == CSSValueNoPunctuation)
            return true;
        break;
    case CSSPropertyTableLayout: // auto | fixed | inherit
        if (valueID == CSSValueAuto || valueID == CSSValueFixed)
            return true;
        break;
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextUnderlineMode:
        if (valueID == CSSValueContinuous || valueID == CSSValueSkipWhiteSpace)
            return true;
        break;
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextUnderlineStyle:
        if (valueID == CSSValueNone || valueID == CSSValueSolid || valueID == CSSValueDouble || valueID == CSSValueDashed || valueID == CSSValueDotDash || valueID == CSSValueDotDotDash || valueID == CSSValueWave)
            return true;
        break;
    case CSSPropertyTextOverflow: // clip | ellipsis
        if (valueID == CSSValueClip || valueID == CSSValueEllipsis)
            return true;
        break;
    case CSSPropertyTextRendering: // auto | optimizeSpeed | optimizeLegibility | geometricPrecision
        if (valueID == CSSValueAuto || valueID == CSSValueOptimizespeed || valueID == CSSValueOptimizelegibility || valueID == CSSValueGeometricprecision)
            return true;
        break;
    case CSSPropertyTextTransform: // capitalize | uppercase | lowercase | none | inherit
        if ((valueID >= CSSValueCapitalize && valueID <= CSSValueLowercase) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyVisibility: // visible | hidden | collapse | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueCollapse)
            return true;
        break;
    case CSSPropertyWebkitAppearance:
        if ((valueID >= CSSValueCheckbox && valueID <= CSSValueCapsLockIndicator) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitBackfaceVisibility:
        if (valueID == CSSValueVisible || valueID == CSSValueHidden)
            return true;
        break;
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
        if (parserContext.isCSSCompositingEnabled && (valueID == CSSValueNormal || valueID == CSSValueMultiply || valueID == CSSValueScreen
            || valueID == CSSValueOverlay || valueID == CSSValueDarken || valueID == CSSValueLighten ||  valueID == CSSValueColorDodge
            || valueID == CSSValueColorBurn || valueID == CSSValueHardLight || valueID == CSSValueSoftLight || valueID == CSSValueDifference
            || valueID == CSSValueExclusion || valueID == CSSValuePlusDarker || valueID == CSSValuePlusLighter))
            return true;
        break;
    case CSSPropertyIsolation:
        if (parserContext.isCSSCompositingEnabled && (valueID == CSSValueAuto || valueID == CSSValueIsolate))
            return true;
        break;
#endif
    case CSSPropertyWebkitBorderFit:
        if (valueID == CSSValueBorder || valueID == CSSValueLines)
            return true;
        break;
    case CSSPropertyWebkitBoxAlign:
        if (valueID == CSSValueStretch || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline)
            return true;
        break;
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
         if (valueID == CSSValueClone || valueID == CSSValueSlice)
             return true;
         break;
#endif
    case CSSPropertyWebkitBoxDirection:
        if (valueID == CSSValueNormal || valueID == CSSValueReverse)
            return true;
        break;
    case CSSPropertyWebkitBoxLines:
        if (valueID == CSSValueSingle || valueID == CSSValueMultiple)
                return true;
        break;
    case CSSPropertyWebkitBoxOrient:
        if (valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueInlineAxis || valueID == CSSValueBlockAxis)
            return true;
        break;
    case CSSPropertyWebkitBoxPack:
        if (valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueJustify)
            return true;
        break;
    case CSSPropertyColumnFill:
        if (valueID == CSSValueAuto || valueID == CSSValueBalance)
            return true;
        break;
    case CSSPropertyFlexDirection:
        if (valueID == CSSValueRow || valueID == CSSValueRowReverse || valueID == CSSValueColumn || valueID == CSSValueColumnReverse)
            return true;
        break;
    case CSSPropertyFlexWrap:
        if (valueID == CSSValueNowrap || valueID == CSSValueWrap || valueID == CSSValueWrapReverse)
             return true;
        break;
    case CSSPropertyWebkitFontKerning:
        if (valueID == CSSValueAuto || valueID == CSSValueNormal || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitFontSmoothing:
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueAntialiased || valueID == CSSValueSubpixelAntialiased)
            return true;
        break;
    case CSSPropertyWebkitHyphens:
        if (valueID == CSSValueNone || valueID == CSSValueManual || valueID == CSSValueAuto)
            return true;
        break;
    case CSSPropertyWebkitLineAlign:
        if (valueID == CSSValueNone || valueID == CSSValueEdges)
            return true;
        break;
    case CSSPropertyWebkitLineBreak: // auto | loose | normal | strict | after-white-space
        if (valueID == CSSValueAuto || valueID == CSSValueLoose || valueID == CSSValueNormal || valueID == CSSValueStrict || valueID == CSSValueAfterWhiteSpace)
            return true;
        break;
    case CSSPropertyWebkitLineSnap:
        if (valueID == CSSValueNone || valueID == CSSValueBaseline || valueID == CSSValueContain)
            return true;
        break;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
        if (valueID == CSSValueCollapse || valueID == CSSValueSeparate || valueID == CSSValueDiscard)
            return true;
        break;
    case CSSPropertyWebkitMarqueeDirection:
        if (valueID == CSSValueForwards || valueID == CSSValueBackwards || valueID == CSSValueAhead || valueID == CSSValueReverse || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueDown
            || valueID == CSSValueUp || valueID == CSSValueAuto)
            return true;
        break;
    case CSSPropertyWebkitMarqueeStyle:
        if (valueID == CSSValueNone || valueID == CSSValueSlide || valueID == CSSValueScroll || valueID == CSSValueAlternate)
            return true;
        break;
    case CSSPropertyWebkitNbspMode: // normal | space
        if (valueID == CSSValueNormal || valueID == CSSValueSpace)
            return true;
        break;
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    case CSSPropertyWebkitOverflowScrolling:
        if (valueID == CSSValueAuto || valueID == CSSValueTouch)
            return true;
        break;
#endif
    case CSSPropertyWebkitPrintColorAdjust:
        if (valueID == CSSValueExact || valueID == CSSValueEconomy)
            return true;
        break;
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
        if (parserContext.isCSSRegionsEnabled && (valueID == CSSValueAuto || valueID == CSSValueAlways || valueID == CSSValueAvoid || valueID == CSSValueLeft || valueID == CSSValueRight))
            return true;
        break;
    case CSSPropertyWebkitRegionBreakInside:
        if (parserContext.isCSSRegionsEnabled && (valueID == CSSValueAuto || valueID == CSSValueAvoid))
            return true;
        break;
    case CSSPropertyWebkitRegionFragment:
        if (parserContext.isCSSRegionsEnabled && (valueID == CSSValueAuto || valueID == CSSValueBreak))
            return true;
        break;
#endif
    case CSSPropertyWebkitRtlOrdering:
        if (valueID == CSSValueLogical || valueID == CSSValueVisual)
            return true;
        break;

    case CSSPropertyWebkitRubyPosition:
        if (valueID == CSSValueBefore || valueID == CSSValueAfter || valueID == CSSValueInterCharacter)
            return true;
        break;

#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
        // auto | start | end | left | right | center | justify
        if ((valueID >= CSSValueLeft && valueID <= CSSValueJustify) || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueAuto)
            return true;
        break;
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextCombine:
        if (valueID == CSSValueNone || valueID == CSSValueHorizontal)
            return true;
        break;
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextJustify:
        // auto | none | inter-word | distribute
        if (valueID == CSSValueInterWord || valueID == CSSValueDistribute || valueID == CSSValueAuto || valueID == CSSValueNone)
            return true;
        break;
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextSecurity:
        // disc | circle | square | none | inherit
        if (valueID == CSSValueDisc || valueID == CSSValueCircle || valueID == CSSValueSquare || valueID == CSSValueNone)
            return true;
        break;
#if ENABLE(IOS_TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust:
        if (valueID == CSSValueAuto || valueID == CSSValueNone)
            return true;
        break;
#endif
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
        if (valueID == CSSValueFlat || valueID == CSSValuePreserve3d)
            return true;
        break;
    case CSSPropertyWebkitUserDrag: // auto | none | element
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueElement)
            return true;
        break;
    case CSSPropertyWebkitUserModify: // read-only | read-write
        if (valueID == CSSValueReadOnly || valueID == CSSValueReadWrite || valueID == CSSValueReadWritePlaintextOnly) {
            if (styleSheetContents)
                styleSheetContents->parserSetUsesStyleBasedEditability();
            return true;
        }
        break;
    case CSSPropertyWebkitUserSelect: // auto | none | text | all
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueText)
            return true;
        if (valueID == CSSValueAll) {
            if (styleSheetContents)
                styleSheetContents->parserSetUsesStyleBasedEditability();
            return true;
        }
        break;
    case CSSPropertyWebkitWritingMode:
        if (valueID >= CSSValueHorizontalTb && valueID <= CSSValueHorizontalBt)
            return true;
        break;
    case CSSPropertyWhiteSpace: // normal | pre | nowrap | inherit
        if (valueID == CSSValueNormal || valueID == CSSValuePre || valueID == CSSValuePreWrap || valueID == CSSValuePreLine || valueID == CSSValueNowrap)
            return true;
        break;
    case CSSPropertyWordBreak: // normal | break-all | keep-all | break-word (this is a custom extension)
        if (valueID == CSSValueNormal || valueID == CSSValueBreakAll || valueID == CSSValueKeepAll || valueID == CSSValueBreakWord)
            return true;
        break;
#if ENABLE(CSS_TRAILING_WORD)
    case CSSPropertyAppleTrailingWord: // auto | -apple-partially-balanced
        if (valueID == CSSValueAuto || valueID == CSSValueWebkitPartiallyBalanced)
            return true;
        break;
#endif
    case CSSPropertyFontVariantPosition: // normal | sub | super
        if (valueID == CSSValueNormal || valueID == CSSValueSub || valueID == CSSValueSuper)
            return true;
        break;
    case CSSPropertyFontVariantCaps: // normal | small-caps | all-small-caps | petite-caps | all-petite-caps | unicase | titling-caps
        if (valueID == CSSValueNormal || valueID == CSSValueSmallCaps || valueID == CSSValueAllSmallCaps || valueID == CSSValuePetiteCaps || valueID == CSSValueAllPetiteCaps || valueID == CSSValueUnicase || valueID == CSSValueTitlingCaps)
            return true;
        break;
    case CSSPropertyFontVariantAlternates: // We only support the normal and historical-forms values.
        if (valueID == CSSValueNormal || valueID == CSSValueHistoricalForms)
            return true;
        break;
            
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
        // auto | avoid | left | right | recto | verso | column | page | region | avoid-page | avoid-column | avoid-region
        if (valueID == CSSValueAuto || valueID == CSSValueAvoid || valueID == CSSValueLeft || valueID == CSSValueRight
            || valueID == CSSValueRecto || valueID == CSSValueVerso || valueID == CSSValueColumn || valueID == CSSValuePage
            || valueID == CSSValueRegion || valueID == CSSValueAvoidColumn || valueID == CSSValueAvoidPage || valueID == CSSValueAvoidRegion)
            return true;
        break;
    case CSSPropertyBreakInside:
        // auto | avoid | avoid-page | avoid-column | avoid-region
        if (valueID == CSSValueAuto || valueID == CSSValueAvoid || valueID == CSSValueAvoidColumn || valueID == CSSValueAvoidPage || valueID == CSSValueAvoidRegion)
            return true;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
#if !ENABLE(CSS_COMPOSITING) && !ENABLE(CSS_REGIONS)
    UNUSED_PARAM(parserContext);
#endif
    return false;
}

static inline bool isKeywordPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontStyle:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertySpeak:
    case CSSPropertyTableLayout:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
#endif
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
#endif
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyColumnFill:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitNbspMode:
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    case CSSPropertyWebkitOverflowScrolling:
#endif
    case CSSPropertyWebkitPrintColorAdjust:
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitRegionFragment:
#endif
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextCombine:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextJustify:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyTouchAction:
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPropertyWebkitScrollSnapType:
#endif
#if ENABLE(CSS_TRAILING_WORD)
    case CSSPropertyAppleTrailingWord:
#endif
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantAlternates:
        return true;
    default:
        return false;
    }
}

static bool isUniversalKeyword(const String& string)
{
    // These keywords can be used for all properties.
    return equalLettersIgnoringASCIICase(string, "initial")
        || equalLettersIgnoringASCIICase(string, "inherit")
        || equalLettersIgnoringASCIICase(string, "unset")
        || equalLettersIgnoringASCIICase(string, "revert");
}

static CSSParser::ParseResult parseKeywordValue(MutableStyleProperties* declaration, CSSPropertyID propertyId, const String& string, bool important, const CSSParserContext& parserContext, StyleSheetContents* styleSheetContents)
{
    ASSERT(!string.isEmpty());

    if (!isKeywordPropertyID(propertyId)) {
        if (!isUniversalKeyword(string))
            return CSSParser::ParseResult::Error;

        // Don't try to parse initial/inherit/unset/revert shorthands; return an error so the caller will use the full CSS parser.
        if (shorthandForProperty(propertyId).length())
            return CSSParser::ParseResult::Error;
    }

    CSSParserString cssString;
    cssString.init(string);
    CSSValueID valueID = cssValueKeywordID(cssString);

    if (!valueID)
        return CSSParser::ParseResult::Error;

    RefPtr<CSSValue> value;
    if (valueID == CSSValueInherit)
        value = CSSValuePool::singleton().createInheritedValue();
    else if (valueID == CSSValueInitial)
        value = CSSValuePool::singleton().createExplicitInitialValue();
    else if (valueID == CSSValueUnset)
        value = CSSValuePool::singleton().createUnsetValue();
    else if (valueID == CSSValueRevert)
        value = CSSValuePool::singleton().createRevertValue();
    else if (isValidKeywordPropertyAndValue(propertyId, valueID, parserContext, styleSheetContents))
        value = CSSValuePool::singleton().createIdentifierValue(valueID);
    else
        return CSSParser::ParseResult::Error;

    return declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

template <typename CharacterType>
static bool parseTransformTranslateArguments(WebKitCSSTransformValue& transformValue, CharacterType* characters, unsigned length, unsigned start, unsigned expectedCount)
{
    auto& cssValuePool = CSSValuePool::singleton();
    while (expectedCount) {
        size_t end = WTF::find(characters, length, expectedCount == 1 ? ')' : ',', start);
        if (end == notFound || (expectedCount == 1 && end != length - 1))
            return false;
        unsigned argumentLength = end - start;
        CSSPrimitiveValue::UnitTypes unit = CSSPrimitiveValue::CSS_NUMBER;
        double number;
        if (!parseSimpleLength(characters + start, argumentLength, unit, number))
            return false;
        if (unit != CSSPrimitiveValue::CSS_PX && (number || unit != CSSPrimitiveValue::CSS_NUMBER))
            return false;
        transformValue.append(cssValuePool.createValue(number, CSSPrimitiveValue::CSS_PX));
        start = end + 1;
        --expectedCount;
    }
    return true;
}

static CSSParser::ParseResult parseTranslateTransformValue(MutableStyleProperties* properties, CSSPropertyID propertyID, const String& string, bool important)
{
    if (propertyID != CSSPropertyTransform)
        return CSSParser::ParseResult::Error;

    static const unsigned shortestValidTransformStringLength = 12;
    static const unsigned likelyMultipartTransformStringLengthCutoff = 32;
    if (string.length() < shortestValidTransformStringLength || string.length() > likelyMultipartTransformStringLengthCutoff)
        return CSSParser::ParseResult::Error;

    if (!string.startsWith("translate", false))
        return CSSParser::ParseResult::Error;

    UChar c9 = toASCIILower(string[9]);
    UChar c10 = toASCIILower(string[10]);

    WebKitCSSTransformValue::TransformOperationType transformType;
    unsigned expectedArgumentCount = 1;
    unsigned argumentStart = 11;
    if (c9 == 'x' && c10 == '(')
        transformType = WebKitCSSTransformValue::TranslateXTransformOperation;
    else if (c9 == 'y' && c10 == '(')
        transformType = WebKitCSSTransformValue::TranslateYTransformOperation;
    else if (c9 == 'z' && c10 == '(')
        transformType = WebKitCSSTransformValue::TranslateZTransformOperation;
    else if (c9 == '(') {
        transformType = WebKitCSSTransformValue::TranslateTransformOperation;
        expectedArgumentCount = 2;
        argumentStart = 10;
    } else if (c9 == '3' && c10 == 'd' && string[11] == '(') {
        transformType = WebKitCSSTransformValue::Translate3DTransformOperation;
        expectedArgumentCount = 3;
        argumentStart = 12;
    } else
        return CSSParser::ParseResult::Error;

    RefPtr<WebKitCSSTransformValue> transformValue = WebKitCSSTransformValue::create(transformType);
    bool success;
    if (string.is8Bit())
        success = parseTransformTranslateArguments(*transformValue, string.characters8(), string.length(), argumentStart, expectedArgumentCount);
    else
        success = parseTransformTranslateArguments(*transformValue, string.characters16(), string.length(), argumentStart, expectedArgumentCount);
    if (!success)
        return CSSParser::ParseResult::Error;

    RefPtr<CSSValueList> result = CSSValueList::createSpaceSeparated();
    result->append(transformValue.releaseNonNull());
    return properties->addParsedProperty(CSSProperty(CSSPropertyTransform, result.release(), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;
}

RefPtr<CSSValueList> CSSParser::parseFontFaceValue(const AtomicString& string)
{
    if (string.isEmpty())
        return nullptr;

    Ref<CSSValueList> valueList = CSSValueList::createCommaSeparated();

    Vector<String> familyNames;
    string.string().split(',', true, familyNames);

    auto& cssValuePool = CSSValuePool::singleton();
    for (auto& familyName : familyNames) {
        String stripped = stripLeadingAndTrailingHTMLSpaces(familyName);
        if (stripped.isEmpty())
            return nullptr;

        RefPtr<CSSValue> value;
        for (auto propertyID : { CSSValueSerif, CSSValueSansSerif, CSSValueCursive, CSSValueFantasy, CSSValueMonospace, CSSValueWebkitBody }) {
            if (equalIgnoringASCIICase(stripped, getValueName(propertyID))) {
                value = cssValuePool.createIdentifierValue(propertyID);
                break;
            }
        }
        if (!value)
            value = cssValuePool.createFontFamilyValue(stripped);
        valueList->append(value.releaseNonNull());
    }

    return WTFMove(valueList);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties* declaration, CSSPropertyID propertyID, const String& string, bool important, CSSParserMode cssParserMode, StyleSheetContents* contextStyleSheet)
{
    ASSERT(!string.isEmpty());
    CSSParser::ParseResult result = parseSimpleLengthValue(declaration, propertyID, string, important, cssParserMode);
    if (result != ParseResult::Error)
        return result;

    result = parseColorValue(declaration, propertyID, string, important, cssParserMode);
    if (result != ParseResult::Error)
        return result;

    CSSParserContext context(cssParserMode);
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.mode = cssParserMode;
    }

    result = parseKeywordValue(declaration, propertyID, string, important, context, contextStyleSheet);
    if (result != ParseResult::Error)
        return result;

    result = parseTranslateTransformValue(declaration, propertyID, string, important);
    if (result != ParseResult::Error)
        return result;

    CSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important, contextStyleSheet);
}

CSSParser::ParseResult CSSParser::parseCustomPropertyValue(MutableStyleProperties* declaration, const AtomicString& propertyName, const String& string, bool important, CSSParserMode cssParserMode, StyleSheetContents* contextStyleSheet)
{
    CSSParserContext context(cssParserMode);
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.mode = cssParserMode;
    }

    CSSParser parser(context);
    parser.setCustomPropertyName(propertyName);
    return parser.parseValue(declaration, CSSPropertyCustom, string, important, contextStyleSheet);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties* declaration, CSSPropertyID propertyID, const String& string, bool important, StyleSheetContents* contextStyleSheet)
{
    setStyleSheet(contextStyleSheet);

    setupParser("@-webkit-value{", string, "} ");

    m_id = propertyID;
    m_important = important;

    cssyyparse(this);

    m_rule = nullptr;

    ParseResult result = ParseResult::Error;

    if (!m_parsedProperties.isEmpty()) {
        result = declaration->addParsedProperties(m_parsedProperties) ? ParseResult::Changed : ParseResult::Unchanged;
        clearProperties();
    }

    return result;
}

// The color will only be changed when string contains a valid CSS color, so callers
// can set it to a default color and ignore the boolean result.
bool CSSParser::parseColor(RGBA32& color, const String& string, bool strict)
{
    if (string.isEmpty())
        return false;

    // First try creating a color specified by name, rgba(), rgb() or "#" syntax.
    if (fastParseColor(color, string, strict))
        return true;

    CSSParser parser(CSSStrictMode);

    // In case the fast-path parser didn't understand the color, try the full parser.
    if (!parser.parseColor(string))
        return false;

    CSSValue& value = *parser.m_parsedProperties.first().value();
    if (!is<CSSPrimitiveValue>(value))
        return false;

    CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(value);
    if (!primitiveValue.isRGBColor())
        return false;

    color = primitiveValue.getRGBA32Value();
    return true;
}

bool CSSParser::parseColor(const String& string)
{
    setupParser("@-webkit-decls{color:", string, "} ");
    cssyyparse(this);
    m_rule = nullptr;

    return !m_parsedProperties.isEmpty() && m_parsedProperties.first().id() == CSSPropertyColor;
}

bool CSSParser::parseSystemColor(RGBA32& color, const String& string, Document* document)
{
    if (!document || !document->page())
        return false;

    CSSParserString cssColor;
    cssColor.init(string);
    CSSValueID id = cssValueKeywordID(cssColor);
    if (!validPrimitiveValueColor(id))
        return false;

    Color parsedColor = document->page()->theme().systemColor(id);
    if (!parsedColor.isValid())
        return false;

    color = parsedColor.rgb();
    return true;
}

void CSSParser::parseSelector(const String& string, CSSSelectorList& selectorList)
{
    m_selectorListForParseSelector = &selectorList;

    setupParser("@-webkit-selector{", string, "}");

    cssyyparse(this);

    m_selectorListForParseSelector = nullptr;
}

Ref<ImmutableStyleProperties> CSSParser::parseInlineStyleDeclaration(const String& string, Element* element)
{
    CSSParserContext context = element->document().elementSheet().contents().parserContext();
    context.mode = strictToCSSParserMode(element->isHTMLElement() && !element->document().inQuirksMode());
    return CSSParser(context).parseDeclaration(string, &element->document().elementSheet().contents());
}

Ref<ImmutableStyleProperties> CSSParser::parseDeclaration(const String& string, StyleSheetContents* contextStyleSheet)
{
    setStyleSheet(contextStyleSheet);

    setupParser("@-webkit-decls{", string, "} ");
    cssyyparse(this);
    m_rule = nullptr;

    Ref<ImmutableStyleProperties> style = createStyleProperties();
    clearProperties();
    return style;
}


bool CSSParser::parseDeclaration(MutableStyleProperties* declaration, const String& string, PassRefPtr<CSSRuleSourceData> prpRuleSourceData, StyleSheetContents* contextStyleSheet)
{
    // Length of the "@-webkit-decls{" prefix.
    static const unsigned prefixLength = 15;

    setStyleSheet(contextStyleSheet);

    RefPtr<CSSRuleSourceData> ruleSourceData = prpRuleSourceData;
    if (ruleSourceData) {
        m_currentRuleDataStack = std::make_unique<RuleSourceDataList>();
        m_currentRuleDataStack->append(ruleSourceData);
    }

    setupParser("@-webkit-decls{", string, "} ");
    cssyyparse(this);
    m_rule = nullptr;

    bool ok = false;
    if (!m_parsedProperties.isEmpty()) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties);
        clearProperties();
    }

    if (ruleSourceData) {
        ASSERT(m_currentRuleDataStack->size() == 1);
        ruleSourceData->ruleBodyRange.start = 0;
        ruleSourceData->ruleBodyRange.end = string.length();
        for (size_t i = 0, size = ruleSourceData->styleSourceData->propertyData.size(); i < size; ++i) {
            CSSPropertySourceData& propertyData = ruleSourceData->styleSourceData->propertyData.at(i);
            propertyData.range.start -= prefixLength;
            propertyData.range.end -= prefixLength;
        }

        fixUnparsedPropertyRanges(ruleSourceData.get());
        m_currentRuleDataStack.reset();
    }

    return ok;
}

std::unique_ptr<MediaQuery> CSSParser::parseMediaQuery(const String& string)
{
    if (string.isEmpty())
        return nullptr;

    ASSERT(!m_mediaQuery);

    // can't use { because tokenizer state switches from mediaquery to initial state when it sees { token.
    // instead insert one " " (which is WHITESPACE in CSSGrammar.y)
    setupParser("@-webkit-mediaquery ", string, "} ");
    cssyyparse(this);

    return WTFMove(m_mediaQuery);
}

Vector<CSSParser::SourceSize> CSSParser::parseSizesAttribute(StringView string)
{
    Vector<SourceSize> result;

    if (string.isEmpty())
        return result;

    ASSERT(!m_sourceSizeList);

    setupParser("@-webkit-sizesattr ", string, "}");
    cssyyparse(this);

    if (!m_sourceSizeList)
        return result;

    result = WTFMove(*m_sourceSizeList);
    m_sourceSizeList = nullptr;
    return result;
}

// FIXME(141289): The following two constructors are only needed because of a bug in MSVC 2013 (and prior).
// We should remove this code as soon as a Visual Studio update that fixes this problem is released.
CSSParser::SourceSize::SourceSize(CSSParser::SourceSize&& original)
    : expression(WTFMove(original.expression))
    , length(original.length)
{
}

CSSParser::SourceSize::SourceSize(std::unique_ptr<MediaQueryExp>&& origExp, RefPtr<CSSValue> value)
    : expression(WTFMove(origExp))
    , length(value)
{
}

CSSParser::SourceSize CSSParser::sourceSize(std::unique_ptr<MediaQueryExp>&& expression, CSSParserValue& parserValue)
{
    RefPtr<CSSValue> value;
    if (isCalculation(parserValue)) {
        auto* args = parserValue.function->args.get();
        if (args && args->size())
            value = CSSCalcValue::create(parserValue.function->name, *args, CalculationRangeNonNegative);
    }
    if (!value)
        value = parserValue.createCSSValue();
    destroy(parserValue);
    // FIXME: Calling the constructor explicitly here to work around an MSVC bug.
    // For other compilers, we did not need to define the constructors and we could use aggregate initialization syntax.
    return SourceSize(WTFMove(expression), WTFMove(value));
}

static inline void filterProperties(bool important, const CSSParser::ParsedPropertyVector& input, Vector<CSSProperty, 256>& output, size_t& unusedEntries, std::bitset<numCSSProperties>& seenProperties, HashSet<AtomicString>& seenCustomProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (int i = input.size() - 1; i >= 0; --i) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        
        if (property.id() == CSSPropertyCustom) {
            if (property.value()) {
                const AtomicString& name = downcast<CSSCustomPropertyValue>(*property.value()).name();
                if (seenCustomProperties.contains(name))
                    continue;
                seenCustomProperties.add(name);
                output[--unusedEntries] = property;
            }
            continue;
        }

        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        ASSERT(propertyIDIndex < seenProperties.size());
        if (seenProperties[propertyIDIndex])
            continue;
        seenProperties.set(propertyIDIndex);
        output[--unusedEntries] = property;
    }
}

Ref<ImmutableStyleProperties> CSSParser::createStyleProperties()
{
    std::bitset<numCSSProperties> seenProperties;
    size_t unusedEntries = m_parsedProperties.size();
    Vector<CSSProperty, 256> results(unusedEntries);

    // Important properties have higher priority, so add them first. Duplicate definitions can then be ignored when found.
    HashSet<AtomicString> seenCustomProperties;
    filterProperties(true, m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    filterProperties(false, m_parsedProperties, results, unusedEntries, seenProperties, seenCustomProperties);
    if (unusedEntries)
        results.remove(0, unusedEntries);

    return ImmutableStyleProperties::create(results.data(), results.size(), m_context.mode);
}

void CSSParser::addPropertyWithPrefixingVariant(CSSPropertyID propId, PassRefPtr<CSSValue> value, bool important, bool implicit)
{
    RefPtr<CSSValue> val = value.get();
    addProperty(propId, value, important, implicit);

    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(propId);
    if (prefixingVariant == propId)
        return;

    if (m_currentShorthand) {
        // We can't use ShorthandScope here as we can already be inside one (e.g we are parsing CSSTransition).
        m_currentShorthand = prefixingVariantForPropertyId(m_currentShorthand);
        addProperty(prefixingVariant, val.release(), important, implicit);
        m_currentShorthand = prefixingVariantForPropertyId(m_currentShorthand);
    } else
        addProperty(prefixingVariant, val.release(), important, implicit);
}

void CSSParser::addProperty(CSSPropertyID propId, PassRefPtr<CSSValue> value, bool important, bool implicit)
{
    // This property doesn't belong to a shorthand or is a CSS variable (which will be resolved later).
    if (!m_currentShorthand) {
        m_parsedProperties.append(CSSProperty(propId, value, important, false, CSSPropertyInvalid, m_implicitShorthand || implicit));
        return;
    }

    Vector<StylePropertyShorthand> shorthands = matchingShorthandsForLonghand(propId);
    if (shorthands.size() == 1)
        m_parsedProperties.append(CSSProperty(propId, value, important, true, CSSPropertyInvalid, m_implicitShorthand || implicit));
    else
        m_parsedProperties.append(CSSProperty(propId, value, important, true, indexOfShorthandForLonghand(m_currentShorthand, shorthands), m_implicitShorthand || implicit));
}

void CSSParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(m_parsedProperties.size() >= static_cast<unsigned>(num));
    m_parsedProperties.shrink(m_parsedProperties.size() - num);
}

void CSSParser::clearProperties()
{
    m_parsedProperties.clear();
    m_numParsedPropertiesBeforeMarginBox = INVALID_NUM_PARSED_PROPERTIES;
}

URL CSSParser::completeURL(const CSSParserContext& context, const String& url)
{
    if (url.isNull())
        return URL();
    if (context.charset.isEmpty())
        return URL(context.baseURL, url);
    return URL(context.baseURL, url, context.charset);
}

URL CSSParser::completeURL(const String& url) const
{
    return completeURL(m_context, url);
}

bool CSSParser::validateCalculationUnit(ValueWithCalculation& valueWithCalculation, Units unitFlags)
{
    bool mustBeNonNegative = unitFlags & FNonNeg;

    RefPtr<CSSCalcValue> calculation;
    if (valueWithCalculation.calculation()) {
        // The calculation value was already parsed so we reuse it. However, we may need to update its range.
        calculation = valueWithCalculation.calculation();
        calculation->setPermittedValueRange(mustBeNonNegative ? CalculationRangeNonNegative : CalculationRangeAll);
    } else {
        valueWithCalculation.setCalculation(parseCalculation(valueWithCalculation, mustBeNonNegative ? CalculationRangeNonNegative : CalculationRangeAll));
        calculation = valueWithCalculation.calculation();
        if (!calculation)
            return false;
    }

    bool isValid = false;
    switch (calculation->category()) {
    case CalcNumber:
        isValid = (unitFlags & FNumber);
        if (!isValid && (unitFlags & FInteger) && calculation->isInt())
            isValid = true;
        if (!isValid && (unitFlags & FPositiveInteger) && calculation->isInt() && calculation->isPositive())
            isValid = true;
        break;
    case CalcLength:
        isValid = (unitFlags & FLength);
        break;
    case CalcPercent:
        isValid = (unitFlags & FPercent);
        break;
    case CalcPercentLength:
        isValid = (unitFlags & FPercent) && (unitFlags & FLength);
        break;
    case CalcPercentNumber:
        isValid = (unitFlags & FPercent) && (unitFlags & FNumber);
        break;
    case CalcAngle:
        isValid = (unitFlags & FAngle);
        break;
    case CalcTime:
        isValid = (unitFlags & FTime);
        break;
    case CalcFrequency:
        isValid = (unitFlags & FFrequency);
        break;
    case CalcOther:
        break;
    }

    return isValid;
}

inline bool CSSParser::shouldAcceptUnitLessValues(CSSParserValue& value, Units unitFlags, CSSParserMode cssParserMode)
{
    // Qirks mode and svg presentation attributes accept unit less values.
    return (unitFlags & (FLength | FAngle | FTime)) && (!value.fValue || cssParserMode == CSSQuirksMode || cssParserMode == SVGAttributeMode);
}

bool CSSParser::validateUnit(ValueWithCalculation& valueWithCalculation, Units unitFlags, CSSParserMode cssParserMode)
{
    if (isCalculation(valueWithCalculation))
        return validateCalculationUnit(valueWithCalculation, unitFlags);
        
    bool b = false;
    switch (valueWithCalculation.value().unit) {
    case CSSPrimitiveValue::CSS_NUMBER:
        b = (unitFlags & FNumber);
        if (!b && shouldAcceptUnitLessValues(valueWithCalculation, unitFlags, cssParserMode)) {
            valueWithCalculation.value().unit = (unitFlags & FLength) ? CSSPrimitiveValue::CSS_PX :
                          ((unitFlags & FAngle) ? CSSPrimitiveValue::CSS_DEG : CSSPrimitiveValue::CSS_MS);
            b = true;
        }
        if (!b && (unitFlags & FInteger) && valueWithCalculation.value().isInt)
            b = true;
        if (!b && (unitFlags & FPositiveInteger) && valueWithCalculation.value().isInt && valueWithCalculation.value().fValue > 0)
            b = true;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        b = (unitFlags & FPercent);
        break;
    case CSSParserValue::Q_EMS:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
    case CSSPrimitiveValue::CSS_VW:
    case CSSPrimitiveValue::CSS_VH:
    case CSSPrimitiveValue::CSS_VMIN:
    case CSSPrimitiveValue::CSS_VMAX:
        b = (unitFlags & FLength);
        break;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        b = (unitFlags & FTime);
        break;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        b = (unitFlags & FAngle);
        break;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    case CSSPrimitiveValue::CSS_DPPX:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
        b = (unitFlags & FResolution);
        break;
#endif
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
        break;
    }
    if (b && unitFlags & FNonNeg && valueWithCalculation.value().fValue < 0)
        b = false;
    if (b && std::isinf(valueWithCalculation.value().fValue))
        b = false;
    return b;
}

inline Ref<CSSPrimitiveValue> CSSParser::createPrimitiveNumericValue(ValueWithCalculation& valueWithCalculation)
{
    if (valueWithCalculation.calculation())
        return CSSPrimitiveValue::create(valueWithCalculation.calculation());

    CSSParserValue& value = valueWithCalculation;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    ASSERT((value.unit >= CSSPrimitiveValue::CSS_NUMBER && value.unit <= CSSPrimitiveValue::CSS_KHZ)
        || (value.unit >= CSSPrimitiveValue::CSS_TURN && value.unit <= CSSPrimitiveValue::CSS_CHS)
        || (value.unit >= CSSPrimitiveValue::CSS_VW && value.unit <= CSSPrimitiveValue::CSS_VMAX)
        || (value.unit >= CSSPrimitiveValue::CSS_DPPX && value.unit <= CSSPrimitiveValue::CSS_DPCM));
#else
    ASSERT((value.unit >= CSSPrimitiveValue::CSS_NUMBER && value.unit <= CSSPrimitiveValue::CSS_KHZ)
        || (value.unit >= CSSPrimitiveValue::CSS_TURN && value.unit <= CSSPrimitiveValue::CSS_CHS)
        || (value.unit >= CSSPrimitiveValue::CSS_VW && value.unit <= CSSPrimitiveValue::CSS_VMAX));
#endif
    return CSSValuePool::singleton().createValue(value.fValue, static_cast<CSSPrimitiveValue::UnitTypes>(value.unit));
}

inline Ref<CSSPrimitiveValue> CSSParser::createPrimitiveStringValue(CSSParserValue& value)
{
    ASSERT(value.unit == CSSPrimitiveValue::CSS_STRING || value.unit == CSSPrimitiveValue::CSS_IDENT);
    return CSSValuePool::singleton().createValue(value.string, CSSPrimitiveValue::CSS_STRING);
}

static inline bool isComma(CSSParserValue* value)
{ 
    return value && value->unit == CSSParserValue::Operator && value->iValue == ','; 
}

static inline bool isForwardSlashOperator(CSSParserValue& value)
{
    return value.unit == CSSParserValue::Operator && value.iValue == '/';
}

bool CSSParser::isValidSize(ValueWithCalculation& valueWithCalculation)
{
    int id = valueWithCalculation.value().id;
    if (id == CSSValueIntrinsic || id == CSSValueMinIntrinsic || id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent || id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent)
        return true;
    return !id && validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg);
}

inline RefPtr<CSSPrimitiveValue> CSSParser::parseValidPrimitive(CSSValueID identifier, ValueWithCalculation& valueWithCalculation)
{
    if (identifier)
        return CSSValuePool::singleton().createIdentifierValue(identifier);

    if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_STRING)
        return createPrimitiveStringValue(valueWithCalculation);
    if (valueWithCalculation.value().unit >= CSSPrimitiveValue::CSS_NUMBER && valueWithCalculation.value().unit <= CSSPrimitiveValue::CSS_KHZ)
        return createPrimitiveNumericValue(valueWithCalculation);
    if (valueWithCalculation.value().unit >= CSSPrimitiveValue::CSS_TURN && valueWithCalculation.value().unit <= CSSPrimitiveValue::CSS_CHS)
        return createPrimitiveNumericValue(valueWithCalculation);
    if (valueWithCalculation.value().unit >= CSSPrimitiveValue::CSS_VW && valueWithCalculation.value().unit <= CSSPrimitiveValue::CSS_VMAX)
        return createPrimitiveNumericValue(valueWithCalculation);
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
    if (valueWithCalculation.value().unit >= CSSPrimitiveValue::CSS_DPPX && valueWithCalculation.value().unit <= CSSPrimitiveValue::CSS_DPCM)
        return createPrimitiveNumericValue(valueWithCalculation);
#endif
    if (valueWithCalculation.value().unit >= CSSParserValue::Q_EMS)
        return CSSPrimitiveValue::createAllowingMarginQuirk(valueWithCalculation.value().fValue, CSSPrimitiveValue::CSS_EMS);
    if (valueWithCalculation.calculation())
        return CSSPrimitiveValue::create(valueWithCalculation.calculation());

    return nullptr;
}

void CSSParser::addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtr<CSSValue> prpValue, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    unsigned shorthandLength = shorthand.length();
    if (!shorthandLength) {
        addProperty(propId, prpValue, important);
        return;
    }

    RefPtr<CSSValue> value = prpValue;
    ShorthandScope scope(this, propId);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], value, important);
}

RefPtr<CSSValue> CSSParser::parseVariableDependentValue(CSSPropertyID propID, const CSSVariableDependentValue& dependentValue, const CustomPropertyValueMap& customProperties)
{
    m_valueList.reset(new CSSParserValueList());
    if (!dependentValue.valueList()->buildParserValueListSubstitutingVariables(m_valueList.get(), customProperties))
        return nullptr;
    bool parsed = parseValue(dependentValue.propertyID(), false);
    if (!parsed)
        return nullptr;
    for (size_t i = 0; i < m_parsedProperties.size(); ++i) {
        if (m_parsedProperties[i].id() == propID)
            return m_parsedProperties[i].value();
    }
    return nullptr;
}

bool CSSParser::parseValue(CSSPropertyID propId, bool important)
{
    if (!m_valueList || !m_valueList->current())
        return false;
    
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    CSSValueID id = valueWithCalculation.value().id;
    
    if (propId == CSSPropertyCustom)
        return parseCustomPropertyDeclaration(important, id);

    if (m_valueList->containsVariables()) {
        RefPtr<CSSValueList> valueList = CSSValueList::createFromParserValueList(*m_valueList);
        addExpandedPropertyForValue(propId, CSSVariableDependentValue::create(valueList, propId), important);
        return true;
    }

    auto& cssValuePool = CSSValuePool::singleton();
    unsigned num = inShorthand() ? 1 : m_valueList->size();

    if (id == CSSValueInherit) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool.createInheritedValue(), important);
        return true;
    }
    else if (id == CSSValueInitial) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool.createExplicitInitialValue(), important);
        return true;
    } else if (id == CSSValueUnset) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool.createUnsetValue(), important);
        return true;
    } else if (id == CSSValueRevert) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool.createRevertValue(), important);
        return true;
    }
    
    if (propId == CSSPropertyAll)
        return false; // "all" doesn't allow you to specify anything other than inherit/initial/unset.

    if (isKeywordPropertyID(propId)) {
        if (!isValidKeywordPropertyAndValue(propId, id, m_context, m_styleSheet))
            return false;
        if (m_valueList->next() && !inShorthand())
            return false;
        addProperty(propId, cssValuePool.createIdentifierValue(id), important);
        return true;
    }

#if ENABLE(CSS_DEVICE_ADAPTATION)
    if (inViewport())
        return parseViewportProperty(propId, important);
#endif

    bool validPrimitive = false;
    RefPtr<CSSValue> parsedValue;

    switch (propId) {
    case CSSPropertySize:                 // <length>{1,2} | auto | [ <page-size> || [ portrait | landscape] ]
        return parseSize(propId, important);

    case CSSPropertyQuotes:               // [<string> <string>]+ | none | inherit
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            return parseQuotes(propId, important);
        break;
    case CSSPropertyUnicodeBidi: // normal | embed | bidi-override | isolate | isolate-override | plaintext | inherit
        if (id == CSSValueNormal
            || id == CSSValueEmbed
            || id == CSSValueBidiOverride
            || id == CSSValueWebkitIsolate
            || id == CSSValueWebkitIsolateOverride
            || id == CSSValueWebkitPlaintext)
            validPrimitive = true;
        break;

    case CSSPropertyContent:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
        // close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent(propId, important);

    case CSSPropertyAlt: // [ <string> | attr(X) ]
        return parseAlt(propId, important);
            
    case CSSPropertyClip:                 // <shape> | auto | inherit
        if (id == CSSValueAuto)
            validPrimitive = true;
        else if (valueWithCalculation.value().unit == CSSParserValue::Function)
            return parseClipShape(propId, important);
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShorthand to work
     * correctly and allows optimization in WebCore::applyRule(..)
     */
    case CSSPropertyOverflow: {
        ShorthandScope scope(this, propId);
        if (num != 1 || !parseValue(CSSPropertyOverflowY, important))
            return false;

        RefPtr<CSSValue> overflowXValue;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
            overflowXValue = cssValuePool.createIdentifierValue(CSSValueAuto);
        else
            overflowXValue = m_parsedProperties.last().value();
        addProperty(CSSPropertyOverflowX, overflowXValue.release(), important);
        return true;
    }

    case CSSPropertyTextAlign:
        // left | right | center | justify | -webkit-left | -webkit-right | -webkit-center | -webkit-match-parent
        // | start | end | inherit | -webkit-auto (converted to start)
        // NOTE: <string> is not supported.
        if ((id >= CSSValueWebkitAuto && id <= CSSValueWebkitMatchParent) || id == CSSValueStart || id == CSSValueEnd)
            validPrimitive = true;
        break;

    case CSSPropertyFontWeight:  { // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | inherit
        if (m_valueList->size() != 1)
            return false;
        return parseFontWeight(important);
    }

    case CSSPropertyFontSynthesis: // none | [ weight || style ]
        return parseFontSynthesis(important);

    case CSSPropertyBorderSpacing: {
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(CSSPropertyWebkitBorderHorizontalSpacing, important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            addProperty(CSSPropertyWebkitBorderVerticalSpacing, value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(CSSPropertyWebkitBorderHorizontalSpacing, important) || !parseValue(CSSPropertyWebkitBorderVerticalSpacing, important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        validPrimitive = validateUnit(valueWithCalculation, FLength | FNonNeg);
        break;
    case CSSPropertyOutlineColor:        // <color> | invert | inherit
        // Outline color has "invert" as additional keyword.
        // Also, we want to allow the special focus color even in strict parsing mode.
        if (id == CSSValueInvert || id == CSSValueWebkitFocusRingColor) {
            validPrimitive = true;
            break;
        }
        FALLTHROUGH;
    case CSSPropertyBackgroundColor: // <color> | inherit
    case CSSPropertyBorderTopColor: // <color> | inherit
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyColor: // <color> | inherit
    case CSSPropertyTextLineThroughColor: // CSS3 text decoration colors
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyWebkitTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        if (id == CSSValueWebkitText)
            validPrimitive = true; // Always allow this, even when strict parsing is on,
                                    // since we use this in our UA sheets.
        else if (id == CSSValueCurrentcolor)
            validPrimitive = true;
        else if (isValidSystemColorValue(id) || id == CSSValueMenu
            || (id >= CSSValueWebkitFocusRingColor && id < CSSValueWebkitText && inQuirksMode())) {
            validPrimitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyCursor: {
        // Grammar defined by CSS3 UI and modified by CSS4 images:
        // [ [<image> [<x> <y>]?,]*
        // [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
        // nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | ew-resize |
        // ns-resize | nesw-resize | nwse-resize | col-resize | row-resize | text | wait | help |
        // vertical-text | cell | context-menu | alias | copy | no-drop | not-allowed |
        // zoom-in | zoom-out | all-scroll | -webkit-grab | -webkit-grabbing | -webkit-zoom-in |
        // -webkit-zoom-out ] ] | inherit
        RefPtr<CSSValueList> list;
        CSSParserValue* value = &valueWithCalculation.value();
        while (value) {
            RefPtr<CSSValue> image;
            if (value->unit == CSSPrimitiveValue::CSS_URI) {
                String uri = value->string;
                if (!uri.isNull())
                    image = CSSImageValue::create(completeURL(uri));
#if ENABLE(CSS_IMAGE_SET) && ENABLE(MOUSE_CURSOR_SCALE)
            } else if (value->unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(value->function->name, "-webkit-image-set(")) {
                image = parseImageSet();
                if (!image)
                    break;
#endif
            } else
                break;

            Vector<int> coords;
            value = m_valueList->next();
            while (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                coords.append(int(value->fValue));
                value = m_valueList->next();
            }
            bool hasHotSpot = false;
            IntPoint hotSpot(-1, -1);
            int nrcoords = coords.size();
            if (nrcoords > 0 && nrcoords != 2)
                return false;
            if (nrcoords == 2) {
                hasHotSpot = true;
                hotSpot = IntPoint(coords[0], coords[1]);
            }

            if (!list)
                list = CSSValueList::createCommaSeparated();

            if (image)
                list->append(CSSCursorImageValue::create(image.releaseNonNull(), hasHotSpot, hotSpot));

            if ((inStrictMode() && !value) || (value && !(value->unit == CSSParserValue::Operator && value->iValue == ',')))
                return false;
            value = m_valueList->next(); // comma
        }
        if (list) {
            if (!value) {
                // no value after url list (MSIE 5 compatibility)
                if (list->length() != 1)
                    return false;
            } else if (inQuirksMode() && value->id == CSSValueHand) // MSIE 5 compatibility :/
                list->append(cssValuePool.createIdentifierValue(CSSValuePointer));
            else if ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitZoomOut) || value->id == CSSValueCopy || value->id == CSSValueNone)
                list->append(cssValuePool.createIdentifierValue(value->id));
            m_valueList->next();
            parsedValue = list.release();
            break;
        } else if (value) {
            id = value->id;
            if (inQuirksMode() && value->id == CSSValueHand) {
                // MSIE 5 compatibility :/
                id = CSSValuePointer;
                validPrimitive = true;
            } else if ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitZoomOut) || value->id == CSSValueCopy || value->id == CSSValueNone)
                validPrimitive = true;
        } else {
            ASSERT_NOT_REACHED();
            return false;
        }
        break;
    }

#if ENABLE(CURSOR_VISIBILITY)
    case CSSPropertyWebkitCursorVisibility:
        if (id == CSSValueAuto || id == CSSValueAutoHide)
            validPrimitive = true;
        break;
#endif

    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskSourceType:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    {
        RefPtr<CSSValue> val1;
        RefPtr<CSSValue> val2;
        CSSPropertyID propId1, propId2;
        bool result = false;
        if (parseFillProperty(propId, propId1, propId2, val1, val2)) {
            std::unique_ptr<ShorthandScope> shorthandScope;
            if (propId == CSSPropertyBackgroundPosition ||
                propId == CSSPropertyBackgroundRepeat ||
                propId == CSSPropertyWebkitMaskPosition ||
                propId == CSSPropertyWebkitMaskRepeat) {
                shorthandScope = std::make_unique<ShorthandScope>(this, propId);
            }
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            result = true;
        }
        m_implicitShorthand = false;
        return result;
    }
    case CSSPropertyListStyleImage:     // <uri> | none | inherit
    case CSSPropertyBorderImageSource:
    case CSSPropertyWebkitMaskBoxImageSource:
        if (id == CSSValueNone) {
            parsedValue = cssValuePool.createIdentifierValue(CSSValueNone);
            m_valueList->next();
        } else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSImageValue::create(completeURL(valueWithCalculation.value().string));
            m_valueList->next();
        } else if (isGeneratedImageValue(valueWithCalculation)) {
            if (parseGeneratedImage(*m_valueList, parsedValue))
                m_valueList->next();
            else
                return false;
        }
#if ENABLE(CSS_IMAGE_SET)
        else if (valueWithCalculation.value().unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(valueWithCalculation.value().function->name, "-webkit-image-set(")) {
            parsedValue = parseImageSet();
            if (!parsedValue)
                return false;
            m_valueList->next();
        }
#endif
        break;

    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyOutlineWidth:        // <border-width> | inherit
    case CSSPropertyBorderTopWidth:     //// <border-width> | inherit
    case CSSPropertyBorderRightWidth:   //   Which is defined as
    case CSSPropertyBorderBottomWidth:  //   thin | medium | thick | <length>
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyColumnRuleWidth:
        if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FLength | FNonNeg);
        break;

    case CSSPropertyLetterSpacing:       // normal | <length> | inherit
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FLength);
        break;

    case CSSPropertyWordSpacing:         // normal | <length> | <percentage> | inherit
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FLength | FPercent);
        break;

    case CSSPropertyTextIndent:
        parsedValue = parseTextIndent();
        break;

    case CSSPropertyPaddingTop:          //// <padding-width> | inherit
    case CSSPropertyPaddingRight:        //   Which is defined as
    case CSSPropertyPaddingBottom:       //   <length> | <percentage>
    case CSSPropertyPaddingLeft:         ////
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
        validPrimitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg));
        break;

    case CSSPropertyMaxWidth:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyMaxHeight:
    case CSSPropertyWebkitMaxLogicalHeight:
        validPrimitive = (id == CSSValueNone || isValidSize(valueWithCalculation));
        break;

    case CSSPropertyMinWidth:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWebkitMinLogicalHeight:
        validPrimitive = id == CSSValueAuto || isValidSize(valueWithCalculation);
        break;

    case CSSPropertyWidth:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyHeight:
    case CSSPropertyWebkitLogicalHeight:
        validPrimitive = (id == CSSValueAuto || isValidSize(valueWithCalculation));
        break;

    case CSSPropertyFontSize:
        return parseFontSize(important);

    case CSSPropertyVerticalAlign:
        // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if (id >= CSSValueBaseline && id <= CSSValueWebkitBaselineMiddle)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent));
        break;

    case CSSPropertyBottom:               // <length> | <percentage> | auto | inherit
    case CSSPropertyLeft:                 // <length> | <percentage> | auto | inherit
    case CSSPropertyRight:                // <length> | <percentage> | auto | inherit
    case CSSPropertyTop:                  // <length> | <percentage> | auto | inherit
    case CSSPropertyMarginTop:           //// <margin-width> | inherit
    case CSSPropertyMarginRight:         //   Which is defined as
    case CSSPropertyMarginBottom:        //   <length> | <percentage> | auto | inherit
    case CSSPropertyMarginLeft:          ////
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent));
        break;

    case CSSPropertyZIndex:              // auto | <integer> | inherit
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FInteger, CSSQuirksMode));
        break;

    case CSSPropertyOrphans: // <integer> | inherit | auto (We've added support for auto for backwards compatibility)
    case CSSPropertyWidows: // <integer> | inherit | auto (Ditto)
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FPositiveInteger, CSSQuirksMode));
        break;

    case CSSPropertyLineHeight:
        return parseLineHeight(important);
    case CSSPropertyCounterIncrement:    // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 1, important);
        validPrimitive = true;
        break;
    case CSSPropertyCounterReset:        // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 0, important);
        validPrimitive = true;
        break;
    case CSSPropertyFontFamily:
        // [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
        parsedValue = parseFontFamily();
        break;
    }

    case CSSPropertyWebkitTextDecoration:
        // [ <text-decoration-line> || <text-decoration-style> || <text-decoration-color> ] | inherit
        return parseShorthand(CSSPropertyWebkitTextDecoration, webkitTextDecorationShorthand(), important);

    case CSSPropertyTextDecoration:
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyWebkitTextDecorationLine:
        // none | [ underline || overline || line-through || blink ] | inherit
        return parseTextDecoration(propId, important);

    case CSSPropertyWebkitTextDecorationStyle:
        // solid | double | dotted | dashed | wavy
        if (id == CSSValueSolid || id == CSSValueDouble || id == CSSValueDotted || id == CSSValueDashed || id == CSSValueWavy)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitTextDecorationSkip:
        // none | [ objects || spaces || ink || edges || box-decoration ]
        return parseTextDecorationSkip(important);

    case CSSPropertyWebkitTextUnderlinePosition:
        // auto | alphabetic | under
        return parseTextUnderlinePosition(important);

    case CSSPropertyZoom:
        // normal | reset | document | <number> | <percentage> | inherit
        if (id == CSSValueNormal || id == CSSValueReset || id == CSSValueDocument)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FNumber | FPercent | FNonNeg, CSSStrictMode));
        break;
    
    case CSSPropertyWebkitTextZoom:
        // normal | reset
        if (id == CSSValueNormal || id == CSSValueReset)
            validPrimitive = true;
        break;

    case CSSPropertySrc: // Only used within @font-face and @-webkit-filter, so cannot use inherit | initial or be !important. This is a list of urls or local references.
        return parseFontFaceSrc();

    case CSSPropertyUnicodeRange:
        return parseFontFaceUnicodeRange();

    /* CSS3 properties */

    case CSSPropertyBorderImage: {
        RefPtr<CSSValue> result;
        return parseBorderImage(propId, result, important);
    }
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage: {
        RefPtr<CSSValue> result;
        if (parseBorderImage(propId, result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset: {
        RefPtr<CSSPrimitiveValue> result;
        if (parseBorderImageOutset(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat: {
        RefPtr<CSSValue> result;
        if (parseBorderImageRepeat(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice: {
        RefPtr<CSSBorderImageSliceValue> result;
        if (parseBorderImageSlice(propId, result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth: {
        RefPtr<CSSPrimitiveValue> result;
        if (parseBorderImageWidth(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius: {
        if (num != 1 && num != 2)
            return false;
        validPrimitive = validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg);
        if (!validPrimitive)
            return false;
        RefPtr<CSSPrimitiveValue> parsedValue1 = createPrimitiveNumericValue(valueWithCalculation);
        RefPtr<CSSPrimitiveValue> parsedValue2;
        if (num == 2) {
            ValueWithCalculation nextValueWithCalculation(*m_valueList->next());
            validPrimitive = validateUnit(nextValueWithCalculation, FLength | FPercent | FNonNeg);
            if (!validPrimitive)
                return false;
            parsedValue2 = createPrimitiveNumericValue(nextValueWithCalculation);
        } else
            parsedValue2 = parsedValue1;

        addProperty(propId, createPrimitiveValuePair(parsedValue1.release(), parsedValue2.release()), important);
        return true;
    }
    case CSSPropertyTabSize:
        validPrimitive = validateUnit(valueWithCalculation, FInteger | FNonNeg);
        break;
    case CSSPropertyWebkitAspectRatio:
        return parseAspectRatio(important);
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        return parseBorderRadius(propId, important);
    case CSSPropertyOutlineOffset:
        validPrimitive = validateUnit(valueWithCalculation, FLength);
        break;
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtr<CSSValueList> shadowValueList = parseShadow(*m_valueList, propId);
            if (shadowValueList) {
                addProperty(propId, shadowValueList.release(), important);
                m_valueList->next();
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyWebkitInitialLetter: {
        if (id == CSSValueNormal)
            validPrimitive = true;
        else {
            if (num != 1 && num != 2)
                return false;
            validPrimitive = validateUnit(valueWithCalculation, FPositiveInteger);
            if (!validPrimitive)
                return false;
            RefPtr<CSSPrimitiveValue> height = createPrimitiveNumericValue(valueWithCalculation);
            RefPtr<CSSPrimitiveValue> position;
            if (num == 2) {
                ValueWithCalculation nextValueWithCalculation(*m_valueList->next());
                validPrimitive = validateUnit(nextValueWithCalculation, FPositiveInteger);
                if (!validPrimitive)
                    return false;
                position = createPrimitiveNumericValue(nextValueWithCalculation);
            } else
                position = height;
            addProperty(propId, createPrimitiveValuePair(position.release(), height.release()), important);
            return true;
        }
        break;
    }
    case CSSPropertyWebkitBoxReflect:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            return parseReflect(propId, important);
        break;
    case CSSPropertyOpacity:
        validPrimitive = validateUnit(valueWithCalculation, FNumber);
        break;
    case CSSPropertyWebkitBoxFlex:
        validPrimitive = validateUnit(valueWithCalculation, FNumber);
        break;
    case CSSPropertyWebkitBoxFlexGroup:
        validPrimitive = validateUnit(valueWithCalculation, FInteger | FNonNeg, CSSStrictMode);
        break;
    case CSSPropertyWebkitBoxOrdinalGroup:
        validPrimitive = validateUnit(valueWithCalculation, FInteger | FNonNeg, CSSStrictMode) && valueWithCalculation.value().fValue;
        break;
    case CSSPropertyFilter:
#if ENABLE(FILTERS_LEVEL_2)
    case CSSPropertyWebkitBackdropFilter:
#endif
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtr<CSSValue> currValue;
            if (!parseFilter(*m_valueList, currValue))
                return false;
            addProperty(propId, currValue, important);
            return true;
        }
        break;
#if ENABLE(CSS_COMPOSITING)
    case CSSPropertyMixBlendMode:
        if (cssCompositingEnabled())
            validPrimitive = true;
        break;
    case CSSPropertyIsolation:
        if (cssCompositingEnabled())
            validPrimitive = true;
        break;
#endif
    case CSSPropertyFlex: {
        ShorthandScope scope(this, propId);
        if (id == CSSValueNone) {
            addProperty(CSSPropertyFlexGrow, cssValuePool.createValue(0, CSSPrimitiveValue::CSS_NUMBER), important);
            addProperty(CSSPropertyFlexShrink, cssValuePool.createValue(0, CSSPrimitiveValue::CSS_NUMBER), important);
            addProperty(CSSPropertyFlexBasis, cssValuePool.createIdentifierValue(CSSValueAuto), important);
            return true;
        }
        return parseFlex(*m_valueList, important);
    }
    case CSSPropertyFlexBasis:
        // FIXME: Support intrinsic dimensions too.
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg));
        break;
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
        validPrimitive = validateUnit(valueWithCalculation, FNumber | FNonNeg);
        break;
    case CSSPropertyOrder:
        if (validateUnit(valueWithCalculation, FInteger, CSSStrictMode)) {
            // We restrict the smallest value to int min + 2 because we use int min and int min + 1 as special values in a hash set.
            double result = std::max<double>(std::numeric_limits<int>::min() + 2, parsedDouble(valueWithCalculation));
            parsedValue = cssValuePool.createValue(result, CSSPrimitiveValue::CSS_NUMBER);
            m_valueList->next();
        }
        break;
    case CSSPropertyWebkitMarquee:
        return parseShorthand(propId, webkitMarqueeShorthand(), important);
    case CSSPropertyWebkitMarqueeIncrement:
        if (id == CSSValueSmall || id == CSSValueLarge || id == CSSValueMedium)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FLength | FPercent);
        break;
    case CSSPropertyWebkitMarqueeRepetition:
        if (id == CSSValueInfinite)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FInteger | FNonNeg);
        break;
    case CSSPropertyWebkitMarqueeSpeed:
        if (id == CSSValueNormal || id == CSSValueSlow || id == CSSValueFast)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FTime | FInteger | FNonNeg);
        break;
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitFlowInto:
        if (!cssRegionsEnabled())
            return false;
        return parseFlowThread(propId, important);
    case CSSPropertyWebkitFlowFrom:
        if (!cssRegionsEnabled())
            return false;
        return parseRegionThread(propId, important);
#endif
    case CSSPropertyTransform:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtr<CSSValue> transformValue = parseTransform();
            if (transformValue) {
                addProperty(propId, transformValue.release(), important);
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyTransformOrigin:
    case CSSPropertyTransformOriginX:
    case CSSPropertyTransformOriginY:
    case CSSPropertyTransformOriginZ: {
        RefPtr<CSSPrimitiveValue> val1;
        RefPtr<CSSPrimitiveValue> val2;
        RefPtr<CSSValue> val3;
        CSSPropertyID propId1, propId2, propId3;
        if (parseTransformOrigin(propId, propId1, propId2, propId3, val1, val2, val3)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            if (val3)
                addProperty(propId3, val3.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyPerspective:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            // Accepting valueless numbers is a quirk of the -webkit prefixed version of the property.
            if (validateUnit(valueWithCalculation, FNumber | FLength | FNonNeg)) {
                RefPtr<CSSValue> val = createPrimitiveNumericValue(valueWithCalculation);
                if (val) {
                    addProperty(propId, val.release(), important);
                    return true;
                }
                return false;
            }
        }
        break;
    case CSSPropertyPerspectiveOrigin:
    case CSSPropertyPerspectiveOriginX:
    case CSSPropertyPerspectiveOriginY: {
        RefPtr<CSSPrimitiveValue> val1;
        RefPtr<CSSPrimitiveValue> val2;
        CSSPropertyID propId1, propId2;
        if (parsePerspectiveOrigin(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationTimingFunction:
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    case CSSPropertyWebkitAnimationTrigger:
#endif
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionProperty:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitTransitionProperty: {
        RefPtr<CSSValue> val;
        AnimationParseContext context;
        if (parseAnimationProperty(propId, val, context)) {
            addPropertyWithPrefixingVariant(propId, val.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyJustifyContent:
        parsedValue = parseContentDistributionOverflowPosition();
        break;
    case CSSPropertyJustifySelf:
        return parseItemPositionOverflowPosition(propId, important);
    case CSSPropertyJustifyItems:
        if (parseLegacyPosition(propId, important))
            return true;
        m_valueList->setCurrentIndex(0);
        return parseItemPositionOverflowPosition(propId, important);
#if ENABLE(CSS_GRID_LAYOUT)
    case CSSPropertyWebkitGridAutoColumns:
    case CSSPropertyWebkitGridAutoRows:
        parsedValue = parseGridTrackSize(*m_valueList);
        break;

    case CSSPropertyWebkitGridTemplateColumns:
    case CSSPropertyWebkitGridTemplateRows:
        parsedValue = parseGridTrackList();
        break;

    case CSSPropertyWebkitGridColumnStart:
    case CSSPropertyWebkitGridColumnEnd:
    case CSSPropertyWebkitGridRowStart:
    case CSSPropertyWebkitGridRowEnd:
        parsedValue = parseGridPosition();
        break;

    case CSSPropertyWebkitGridColumnGap:
    case CSSPropertyWebkitGridRowGap:
        validPrimitive = validateUnit(valueWithCalculation, FLength | FNonNeg);
        break;

    case CSSPropertyWebkitGridGap:
        return parseGridGapShorthand(important);

    case CSSPropertyWebkitGridColumn:
    case CSSPropertyWebkitGridRow: {
        return parseGridItemPositionShorthand(propId, important);
    }

    case CSSPropertyWebkitGridTemplate:
        return parseGridTemplateShorthand(important);

    case CSSPropertyWebkitGrid:
        return parseGridShorthand(important);

    case CSSPropertyWebkitGridArea:
        return parseGridAreaShorthand(important);

    case CSSPropertyWebkitGridTemplateAreas:
        parsedValue = parseGridTemplateAreas();
        break;
    case CSSPropertyWebkitGridAutoFlow:
        parsedValue = parseGridAutoFlow(*m_valueList);
        break;
#endif /* ENABLE(CSS_GRID_LAYOUT) */
    case CSSPropertyWebkitMarginCollapse: {
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(webkitMarginCollapseShorthand().properties()[0], important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            addProperty(webkitMarginCollapseShorthand().properties()[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(webkitMarginCollapseShorthand().properties()[0], important) || !parseValue(webkitMarginCollapseShorthand().properties()[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineWidth:
        if (id == CSSValueAuto || id == CSSValueNormal || id == CSSValueThin ||
            id == CSSValueMedium || id == CSSValueThick)
            validPrimitive = true;
        else
            validPrimitive = !id && validateUnit(valueWithCalculation, FNumber | FLength | FPercent);
        break;
    case CSSPropertyColumnCount:
        parsedValue = parseColumnCount();
        break;
    case CSSPropertyColumnGap: // normal | <length>
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validateUnit(valueWithCalculation, FLength | FNonNeg);
        break;
    case CSSPropertyWebkitColumnAxis:
        if (id == CSSValueHorizontal || id == CSSValueVertical || id == CSSValueAuto)
            validPrimitive = true;
        break;
    case CSSPropertyColumnProgression:
        if (id == CSSValueNormal || id == CSSValueReverse)
            validPrimitive = true;
        break;
    case CSSPropertyColumnSpan: // none | all | 1 (will be dropped in the unprefixed property)
        if (id == CSSValueAll || id == CSSValueNone)
            validPrimitive = true;
        else if (validateUnit(valueWithCalculation, FNumber | FNonNeg) && parsedDouble(valueWithCalculation) == 1) {
            addProperty(CSSPropertyColumnSpan, cssValuePool.createValue(1, CSSPrimitiveValue::CSS_NUMBER), important);
            return true;
        }
        break;
    case CSSPropertyColumnWidth: // auto | <length>
        parsedValue = parseColumnWidth();
        break;
    // End of CSS3 properties

    case CSSPropertyWillChange: // auto | [scroll-position | contents | <custom-ident>]#
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            return parseWillChange(important);
        break;

    // Apple specific properties.  These will never be standardized and are purely to
    // support custom WebKit-based Apple applications.
    case CSSPropertyWebkitLineClamp:
        // When specifying number of lines, don't allow 0 as a valid value
        // When specifying either type of unit, require non-negative integers
        validPrimitive = (!id && (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_PERCENTAGE || valueWithCalculation.value().fValue) && validateUnit(valueWithCalculation, FInteger | FPercent | FNonNeg, CSSQuirksMode));
        break;
#if ENABLE(IOS_TEXT_AUTOSIZING)
    case CSSPropertyWebkitTextSizeAdjust:
        if (id == CSSValueAuto || id == CSSValueNone)
            validPrimitive = true;
        else {
            // FIXME: Handle multilength case where we allow relative units.
            validPrimitive = (!id && validateUnit(valueWithCalculation, FPercent | FNonNeg, CSSStrictMode));
        }
        break;
#endif

    case CSSPropertyWebkitFontSizeDelta:           // <length>
        validPrimitive = validateUnit(valueWithCalculation, FLength);
        break;

    case CSSPropertyWebkitHyphenateCharacter:
        if (id == CSSValueAuto || valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitAfter:
        if (id == CSSValueAuto || validateUnit(valueWithCalculation, FInteger | FNonNeg, CSSStrictMode))
            validPrimitive = true;
        break;

    case CSSPropertyWebkitHyphenateLimitLines:
        if (id == CSSValueNoLimit || validateUnit(valueWithCalculation, FInteger | FNonNeg, CSSStrictMode))
            validPrimitive = true;
        break;

    case CSSPropertyWebkitLineGrid:
        if (id == CSSValueNone)
            validPrimitive = true;
        else if (valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_IDENT) {
            String lineGridValue = String(valueWithCalculation.value().string);
            if (!lineGridValue.isEmpty()) {
                addProperty(propId, cssValuePool.createValue(lineGridValue, CSSPrimitiveValue::CSS_STRING), important);
                return true;
            }
        }
        break;
    case CSSPropertyWebkitLocale:
        if (id == CSSValueAuto || valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion: // <dashboard-region> | <dashboard-region>
        if (valueWithCalculation.value().unit == CSSParserValue::Function || id == CSSValueNone)
            return parseDashboardRegions(propId, important);
        break;
#endif

#if PLATFORM(IOS)
    case CSSPropertyWebkitTouchCallout:
        if (id == CSSValueDefault || id == CSSValueNone)
            validPrimitive = true;
        break;
#endif
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyWebkitTapHighlightColor:
        if (isValidSystemColorValue(id) || id == CSSValueMenu
            || (id >= CSSValueWebkitFocusRingColor && id < CSSValueWebkitText && inQuirksMode())) {
            validPrimitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                m_valueList->next();
        }
        break;
#endif
    // End Apple-specific properties

        /* shorthand properties */
    case CSSPropertyBackground: {
        // Position must come before color in this array because a plain old "0" is a legal color
        // in quirks mode but it's usually the X coordinate of a position.
        const CSSPropertyID properties[] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat,
                                   CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyBackgroundOrigin,
                                   CSSPropertyBackgroundClip, CSSPropertyBackgroundColor, CSSPropertyBackgroundSize };
        return parseFillShorthand(propId, properties, WTF_ARRAY_LENGTH(properties), important);
    }
    case CSSPropertyWebkitMask: {
        const CSSPropertyID properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskSourceType, CSSPropertyWebkitMaskRepeat,
            CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskOrigin, CSSPropertyWebkitMaskClip, CSSPropertyWebkitMaskSize };
        return parseFillShorthand(propId, properties, WTF_ARRAY_LENGTH(properties), important);
    }
    case CSSPropertyBorder:
        // [ 'border-width' || 'border-style' || <color> ] | inherit
    {
        if (parseShorthand(propId, borderAbridgedShorthand(), important)) {
            // The CSS3 Borders and Backgrounds specification says that border also resets border-image. It's as
            // though a value of none was specified for the image.
            addExpandedPropertyForValue(CSSPropertyBorderImage, cssValuePool.createImplicitInitialValue(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyBorderTop:
        // [ 'border-top-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderTopShorthand(), important);
    case CSSPropertyBorderRight:
        // [ 'border-right-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
        // [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
        // [ 'border-left-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderLeftShorthand(), important);
    case CSSPropertyWebkitBorderStart:
        return parseShorthand(propId, webkitBorderStartShorthand(), important);
    case CSSPropertyWebkitBorderEnd:
        return parseShorthand(propId, webkitBorderEndShorthand(), important);
    case CSSPropertyWebkitBorderBefore:
        return parseShorthand(propId, webkitBorderBeforeShorthand(), important);
    case CSSPropertyWebkitBorderAfter:
        return parseShorthand(propId, webkitBorderAfterShorthand(), important);
    case CSSPropertyOutline:
        // [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
        return parseShorthand(propId, outlineShorthand(), important);
    case CSSPropertyBorderColor:
        // <color>{1,4} | inherit
        return parse4Values(propId, borderColorShorthand().properties(), important);
    case CSSPropertyBorderWidth:
        // <border-width>{1,4} | inherit
        return parse4Values(propId, borderWidthShorthand().properties(), important);
    case CSSPropertyBorderStyle:
        // <border-style>{1,4} | inherit
        return parse4Values(propId, borderStyleShorthand().properties(), important);
    case CSSPropertyMargin:
        // <margin-width>{1,4} | inherit
        return parse4Values(propId, marginShorthand().properties(), important);
    case CSSPropertyPadding:
        // <padding-width>{1,4} | inherit
        return parse4Values(propId, paddingShorthand().properties(), important);
    case CSSPropertyFlexFlow:
        return parseShorthand(propId, flexFlowShorthand(), important);
    case CSSPropertyFont:
        // [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
        // 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
        if (num == 1 && id >= CSSValueCaption && id <= CSSValueStatusBar) {
            parseSystemFont(important);
            return true;
        }
        return parseFont(important);
    case CSSPropertyListStyle:
        return parseShorthand(propId, listStyleShorthand(), important);
    case CSSPropertyColumns:
        return parseColumnsShorthand(important);
    case CSSPropertyColumnRule:
        return parseShorthand(propId, columnRuleShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return parseShorthand(propId, webkitTextStrokeShorthand(), important);
    case CSSPropertyAnimation:
    case CSSPropertyWebkitAnimation:
        return parseAnimationShorthand(propId, important);
    case CSSPropertyTransition:
    case CSSPropertyWebkitTransition:
        return parseTransitionShorthand(propId, important);
    case CSSPropertyInvalid:
        return false;
    case CSSPropertyPage:
        return parsePage(propId, important);
    case CSSPropertyFontStretch:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextOverline:
    case CSSPropertyTextUnderline:
        return false;
    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitTextEmphasis:
        return parseShorthand(propId, webkitTextEmphasisShorthand(), important);

    case CSSPropertyWebkitTextEmphasisStyle:
        return parseTextEmphasisStyle(important);

    case CSSPropertyWebkitTextEmphasisPosition:
        return parseTextEmphasisPosition(important);

    case CSSPropertyWebkitTextOrientation:
        if (id == CSSValueSideways || id == CSSValueSidewaysRight || id == CSSValueVerticalRight || id == CSSValueMixed || id == CSSValueUpright)
            validPrimitive = true;
        break;

    case CSSPropertyHangingPunctuation:
        return parseHangingPunctuation(important);
    case CSSPropertyWebkitLineBoxContain:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            return parseLineBoxContain(important);
        break;
    case CSSPropertyFontFeatureSettings:
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            return parseFontFeatureSettings(important);
        break;
    case CSSPropertyFontVariantLigatures:
        if (id == CSSValueNormal || id == CSSValueNone)
            validPrimitive = true;
        else
            return parseFontVariantLigatures(important, true, false);
        break;
    case CSSPropertyFontVariantNumeric:
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            return parseFontVariantNumeric(important, true, false);
        break;
    case CSSPropertyFontVariantEastAsian:
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            return parseFontVariantEastAsian(important, true, false);
        break;
    case CSSPropertyFontVariant:
        if (id == CSSValueNormal) {
            ShorthandScope scope(this, CSSPropertyFontVariant);
            addProperty(CSSPropertyFontVariantLigatures, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            addProperty(CSSPropertyFontVariantPosition, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            addProperty(CSSPropertyFontVariantNumeric, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            addProperty(CSSPropertyFontVariantAlternates, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            addProperty(CSSPropertyFontVariantEastAsian, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
            return true;
        }
        if (id == CSSValueNone) {
            ShorthandScope scope(this, CSSPropertyFontVariant);
            addProperty(CSSPropertyFontVariantLigatures, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important, true);
            return true;
        }
        return parseFontVariant(important);

    case CSSPropertyWebkitClipPath:
        parsedValue = parseClipPath();
        break;
#if ENABLE(CSS_SHAPES)
    case CSSPropertyWebkitShapeOutside:
        parsedValue = parseShapeProperty(propId);
        break;
    case CSSPropertyWebkitShapeMargin:
        validPrimitive = (RuntimeEnabledFeatures::sharedFeatures().cssShapesEnabled() && !id && validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg));
        break;
    case CSSPropertyWebkitShapeImageThreshold:
        validPrimitive = (RuntimeEnabledFeatures::sharedFeatures().cssShapesEnabled() && !id && validateUnit(valueWithCalculation, FNumber));
        break;
#endif
#if ENABLE(CSS_IMAGE_ORIENTATION)
    case CSSPropertyImageOrientation:
        validPrimitive = !id && validateUnit(valueWithCalculation, FAngle);
        break;
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    case CSSPropertyImageResolution:
        parsedValue = parseImageResolution();
        break;
#endif
    case CSSPropertyAlignContent:
        parsedValue = parseContentDistributionOverflowPosition();
        break;
    case CSSPropertyAlignSelf:
        return parseItemPositionOverflowPosition(propId, important);

    case CSSPropertyAlignItems:
        return parseItemPositionOverflowPosition(propId, important);
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyBreakAfter:
    case CSSPropertyBreakBefore:
    case CSSPropertyBreakInside:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontStyle:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertySpeak:
    case CSSPropertyTableLayout:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
#if ENABLE(CSS_BOX_DECORATION_BREAK)
    case CSSPropertyWebkitBoxDecorationBreak:
#endif
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyColumnFill:
    case CSSPropertyColumnRuleStyle:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitNbspMode:
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    case CSSPropertyWebkitOverflowScrolling:
#endif
    case CSSPropertyWebkitPrintColorAdjust:
#if ENABLE(CSS_REGIONS)
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitRegionFragment:
#endif
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextCombine:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextJustify:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:
#if ENABLE(TOUCH_EVENTS)
    case CSSPropertyTouchAction:
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPropertyWebkitScrollSnapType:
#endif
#if ENABLE(CSS_TRAILING_WORD)
    case CSSPropertyAppleTrailingWord:
#endif
        // These properties should be handled before in isValidKeywordPropertyAndValue().
        ASSERT_NOT_REACHED();
        return false;
#if ENABLE(CSS_DEVICE_ADAPTATION)
    // Properties bellow are validated inside parseViewportProperty, because we
    // check for parser state inViewportScope. We need to invalidate if someone
    // adds them outside a @viewport rule.
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
        validPrimitive = false;
        break;
#endif
#if ENABLE(CSS_SCROLL_SNAP)
    case CSSPropertyWebkitScrollSnapPointsX:
    case CSSPropertyWebkitScrollSnapPointsY:
        if (id == CSSValueElements) {
            validPrimitive = true;
            break;
        }
        return parseNonElementSnapPoints(propId, important);
    case CSSPropertyWebkitScrollSnapDestination: // <length>{2}
        return parseScrollSnapDestination(propId, important);
    case CSSPropertyWebkitScrollSnapCoordinate:
        return parseScrollSnapCoordinate(propId, important);
#endif

    default:
        return parseSVGValue(propId, important);
    }

    if (validPrimitive) {
        parsedValue = parseValidPrimitive(id, valueWithCalculation);
        m_valueList->next();
    }

    if (parsedValue) {
        if (!m_valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue.release(), important);
            return true;
        }
    }
    return false;
}

void CSSParser::addFillValue(RefPtr<CSSValue>& lval, Ref<CSSValue>&& rval)
{
    if (!lval) {
        lval = WTFMove(rval);
        return;
    }

    if (lval->isBaseValueList()) {
        downcast<CSSValueList>(*lval).append(WTFMove(rval));
        return;
    }

    auto list = CSSValueList::createCommaSeparated();
    list.get().append(lval.releaseNonNull());
    list.get().append(WTFMove(rval));
    lval = WTFMove(list);
}

static bool isContentDistributionKeyword(CSSValueID id)
{
    return id == CSSValueSpaceBetween || id == CSSValueSpaceAround
        || id == CSSValueSpaceEvenly || id == CSSValueStretch;
}

static bool isContentPositionKeyword(CSSValueID id)
{
    return id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueFlexStart || id == CSSValueFlexEnd
        || id == CSSValueLeft || id == CSSValueRight;
}

static inline bool isBaselinePositionKeyword(CSSValueID id)
{
    return id == CSSValueBaseline || id == CSSValueLastBaseline;
}

static bool isAlignmentOverflowKeyword(CSSValueID id)
{
    return id == CSSValueUnsafe || id == CSSValueSafe;
}

static bool isItemPositionKeyword(CSSValueID id)
{
    return id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueSelfStart || id == CSSValueSelfEnd || id == CSSValueFlexStart
        || id == CSSValueFlexEnd || id == CSSValueLeft || id == CSSValueRight;
}

bool CSSParser::parseLegacyPosition(CSSPropertyID propId, bool important)
{
    // [ legacy && [ left | right | center ]

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->id == CSSValueLegacy) {
        value = m_valueList->next();
        if (!value)
            return false;
        if (value->id != CSSValueCenter && value->id != CSSValueLeft && value->id != CSSValueRight)
            return false;
    } else if (value->id == CSSValueCenter || value->id == CSSValueLeft || value->id == CSSValueRight) {
        if (!m_valueList->next() || m_valueList->current()->id != CSSValueLegacy)
            return false;
    } else
        return false;

    auto& cssValuePool = CSSValuePool::singleton();
    addProperty(propId, createPrimitiveValuePair(cssValuePool.createIdentifierValue(CSSValueLegacy), cssValuePool.createIdentifierValue(value->id)), important);
    return !m_valueList->next();
}

RefPtr<CSSValue> CSSParser::parseContentDistributionOverflowPosition()
{
    // auto | <baseline-position> | <content-distribution> || [ <overflow-position>? && <content-position> ]
    // <baseline-position> = baseline | last-baseline;
    // <content-distribution> = space-between | space-around | space-evenly | stretch;
    // <content-position> = center | start | end | flex-start | flex-end | left | right;
    // <overflow-position> = true | safe

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return nullptr;

    // auto | <baseline-position>
    if (value->id == CSSValueAuto || isBaselinePositionKeyword(value->id)) {
        m_valueList->next();
        return CSSContentDistributionValue::create(CSSValueInvalid, value->id, CSSValueInvalid);
    }

    CSSValueID distribution = CSSValueInvalid;
    CSSValueID position = CSSValueInvalid;
    CSSValueID overflow = CSSValueInvalid;
    while (value) {
        if (isContentDistributionKeyword(value->id)) {
            if (distribution != CSSValueInvalid)
                return nullptr;
            distribution = value->id;
        } else if (isContentPositionKeyword(value->id)) {
            if (position != CSSValueInvalid)
                return nullptr;
            position = value->id;
        } else if (isAlignmentOverflowKeyword(value->id)) {
            if (overflow != CSSValueInvalid)
                return nullptr;
            overflow = value->id;
        } else
            return nullptr;
        value = m_valueList->next();
    }

    // The grammar states that we should have at least <content-distribution> or
    // <content-position> ( <content-distribution> || <content-position> ).
    if (position == CSSValueInvalid && distribution == CSSValueInvalid)
        return nullptr;

    // The grammar states that <overflow-position> must be associated to <content-position>.
    if (overflow != CSSValueInvalid && position == CSSValueInvalid)
        return nullptr;

    return CSSContentDistributionValue::create(distribution, position, overflow);
}

bool CSSParser::parseItemPositionOverflowPosition(CSSPropertyID propId, bool important)
{
    // auto | stretch | <baseline-position> | [<item-position> && <overflow-position>? ]
    // <baseline-position> = baseline | last-baseline;
    // <item-position> = center | start | end | self-start | self-end | flex-start | flex-end | left | right;
    // <overflow-position> = true | safe

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->id == CSSValueAuto || value->id == CSSValueStretch || isBaselinePositionKeyword(value->id)) {
        if (m_valueList->next())
            return false;

        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(value->id), important);
        return true;
    }

    RefPtr<CSSPrimitiveValue> position;
    RefPtr<CSSPrimitiveValue> overflowAlignmentKeyword;
    if (isItemPositionKeyword(value->id)) {
        position = CSSValuePool::singleton().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value) {
            if (value->id != CSSValueUnsafe && value->id != CSSValueSafe)
                return false;
            overflowAlignmentKeyword = CSSValuePool::singleton().createIdentifierValue(value->id);
        }
    } else if (isAlignmentOverflowKeyword(value->id)) {
        overflowAlignmentKeyword = CSSValuePool::singleton().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value && isItemPositionKeyword(value->id))
            position = CSSValuePool::singleton().createIdentifierValue(value->id);
        else
            return false;
    } else
        return false;

    if (m_valueList->next())
        return false;

    ASSERT(position);
    if (overflowAlignmentKeyword)
        addProperty(propId, createPrimitiveValuePair(position.release(), overflowAlignmentKeyword.release()), important);
    else
        addProperty(propId, position.release(), important);

    return true;
}

static bool parseBackgroundClip(CSSParserValue& parserValue, RefPtr<CSSValue>& cssValue)
{
    if (parserValue.id == CSSValueBorderBox || parserValue.id == CSSValuePaddingBox
        || parserValue.id == CSSValueContentBox || parserValue.id == CSSValueWebkitText) {
        cssValue = CSSValuePool::singleton().createIdentifierValue(parserValue.id);
        return true;
    }
    return false;
}

bool CSSParser::useLegacyBackgroundSizeShorthandBehavior() const
{
    return m_context.useLegacyBackgroundSizeShorthandBehavior;
}

#if ENABLE(CSS_SCROLL_SNAP)
bool CSSParser::parseNonElementSnapPoints(CSSPropertyID propId, bool important)
{
    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    while (CSSParserValue* value = m_valueList->current()) {
        ValueWithCalculation valueWithCalculation(*value);
        if (validateUnit(valueWithCalculation, FPercent | FLength))
            values->append(createPrimitiveNumericValue(valueWithCalculation));
        else if (value->unit == CSSParserValue::Function
            && value->function->args
            && value->function->args->size() == 1
            && equalLettersIgnoringASCIICase(value->function->name, "repeat(")) {
            ValueWithCalculation argumentWithCalculation(*value->function->args.get()->current());
            if (validateUnit(argumentWithCalculation, FLength | FPercent | FNonNeg)) {
                values->append(CSSValuePool::singleton().createValue(LengthRepeat::create(createPrimitiveNumericValue(argumentWithCalculation))));
                m_valueList->next();
                if (m_valueList->current())
                    return false;
                break;
            }
        } else
            return false;
        m_valueList->next();
    }
    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }
    return false;
}

bool CSSParser::parseScrollSnapPositions(RefPtr<CSSValue>& cssValueX, RefPtr<CSSValue>& cssValueY)
{
    cssValueX = parsePositionX(*m_valueList);
    if (!cssValueX)
        return false;

    // Don't accept odd-length lists of positions (must always have an X and a Y):
    if (!m_valueList->next())
        return false;

    cssValueY = parsePositionY(*m_valueList);
    if (!cssValueY)
        return false;

    return true;
}

bool CSSParser::parseScrollSnapDestination(CSSPropertyID propId, bool important)
{
    RefPtr<CSSValueList> position = CSSValueList::createSpaceSeparated();
    if (m_valueList->size() != 2)
        return false;

    RefPtr<CSSValue> cssValueX, cssValueY;
    if (!parseScrollSnapPositions(cssValueX, cssValueY))
        return false;

    position->append(cssValueX.releaseNonNull());
    position->append(cssValueY.releaseNonNull());
    addProperty(propId, position, important);
    m_valueList->next();
    return true;
}

bool CSSParser::parseScrollSnapCoordinate(CSSPropertyID propId, bool important)
{
    RefPtr<CSSValueList> positions = CSSValueList::createSpaceSeparated();
    while (m_valueList->current()) {
        RefPtr<CSSValue> cssValueX, cssValueY;
        if (!parseScrollSnapPositions(cssValueX, cssValueY))
            return false;

        positions->append(cssValueX.releaseNonNull());
        positions->append(cssValueY.releaseNonNull());
        m_valueList->next();
    }

    if (positions->length()) {
        addProperty(propId, positions.release(), important);
        return true;
    }
    return false;
}
#endif

const int cMaxFillProperties = 9;

bool CSSParser::parseFillShorthand(CSSPropertyID propId, const CSSPropertyID* properties, int numProperties, bool important)
{
    ASSERT(numProperties <= cMaxFillProperties);
    if (numProperties > cMaxFillProperties)
        return false;

    ShorthandScope scope(this, propId);

    bool parsedProperty[cMaxFillProperties] = { false };
    RefPtr<CSSValue> values[cMaxFillProperties];
    RefPtr<CSSValue> clipValue;
    RefPtr<CSSValue> positionYValue;
    RefPtr<CSSValue> repeatYValue;
    bool foundClip = false;
    int i;
    bool foundPositionCSSProperty = false;

    auto& cssValuePool = CSSValuePool::singleton();
    while (m_valueList->current()) {
        CSSParserValue& currentValue = *m_valueList->current();
        if (currentValue.unit == CSSParserValue::Operator && currentValue.iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSSPropertyBackgroundColor && parsedProperty[i])
                    // Color is not allowed except as the last item in a list for backgrounds.
                    // Reject the entire property.
                    return false;

                if (!parsedProperty[i] && properties[i] != CSSPropertyBackgroundColor) {
                    addFillValue(values[i], cssValuePool.createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, cssValuePool.createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, cssValuePool.createImplicitInitialValue());
                    if ((properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) && !parsedProperty[i]) {
                        // If background-origin wasn't present, then reset background-clip also.
                        addFillValue(clipValue, cssValuePool.createImplicitInitialValue());
                    }
                }
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }

        bool sizeCSSPropertyExpected = false;
        if (isForwardSlashOperator(currentValue) && foundPositionCSSProperty) {
            sizeCSSPropertyExpected = true;
            m_valueList->next();
        }

        foundPositionCSSProperty = false;
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {

            if (sizeCSSPropertyExpected && (properties[i] != CSSPropertyBackgroundSize && properties[i] != CSSPropertyWebkitMaskSize))
                continue;
            if (!sizeCSSPropertyExpected && (properties[i] == CSSPropertyBackgroundSize || properties[i] == CSSPropertyWebkitMaskSize))
                continue;

            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val1;
                RefPtr<CSSValue> val2;
                CSSPropertyID propId1, propId2;
                CSSParserValue& parserValue = *m_valueList->current();

                if (parseFillProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addFillValue(values[i], val1.releaseNonNull());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, val2.releaseNonNull());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, val2.releaseNonNull());
                    if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                        // Reparse the value as a clip, and see if we succeed.
                        if (parseBackgroundClip(parserValue, val1))
                            addFillValue(clipValue, val1.releaseNonNull()); // The property parsed successfully.
                        else
                            addFillValue(clipValue, cssValuePool.createImplicitInitialValue()); // Some value was used for origin that is not supported by clip. Just reset clip instead.
                    }
                    if (properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip)
                        foundClip = true;
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        foundPositionCSSProperty = true;
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; ++i) {
        // Fill in any remaining properties with the initial value.
        if (!parsedProperty[i]) {
            addFillValue(values[i], cssValuePool.createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                addFillValue(positionYValue, cssValuePool.createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                addFillValue(repeatYValue, cssValuePool.createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                // If background-origin wasn't present, then reset background-clip also.
                addFillValue(clipValue, cssValuePool.createImplicitInitialValue());
            }
        }
        if (properties[i] == CSSPropertyBackgroundPosition) {
            addProperty(CSSPropertyBackgroundPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskPosition) {
            addProperty(CSSPropertyWebkitMaskPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyWebkitMaskPosition once
            addProperty(CSSPropertyWebkitMaskPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyBackgroundRepeat) {
            addProperty(CSSPropertyBackgroundRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundRepeatY, repeatYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskRepeat) {
            addProperty(CSSPropertyWebkitMaskRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyWebkitMaskRepeatY, repeatYValue.release(), important);
        } else if ((properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip) && !foundClip)
            // Value is already set while updating origin
            continue;
        else if (properties[i] == CSSPropertyBackgroundSize && !parsedProperty[i] && useLegacyBackgroundSizeShorthandBehavior())
            continue;
        else
            addProperty(properties[i], values[i].release(), important);

        // Add in clip values when we hit the corresponding origin property.
        if (properties[i] == CSSPropertyBackgroundOrigin && !foundClip)
            addProperty(CSSPropertyBackgroundClip, clipValue.release(), important);
        else if (properties[i] == CSSPropertyWebkitMaskOrigin && !foundClip)
            addProperty(CSSPropertyWebkitMaskClip, clipValue.release(), important);
    }

    return true;
}

void CSSParser::addAnimationValue(RefPtr<CSSValue>& lval, Ref<CSSValue>&& rval)
{
    if (!lval) {
        lval = WTFMove(rval);
        return;
    }

    if (is<CSSValueList>(*lval)) {
        downcast<CSSValueList>(*lval).append(WTFMove(rval));
        return;
    }

    auto list = CSSValueList::createCommaSeparated();
    list.get().append(lval.releaseNonNull());
    list.get().append(WTFMove(rval));
    lval = WTFMove(list);
}

bool CSSParser::parseAnimationShorthand(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertyAnimation || propId == CSSPropertyWebkitAnimation);

    const unsigned numProperties = 7;
    const StylePropertyShorthand& shorthand = animationShorthandForParsing(propId);

    // The list of properties in the shorthand should be the same
    // length as the list with animation name in last position, even though they are
    // in a different order.
    ASSERT(numProperties == shorthand.length());
    ASSERT(numProperties == animationShorthand().length());
    ASSERT(numProperties == webkitAnimationShorthand().length());

    ShorthandScope scope(this, propId);

    bool parsedProperty[numProperties] = { false };
    AnimationParseContext context;
    RefPtr<CSSValue> values[numProperties];

    auto& cssValuePool = CSSValuePool::singleton();
    unsigned i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], cssValuePool.createImplicitInitialValue());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
            context.commitFirstAnimation();
        }

        bool found = false;
        for (i = 0; i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val;
                if (parseAnimationProperty(shorthand.properties()[i], val, context)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.releaseNonNull());
                    break;
                }
            }

            // There are more values to process but 'none' or 'all' were already defined as the animation property, the declaration becomes invalid.
            if (!context.animationPropertyKeywordAllowed() && context.hasCommittedFirstAnimation())
                return false;
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    for (i = 0; i < numProperties; ++i) {
        // If we didn't find the property, set an intial value.
        if (!parsedProperty[i])
            addAnimationValue(values[i], cssValuePool.createImplicitInitialValue());

        addProperty(shorthand.properties()[i], values[i].release(), important);
    }

    return true;
}

RefPtr<CSSValue> CSSParser::parseColumnWidth()
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    CSSValueID id = valueWithCalculation.value().id;
    // Always parse this property in strict mode, since it would be ambiguous otherwise when used in the 'columns' shorthand property.
    if (id != CSSValueAuto && !(validateUnit(valueWithCalculation, FLength | FNonNeg, CSSStrictMode) && parsedDouble(valueWithCalculation)))
        return nullptr;

    RefPtr<CSSValue> parsedValue = parseValidPrimitive(id, valueWithCalculation);
    m_valueList->next();
    return parsedValue;
}

RefPtr<CSSValue> CSSParser::parseColumnCount()
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    CSSValueID id = valueWithCalculation.value().id;

    if (id != CSSValueAuto && !validateUnit(valueWithCalculation, FPositiveInteger, CSSQuirksMode))
        return nullptr;

    RefPtr<CSSValue> parsedValue = parseValidPrimitive(id, valueWithCalculation);
    m_valueList->next();
    return parsedValue;
}

bool CSSParser::parseColumnsShorthand(bool important)
{
    RefPtr<CSSValue> columnWidth;
    RefPtr<CSSValue> columnCount;
    bool hasPendingExplicitAuto = false;

    for (unsigned propertiesParsed = 0; CSSParserValue* value = m_valueList->current(); propertiesParsed++) {
        if (propertiesParsed >= 2)
            return false; // Too many values for this shorthand. Invalid declaration.
        if (!propertiesParsed && value->id == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point
            // we don't know which one(s) it is meant for. We need to see if there are other values first.
            m_valueList->next();
            hasPendingExplicitAuto = true;
        } else {
            if (!columnWidth) {
                if ((columnWidth = parseColumnWidth()))
                    continue;
            }
            if (!columnCount) {
                if ((columnCount = parseColumnCount()))
                    continue;
            }
            // If we didn't find at least one match, this is an invalid shorthand and we have to ignore it.
            return false;
        }
    }

    // Any unassigned property at this point will become implicit 'auto'.
    if (columnWidth)
        addProperty(CSSPropertyColumnWidth, columnWidth, important);
    else {
        addProperty(CSSPropertyColumnWidth, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important, !hasPendingExplicitAuto /* implicit */);
        hasPendingExplicitAuto = false;
    }

    if (columnCount)
        addProperty(CSSPropertyColumnCount, columnCount, important);
    else
        addProperty(CSSPropertyColumnCount, CSSValuePool::singleton().createIdentifierValue(CSSValueAuto), important, !hasPendingExplicitAuto /* implicit */);

    return true;
}

bool CSSParser::parseTransitionShorthand(CSSPropertyID propId, bool important)
{
    const unsigned numProperties = 4;
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    ASSERT(numProperties == shorthand.length());

    ShorthandScope scope(this, propId);

    bool parsedProperty[numProperties] = { false };
    AnimationParseContext context;
    RefPtr<CSSValue> values[numProperties];

    auto& cssValuePool = CSSValuePool::singleton();
    unsigned i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end. Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], cssValuePool.createImplicitInitialValue());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
            context.commitFirstAnimation();
        }

        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val;
                if (parseAnimationProperty(shorthand.properties()[i], val, context)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.releaseNonNull());
                }

                // There are more values to process but 'none' or 'all' were already defined as the animation property, the declaration becomes invalid.
                if (!context.animationPropertyKeywordAllowed() && context.hasCommittedFirstAnimation())
                    return false;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i])
            addAnimationValue(values[i], cssValuePool.createImplicitInitialValue());
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; ++i)
        addPropertyWithPrefixingVariant(shorthand.properties()[i], values[i].release(), important);

    return true;
}

bool CSSParser::parseShorthand(CSSPropertyID propId, const StylePropertyShorthand& shorthand, bool important)
{
    // We try to match as many properties as possible
    // We set up an array of booleans to mark which property has been found,
    // and we try to search for properties until it makes no longer any sense.
    ShorthandScope scope(this, propId);

    bool found = false;
    unsigned propertiesParsed = 0;
    bool propertyFound[6]= { false, false, false, false, false, false }; // 6 is enough size.

    while (m_valueList->current()) {
        found = false;
        for (unsigned propIndex = 0; !found && propIndex < shorthand.length(); ++propIndex) {
            if (!propertyFound[propIndex] && parseValue(shorthand.properties()[propIndex], important)) {
                    propertyFound[propIndex] = found = true;
                    propertiesParsed++;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    if (propertiesParsed == shorthand.length())
        return true;

    // Fill in any remaining properties with the initial value.
    auto& cssValuePool = CSSValuePool::singleton();
    ImplicitScope implicitScope(*this, PropertyImplicit);
    const StylePropertyShorthand* propertiesForInitialization = shorthand.propertiesForInitialization();
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (propertyFound[i])
            continue;

        if (propertiesForInitialization) {
            const StylePropertyShorthand& initProperties = propertiesForInitialization[i];
            for (unsigned propIndex = 0; propIndex < initProperties.length(); ++propIndex)
                addProperty(initProperties.properties()[propIndex], cssValuePool.createImplicitInitialValue(), important);
        } else
            addProperty(shorthand.properties()[i], cssValuePool.createImplicitInitialValue(), important);
    }

    return true;
}

bool CSSParser::parse4Values(CSSPropertyID propId, const CSSPropertyID *properties,  bool important)
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    unsigned num = inShorthand() ? 1 : m_valueList->size();

    ShorthandScope scope(this, propId);

    // the order is top, right, bottom, left
    switch (num) {
        case 1: {
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            ImplicitScope implicitScope(*this, PropertyImplicit);
            addProperty(properties[1], value, important);
            addProperty(properties[2], value, important);
            addProperty(properties[3], value, important);
            break;
        }
        case 2: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            CSSValue* value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            ImplicitScope implicitScope(*this, PropertyImplicit);
            addProperty(properties[2], value, important);
            value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            addProperty(properties[3], value, important);
            break;
        }
        case 3: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) || !parseValue(properties[2], important))
                return false;
            CSSValue* value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            ImplicitScope implicitScope(*this, PropertyImplicit);
            addProperty(properties[3], value, important);
            break;
        }
        case 4: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) ||
                !parseValue(properties[2], important) || !parseValue(properties[3], important))
                return false;
            break;
        }
        default: {
            return false;
        }
    }

    return true;
}

// auto | <identifier>
bool CSSParser::parsePage(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertyPage);

    if (m_valueList->size() != 1)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->id == CSSValueAuto) {
        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(value->id), important);
        return true;
    } else if (value->id == 0 && value->unit == CSSPrimitiveValue::CSS_IDENT) {
        addProperty(propId, createPrimitiveStringValue(*value), important);
        return true;
    }
    return false;
}

// <length>{1,2} | auto | [ <page-size> || [ portrait | landscape] ]
bool CSSParser::parseSize(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertySize);

    if (m_valueList->size() > 2)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();

    // First parameter.
    SizeParameterType paramType = parseSizeParameter(*parsedValues, *value, None);
    if (paramType == None)
        return false;

    // Second parameter, if any.
    value = m_valueList->next();
    if (value) {
        paramType = parseSizeParameter(*parsedValues, *value, paramType);
        if (paramType == None)
            return false;
    }

    addProperty(propId, parsedValues.release(), important);
    return true;
}

CSSParser::SizeParameterType CSSParser::parseSizeParameter(CSSValueList& parsedValues, CSSParserValue& value, SizeParameterType prevParamType)
{
    switch (value.id) {
    case CSSValueAuto:
        if (prevParamType == None) {
            parsedValues.append(CSSValuePool::singleton().createIdentifierValue(value.id));
            return Auto;
        }
        return None;
    case CSSValueLandscape:
    case CSSValuePortrait:
        if (prevParamType == None || prevParamType == PageSize) {
            parsedValues.append(CSSValuePool::singleton().createIdentifierValue(value.id));
            return Orientation;
        }
        return None;
    case CSSValueA3:
    case CSSValueA4:
    case CSSValueA5:
    case CSSValueB4:
    case CSSValueB5:
    case CSSValueLedger:
    case CSSValueLegal:
    case CSSValueLetter:
        if (prevParamType == None || prevParamType == Orientation) {
            // Normalize to Page Size then Orientation order by prepending.
            // This is not specified by the CSS3 Paged Media specification, but for simpler processing later (StyleResolver::applyPageSizeProperty).
            parsedValues.prepend(CSSValuePool::singleton().createIdentifierValue(value.id));
            return PageSize;
        }
        return None;
    case CSSValueInvalid: {
        ValueWithCalculation valueWithCalculation(value);
        if (validateUnit(valueWithCalculation, FLength | FNonNeg) && (prevParamType == None || prevParamType == Length)) {
            parsedValues.append(createPrimitiveNumericValue(valueWithCalculation));
            return Length;
        }
        return None;
    }
    default:
        return None;
    }
}

// [ <string> <string> ]+ | inherit | none
// inherit and none are handled in parseValue.
bool CSSParser::parseQuotes(CSSPropertyID propId, bool important)
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    while (CSSParserValue* value = m_valueList->current()) {
        if (value->unit != CSSPrimitiveValue::CSS_STRING)
            break;
        values->append(CSSPrimitiveValue::create(value->string, CSSPrimitiveValue::CSS_STRING));
        m_valueList->next();
    }
    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }
    return false;
}

bool CSSParser::parseAlt(CSSPropertyID propID, bool important)
{
    CSSParserValue& currentValue = *m_valueList->current();
    RefPtr<CSSValue> parsedValue;

    if (currentValue.unit == CSSPrimitiveValue::CSS_STRING)
        parsedValue = createPrimitiveStringValue(currentValue);
    else if (currentValue.unit == CSSParserValue::Function) {
        CSSParserValueList* args = currentValue.function->args.get();
        if (!args)
            return false;
        if (equalLettersIgnoringASCIICase(currentValue.function->name, "attr("))
            parsedValue = parseAttr(*args);
    }
    
    if (parsedValue) {
        addProperty(propID, parsedValue.release(), important);
        m_valueList->next();
        return true;
    }

    return false;
}

bool CSSParser::parseCustomPropertyDeclaration(bool important, CSSValueID id)
{
    if (m_customPropertyName.isEmpty() || !m_valueList)
        return false;
    
    auto& cssValuePool = CSSValuePool::singleton();
    RefPtr<CSSValue> value;
    if (id == CSSValueInherit)
        value = cssValuePool.createInheritedValue();
    else if (id == CSSValueInitial)
        value = cssValuePool.createExplicitInitialValue();
    else if (id == CSSValueUnset)
        value = cssValuePool.createUnsetValue();
    else if (id == CSSValueRevert)
        value = cssValuePool.createRevertValue();
    else {
        RefPtr<CSSValueList> valueList = CSSValueList::createFromParserValueList(*m_valueList);
        if (m_valueList->containsVariables())
            value = CSSVariableDependentValue::create(valueList, CSSPropertyCustom);
        else
            value = valueList;
    }

    addProperty(CSSPropertyCustom, CSSCustomPropertyValue::create(m_customPropertyName, value), important, false);
    return true;
}

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSParser::parseContent(CSSPropertyID propId, bool important)
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    while (CSSParserValue* value = m_valueList->current()) {
        RefPtr<CSSValue> parsedValue;
        if (value->unit == CSSPrimitiveValue::CSS_URI) {
            // url
            parsedValue = CSSImageValue::create(completeURL(value->string));
        } else if (value->unit == CSSParserValue::Function) {
            // attr(X) | counter(X [,Y]) | counters(X, Y, [,Z]) | -webkit-gradient(...)
            CSSParserValueList* args = value->function->args.get();
            if (!args)
                return false;
            if (equalLettersIgnoringASCIICase(value->function->name, "attr(")) {
                parsedValue = parseAttr(*args);
                if (!parsedValue)
                    return false;
            } else if (equalLettersIgnoringASCIICase(value->function->name, "counter(")) {
                parsedValue = parseCounterContent(*args, false);
                if (!parsedValue)
                    return false;
            } else if (equalLettersIgnoringASCIICase(value->function->name, "counters(")) {
                parsedValue = parseCounterContent(*args, true);
                if (!parsedValue)
                    return false;
#if ENABLE(CSS_IMAGE_SET)
            } else if (equalLettersIgnoringASCIICase(value->function->name, "-webkit-image-set(")) {
                parsedValue = parseImageSet();
                if (!parsedValue)
                    return false;
#endif
            } else if (isGeneratedImageValue(*value)) {
                if (!parseGeneratedImage(*m_valueList, parsedValue))
                    return false;
            } else
                return false;
        } else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            // open-quote
            // close-quote
            // no-open-quote
            // no-close-quote
            // inherit
            // FIXME: These are not yet implemented (http://bugs.webkit.org/show_bug.cgi?id=6503).
            // none
            // normal
            switch (value->id) {
            case CSSValueOpenQuote:
            case CSSValueCloseQuote:
            case CSSValueNoOpenQuote:
            case CSSValueNoCloseQuote:
            case CSSValueNone:
            case CSSValueNormal:
                parsedValue = CSSValuePool::singleton().createIdentifierValue(value->id);
                break;
            default:
                break;
            }
        } else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            parsedValue = createPrimitiveStringValue(*value);
        }
        if (!parsedValue)
            break;
        values->append(parsedValue.releaseNonNull());
        m_valueList->next();
    }

    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }

    return false;
}

RefPtr<CSSValue> CSSParser::parseAttr(CSSParserValueList& args)
{
    if (args.size() != 1)
        return nullptr;

    CSSParserValue& argument = *args.current();

    if (argument.unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;

    ASSERT(argument.string.length());

    // CSS allows identifiers with "-" at the start, like "-webkit-mask-image".
    // But HTML attribute names can't have those characters, and we should not
    // even parse them inside attr().
    if (argument.string[0] == '-')
        return nullptr;

    if (m_context.isHTMLDocument)
        argument.string.convertToASCIILowercaseInPlace();

    // FIXME: Is there some small benefit to creating an AtomicString here instead of a String?
    return CSSValuePool::singleton().createValue(String(argument.string), CSSPrimitiveValue::CSS_ATTR);
}

RefPtr<CSSValue> CSSParser::parseBackgroundColor()
{
    CSSValueID id = m_valueList->current()->id;
    if (id == CSSValueWebkitText || isValidSystemColorValue(id) || id == CSSValueMenu || id == CSSValueCurrentcolor
        || (id >= CSSValueGrey && id < CSSValueWebkitText && inQuirksMode()))
        return CSSValuePool::singleton().createIdentifierValue(id);
    return parseColor();
}

bool CSSParser::parseFillImage(CSSParserValueList& valueList, RefPtr<CSSValue>& value)
{
    if (valueList.current()->id == CSSValueNone) {
        value = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        return true;
    }
    if (valueList.current()->unit == CSSPrimitiveValue::CSS_URI) {
        value = CSSImageValue::create(completeURL(valueList.current()->string));
        return true;
    }

    if (isGeneratedImageValue(*valueList.current()))
        return parseGeneratedImage(valueList, value);
    
#if ENABLE(CSS_IMAGE_SET)
    if (valueList.current()->unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(valueList.current()->function->name, "-webkit-image-set(")) {
        value = parseImageSet();
        if (value)
            return true;
    }
#endif

    return false;
}

RefPtr<CSSPrimitiveValue> CSSParser::parsePositionX(CSSParserValueList& valueList)
{
    int id = valueList.current()->id;
    if (id == CSSValueLeft || id == CSSValueRight || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueRight)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return CSSValuePool::singleton().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    ValueWithCalculation valueWithCalculation(*valueList.current());
    if (validateUnit(valueWithCalculation, FPercent | FLength))
        return createPrimitiveNumericValue(valueWithCalculation);
    return nullptr;
}

RefPtr<CSSPrimitiveValue> CSSParser::parsePositionY(CSSParserValueList& valueList)
{
    int id = valueList.current()->id;
    if (id == CSSValueTop || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueBottom)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return CSSValuePool::singleton().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    ValueWithCalculation valueWithCalculation(*valueList.current());
    if (validateUnit(valueWithCalculation, FPercent | FLength))
        return createPrimitiveNumericValue(valueWithCalculation);
    return nullptr;
}

RefPtr<CSSPrimitiveValue> CSSParser::parseFillPositionComponent(CSSParserValueList& valueList, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode parsingMode)
{
    CSSValueID id = valueList.current()->id;
    if (id == CSSValueLeft || id == CSSValueTop || id == CSSValueRight || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueLeft || id == CSSValueRight) {
            if (cumulativeFlags & XFillPosition)
                return nullptr;
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
            if (id == CSSValueRight)
                percent = 100;
        }
        else if (id == CSSValueTop || id == CSSValueBottom) {
            if (cumulativeFlags & YFillPosition)
                return nullptr;
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
            if (id == CSSValueBottom)
                percent = 100;
        } else if (id == CSSValueCenter) {
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
            cumulativeFlags |= AmbiguousFillPosition;
            individualFlag = AmbiguousFillPosition;
        }

        if (parsingMode == ResolveValuesAsKeyword)
            return CSSValuePool::singleton().createIdentifierValue(id);

        return CSSValuePool::singleton().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    ValueWithCalculation valueWithCalculation(*valueList.current());
    if (validateUnit(valueWithCalculation, FPercent | FLength)) {
        if (!cumulativeFlags) {
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
        } else if (cumulativeFlags & (XFillPosition | AmbiguousFillPosition)) {
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
        } else
            return nullptr;
        return createPrimitiveNumericValue(valueWithCalculation);
    }
    return nullptr;
}

static bool isValueConflictingWithCurrentEdge(int value1, int value2)
{
    if ((value1 == CSSValueLeft || value1 == CSSValueRight) && (value2 == CSSValueLeft || value2 == CSSValueRight))
        return true;

    if ((value1 == CSSValueTop || value1 == CSSValueBottom) && (value2 == CSSValueTop || value2 == CSSValueBottom))
        return true;

    return false;
}

static bool isFillPositionKeyword(CSSValueID value)
{
    return value == CSSValueLeft || value == CSSValueTop || value == CSSValueBottom || value == CSSValueRight || value == CSSValueCenter;
}

void CSSParser::parse4ValuesFillPosition(CSSParserValueList& valueList, RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2, RefPtr<CSSPrimitiveValue>&& parsedValue1, RefPtr<CSSPrimitiveValue>&& parsedValue2)
{
    // [ left | right ] [ <percentage] | <length> ] && [ top | bottom ] [ <percentage> | <length> ]
    // In the case of 4 values <position> requires the second value to be a length or a percentage.
    if (isFillPositionKeyword(parsedValue2->getValueID()))
        return;

    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);
    if (!value3)
        return;

    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident3 = value3->getValueID();

    if (ident1 == CSSValueCenter)
        return;

    if (!isFillPositionKeyword(ident3) || ident3 == CSSValueCenter)
        return;

    // We need to check if the values are not conflicting, e.g. they are not on the same edge. It is
    // needed as the second call to parseFillPositionComponent was on purpose not checking it. In the
    // case of two values top 20px is invalid but in the case of 4 values it becomes valid.
    if (isValueConflictingWithCurrentEdge(ident1, ident3))
        return;

    valueList.next();

    cumulativeFlags = 0;
    FillPositionFlag value4Flag = InvalidFillPosition;
    RefPtr<CSSPrimitiveValue> value4 = parseFillPositionComponent(valueList, cumulativeFlags, value4Flag, ResolveValuesAsKeyword);
    if (!value4)
        return;

    // 4th value must be a length or a percentage.
    if (isFillPositionKeyword(value4->getValueID()))
        return;

    value1 = createPrimitiveValuePair(WTFMove(parsedValue1), WTFMove(parsedValue2));
    value2 = createPrimitiveValuePair(value3.copyRef(), value4.copyRef());

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom)
        value1.swap(value2);

    valueList.next();
}
void CSSParser::parse3ValuesFillPosition(CSSParserValueList& valueList, RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2, RefPtr<CSSPrimitiveValue>&& parsedValue1, RefPtr<CSSPrimitiveValue>&& parsedValue2)
{
    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);

    // value3 is not an expected value, we return.
    if (!value3)
        return;

    valueList.next();

    bool swapNeeded = false;
    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident2 = parsedValue2->getValueID();
    CSSValueID ident3 = value3->getValueID();

    CSSValueID firstPositionKeyword;
    CSSValueID secondPositionKeyword;

    auto& cssValuePool = CSSValuePool::singleton();
    if (ident1 == CSSValueCenter) {
        // <position> requires the first 'center' to be followed by a keyword.
        if (!isFillPositionKeyword(ident2))
            return;

        // If 'center' is the first keyword then the last one needs to be a length.
        if (isFillPositionKeyword(ident3))
            return;

        firstPositionKeyword = CSSValueLeft;
        if (ident2 == CSSValueLeft || ident2 == CSSValueRight) {
            firstPositionKeyword = CSSValueTop;
            swapNeeded = true;
        }
        value1 = createPrimitiveValuePair(cssValuePool.createIdentifierValue(firstPositionKeyword), cssValuePool.createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE));
        value2 = createPrimitiveValuePair(WTFMove(parsedValue2), value3.copyRef());
    } else if (ident3 == CSSValueCenter) {
        if (isFillPositionKeyword(ident2))
            return;

        secondPositionKeyword = CSSValueTop;
        if (ident1 == CSSValueTop || ident1 == CSSValueBottom) {
            secondPositionKeyword = CSSValueLeft;
            swapNeeded = true;
        }
        value1 = createPrimitiveValuePair(WTFMove(parsedValue1), parsedValue2.copyRef());
        value2 = createPrimitiveValuePair(cssValuePool.createIdentifierValue(secondPositionKeyword), cssValuePool.createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE));
    } else {
        RefPtr<CSSPrimitiveValue> firstPositionValue;
        RefPtr<CSSPrimitiveValue> secondPositionValue;

        if (isFillPositionKeyword(ident2)) {
            // To match CSS grammar, we should only accept: [ center | left | right | bottom | top ] [ left | right | top | bottom ] [ <percentage> | <length> ].
            ASSERT(ident2 != CSSValueCenter);

            if (isFillPositionKeyword(ident3))
                return;

            secondPositionValue = value3;
            secondPositionKeyword = ident2;
            firstPositionValue = cssValuePool.createValue(0, CSSPrimitiveValue::CSS_PERCENTAGE);
        } else {
            // Per CSS, we should only accept: [ right | left | top | bottom ] [ <percentage> | <length> ] [ center | left | right | bottom | top ].
            if (!isFillPositionKeyword(ident3))
                return;

            firstPositionValue = parsedValue2;
            secondPositionKeyword = ident3;
            secondPositionValue = cssValuePool.createValue(0, CSSPrimitiveValue::CSS_PERCENTAGE);
        }

        if (isValueConflictingWithCurrentEdge(ident1, secondPositionKeyword))
            return;

        value1 = createPrimitiveValuePair(WTFMove(parsedValue1), firstPositionValue.copyRef());
        value2 = createPrimitiveValuePair(cssValuePool.createIdentifierValue(secondPositionKeyword), secondPositionValue.copyRef());
    }

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom || swapNeeded)
        value1.swap(value2);

#ifndef NDEBUG
    CSSPrimitiveValue& first = *value1;
    CSSPrimitiveValue& second = *value2;
    ident1 = first.getPairValue()->first()->getValueID();
    ident2 = second.getPairValue()->first()->getValueID();
    ASSERT(ident1 == CSSValueLeft || ident1 == CSSValueRight);
    ASSERT(ident2 == CSSValueBottom || ident2 == CSSValueTop);
#endif
}

inline bool CSSParser::isPotentialPositionValue(CSSParserValue& value)
{
    if (isFillPositionKeyword(value.id))
        return true;
    ValueWithCalculation valueWithCalculation(value);
    return validateUnit(valueWithCalculation, FPercent | FLength);
}

void CSSParser::parseFillPosition(CSSParserValueList& valueList, RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2)
{
    unsigned numberOfValues = 0;
    for (unsigned i = valueList.currentIndex(); i < valueList.size(); ++i, ++numberOfValues) {
        CSSParserValue* current = valueList.valueAt(i);
        if (!current || isComma(current) || isForwardSlashOperator(*current) || !isPotentialPositionValue(*current))
            break;
    }

    if (numberOfValues > 4)
        return;

    // If we are parsing two values, we can safely call the CSS 2.1 parsing function and return.
    if (numberOfValues <= 2) {
        parse2ValuesFillPosition(valueList, value1, value2);
        return;
    }

    ASSERT(numberOfValues > 2 && numberOfValues <= 4);

    CSSParserValue* value = valueList.current();

    // <position> requires the first value to be a background keyword.
    if (!isFillPositionKeyword(value->id))
        return;

    // Parse the first value. We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag, ResolveValuesAsKeyword);
    if (!value1)
        return;

    value = valueList.next();

    // In case we are parsing more than two values, relax the check inside of parseFillPositionComponent. top 20px is
    // a valid start for <position>.
    cumulativeFlags = AmbiguousFillPosition;
    value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag, ResolveValuesAsKeyword);
    if (value2)
        valueList.next();
    else {
        value1 = nullptr;
        return;
    }

    RefPtr<CSSPrimitiveValue> parsedValue1 = value1;
    RefPtr<CSSPrimitiveValue> parsedValue2 = value2;

    value1 = nullptr;
    value2 = nullptr;

    // Per CSS3 syntax, <position> can't have 'center' as its second keyword as we have more arguments to follow.
    if (parsedValue2->getValueID() == CSSValueCenter)
        return;

    if (numberOfValues == 3)
        parse3ValuesFillPosition(valueList, value1, value2, WTFMove(parsedValue1), WTFMove(parsedValue2));
    else
        parse4ValuesFillPosition(valueList, value1, value2, WTFMove(parsedValue1), WTFMove(parsedValue2));
}

void CSSParser::parse2ValuesFillPosition(CSSParserValueList& valueList, RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2)
{
    CSSParserValue* value = valueList.current();

    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag);
    if (!value1)
        return;

    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    value = valueList.next();

    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (isComma(value))
        value = nullptr;

    if (value) {
        value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag);
        if (value2)
            valueList.next();
        else {
            if (!inShorthand()) {
                value1 = nullptr;
                return;
            }
        }
    }

    if (!value2)
        // Only one value was specified. If that value was not a keyword, then it sets the x position, and the y position
        // is simply 50%. This is our default.
        // For keywords, the keyword was either an x-keyword (left/right), a y-keyword (top/bottom), or an ambiguous keyword (center).
        // For left/right/center, the default of 50% in the y is still correct.
        value2 = CSSValuePool::singleton().createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    if (value1Flag == YFillPosition || value2Flag == XFillPosition)
        value1.swap(value2);
}

void CSSParser::parseFillRepeat(RefPtr<CSSValue>& value1, RefPtr<CSSValue>& value2)
{
    CSSValueID id = m_valueList->current()->id;
    if (id == CSSValueRepeatX) {
        m_implicitShorthand = true;
        value1 = CSSValuePool::singleton().createIdentifierValue(CSSValueRepeat);
        value2 = CSSValuePool::singleton().createIdentifierValue(CSSValueNoRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeatY) {
        m_implicitShorthand = true;
        value1 = CSSValuePool::singleton().createIdentifierValue(CSSValueNoRepeat);
        value2 = CSSValuePool::singleton().createIdentifierValue(CSSValueRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace)
        value1 = CSSValuePool::singleton().createIdentifierValue(id);
    else {
        value1 = nullptr;
        return;
    }

    CSSParserValue* value = m_valueList->next();

    // Parse the second value if one is available
    if (value && !isComma(value)) {
        id = value->id;
        if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace) {
            value2 = CSSValuePool::singleton().createIdentifierValue(id);
            m_valueList->next();
            return;
        }
    }

    // If only one value was specified, value2 is the same as value1.
    m_implicitShorthand = true;
    value2 = CSSValuePool::singleton().createIdentifierValue(downcast<CSSPrimitiveValue>(*value1).getValueID());
}

RefPtr<CSSValue> CSSParser::parseFillSize(CSSPropertyID propId, bool& allowComma)
{
    allowComma = true;
    CSSParserValue* value = m_valueList->current();

    if (value->id == CSSValueContain || value->id == CSSValueCover)
        return CSSValuePool::singleton().createIdentifierValue(value->id);

    RefPtr<CSSPrimitiveValue> parsedValue1;

    if (value->id == CSSValueAuto)
        parsedValue1 = CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    else {
        ValueWithCalculation valueWithCalculation(*value);
        if (!validateUnit(valueWithCalculation, FLength | FPercent))
            return nullptr;
        parsedValue1 = createPrimitiveNumericValue(valueWithCalculation);
    }

    RefPtr<CSSPrimitiveValue> parsedValue2;
    if ((value = m_valueList->next())) {
        if (value->unit == CSSParserValue::Operator && value->iValue == ',')
            allowComma = false;
        else if (value->id != CSSValueAuto) {
            ValueWithCalculation valueWithCalculation(*value);
            if (!validateUnit(valueWithCalculation, FLength | FPercent)) {
                if (!inShorthand())
                    return nullptr;
                // We need to rewind the value list, so that when it is advanced we'll end up back at this value.
                m_valueList->previous();
            } else
                parsedValue2 = createPrimitiveNumericValue(valueWithCalculation);
        }
    } else if (!parsedValue2 && propId == CSSPropertyWebkitBackgroundSize) {
        // For backwards compatibility we set the second value to the first if it is omitted.
        // We only need to do this for -webkit-background-size. It should be safe to let masks match
        // the real property.
        parsedValue2 = parsedValue1;
    }

    if (!parsedValue2)
        return parsedValue1;
    return createPrimitiveValuePair(WTFMove(parsedValue1), WTFMove(parsedValue2), propId == CSSPropertyWebkitBackgroundSize ? Pair::IdenticalValueEncoding::Coalesce : Pair::IdenticalValueEncoding::DoNotCoalesce);
}

bool CSSParser::parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,
                                  RefPtr<CSSValue>& retValue1, RefPtr<CSSValue>& retValue2)
{
    RefPtr<CSSValueList> values;
    RefPtr<CSSValueList> values2;
    CSSParserValue* currentValue;
    RefPtr<CSSValue> value;
    RefPtr<CSSValue> value2;

    bool allowComma = false;

    retValue1 = retValue2 = nullptr;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyBackgroundPosition) {
        propId1 = CSSPropertyBackgroundPositionX;
        propId2 = CSSPropertyBackgroundPositionY;
    } else if (propId == CSSPropertyWebkitMaskPosition) {
        propId1 = CSSPropertyWebkitMaskPositionX;
        propId2 = CSSPropertyWebkitMaskPositionY;
    } else if (propId == CSSPropertyBackgroundRepeat) {
        propId1 = CSSPropertyBackgroundRepeatX;
        propId2 = CSSPropertyBackgroundRepeatY;
    } else if (propId == CSSPropertyWebkitMaskRepeat) {
        propId1 = CSSPropertyWebkitMaskRepeatX;
        propId2 = CSSPropertyWebkitMaskRepeatY;
    }

    while ((currentValue = m_valueList->current())) {
        RefPtr<CSSValue> currValue;
        RefPtr<CSSValue> currValue2;

        if (allowComma) {
            if (!isComma(currentValue))
                return false;
            m_valueList->next();
            allowComma = false;
        } else {
            allowComma = true;
            switch (propId) {
                case CSSPropertyBackgroundColor:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyBackgroundAttachment:
                    if (currentValue->id == CSSValueScroll || currentValue->id == CSSValueFixed || currentValue->id == CSSValueLocal) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundImage:
                case CSSPropertyWebkitMaskImage:
                    if (parseFillImage(*m_valueList, currValue))
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitBackgroundClip:
                case CSSPropertyWebkitBackgroundOrigin:
                case CSSPropertyWebkitMaskClip:
                case CSSPropertyWebkitMaskOrigin:
                    // The first three values here are deprecated and do not apply to the version of the property that has
                    // the -webkit- prefix removed.
                    if (currentValue->id == CSSValueBorder || currentValue->id == CSSValuePadding || currentValue->id == CSSValueContent
                        || currentValue->id == CSSValueBorderBox || currentValue->id == CSSValuePaddingBox || currentValue->id == CSSValueContentBox
                        || ((propId == CSSPropertyWebkitBackgroundClip || propId == CSSPropertyWebkitMaskClip)
                        && (currentValue->id == CSSValueText || currentValue->id == CSSValueWebkitText))) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundClip:
                    if (parseBackgroundClip(*currentValue, currValue))
                        m_valueList->next();
                    break;
                case CSSPropertyBackgroundOrigin:
                    if (currentValue->id == CSSValueBorderBox || currentValue->id == CSSValuePaddingBox || currentValue->id == CSSValueContentBox) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundPosition:
                case CSSPropertyWebkitMaskPosition: {
                    RefPtr<CSSPrimitiveValue> value1;
                    RefPtr<CSSPrimitiveValue> value2;
                    parseFillPosition(*m_valueList, value1, value2);
                    currValue = value1;
                    currValue2 = value2;
                    // parseFillPosition advances the m_valueList pointer.
                    break;
                }
                case CSSPropertyBackgroundPositionX:
                case CSSPropertyWebkitMaskPositionX: {
                    currValue = parsePositionX(*m_valueList);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyBackgroundPositionY:
                case CSSPropertyWebkitMaskPositionY: {
                    currValue = parsePositionY(*m_valueList);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyWebkitBackgroundComposite:
                case CSSPropertyWebkitMaskComposite:
                    if (currentValue->id >= CSSValueClear && currentValue->id <= CSSValuePlusLighter) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundBlendMode:
                    if (cssCompositingEnabled() && (currentValue->id == CSSValueNormal || currentValue->id == CSSValueMultiply
                        || currentValue->id == CSSValueScreen || currentValue->id == CSSValueOverlay || currentValue->id == CSSValueDarken
                        || currentValue->id == CSSValueLighten ||  currentValue->id == CSSValueColorDodge || currentValue->id == CSSValueColorBurn
                        || currentValue->id == CSSValueHardLight || currentValue->id == CSSValueSoftLight || currentValue->id == CSSValueDifference
                        || currentValue->id == CSSValueExclusion)) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundRepeat:
                case CSSPropertyWebkitMaskRepeat:
                    parseFillRepeat(currValue, currValue2);
                    // parseFillRepeat advances the m_valueList pointer
                    break;
                case CSSPropertyBackgroundSize:
                case CSSPropertyWebkitBackgroundSize:
                case CSSPropertyWebkitMaskSize: {
                    currValue = parseFillSize(propId, allowComma);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyWebkitMaskSourceType: {
                    if (currentValue->id == CSSValueAuto || currentValue->id == CSSValueAlpha || currentValue->id == CSSValueLuminance) {
                        currValue = CSSValuePool::singleton().createIdentifierValue(currentValue->id);
                        m_valueList->next();
                    } else
                        currValue = nullptr;
                    break;
                }
                default:
                    break;
            }
            if (!currValue)
                return false;

            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.releaseNonNull());
            }

            if (value2 && !values2) {
                values2 = CSSValueList::createCommaSeparated();
                values2->append(value2.releaseNonNull());
            }

            if (values)
                values->append(currValue.releaseNonNull());
            else
                value = currValue.release();
            if (currValue2) {
                if (values2)
                    values2->append(currValue2.releaseNonNull());
                else
                    value2 = currValue2.release();
            }
        }

        // When parsing any fill shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;
    }

    if (values && values->length()) {
        retValue1 = values.release();
        if (values2 && values2->length())
            retValue2 = values2.release();
        return true;
    }
    if (value) {
        retValue1 = value.release();
        retValue2 = value2.release();
        return true;
    }
    return false;
}

RefPtr<CSSValue> CSSParser::parseAnimationDelay()
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    if (validateUnit(valueWithCalculation, FTime))
        return createPrimitiveNumericValue(valueWithCalculation);
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationDirection()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNormal || value->id == CSSValueAlternate || value->id == CSSValueReverse || value->id == CSSValueAlternateReverse)
        return CSSValuePool::singleton().createIdentifierValue(value->id);
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationDuration()
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    if (validateUnit(valueWithCalculation, FTime | FNonNeg))
        return createPrimitiveNumericValue(valueWithCalculation);
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationFillMode()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNone || value->id == CSSValueForwards || value->id == CSSValueBackwards || value->id == CSSValueBoth)
        return CSSValuePool::singleton().createIdentifierValue(value->id);
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationIterationCount()
{
    CSSParserValue& value = *m_valueList->current();
    if (value.id == CSSValueInfinite)
        return CSSValuePool::singleton().createIdentifierValue(value.id);
    ValueWithCalculation valueWithCalculation(value);
    if (validateUnit(valueWithCalculation, FNumber | FNonNeg))
        return createPrimitiveNumericValue(valueWithCalculation);
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationName()
{
    CSSParserValue& value = *m_valueList->current();
    if (value.unit == CSSPrimitiveValue::CSS_STRING || value.unit == CSSPrimitiveValue::CSS_IDENT) {
        if (value.id == CSSValueNone || (value.unit == CSSPrimitiveValue::CSS_STRING && equalLettersIgnoringASCIICase(value, "none"))) {
            return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
        }
        return createPrimitiveStringValue(value);
    }
    return nullptr;
}

RefPtr<CSSValue> CSSParser::parseAnimationPlayState()
{
    CSSParserValue& value = *m_valueList->current();
    if (value.id == CSSValueRunning || value.id == CSSValuePaused)
        return CSSValuePool::singleton().createIdentifierValue(value.id);
    return nullptr;
}

#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
RefPtr<CSSValue> CSSParser::parseAnimationTrigger()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    if (value->unit != CSSParserValue::Function)
        return nullptr;

    CSSParserValueList* args = value->function->args.get();

    if (equalLettersIgnoringASCIICase(value->function->name, "container-scroll(")) {
        if (!args || (args->size() != 1 && args->size() != 3))
            return nullptr;

        CSSParserValue* argument = args->current();
        ValueWithCalculation firstArgumentWithCalculation(*argument);
        if (!validateUnit(firstArgumentWithCalculation, FLength))
            return nullptr;

        RefPtr<CSSValue> startValue = createPrimitiveNumericValue(firstArgumentWithCalculation);

        argument = args->next();

        if (!argument)
            return CSSAnimationTriggerScrollValue::create(startValue.release());

        if (!isComma(argument))
            return nullptr;

        argument = args->next();
        ValueWithCalculation secondArgumentWithCalculation(*argument);
        if (!validateUnit(secondArgumentWithCalculation, FLength))
            return nullptr;

        RefPtr<CSSValue> endValue = createPrimitiveNumericValue(secondArgumentWithCalculation);

        return CSSAnimationTriggerScrollValue::create(startValue.release(), endValue.release());
    }

    return nullptr;
}
#endif

RefPtr<CSSValue> CSSParser::parseAnimationProperty(AnimationParseContext& context)
{
    CSSParserValue& value = *m_valueList->current();
    if (value.unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;
    CSSPropertyID result = cssPropertyID(value.string);
    if (result && result != CSSPropertyAll) // "all" value in animation is not equivalent to the all property.
        return CSSValuePool::singleton().createIdentifierValue(result);
    if (equalLettersIgnoringASCIICase(value, "all")) {
        context.sawAnimationPropertyKeyword();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAll);
    }
    if (equalLettersIgnoringASCIICase(value, "none")) {
        context.commitAnimationPropertyKeyword();
        context.sawAnimationPropertyKeyword();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }
    return nullptr;
}

/* static */
Vector<double> CSSParser::parseKeyframeSelector(const String& selector) {
    Vector<double> keys;
    Vector<String> strings;
    selector.split(',', strings);

    for (size_t i = 0; i < strings.size(); ++i) {
        double key = -1;
        String cur = strings[i].stripWhiteSpace();

        // For now the syntax MUST be 'xxx%' or 'from' or 'to', where xxx is a legal floating point number
        if (equalLettersIgnoringASCIICase(cur, "from"))
            key = 0;
        else if (equalLettersIgnoringASCIICase(cur, "to"))
            key = 1;
        else if (cur.endsWith('%')) {
            double k = cur.substring(0, cur.length() - 1).toDouble();
            if (k >= 0 && k <= 100)
                key = k / 100;
        }
        if (key < 0) {
            keys.clear();
            break;
        }
        keys.append(key);
    }

    return keys;
}

bool CSSParser::parseTransformOriginShorthand(RefPtr<CSSPrimitiveValue>& value1, RefPtr<CSSPrimitiveValue>& value2, RefPtr<CSSValue>& value3)
{
    parse2ValuesFillPosition(*m_valueList, value1, value2);

    // now get z
    if (m_valueList->current()) {
        ValueWithCalculation valueWithCalculation(*m_valueList->current());
        if (validateUnit(valueWithCalculation, FLength)) {
            value3 = createPrimitiveNumericValue(valueWithCalculation);
            m_valueList->next();
            return true;
        }
        return false;
    }
    value3 = CSSValuePool::singleton().createImplicitInitialValue();
    return true;
}

bool CSSParser::parseCubicBezierTimingFunctionValue(CSSParserValueList& args, double& result)
{
    ValueWithCalculation argumentWithCalculation(*args.current());
    if (!validateUnit(argumentWithCalculation, FNumber))
        return false;
    result = parsedDouble(argumentWithCalculation);
    CSSParserValue* nextValue = args.next();
    if (!nextValue) {
        // The last number in the function has no comma after it, so we're done.
        return true;
    }
    if (!isComma(nextValue))
        return false;
    args.next();
    return true;
}

RefPtr<CSSValue> CSSParser::parseAnimationTimingFunction()
{
    CSSParserValue& value = *m_valueList->current();
    if (value.id == CSSValueEase || value.id == CSSValueLinear || value.id == CSSValueEaseIn || value.id == CSSValueEaseOut
        || value.id == CSSValueEaseInOut || value.id == CSSValueStepStart || value.id == CSSValueStepEnd)
        return CSSValuePool::singleton().createIdentifierValue(value.id);

    // We must be a function.
    if (value.unit != CSSParserValue::Function)
        return nullptr;

    CSSParserValueList* args = value.function->args.get();

    if (equalLettersIgnoringASCIICase(value.function->name, "steps(")) {
        // For steps, 1 or 2 params must be specified (comma-separated)
        if (!args || (args->size() != 1 && args->size() != 3))
            return nullptr;

        // There are two values.
        int numSteps;
        bool stepAtStart = false;

        CSSParserValue* argument = args->current();
        ValueWithCalculation argumentWithCalculation(*argument);
        if (!validateUnit(argumentWithCalculation, FInteger))
            return nullptr;
        numSteps = clampToInteger(parsedDouble(argumentWithCalculation));
        if (numSteps < 1)
            return nullptr;
        argument = args->next();

        if (argument) {
            // There is a comma so we need to parse the second value
            if (!isComma(argument))
                return nullptr;
            argument = args->next();
            if (argument->id != CSSValueStart && argument->id != CSSValueEnd)
                return nullptr;
            stepAtStart = argument->id == CSSValueStart;
        }

        return CSSStepsTimingFunctionValue::create(numSteps, stepAtStart);
    }

    if (equalLettersIgnoringASCIICase(value.function->name, "cubic-bezier(")) {
        // For cubic bezier, 4 values must be specified.
        if (!args || args->size() != 7)
            return nullptr;

        // There are two points specified. The x values must be between 0 and 1 but the y values can exceed this range.
        double x1, y1, x2, y2;

        if (!parseCubicBezierTimingFunctionValue(*args, x1))
            return nullptr;
        if (x1 < 0 || x1 > 1)
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(*args, y1))
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(*args, x2))
            return nullptr;
        if (x2 < 0 || x2 > 1)
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(*args, y2))
            return nullptr;

        return CSSCubicBezierTimingFunctionValue::create(x1, y1, x2, y2);
    }

    return nullptr;
}

bool CSSParser::parseAnimationProperty(CSSPropertyID propId, RefPtr<CSSValue>& result, AnimationParseContext& context)
{
    RefPtr<CSSValueList> values;
    CSSParserValue* val;
    RefPtr<CSSValue> value;
    bool allowComma = false;

    result = nullptr;

    while ((val = m_valueList->current())) {
        RefPtr<CSSValue> currValue;
        if (allowComma) {
            if (!isComma(val))
                return false;
            m_valueList->next();
            allowComma = false;
        }
        else {
            switch (propId) {
            case CSSPropertyAnimationDelay:
            case CSSPropertyWebkitAnimationDelay:
            case CSSPropertyTransitionDelay:
            case CSSPropertyWebkitTransitionDelay:
                currValue = parseAnimationDelay();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationDirection:
            case CSSPropertyWebkitAnimationDirection:
                currValue = parseAnimationDirection();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationDuration:
            case CSSPropertyWebkitAnimationDuration:
            case CSSPropertyTransitionDuration:
            case CSSPropertyWebkitTransitionDuration:
                currValue = parseAnimationDuration();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationFillMode:
            case CSSPropertyWebkitAnimationFillMode:
                currValue = parseAnimationFillMode();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationIterationCount:
            case CSSPropertyWebkitAnimationIterationCount:
                currValue = parseAnimationIterationCount();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationName:
            case CSSPropertyWebkitAnimationName:
                currValue = parseAnimationName();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationPlayState:
            case CSSPropertyWebkitAnimationPlayState:
                currValue = parseAnimationPlayState();
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyTransitionProperty:
            case CSSPropertyWebkitTransitionProperty:
                currValue = parseAnimationProperty(context);
                if (value && !context.animationPropertyKeywordAllowed())
                    return false;
                if (currValue)
                    m_valueList->next();
                break;
            case CSSPropertyAnimationTimingFunction:
            case CSSPropertyWebkitAnimationTimingFunction:
            case CSSPropertyTransitionTimingFunction:
            case CSSPropertyWebkitTransitionTimingFunction:
                currValue = parseAnimationTimingFunction();
                if (currValue)
                    m_valueList->next();
                break;
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
            case CSSPropertyWebkitAnimationTrigger:
                currValue = parseAnimationTrigger();
                if (currValue)
                    m_valueList->next();
                break;
#endif
            default:
                ASSERT_NOT_REACHED();
                return false;
            }

            if (!currValue)
                return false;

            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.releaseNonNull());
            }

            if (values)
                values->append(currValue.releaseNonNull());
            else
                value = currValue.release();

            allowComma = true;
        }

        // When parsing the 'transition' shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;
    }

    if (values && values->length()) {
        result = values.release();
        return true;
    }
    if (value) {
        result = value.release();
        return true;
    }
    return false;
}

#if ENABLE(CSS_GRID_LAYOUT)
static inline bool isValidGridPositionCustomIdent(const CSSParserValue& value)
{
    return value.unit == CSSPrimitiveValue::CSS_IDENT && value.id != CSSValueSpan && value.id != CSSValueAuto;
}

// The function parses [ <integer> || <custom-ident> ] in <grid-line> (which can be stand alone or with 'span').
bool CSSParser::parseIntegerOrCustomIdentFromGridPosition(RefPtr<CSSPrimitiveValue>& numericValue, RefPtr<CSSPrimitiveValue>& gridLineName)
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    if (validateUnit(valueWithCalculation, FInteger) && valueWithCalculation.value().fValue) {
        numericValue = createPrimitiveNumericValue(valueWithCalculation);
        CSSParserValue* nextValue = m_valueList->next();
        if (nextValue && isValidGridPositionCustomIdent(*nextValue)) {
            gridLineName = createPrimitiveStringValue(*nextValue);
            m_valueList->next();
        }
        return true;
    }

    if (isValidGridPositionCustomIdent(valueWithCalculation)) {
        gridLineName = createPrimitiveStringValue(valueWithCalculation);
        if (CSSParserValue* nextValue = m_valueList->next()) {
            ValueWithCalculation nextValueWithCalculation(*nextValue);
            if (validateUnit(nextValueWithCalculation, FInteger) && nextValueWithCalculation.value().fValue) {
                numericValue = createPrimitiveNumericValue(nextValueWithCalculation);
                m_valueList->next();
            }
        }
        return true;
    }

    return false;
}

RefPtr<CSSValue> CSSParser::parseGridPosition()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto) {
        m_valueList->next();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
    }

    RefPtr<CSSPrimitiveValue> numericValue;
    RefPtr<CSSPrimitiveValue> gridLineName;
    bool hasSeenSpanKeyword = false;

    if (value->id == CSSValueSpan) {
        hasSeenSpanKeyword = true;
        if (auto* nextValue = m_valueList->next()) {
            if (!isForwardSlashOperator(*nextValue) && !parseIntegerOrCustomIdentFromGridPosition(numericValue, gridLineName))
                return nullptr;
        }
    } else if (parseIntegerOrCustomIdentFromGridPosition(numericValue, gridLineName)) {
        value = m_valueList->current();
        if (value && value->id == CSSValueSpan) {
            hasSeenSpanKeyword = true;
            m_valueList->next();
        }
    }

    // Check that we have consumed all the value list. For shorthands, the parser will pass
    // the whole value list (including the opposite position).
    if (m_valueList->current() && !isForwardSlashOperator(*m_valueList->current()))
        return nullptr;

    // If we didn't parse anything, this is not a valid grid position.
    if (!hasSeenSpanKeyword && !gridLineName && !numericValue)
        return nullptr;

    // Negative numbers are not allowed for span (but are for <integer>).
    if (hasSeenSpanKeyword && numericValue && numericValue->getIntValue() < 0)
        return nullptr;

    // For the <custom-ident> case.
    if (gridLineName && !numericValue && !hasSeenSpanKeyword)
        return CSSValuePool::singleton().createValue(gridLineName->getStringValue(), CSSPrimitiveValue::CSS_STRING);

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (hasSeenSpanKeyword)
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSpan));
    if (numericValue)
        values->append(numericValue.releaseNonNull());
    if (gridLineName)
        values->append(gridLineName.releaseNonNull());
    ASSERT(values->length());
    return values;
}

static RefPtr<CSSValue> gridMissingGridPositionValue(CSSValue& value)
{
    if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isString())
        return &value;

    return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);
}

bool CSSParser::parseGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    ShorthandScope scope(this, shorthandId);
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);

    RefPtr<CSSValue> startValue = parseGridPosition();
    if (!startValue)
        return false;

    RefPtr<CSSValue> endValue;
    if (m_valueList->current()) {
        if (!isForwardSlashOperator(*m_valueList->current()))
            return false;

        if (!m_valueList->next())
            return false;

        endValue = parseGridPosition();
        if (!endValue || m_valueList->current())
            return false;
    } else
        endValue = gridMissingGridPositionValue(*startValue);

    addProperty(shorthand.properties()[0], startValue, important);
    addProperty(shorthand.properties()[1], endValue, important);
    return true;
}

bool CSSParser::parseGridGapShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyWebkitGridGap);
    ASSERT(shorthandForProperty(CSSPropertyWebkitGridGap).length() == 2);

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    ValueWithCalculation rowValueWithCalculation(*value);
    if (!validateUnit(rowValueWithCalculation, FLength | FNonNeg))
        return false;

    RefPtr<CSSPrimitiveValue> rowGap = createPrimitiveNumericValue(rowValueWithCalculation);

    value = m_valueList->next();
    if (!value) {
        addProperty(CSSPropertyWebkitGridColumnGap, rowGap, important);
        addProperty(CSSPropertyWebkitGridRowGap, rowGap, important);
        return true;
    }

    ValueWithCalculation columnValueWithCalculation(*value);
    if (!validateUnit(columnValueWithCalculation, FLength | FNonNeg))
        return false;

    if (m_valueList->next())
        return false;

    RefPtr<CSSPrimitiveValue> columnGap = createPrimitiveNumericValue(columnValueWithCalculation);

    addProperty(CSSPropertyWebkitGridRowGap, rowGap, important);
    addProperty(CSSPropertyWebkitGridColumnGap, columnGap, important);

    return true;
}

RefPtr<CSSValue> CSSParser::parseGridTemplateColumns()
{
    if (!(m_valueList->current() && isForwardSlashOperator(*m_valueList->current()) && m_valueList->next()))
        return nullptr;
    if (auto columnsValue = parseGridTrackList()) {
        if (m_valueList->current())
            return nullptr;
        return columnsValue;
    }

    return nullptr;
}

bool CSSParser::parseGridTemplateRowsAndAreasAndColumns(bool important)
{
    // At least template-areas strings must be defined.
    if (!m_valueList->current() || isForwardSlashOperator(*m_valueList->current()))
        return false;

    NamedGridAreaMap gridAreaMap;
    unsigned rowCount = 0;
    unsigned columnCount = 0;
    bool trailingIdentWasAdded = false;
    RefPtr<CSSValueList> templateRows = CSSValueList::createSpaceSeparated();

    while (m_valueList->current() && !isForwardSlashOperator(*m_valueList->current())) {
        // Handle leading <custom-ident>*.
        if (m_valueList->current()->unit == CSSParserValue::ValueList) {
            if (trailingIdentWasAdded) {
                // A row's trailing ident must be concatenated with the next row's leading one.
                parseGridLineNames(*m_valueList, *templateRows, downcast<CSSGridLineNamesValue>(templateRows->item(templateRows->length() - 1)));
            } else
                parseGridLineNames(*m_valueList, *templateRows);
        }

        // Handle a template-area's row.
        if (!parseGridTemplateAreasRow(gridAreaMap, rowCount, columnCount))
            return false;
        ++rowCount;

        // Handle template-rows's track-size.
        if (m_valueList->current() && m_valueList->current()->unit != CSSParserValue::Operator && m_valueList->current()->unit != CSSParserValue::ValueList && m_valueList->current()->unit != CSSPrimitiveValue::CSS_STRING) {
            RefPtr<CSSValue> value = parseGridTrackSize(*m_valueList);
            if (!value)
                return false;
            templateRows->append(value.releaseNonNull());
        } else
            templateRows->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAuto));

        // This will handle the trailing/leading <custom-ident>* in the grammar.
        trailingIdentWasAdded = false;
        if (m_valueList->current() && m_valueList->current()->unit == CSSParserValue::ValueList)
            trailingIdentWasAdded = parseGridLineNames(*m_valueList, *templateRows);
    }

    // [<track-list> /]?
    RefPtr<CSSValue> templateColumns = nullptr;
    if (m_valueList->current()) {
        ASSERT(isForwardSlashOperator(*m_valueList->current()));
        templateColumns = parseGridTemplateColumns();
        if (!templateColumns)
            return false;
        // The template-columns <track-list> can't be 'none'.
        if (templateColumns->isPrimitiveValue() && downcast<CSSPrimitiveValue>(*templateColumns).getValueID() == CSSValueNone)
            return false;
    }

    addProperty(CSSPropertyWebkitGridTemplateRows, templateRows.release(), important);
    addProperty(CSSPropertyWebkitGridTemplateColumns, templateColumns ? templateColumns.release() : CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);

    RefPtr<CSSValue> templateAreas = CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
    addProperty(CSSPropertyWebkitGridTemplateAreas, templateAreas.release(), important);

    return true;
}

bool CSSParser::parseGridTemplateShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyWebkitGridTemplate);
    ASSERT(shorthandForProperty(CSSPropertyWebkitGridTemplate).length() == 3);

    // At least "none" must be defined.
    if (!m_valueList->current())
        return false;

    bool firstValueIsNone = m_valueList->current()->id == CSSValueNone;

    // 1- 'none' case.
    if (firstValueIsNone && !m_valueList->next()) {
        addProperty(CSSPropertyWebkitGridTemplateColumns, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyWebkitGridTemplateRows, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        addProperty(CSSPropertyWebkitGridTemplateAreas, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    // 2- <grid-template-rows> / <grid-template-columns> syntax.
    RefPtr<CSSValue> rowsValue;
    if (firstValueIsNone)
        rowsValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    else
        rowsValue = parseGridTrackList();

    if (rowsValue) {
        auto columnsValue = parseGridTemplateColumns();
        if (!columnsValue)
            return false;

        addProperty(CSSPropertyWebkitGridTemplateColumns, columnsValue.release(), important);
        addProperty(CSSPropertyWebkitGridTemplateRows, rowsValue.release(), important);
        addProperty(CSSPropertyWebkitGridTemplateAreas, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        return true;
    }


    // 3- [<line-names>? <string> <track-size>? <line-names>? ]+ syntax.
    // It requires to rewind parsing due to previous syntax failures.
    m_valueList->setCurrentIndex(0);
    return parseGridTemplateRowsAndAreasAndColumns(important);
}

bool CSSParser::parseGridShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyWebkitGrid);
    ASSERT(shorthandForProperty(CSSPropertyWebkitGrid).length() == 8);

    // 1- <grid-template>
    if (parseGridTemplateShorthand(important)) {
        // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
        // The sub-properties not specified are set to their initial value, as normal for shorthands.
        addProperty(CSSPropertyWebkitGridAutoFlow, CSSValuePool::singleton().createImplicitInitialValue(), important);
        addProperty(CSSPropertyWebkitGridAutoColumns, CSSValuePool::singleton().createImplicitInitialValue(), important);
        addProperty(CSSPropertyWebkitGridAutoRows, CSSValuePool::singleton().createImplicitInitialValue(), important);
        addProperty(CSSPropertyWebkitGridColumnGap, CSSValuePool::singleton().createImplicitInitialValue(), important);
        addProperty(CSSPropertyWebkitGridRowGap, CSSValuePool::singleton().createImplicitInitialValue(), important);
        return true;
    }

    // Need to rewind parsing to explore the alternative syntax of this shorthand.
    m_valueList->setCurrentIndex(0);

    // 2- <grid-auto-flow> [ <grid-auto-columns> [ / <grid-auto-rows> ]? ]
    if (!parseValue(CSSPropertyWebkitGridAutoFlow, important))
        return false;

    RefPtr<CSSValue> autoColumnsValue;
    RefPtr<CSSValue> autoRowsValue;

    if (m_valueList->current()) {
        autoRowsValue = parseGridTrackSize(*m_valueList);
        if (!autoRowsValue)
            return false;
        if (m_valueList->current()) {
            if (!isForwardSlashOperator(*m_valueList->current()) || !m_valueList->next())
                return false;
            autoColumnsValue = parseGridTrackSize(*m_valueList);
            if (!autoColumnsValue)
                return false;
        }
        if (m_valueList->current())
            return false;
    } else {
        // Other omitted values are set to their initial values.
        autoColumnsValue = CSSValuePool::singleton().createImplicitInitialValue();
        autoRowsValue = CSSValuePool::singleton().createImplicitInitialValue();
    }

    // if <grid-auto-rows> value is omitted, it is set to the value specified for grid-auto-columns.
    if (!autoColumnsValue)
        autoColumnsValue = autoRowsValue;

    addProperty(CSSPropertyWebkitGridAutoColumns, autoColumnsValue.release(), important);
    addProperty(CSSPropertyWebkitGridAutoRows, autoRowsValue.release(), important);

    // It can only be specified the explicit or the implicit grid properties in a single grid declaration.
    // The sub-properties not specified are set to their initial value, as normal for shorthands.
    addProperty(CSSPropertyWebkitGridTemplateColumns, CSSValuePool::singleton().createImplicitInitialValue(), important);
    addProperty(CSSPropertyWebkitGridTemplateRows, CSSValuePool::singleton().createImplicitInitialValue(), important);
    addProperty(CSSPropertyWebkitGridTemplateAreas, CSSValuePool::singleton().createImplicitInitialValue(), important);
    addProperty(CSSPropertyWebkitGridColumnGap, CSSValuePool::singleton().createImplicitInitialValue(), important);
    addProperty(CSSPropertyWebkitGridRowGap, CSSValuePool::singleton().createImplicitInitialValue(), important);

    return true;
}

bool CSSParser::parseGridAreaShorthand(bool important)
{
    ShorthandScope scope(this, CSSPropertyWebkitGridArea);
    ASSERT(shorthandForProperty(CSSPropertyWebkitGridArea).length() == 4);

    RefPtr<CSSValue> rowStartValue = parseGridPosition();
    if (!rowStartValue)
        return false;

    RefPtr<CSSValue> columnStartValue;
    if (!parseSingleGridAreaLonghand(columnStartValue))
        return false;

    RefPtr<CSSValue> rowEndValue;
    if (!parseSingleGridAreaLonghand(rowEndValue))
        return false;

    RefPtr<CSSValue> columnEndValue;
    if (!parseSingleGridAreaLonghand(columnEndValue))
        return false;

    if (!columnStartValue)
        columnStartValue = gridMissingGridPositionValue(*rowStartValue);

    if (!rowEndValue)
        rowEndValue = gridMissingGridPositionValue(*rowStartValue);

    if (!columnEndValue)
        columnEndValue = gridMissingGridPositionValue(*columnStartValue);

    addProperty(CSSPropertyWebkitGridRowStart, rowStartValue, important);
    addProperty(CSSPropertyWebkitGridColumnStart, columnStartValue, important);
    addProperty(CSSPropertyWebkitGridRowEnd, rowEndValue, important);
    addProperty(CSSPropertyWebkitGridColumnEnd, columnEndValue, important);
    return true;
}

bool CSSParser::parseSingleGridAreaLonghand(RefPtr<CSSValue>& property)
{
    if (!m_valueList->current())
        return true;

    if (!isForwardSlashOperator(*m_valueList->current()))
        return false;

    if (!m_valueList->next())
        return false;

    property = parseGridPosition();
    return true;
}

bool CSSParser::parseGridLineNames(CSSParserValueList& inputList, CSSValueList& valueList, CSSGridLineNamesValue* previousNamedAreaTrailingLineNames)
{
    ASSERT(inputList.current() && inputList.current()->unit == CSSParserValue::ValueList);

    CSSParserValueList& identList = *inputList.current()->valueList;
    if (!identList.size()) {
        inputList.next();
        return false;
    }

    // Need to ensure the identList is at the heading index, since the parserList might have been rewound.
    identList.setCurrentIndex(0);
    RefPtr<CSSGridLineNamesValue> lineNames;
    if (previousNamedAreaTrailingLineNames)
        lineNames = previousNamedAreaTrailingLineNames;
    else
        lineNames = CSSGridLineNamesValue::create();
    while (CSSParserValue* identValue = identList.current()) {
        ASSERT(identValue->unit == CSSPrimitiveValue::CSS_IDENT);
        lineNames->append(createPrimitiveStringValue(*identValue));
        identList.next();
    }
    if (!previousNamedAreaTrailingLineNames)
        valueList.append(lineNames.releaseNonNull());

    inputList.next();
    return true;
}

RefPtr<CSSValue> CSSParser::parseGridTrackList()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNone) {
        m_valueList->next();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }

    RefPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    // Handle leading  <custom-ident>*.
    value = m_valueList->current();
    if (value && value->unit == CSSParserValue::ValueList)
        parseGridLineNames(*m_valueList, *values);

    bool seenTrackSizeOrRepeatFunction = false;
    while (CSSParserValue* currentValue = m_valueList->current()) {
        if (isForwardSlashOperator(*currentValue))
            break;
        if (currentValue->unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(currentValue->function->name, "repeat(")) {
            if (!parseGridTrackRepeatFunction(*values))
                return nullptr;
        } else {
            RefPtr<CSSValue> value = parseGridTrackSize(*m_valueList);
            if (!value)
                return nullptr;
            values->append(value.releaseNonNull());
        }
        seenTrackSizeOrRepeatFunction = true;

        // This will handle the trailing <custom-ident>* in the grammar.
        value = m_valueList->current();
        if (value && value->unit == CSSParserValue::ValueList)
            parseGridLineNames(*m_valueList, *values);
    }

    if (!seenTrackSizeOrRepeatFunction)
        return nullptr;

    return values;
}

bool CSSParser::parseGridTrackRepeatFunction(CSSValueList& list)
{
    CSSParserValueList* arguments = m_valueList->current()->function->args.get();
    if (!arguments || arguments->size() < 3)
        return false;

    ValueWithCalculation firstValueWithCalculation(*arguments->valueAt(0));
    if (!validateUnit(firstValueWithCalculation, FPositiveInteger) || !isComma(arguments->valueAt(1)))
        return false;

    // If arguments->valueAt(0)->fValue > SIZE_MAX then repetitions becomes 0 during the type casting, that's why we
    // clamp it down to kGridMaxTracks before the type casting.
    unsigned repetitions = clampTo<unsigned>(parsedDouble(firstValueWithCalculation), 0, kGridMaxTracks);

    RefPtr<CSSValueList> repeatedValues = CSSValueList::createSpaceSeparated();
    arguments->next(); // Skip the repetition count.
    arguments->next(); // Skip the comma.

    // Handle leading <custom-ident>*.
    CSSParserValue* currentValue = arguments->current();
    if (currentValue && currentValue->unit == CSSParserValue::ValueList)
        parseGridLineNames(*arguments, *repeatedValues);

    unsigned numberOfTracks = 0;
    while (arguments->current()) {
        RefPtr<CSSValue> trackSize = parseGridTrackSize(*arguments);
        if (!trackSize)
            return false;

        repeatedValues->append(trackSize.releaseNonNull());
        ++numberOfTracks;

        // This takes care of any trailing <custom-ident>* in the grammar.
        currentValue = arguments->current();
        if (currentValue && currentValue->unit == CSSParserValue::ValueList)
            parseGridLineNames(*arguments, *repeatedValues);
    }

    // We should have found at least one <track-size>, otherwise the declaration is invalid.
    if (!numberOfTracks)
        return false;

    // We clamp the number of repetitions to a multiple of the repeat() track list's size, while staying below the max
    // grid size.
    repetitions = std::min(repetitions, kGridMaxTracks / numberOfTracks);

    for (size_t i = 0; i < repetitions; ++i) {
        for (size_t j = 0; j < repeatedValues->length(); ++j)
            list.append(*repeatedValues->itemWithoutBoundsCheck(j));
    }

    m_valueList->next();
    return true;
}

RefPtr<CSSValue> CSSParser::parseGridTrackSize(CSSParserValueList& inputList)
{
    CSSParserValue& currentValue = *inputList.current();
    inputList.next();

    if (currentValue.id == CSSValueAuto)
        return CSSValuePool::singleton().createIdentifierValue(CSSValueAuto);

    if (currentValue.unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(currentValue.function->name, "minmax(")) {
        // The spec defines the following grammar: minmax( <track-breadth> , <track-breadth> )
        CSSParserValueList* arguments = currentValue.function->args.get();
        if (!arguments || arguments->size() != 3 || !isComma(arguments->valueAt(1)))
            return nullptr;

        RefPtr<CSSPrimitiveValue> minTrackBreadth = parseGridBreadth(*arguments->valueAt(0));
        if (!minTrackBreadth)
            return nullptr;

        RefPtr<CSSPrimitiveValue> maxTrackBreadth = parseGridBreadth(*arguments->valueAt(2));
        if (!maxTrackBreadth)
            return nullptr;

        RefPtr<CSSValueList> parsedArguments = CSSValueList::createCommaSeparated();
        parsedArguments->append(minTrackBreadth.releaseNonNull());
        parsedArguments->append(maxTrackBreadth.releaseNonNull());
        return CSSFunctionValue::create("minmax(", parsedArguments);
    }

    return parseGridBreadth(currentValue);
}

RefPtr<CSSPrimitiveValue> CSSParser::parseGridBreadth(CSSParserValue& value)
{
    if (value.id == CSSValueWebkitMinContent || value.id == CSSValueWebkitMaxContent || value.id == CSSValueAuto)
        return CSSValuePool::singleton().createIdentifierValue(value.id);

    if (value.unit == CSSPrimitiveValue::CSS_FR) {
        double flexValue = value.fValue;

        // Fractional unit is a non-negative dimension.
        if (flexValue <= 0)
            return nullptr;

        return CSSValuePool::singleton().createValue(flexValue, CSSPrimitiveValue::CSS_FR);
    }

    ValueWithCalculation valueWithCalculation(value);
    if (!validateUnit(valueWithCalculation, FNonNeg | FLength | FPercent))
        return nullptr;

    return createPrimitiveNumericValue(valueWithCalculation);
}

static inline bool isValidGridAutoFlowId(CSSValueID id)
{
    return (id == CSSValueRow || id == CSSValueColumn || id == CSSValueDense);
}

RefPtr<CSSValue> CSSParser::parseGridAutoFlow(CSSParserValueList& inputList)
{
    // [ row | column ] || dense
    CSSParserValue* value = inputList.current();
    if (!value)
        return nullptr;

    RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();

    // First parameter.
    CSSValueID firstId = value->id;
    if (!isValidGridAutoFlowId(firstId))
        return nullptr;

    // Second parameter, if any.
    // If second parameter is not valid we should process anyway the first one as we can be inside the "grid" shorthand.
    value = inputList.next();
    if (!value || !isValidGridAutoFlowId(value->id)) {
        if (firstId == CSSValueDense)
            parsedValues->append(CSSValuePool::singleton().createIdentifierValue(CSSValueRow));

        parsedValues->append(CSSValuePool::singleton().createIdentifierValue(firstId));
        return parsedValues;
    }

    switch (firstId) {
    case CSSValueRow:
    case CSSValueColumn:
        parsedValues->append(CSSValuePool::singleton().createIdentifierValue(firstId));
        if (value->id == CSSValueDense) {
            parsedValues->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            inputList.next();
        }
        break;
    case CSSValueDense:
        if (value->id == CSSValueRow || value->id == CSSValueColumn) {
            parsedValues->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            inputList.next();
        }
        parsedValues->append(CSSValuePool::singleton().createIdentifierValue(firstId));
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return parsedValues;
}
#endif /* ENABLE(CSS_GRID_LAYOUT) */

#if ENABLE(DASHBOARD_SUPPORT)

#define DASHBOARD_REGION_NUM_PARAMETERS  6
#define DASHBOARD_REGION_SHORT_NUM_PARAMETERS  2

static CSSParserValue* skipCommaInDashboardRegion(CSSParserValueList *args)
{
    if (args->size() == (DASHBOARD_REGION_NUM_PARAMETERS*2-1) ||
         args->size() == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
        CSSParserValue& current = *args->current();
        if (current.unit == CSSParserValue::Operator && current.iValue == ',')
            return args->next();
    }
    return args->current();
}

bool CSSParser::parseDashboardRegions(CSSPropertyID propId, bool important)
{
    bool valid = true;

    CSSParserValue* value = m_valueList->current();

    if (value->id == CSSValueNone) {
        if (m_valueList->next())
            return false;
        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(value->id), important);
        return valid;
    }

    auto firstRegion = DashboardRegion::create();
    DashboardRegion* region = nullptr;

    while (value) {
        if (!region) {
            region = firstRegion.ptr();
        } else {
            auto nextRegion = DashboardRegion::create();
            region->m_next = nextRegion.copyRef();
            region = nextRegion.ptr();
        }

        if (value->unit != CSSParserValue::Function) {
            valid = false;
            break;
        }

        // Commas count as values, so allow (function name is dashboard-region for DASHBOARD_SUPPORT feature):
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // also allow
        // dashboard-region(label, type) or dashboard-region(label type)
        // dashboard-region(label, type) or dashboard-region(label type)
        CSSParserValueList* args = value->function->args.get();
        if (!equalLettersIgnoringASCIICase(value->function->name, "dashboard-region(") || !args) {
            valid = false;
            break;
        }

        int numArgs = args->size();
        if ((numArgs != DASHBOARD_REGION_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_NUM_PARAMETERS*2-1)) &&
            (numArgs != DASHBOARD_REGION_SHORT_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1))) {
            valid = false;
            break;
        }

        // First arg is a label.
        CSSParserValue* arg = args->current();
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }

        region->m_label = arg->string;

        // Second arg is a type.
        arg = args->next();
        arg = skipCommaInDashboardRegion(args);
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }

        if (equalLettersIgnoringASCIICase(*arg, "circle"))
            region->m_isCircle = true;
        else if (equalLettersIgnoringASCIICase(*arg, "rectangle"))
            region->m_isRectangle = true;
        else {
            valid = false;
            break;
        }

        region->m_geometryType = arg->string;

        if (numArgs == DASHBOARD_REGION_SHORT_NUM_PARAMETERS || numArgs == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
            // This originally used CSSValueInvalid by accident. It might be more logical to use something else.
            RefPtr<CSSPrimitiveValue> amount = CSSValuePool::singleton().createIdentifierValue(CSSValueInvalid);

            region->setTop(amount);
            region->setRight(amount);
            region->setBottom(amount);
            region->setLeft(amount);
        } else {
            // Next four arguments must be offset numbers
            for (int i = 0; i < 4; ++i) {
                arg = args->next();
                arg = skipCommaInDashboardRegion(args);

                ValueWithCalculation argWithCalculation(*arg);
                valid = arg->id == CSSValueAuto || validateUnit(argWithCalculation, FLength);
                if (!valid)
                    break;

                RefPtr<CSSPrimitiveValue> amount = arg->id == CSSValueAuto ? CSSValuePool::singleton().createIdentifierValue(CSSValueAuto) : createPrimitiveNumericValue(argWithCalculation);

                if (i == 0)
                    region->setTop(amount);
                else if (i == 1)
                    region->setRight(amount);
                else if (i == 2)
                    region->setBottom(amount);
                else
                    region->setLeft(amount);
            }
        }

        if (args->next())
            return false;

        value = m_valueList->next();
    }

    if (valid)
        addProperty(propId, CSSValuePool::singleton().createValue(RefPtr<DashboardRegion>(WTFMove(firstRegion))), important);

    return valid;
}

#endif /* ENABLE(DASHBOARD_SUPPORT) */

#if ENABLE(CSS_GRID_LAYOUT)
static Vector<String> parseGridTemplateAreasColumnNames(const String& gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    // Using StringImpl to avoid checks and indirection in every call to String::operator[].
    StringImpl& text = *gridRowNames.impl();
    unsigned length = text.length();
    unsigned index = 0;
    while (index < length) {
        if (text[index] != ' ' && text[index] != '.') {
            unsigned gridAreaStart = index;
            while (index < length && text[index] != ' ' && text[index] != '.')
                index++;
            columnNames.append(text.substring(gridAreaStart, index - gridAreaStart));
            continue;
        }

        if (text[index] == '.') {
            while (index < length && text[index] == '.')
                index++;
            columnNames.append(".");
            continue;
        }

        index++;
    }

    return columnNames;
}

bool CSSParser::parseGridTemplateAreasRow(NamedGridAreaMap& gridAreaMap, const unsigned rowCount, unsigned& columnCount)
{
    CSSParserValue* currentValue = m_valueList->current();
    if (!currentValue || currentValue->unit != CSSPrimitiveValue::CSS_STRING)
        return false;

    String gridRowNames = currentValue->string;
    if (gridRowNames.containsOnlyWhitespace())
        return false;

    Vector<String> columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (!columnCount) {
        columnCount = columnNames.size();
        ASSERT(columnCount);
    } else if (columnCount != columnNames.size()) {
        // The declaration is invalid is all the rows don't have the number of columns.
        return false;
    }

    for (unsigned currentColumn = 0; currentColumn < columnCount; ++currentColumn) {
        const String& gridAreaName = columnNames[currentColumn];

        // Unamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == ".")
            continue;

        // We handle several grid areas with the same name at once to simplify the validation code.
        unsigned lookAheadColumn;
        for (lookAheadColumn = currentColumn + 1; lookAheadColumn < columnCount; ++lookAheadColumn) {
            if (columnNames[lookAheadColumn] != gridAreaName)
                break;
        }

        auto gridAreaIterator = gridAreaMap.find(gridAreaName);
        if (gridAreaIterator == gridAreaMap.end())
            gridAreaMap.add(gridAreaName, GridCoordinate(GridSpan::definiteGridSpan(rowCount, rowCount + 1), GridSpan::definiteGridSpan(currentColumn, lookAheadColumn)));
        else {
            GridCoordinate& gridCoordinate = gridAreaIterator->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (rowCount != gridCoordinate.rows.resolvedFinalPosition())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridCoordinate.columns.resolvedInitialPosition())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridCoordinate.columns.resolvedFinalPosition())
                return false;

            gridCoordinate.rows = GridSpan::definiteGridSpan(gridCoordinate.rows.resolvedInitialPosition(), gridCoordinate.rows.resolvedFinalPosition() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    m_valueList->next();
    return true;
}

RefPtr<CSSValue> CSSParser::parseGridTemplateAreas()
{
    if (m_valueList->current() && m_valueList->current()->id == CSSValueNone) {
        m_valueList->next();
        return CSSValuePool::singleton().createIdentifierValue(CSSValueNone);
    }

    NamedGridAreaMap gridAreaMap;
    unsigned rowCount = 0;
    unsigned columnCount = 0;

    while (m_valueList->current()) {
        if (!parseGridTemplateAreasRow(gridAreaMap, rowCount, columnCount))
            return nullptr;
        ++rowCount;
    }

    if (!rowCount || !columnCount)
        return nullptr;

    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}
#endif /* ENABLE(CSS_GRID_LAYOUT) */

RefPtr<CSSValue> CSSParser::parseCounterContent(CSSParserValueList& args, bool counters)
{
    unsigned numArgs = args.size();
    if (counters && numArgs != 3 && numArgs != 5)
        return nullptr;
    if (!counters && numArgs != 1 && numArgs != 3)
        return nullptr;

    CSSParserValue* argument = args.current();
    if (argument->unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;
    RefPtr<CSSPrimitiveValue> identifier = createPrimitiveStringValue(*argument);

    RefPtr<CSSPrimitiveValue> separator;
    if (!counters)
        separator = CSSValuePool::singleton().createValue(String(), CSSPrimitiveValue::CSS_STRING);
    else {
        argument = args.next();
        if (argument->unit != CSSParserValue::Operator || argument->iValue != ',')
            return nullptr;

        argument = args.next();
        if (argument->unit != CSSPrimitiveValue::CSS_STRING)
            return nullptr;

        separator = createPrimitiveStringValue(*argument);
    }

    RefPtr<CSSPrimitiveValue> listStyle;
    argument = args.next();
    if (!argument) // Make the list style default decimal
        listStyle = CSSValuePool::singleton().createIdentifierValue(CSSValueDecimal);
    else {
        if (argument->unit != CSSParserValue::Operator || argument->iValue != ',')
            return nullptr;

        argument = args.next();
        if (argument->unit != CSSPrimitiveValue::CSS_IDENT)
            return nullptr;

        CSSValueID listStyleID = CSSValueInvalid;
        if (argument->id == CSSValueNone || (argument->id >= CSSValueDisc && argument->id <= CSSValueKatakanaIroha))
            listStyleID = argument->id;
        else
            return nullptr;

        listStyle = CSSValuePool::singleton().createIdentifierValue(listStyleID);
    }

    return CSSValuePool::singleton().createValue(Counter::create(identifier.release(), listStyle.release(), separator.release()));
}

bool CSSParser::parseClipShape(CSSPropertyID propId, bool important)
{
    CSSParserValue& value = *m_valueList->current();
    CSSParserValueList* args = value.function->args.get();

    if (!equalLettersIgnoringASCIICase(value.function->name, "rect(") || !args)
        return false;

    // rect(t, r, b, l) || rect(t r b l)
    if (args->size() != 4 && args->size() != 7)
        return false;
    auto rect = Rect::create();
    bool valid = true;
    int i = 0;
    CSSParserValue* argument = args->current();
    while (argument) {
        ValueWithCalculation argumentWithCalculation(*argument);
        valid = argument->id == CSSValueAuto || validateUnit(argumentWithCalculation, FLength);
        if (!valid)
            break;
        RefPtr<CSSPrimitiveValue> length = argument->id == CSSValueAuto ? CSSValuePool::singleton().createIdentifierValue(CSSValueAuto) : createPrimitiveNumericValue(argumentWithCalculation);
        if (i == 0)
            rect->setTop(length);
        else if (i == 1)
            rect->setRight(length);
        else if (i == 2)
            rect->setBottom(length);
        else
            rect->setLeft(length);
        argument = args->next();
        if (argument && args->size() == 7) {
            if (argument->unit == CSSParserValue::Operator && argument->iValue == ',') {
                argument = args->next();
            } else {
                valid = false;
                break;
            }
        }
        i++;
    }
    if (valid) {
        addProperty(propId, CSSValuePool::singleton().createValue(WTFMove(rect)), important);
        m_valueList->next();
        return true;
    }
    return false;
}

static void completeBorderRadii(RefPtr<CSSPrimitiveValue> radii[4])
{
    if (radii[3])
        return;
    if (!radii[2]) {
        if (!radii[1])
            radii[1] = radii[0];
        radii[2] = radii[0];
    }
    radii[3] = radii[1];
}

// FIXME: This should be refactored with CSSParser::parseBorderRadius.
// CSSParser::parseBorderRadius contains support for some legacy radius construction.
RefPtr<CSSBasicShape> CSSParser::parseInsetRoundedCorners(PassRefPtr<CSSBasicShapeInset> shape, CSSParserValueList& args)
{
    CSSParserValue* argument = args.next();

    if (!argument)
        return nullptr;

    Vector<CSSParserValue*> radiusArguments;
    while (argument) {
        radiusArguments.append(argument);
        argument = args.next();
    }

    unsigned num = radiusArguments.size();
    if (!num || num > 9)
        return nullptr;

    RefPtr<CSSPrimitiveValue> radii[2][4];

    unsigned indexAfterSlash = 0;
    for (unsigned i = 0; i < num; ++i) {
        CSSParserValue& value = *radiusArguments.at(i);
        if (value.unit == CSSParserValue::Operator) {
            if (value.iValue != '/')
                return nullptr;

            if (!i || indexAfterSlash || i + 1 == num || num > i + 5)
                return nullptr;

            indexAfterSlash = i + 1;
            completeBorderRadii(radii[0]);
            continue;
        }

        if (i - indexAfterSlash >= 4)
            return nullptr;

        ValueWithCalculation valueWithCalculation(value);
        if (!validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg))
            return nullptr;

        RefPtr<CSSPrimitiveValue> radius = createPrimitiveNumericValue(valueWithCalculation);

        if (!indexAfterSlash)
            radii[0][i] = radius;
        else
            radii[1][i - indexAfterSlash] = radius.release();
    }

    if (!indexAfterSlash) {
        completeBorderRadii(radii[0]);
        for (unsigned i = 0; i < 4; ++i)
            radii[1][i] = radii[0][i];
    } else
        completeBorderRadii(radii[1]);

    shape->setTopLeftRadius(createPrimitiveValuePair(radii[0][0].release(), radii[1][0].release()));
    shape->setTopRightRadius(createPrimitiveValuePair(radii[0][1].release(), radii[1][1].release()));
    shape->setBottomRightRadius(createPrimitiveValuePair(radii[0][2].release(), radii[1][2].release()));
    shape->setBottomLeftRadius(createPrimitiveValuePair(radii[0][3].release(), radii[1][3].release()));

    return shape;
}

RefPtr<CSSBasicShape> CSSParser::parseBasicShapeInset(CSSParserValueList& args)
{
    RefPtr<CSSBasicShapeInset> shape = CSSBasicShapeInset::create();

    CSSParserValue* argument = args.current();
    Vector<RefPtr<CSSPrimitiveValue> > widthArguments;
    bool hasRoundedInset = false;
    while (argument) {
        if (argument->unit == CSSPrimitiveValue::CSS_IDENT && equalLettersIgnoringASCIICase(argument->string, "round")) {
            hasRoundedInset = true;
            break;
        }

        Units unitFlags = FLength | FPercent;
        ValueWithCalculation argumentWithCalculation(*argument);
        if (!validateUnit(argumentWithCalculation, unitFlags) || widthArguments.size() > 4)
            return nullptr;

        widthArguments.append(createPrimitiveNumericValue(argumentWithCalculation));
        argument = args.next();
    }

    switch (widthArguments.size()) {
    case 1: {
        shape->updateShapeSize1Value(widthArguments[0].get());
        break;
    }
    case 2: {
        shape->updateShapeSize2Values(widthArguments[0].get(), widthArguments[1].get());
        break;
        }
    case 3: {
        shape->updateShapeSize3Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get());
        break;
    }
    case 4: {
        shape->updateShapeSize4Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get(), widthArguments[3].get());
        break;
    }
    default:
        return nullptr;
    }

    if (hasRoundedInset)
        return parseInsetRoundedCorners(shape, args);
    return shape;
}

RefPtr<CSSPrimitiveValue> CSSParser::parseShapeRadius(CSSParserValue& value)
{
    if (value.id == CSSValueClosestSide || value.id == CSSValueFarthestSide)
        return CSSValuePool::singleton().createIdentifierValue(value.id);

    ValueWithCalculation valueWithCalculation(value);
    if (!validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg))
        return nullptr;

    return createPrimitiveNumericValue(valueWithCalculation);
}

RefPtr<CSSBasicShape> CSSParser::parseBasicShapeCircle(CSSParserValueList& args)
{
    // circle(radius)
    // circle(radius at <position>)
    // circle(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtr<CSSBasicShapeCircle> shape = CSSBasicShapeCircle::create();

    for (CSSParserValue* argument = args.current(); argument; argument = args.next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first two. Thus, and index greater than one
        // indicates an invalid production.
        if (args.currentIndex() > 1)
            return nullptr;

        if (!args.currentIndex() && argument->id != CSSValueAt) {
            if (RefPtr<CSSPrimitiveValue> radius = parseShapeRadius(*argument)) {
                shape->setRadius(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id == CSSValueAt && args.next()) {
            RefPtr<CSSPrimitiveValue> centerX;
            RefPtr<CSSPrimitiveValue> centerY;
            parseFillPosition(args, centerX, centerY);
            if (centerX && centerY && !args.current()) {
                shape->setCenterX(centerX);
                shape->setCenterY(centerY);
            } else
                return nullptr;
        } else
            return nullptr;
    }

    return shape;
}

RefPtr<CSSBasicShape> CSSParser::parseBasicShapeEllipse(CSSParserValueList& args)
{
    // ellipse(radiusX)
    // ellipse(radiusX at <position>)
    // ellipse(radiusX radiusY)
    // ellipse(radiusX radiusY at <position>)
    // ellipse(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtr<CSSBasicShapeEllipse> shape = CSSBasicShapeEllipse::create();

    for (CSSParserValue* argument = args.current(); argument; argument = args.next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first three. Thus, an index greater than two
        // indicates an invalid production.
        if (args.currentIndex() > 2)
            return nullptr;

        if (args.currentIndex() < 2 && argument->id != CSSValueAt) {
            if (RefPtr<CSSPrimitiveValue> radius = parseShapeRadius(*argument)) {
                if (!shape->radiusX())
                    shape->setRadiusX(radius);
                else
                    shape->setRadiusY(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id != CSSValueAt || !args.next()) // expecting ellipse(.. at <position>)
            return nullptr;

        RefPtr<CSSPrimitiveValue> centerX;
        RefPtr<CSSPrimitiveValue> centerY;
        parseFillPosition(args, centerX, centerY);
        if (!centerX || !centerY || args.current())
            return nullptr;

        shape->setCenterX(centerX);
        shape->setCenterY(centerY);
    }

    return shape;
}

RefPtr<CSSBasicShape> CSSParser::parseBasicShapePolygon(CSSParserValueList& args)
{
    unsigned size = args.size();
    if (!size)
        return nullptr;

    RefPtr<CSSBasicShapePolygon> shape = CSSBasicShapePolygon::create();

    CSSParserValue* argument = args.current();
    if (argument->id == CSSValueEvenodd || argument->id == CSSValueNonzero) {
        shape->setWindRule(argument->id == CSSValueEvenodd ? RULE_EVENODD : RULE_NONZERO);

        if (!isComma(args.next()))
            return nullptr;

        argument = args.next();
        size -= 2;
    }

    // <length> <length>, ... <length> <length> -> each pair has 3 elements except the last one
    if (!size || (size % 3) - 2)
        return nullptr;

    CSSParserValue* argumentX = argument;
    while (argumentX) {

        ValueWithCalculation argumentXWithCalculation(*argumentX);
        if (!validateUnit(argumentXWithCalculation, FLength | FPercent))
            return nullptr;
        RefPtr<CSSPrimitiveValue> xLength = createPrimitiveNumericValue(argumentXWithCalculation);

        CSSParserValue* argumentY = args.next();
        if (!argumentY)
            return nullptr;
        ValueWithCalculation argumentYWithCalculation(*argumentY);
        if (!validateUnit(argumentYWithCalculation, FLength | FPercent))
            return nullptr;
        RefPtr<CSSPrimitiveValue> yLength = createPrimitiveNumericValue(argumentYWithCalculation);

        shape->appendPoint(xLength.release(), yLength.release());

        CSSParserValue* commaOrNull = args.next();
        if (!commaOrNull)
            argumentX = nullptr;
        else if (!isComma(commaOrNull)) 
            return nullptr;
        else 
            argumentX = args.next();
    }

    return shape;
}

RefPtr<CSSBasicShape> CSSParser::parseBasicShapePath(CSSParserValueList& args)
{
    unsigned size = args.size();
    if (size != 1 && size != 3)
        return nullptr;

    WindRule windRule = RULE_NONZERO;

    CSSParserValue* argument = args.current();
    if (argument->id == CSSValueEvenodd || argument->id == CSSValueNonzero) {
        windRule = argument->id == CSSValueEvenodd ? RULE_EVENODD : RULE_NONZERO;

        if (!isComma(args.next()))
            return nullptr;
        argument = args.next();
    }

    if (argument->unit != CSSPrimitiveValue::CSS_STRING)
        return nullptr;

    auto byteStream = std::make_unique<SVGPathByteStream>();
    if (!buildSVGPathByteStreamFromString(argument->string, *byteStream, UnalteredParsing))
        return nullptr;

    RefPtr<CSSBasicShapePath> shape = CSSBasicShapePath::create(WTFMove(byteStream));
    shape->setWindRule(windRule);

    args.next();
    return shape;
}

static bool isBoxValue(CSSValueID valueId, CSSPropertyID propId)
{
    switch (valueId) {
    case CSSValueContentBox:
    case CSSValuePaddingBox:
    case CSSValueBorderBox:
    case CSSValueMarginBox:
        return true;
    case CSSValueFill:
    case CSSValueStroke:
    case CSSValueViewBox:
        return propId == CSSPropertyWebkitClipPath;
    default: break;
    }

    return false;
}

RefPtr<CSSValue> CSSParser::parseBasicShapeAndOrBox(CSSPropertyID propId)
{
    CSSParserValue* value = m_valueList->current();

    bool shapeFound = false;
    bool boxFound = false;
    CSSValueID valueId;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (unsigned i = 0; i < 2; ++i) {
        if (!value)
            break;
        valueId = value->id;
        if (value->unit == CSSParserValue::Function && !shapeFound) {
            // parseBasicShape already asks for the next value list item.
            RefPtr<CSSPrimitiveValue> shapeValue = parseBasicShape();
            if (!shapeValue)
                return nullptr;
            list->append(shapeValue.releaseNonNull());
            shapeFound = true;
        } else if (isBoxValue(valueId, propId) && !boxFound) {
            RefPtr<CSSPrimitiveValue> parsedValue = CSSValuePool::singleton().createIdentifierValue(valueId);
            list->append(parsedValue.releaseNonNull());
            boxFound = true;
            m_valueList->next();
        } else
            return nullptr;
        value = m_valueList->current();
    }

    if (m_valueList->current())
        return nullptr;
    return list;
}

#if ENABLE(CSS_SHAPES)
RefPtr<CSSValue> CSSParser::parseShapeProperty(CSSPropertyID propId)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().cssShapesEnabled())
        return nullptr;

    CSSParserValue& value = *m_valueList->current();
    CSSValueID valueId = value.id;

    if (valueId == CSSValueNone) {
        m_valueList->next();
        return CSSValuePool::singleton().createIdentifierValue(valueId);
    }

    RefPtr<CSSValue> imageValue;
    if (valueId != CSSValueNone && parseFillImage(*m_valueList, imageValue)) {
        m_valueList->next();
        return imageValue;
    }

    return parseBasicShapeAndOrBox(propId);
}
#endif

RefPtr<CSSValue> CSSParser::parseClipPath()
{
    CSSParserValue& value = *m_valueList->current();
    CSSValueID valueId = value.id;

    if (valueId == CSSValueNone) {
        m_valueList->next();
        return CSSValuePool::singleton().createIdentifierValue(valueId);
    }
    if (value.unit == CSSPrimitiveValue::CSS_URI) {
        m_valueList->next();
        return CSSPrimitiveValue::create(value.string, CSSPrimitiveValue::CSS_URI);
    }

    return parseBasicShapeAndOrBox(CSSPropertyWebkitClipPath);
}

RefPtr<CSSPrimitiveValue> CSSParser::parseBasicShape()
{
    CSSParserValue& value = *m_valueList->current();
    ASSERT(value.unit == CSSParserValue::Function);
    CSSParserValueList* args = value.function->args.get();

    if (!args)
        return nullptr;

    RefPtr<CSSBasicShape> shape;
    if (equalLettersIgnoringASCIICase(value.function->name, "circle("))
        shape = parseBasicShapeCircle(*args);
    else if (equalLettersIgnoringASCIICase(value.function->name, "ellipse("))
        shape = parseBasicShapeEllipse(*args);
    else if (equalLettersIgnoringASCIICase(value.function->name, "polygon("))
        shape = parseBasicShapePolygon(*args);
    else if (equalLettersIgnoringASCIICase(value.function->name, "path("))
        shape = parseBasicShapePath(*args);
    else if (equalLettersIgnoringASCIICase(value.function->name, "inset("))
        shape = parseBasicShapeInset(*args);

    if (!shape)
        return nullptr;

    m_valueList->next();
    return CSSValuePool::singleton().createValue(shape.releaseNonNull());
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSParser::parseFont(bool important)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    for (unsigned i = 0; i < m_valueList->size(); ++i) {
        if (m_valueList->valueAt(i)->id == CSSValueInherit || m_valueList->valueAt(i)->id == CSSValueInitial)
            return false;
    }

    ShorthandScope scope(this, CSSPropertyFont);
    // Optional font-style, font-variant and font-weight.
    bool fontStyleParsed = false;
    bool fontVariantParsed = false;
    bool fontWeightParsed = false;
    CSSParserValue* value;
    while ((value = m_valueList->current())) {
        if (!fontStyleParsed && isValidKeywordPropertyAndValue(CSSPropertyFontStyle, value->id, m_context, m_styleSheet)) {
            addProperty(CSSPropertyFontStyle, CSSValuePool::singleton().createIdentifierValue(value->id), important);
            fontStyleParsed = true;
        } else if (!fontVariantParsed && (value->id == CSSValueNormal || value->id == CSSValueSmallCaps)) {
            // Font variant in the shorthand is particular, it only accepts normal or small-caps.
            addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(value->id), important);
            fontVariantParsed = true;
        } else if (!fontWeightParsed && parseFontWeight(important))
            fontWeightParsed = true;
        else
            break;
        m_valueList->next();
    }

    if (!value)
        return false;

    if (!fontStyleParsed)
        addProperty(CSSPropertyFontStyle, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
    if (!fontVariantParsed)
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);
    if (!fontWeightParsed)
        addProperty(CSSPropertyFontWeight, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);

    // Now a font size _must_ come.
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (!parseFontSize(important))
        return false;

    value = m_valueList->current();
    if (!value)
        return false;

    if (isForwardSlashOperator(*value)) {
        // The line-height property.
        value = m_valueList->next();
        if (!value)
            return false;
        if (!parseLineHeight(important))
            return false;
    } else
        addProperty(CSSPropertyLineHeight, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important, true);

    // Font family must come now.
    RefPtr<CSSValue> parsedFamilyValue = parseFontFamily();
    if (!parsedFamilyValue)
        return false;

    addProperty(CSSPropertyFontFamily, parsedFamilyValue.release(), important);

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires that
    // "font-stretch", "font-size-adjust", and "font-kerning" be reset to their initial values
    // but we don't seem to support them at the moment. They should also be added here once implemented.
    if (m_valueList->current())
        return false;

    return true;
}

void CSSParser::parseSystemFont(bool important)
{
    ASSERT(m_valueList->size() == 1);
    CSSValueID systemFontID = m_valueList->valueAt(0)->id;
    ASSERT(systemFontID >= CSSValueCaption && systemFontID <= CSSValueStatusBar);
    m_valueList->next();

    FontCascadeDescription fontDescription;
    RenderTheme::defaultTheme()->systemFont(systemFontID, fontDescription);
    if (!fontDescription.isAbsoluteSize())
        return;

    // We must set font's constituent properties, even for system fonts, so the cascade functions correctly.
    ShorthandScope scope(this, CSSPropertyFont);
    addProperty(CSSPropertyFontStyle, CSSValuePool::singleton().createIdentifierValue(fontDescription.italic() == FontItalicOn ? CSSValueItalic : CSSValueNormal), important);
    addProperty(CSSPropertyFontWeight, CSSValuePool::singleton().createValue(fontDescription.weight()), important);
    addProperty(CSSPropertyFontSize, CSSValuePool::singleton().createValue(fontDescription.specifiedSize(), CSSPrimitiveValue::CSS_PX), important);
    Ref<CSSValueList> fontFamilyList = CSSValueList::createCommaSeparated();
    fontFamilyList->append(CSSValuePool::singleton().createFontFamilyValue(fontDescription.familyAt(0), FromSystemFontID::Yes));
    addProperty(CSSPropertyFontFamily, WTFMove(fontFamilyList), important);
    addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important);
    addProperty(CSSPropertyLineHeight, CSSValuePool::singleton().createIdentifierValue(CSSValueNormal), important);
}

class FontFamilyValueBuilder {
public:
    FontFamilyValueBuilder(CSSValueList& list)
        : m_list(list)
    {
    }

    void add(const CSSParserString& string)
    {
        if (!m_builder.isEmpty())
            m_builder.append(' ');

        if (string.is8Bit()) {
            m_builder.append(string.characters8(), string.length());
            return;
        }

        m_builder.append(string.characters16(), string.length());
    }

    void commit()
    {
        if (m_builder.isEmpty())
            return;
        m_list.append(CSSValuePool::singleton().createFontFamilyValue(m_builder.toString()));
        m_builder.clear();
    }

private:
    StringBuilder m_builder;
    CSSValueList& m_list;
};

static bool valueIsCSSKeyword(const CSSParserValue& value)
{
    // FIXME: when we add "unset", we should handle it here.
    return value.id == CSSValueInitial || value.id == CSSValueInherit || value.id == CSSValueDefault;
}

RefPtr<CSSValueList> CSSParser::parseFontFamily()
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    CSSParserValue* value = m_valueList->current();

    FontFamilyValueBuilder familyBuilder(*list);
    bool inFamily = false;

    while (value) {
        CSSParserValue* nextValue = m_valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == CSSParserValue::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSSValueSerif && nextValue->id <= CSSValueWebkitBody) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        bool valueIsKeyword = valueIsCSSKeyword(*value);
        if (valueIsKeyword && !inFamily) {
            if (nextValBreaksFont)
                value = m_valueList->next();
            else if (nextValIsFontName)
                value = nextValue;
            continue;
        }

        if (value->id >= CSSValueSerif && value->id <= CSSValueWebkitBody) {
            if (inFamily)
                familyBuilder.add(value->string);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            else {
                familyBuilder.commit();
                familyBuilder.add(value->string);
                inFamily = true;
            }
        } else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            inFamily = false;
            familyBuilder.commit();
            list->append(CSSValuePool::singleton().createFontFamilyValue(value->string));
        } else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (inFamily)
                familyBuilder.add(value->string);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(CSSValuePool::singleton().createFontFamilyValue(value->string));
            else {
                familyBuilder.commit();
                familyBuilder.add(value->string);
                inFamily = true;
            }
        } else {
            break;
        }

        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = m_valueList->next();
            familyBuilder.commit();
            inFamily = false;
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }
    familyBuilder.commit();

    if (!list->length())
        list = nullptr;
    return list;
}

bool CSSParser::parseLineHeight(bool important)
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    CSSValueID id = valueWithCalculation.value().id;
    bool validPrimitive = false;
    // normal | <number> | <length> | <percentage> | inherit
    if (id == CSSValueNormal)
        validPrimitive = true;
    else
        validPrimitive = (!id && validateUnit(valueWithCalculation, FNumber | FLength | FPercent | FNonNeg));
    if (validPrimitive && (!m_valueList->next() || inShorthand()))
        addProperty(CSSPropertyLineHeight, parseValidPrimitive(id, valueWithCalculation), important);
    return validPrimitive;
}

bool CSSParser::parseFontSize(bool important)
{
    ValueWithCalculation valueWithCalculation(*m_valueList->current());
    CSSValueID id = valueWithCalculation.value().id;
    bool validPrimitive = false;
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (id >= CSSValueXxSmall && id <= CSSValueLarger)
        validPrimitive = true;
    else
        validPrimitive = validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg);
    if (validPrimitive && (!m_valueList->next() || inShorthand()))
        addProperty(CSSPropertyFontSize, parseValidPrimitive(id, valueWithCalculation), important);
    return validPrimitive;
}

static CSSValueID createFontWeightValueKeyword(int weight)
{
    ASSERT(!(weight % 100) && weight >= 100 && weight <= 900);
    CSSValueID value = static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1);
    ASSERT(value >= CSSValue100 && value <= CSSValue900);
    return value;
}

bool CSSParser::parseFontWeight(bool important)
{
    CSSParserValue& value = *m_valueList->current();
    if ((value.id >= CSSValueNormal) && (value.id <= CSSValue900)) {
        addProperty(CSSPropertyFontWeight, CSSValuePool::singleton().createIdentifierValue(value.id), important);
        return true;
    }
    ValueWithCalculation valueWithCalculation(value);
    if (validateUnit(valueWithCalculation, FInteger | FNonNeg, CSSQuirksMode)) {
        int weight = static_cast<int>(parsedDouble(valueWithCalculation));
        if (!(weight % 100) && weight >= 100 && weight <= 900) {
            addProperty(CSSPropertyFontWeight, CSSValuePool::singleton().createIdentifierValue(createFontWeightValueKeyword(weight)), important);
            return true;
        }
    }
    return false;
}

bool CSSParser::parseFontSynthesis(bool important)
{
    // none | [ weight || style ]
    CSSParserValue* value = m_valueList->current();
    if (value && value->id == CSSValueNone) {
        addProperty(CSSPropertyFontSynthesis, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        m_valueList->next();
        return true;
    }

    bool encounteredWeight = false;
    bool encounteredStyle = false;
    while (value) {
        switch (value->id) {
        case CSSValueWeight:
            encounteredWeight = true;
            break;
        case CSSValueStyle:
            encounteredStyle = true;
            break;
        default:
            return false;
        }
        value = m_valueList->next();
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (encounteredWeight)
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueWeight));
    if (encounteredStyle)
        list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueStyle));
    addProperty(CSSPropertyFontSynthesis, list.release(), important);
    return true;
}

bool CSSParser::parseFontFaceSrcURI(CSSValueList& valueList)
{
    RefPtr<CSSFontFaceSrcValue> uriValue(CSSFontFaceSrcValue::create(completeURL(m_valueList->current()->string)));

    CSSParserValue* value = m_valueList->next();
    if (!value) {
        valueList.append(uriValue.releaseNonNull());
        return true;
    }
    if (value->unit == CSSParserValue::Operator && value->iValue == ',') {
        m_valueList->next();
        valueList.append(uriValue.releaseNonNull());
        return true;
    }

    if (value->unit != CSSParserValue::Function || !equalLettersIgnoringASCIICase(value->function->name, "format("))
        return false;

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20111004/ says that format() contains a comma-separated list of strings,
    // but CSSFontFaceSrcValue stores only one format. Allowing one format for now.
    CSSParserValueList* args = value->function->args.get();
    if (!args || args->size() != 1 || (args->current()->unit != CSSPrimitiveValue::CSS_STRING && args->current()->unit != CSSPrimitiveValue::CSS_IDENT))
        return false;
    uriValue->setFormat(args->current()->string);
    valueList.append(uriValue.releaseNonNull());
    value = m_valueList->next();
    if (value && value->unit == CSSParserValue::Operator && value->iValue == ',')
        m_valueList->next();
    return true;
}

bool CSSParser::parseFontFaceSrcLocal(CSSValueList& valueList)
{
    CSSParserValueList* args = m_valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    if (args->size() == 1 && args->current()->unit == CSSPrimitiveValue::CSS_STRING)
        valueList.append(CSSFontFaceSrcValue::createLocal(args->current()->string));
    else if (args->current()->unit == CSSPrimitiveValue::CSS_IDENT) {
        StringBuilder builder;
        for (CSSParserValue* localValue = args->current(); localValue; localValue = args->next()) {
            if (localValue->unit != CSSPrimitiveValue::CSS_IDENT)
                return false;
            if (!builder.isEmpty())
                builder.append(' ');
            builder.append(localValue->string);
        }
        valueList.append(CSSFontFaceSrcValue::createLocal(builder.toString()));
    } else
        return false;

    if (CSSParserValue* value = m_valueList->next()) {
        if (value->unit == CSSParserValue::Operator && value->iValue == ',')
            m_valueList->next();
    }
    return true;
}

bool CSSParser::parseFontFaceSrc()
{
    RefPtr<CSSValueList> values(CSSValueList::createCommaSeparated());

    while (CSSParserValue* value = m_valueList->current()) {
        if (value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!parseFontFaceSrcURI(*values))
                return false;
        } else if (value->unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(value->function->name, "local(")) {
            if (!parseFontFaceSrcLocal(*values))
                return false;
        } else
            return false;
    }
    if (!values->length())
        return false;

    addProperty(CSSPropertySrc, values.release(), m_important);
    m_valueList->next();
    return true;
}

bool CSSParser::parseFontFaceUnicodeRange()
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    bool failed = false;
    bool operatorExpected = false;
    for (; m_valueList->current(); m_valueList->next(), operatorExpected = !operatorExpected) {
        if (operatorExpected) {
            if (m_valueList->current()->unit == CSSParserValue::Operator && m_valueList->current()->iValue == ',')
                continue;
            failed = true;
            break;
        }
        if (m_valueList->current()->unit != CSSPrimitiveValue::CSS_UNICODE_RANGE) {
            failed = true;
            break;
        }

        String rangeString = m_valueList->current()->string;
        UChar32 from = 0;
        UChar32 to = 0;
        unsigned length = rangeString.length();

        if (length < 3) {
            failed = true;
            break;
        }

        unsigned i = 2;
        while (i < length) {
            UChar c = rangeString[i];
            if (c == '-' || c == '?')
                break;
            from *= 16;
            if (c >= '0' && c <= '9')
                from += c - '0';
            else if (c >= 'A' && c <= 'F')
                from += 10 + c - 'A';
            else if (c >= 'a' && c <= 'f')
                from += 10 + c - 'a';
            else {
                failed = true;
                break;
            }
            i++;
        }
        if (failed)
            break;

        if (i == length)
            to = from;
        else if (rangeString[i] == '?') {
            unsigned span = 1;
            while (i < length && rangeString[i] == '?') {
                span *= 16;
                from *= 16;
                i++;
            }
            if (i < length)
                failed = true;
            to = from + span - 1;
        } else {
            if (length < i + 2) {
                failed = true;
                break;
            }
            i++;
            while (i < length) {
                UChar c = rangeString[i];
                to *= 16;
                if (c >= '0' && c <= '9')
                    to += c - '0';
                else if (c >= 'A' && c <= 'F')
                    to += 10 + c - 'A';
                else if (c >= 'a' && c <= 'f')
                    to += 10 + c - 'a';
                else {
                    failed = true;
                    break;
                }
                i++;
            }
            if (failed)
                break;
        }
        if (from <= to)
            values->append(CSSUnicodeRangeValue::create(from, to));
    }
    if (failed || !values->length())
        return false;
    addProperty(CSSPropertyUnicodeRange, values.release(), m_important);
    return true;
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int checkForValidDouble(const CharacterType* string, const CharacterType* end, const char terminator)
{
    int length = end - string;
    if (length < 1)
        return 0;

    bool decimalMarkSeen = false;
    int processedLength = 0;

    for (int i = 0; i < length; ++i) {
        if (string[i] == terminator) {
            processedLength = i;
            break;
        }
        if (!isASCIIDigit(string[i])) {
            if (!decimalMarkSeen && string[i] == '.')
                decimalMarkSeen = true;
            else
                return 0;
        }
    }

    if (decimalMarkSeen && processedLength == 1)
        return 0;

    return processedLength;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int parseDouble(const CharacterType* string, const CharacterType* end, const char terminator, double& value)
{
    int length = checkForValidDouble(string, end, terminator);
    if (!length)
        return 0;

    int position = 0;
    double localValue = 0;

    // The consumed characters here are guaranteed to be
    // ASCII digits with or without a decimal mark
    for (; position < length; ++position) {
        if (string[position] == '.')
            break;
        localValue = localValue * 10 + string[position] - '0';
    }

    if (++position == length) {
        value = localValue;
        return length;
    }

    double fraction = 0;
    double scale = 1;

    while (position < length && scale < MAX_SCALE) {
        fraction = fraction * 10 + string[position++] - '0';
        scale *= 10;
    }

    value = localValue + fraction / scale;
    return length;
}

template <typename CharacterType>
static bool parseColorIntOrPercentage(const CharacterType*& string, const CharacterType* end, const char terminator, CSSPrimitiveValue::UnitTypes& expect, int& value)
{
    const CharacterType* current = string;
    double localValue = 0;
    bool negative = false;
    while (current != end && isHTMLSpace(*current))
        current++;
    if (current != end && *current == '-') {
        negative = true;
        current++;
    }
    if (current == end || !isASCIIDigit(*current))
        return false;
    while (current != end && isASCIIDigit(*current)) {
        double newValue = localValue * 10 + *current++ - '0';
        if (newValue >= 255) {
            // Clamp values at 255.
            localValue = 255;
            while (current != end && isASCIIDigit(*current))
                ++current;
            break;
        }
        localValue = newValue;
    }

    if (current == end)
        return false;

    if (expect == CSSPrimitiveValue::CSS_NUMBER && (*current == '.' || *current == '%'))
        return false;

    if (*current == '.') {
        // We already parsed the integral part, try to parse
        // the fraction part of the percentage value.
        double percentage = 0;
        int numCharactersParsed = parseDouble(current, end, '%', percentage);
        if (!numCharactersParsed)
            return false;
        current += numCharactersParsed;
        if (*current != '%')
            return false;
        localValue += percentage;
    }

    if (expect == CSSPrimitiveValue::CSS_PERCENTAGE && *current != '%')
        return false;

    if (*current == '%') {
        expect = CSSPrimitiveValue::CSS_PERCENTAGE;
        localValue = localValue / 100.0 * 256.0;
        // Clamp values at 255 for percentages over 100%
        if (localValue > 255)
            localValue = 255;
        current++;
    } else
        expect = CSSPrimitiveValue::CSS_NUMBER;

    while (current != end && isHTMLSpace(*current))
        current++;
    if (current == end || *current++ != terminator)
        return false;
    // Clamp negative values at zero.
    value = negative ? 0 : static_cast<int>(localValue);
    string = current;
    return true;
}

template <typename CharacterType>
static inline bool isTenthAlpha(const CharacterType* string, const int length)
{
    // "0.X"
    if (length == 3 && string[0] == '0' && string[1] == '.' && isASCIIDigit(string[2]))
        return true;

    // ".X"
    if (length == 2 && string[0] == '.' && isASCIIDigit(string[1]))
        return true;

    return false;
}

template <typename CharacterType>
static inline bool parseAlphaValue(const CharacterType*& string, const CharacterType* end, const char terminator, int& value)
{
    while (string != end && isHTMLSpace(*string))
        string++;

    bool negative = false;

    if (string != end && *string == '-') {
        negative = true;
        string++;
    }

    value = 0;

    int length = end - string;
    if (length < 2)
        return false;

    if (string[length - 1] != terminator || !isASCIIDigit(string[length - 2]))
        return false;

    if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
        if (checkForValidDouble(string, end, terminator)) {
            value = negative ? 0 : 255;
            string = end;
            return true;
        }
        return false;
    }

    if (length == 2 && string[0] != '.') {
        value = !negative && string[0] == '1' ? 255 : 0;
        string = end;
        return true;
    }

    if (isTenthAlpha(string, length - 1)) {
        static const int tenthAlphaValues[] = { 0, 25, 51, 76, 102, 127, 153, 179, 204, 230 };
        value = negative ? 0 : tenthAlphaValues[string[length - 2] - '0'];
        string = end;
        return true;
    }

    double alpha = 0;
    if (!parseDouble(string, end, terminator, alpha))
        return false;
    value = negative ? 0 : static_cast<int>(alpha * nextafter(256.0, 0.0));
    string = end;
    return true;
}

template <typename CharacterType>
static inline bool mightBeRGBA(const CharacterType* characters, unsigned length)
{
    if (length < 5)
        return false;
    return characters[4] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b')
        && isASCIIAlphaCaselessEqual(characters[3], 'a');
}

template <typename CharacterType>
static inline bool mightBeRGB(const CharacterType* characters, unsigned length)
{
    if (length < 4)
        return false;
    return characters[3] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b');
}

template <typename CharacterType>
static inline bool fastParseColorInternal(RGBA32& rgb, const CharacterType* characters, unsigned length , bool strict)
{
    CSSPrimitiveValue::UnitTypes expect = CSSPrimitiveValue::CSS_UNKNOWN;
    
    if (!strict && length >= 3) {
        if (characters[0] == '#') {
            if (Color::parseHexColor(characters + 1, length - 1, rgb))
                return true;
        } else {
            if (Color::parseHexColor(characters, length, rgb))
                return true;
        }
    }

    // Try rgba() syntax.
    if (mightBeRGBA(characters, length)) {
        const CharacterType* current = characters + 5;
        const CharacterType* end = characters + length;
        int red;
        int green;
        int blue;
        int alpha;
        
        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, blue))
            return false;
        if (!parseAlphaValue(current, end, ')', alpha))
            return false;
        if (current != end)
            return false;
        rgb = makeRGBA(red, green, blue, alpha);
        return true;
    }

    // Try rgb() syntax.
    if (mightBeRGB(characters, length)) {
        const CharacterType* current = characters + 4;
        const CharacterType* end = characters + length;
        int red;
        int green;
        int blue;
        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return false;
        if (!parseColorIntOrPercentage(current, end, ')', expect, blue))
            return false;
        if (current != end)
            return false;
        rgb = makeRGB(red, green, blue);
        return true;
    }

    return false;
}

template<typename StringType>
bool CSSParser::fastParseColor(RGBA32& rgb, const StringType& name, bool strict)
{
    unsigned length = name.length();
    bool parseResult;

    if (!length)
        return false;

    if (name.is8Bit())
        parseResult = fastParseColorInternal(rgb, name.characters8(), length, strict);
    else
        parseResult = fastParseColorInternal(rgb, name.characters16(), length, strict);

    if (parseResult)
        return true;

    // Try named colors.
    Color tc;
    tc.setNamedColor(name);
    if (tc.isValid()) {
        rgb = tc.rgb();
        return true;
    }
    return false;
}
    
inline double CSSParser::parsedDouble(ValueWithCalculation& valueWithCalculation)
{
    return valueWithCalculation.calculation() ? valueWithCalculation.calculation()->doubleValue() : valueWithCalculation.value().fValue;
}

bool CSSParser::isCalculation(CSSParserValue& value)
{
    return value.unit == CSSParserValue::Function
        && (equalLettersIgnoringASCIICase(value.function->name, "calc(")
            || equalLettersIgnoringASCIICase(value.function->name, "-webkit-calc("));
}

inline int CSSParser::colorIntFromValue(ValueWithCalculation& valueWithCalculation)
{
    bool isPercent;
    
    if (valueWithCalculation.calculation())
        isPercent = valueWithCalculation.calculation()->category() == CalcPercent;
    else
        isPercent = valueWithCalculation.value().unit == CSSPrimitiveValue::CSS_PERCENTAGE;

    const double doubleValue = parsedDouble(valueWithCalculation);
    
    if (doubleValue <= 0.0)
        return 0;

    if (isPercent) {
        if (doubleValue >= 100.0)
            return 255;
        return static_cast<int>(doubleValue * 256.0 / 100.0);
    }

    if (doubleValue >= 255.0)
        return 255;

    return static_cast<int>(doubleValue);
}

bool CSSParser::parseColorParameters(CSSParserValue& value, int* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value.function->args.get();
    ValueWithCalculation firstArgumentWithCalculation(*args->current());
    Units unitType = FUnknown;
    // Get the first value and its type
    if (validateUnit(firstArgumentWithCalculation, FInteger, CSSStrictMode))
        unitType = FInteger;
    else if (validateUnit(firstArgumentWithCalculation, FPercent, CSSStrictMode))
        unitType = FPercent;
    else
        return false;
    
    colorArray[0] = colorIntFromValue(firstArgumentWithCalculation);
    for (int i = 1; i < 3; i++) {
        CSSParserValue& operatorArgument = *args->next();
        if (operatorArgument.unit != CSSParserValue::Operator && operatorArgument.iValue != ',')
            return false;
        ValueWithCalculation argumentWithCalculation(*args->next());
        if (!validateUnit(argumentWithCalculation, unitType, CSSStrictMode))
            return false;
        colorArray[i] = colorIntFromValue(argumentWithCalculation);
    }
    if (parseAlpha) {
        CSSParserValue& operatorArgument = *args->next();
        if (operatorArgument.unit != CSSParserValue::Operator && operatorArgument.iValue != ',')
            return false;
        ValueWithCalculation argumentWithCalculation(*args->next());
        if (!validateUnit(argumentWithCalculation, FNumber, CSSStrictMode))
            return false;
        double doubleValue = parsedDouble(argumentWithCalculation);
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        colorArray[3] = static_cast<int>(std::max<double>(0, std::min<double>(1, doubleValue)) * nextafter(256.0, 0.0));
    }
    return true;
}

// The CSS3 specification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSParser::parseHSLParameters(CSSParserValue& value, double* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value.function->args.get();
    ValueWithCalculation firstArgumentWithCalculation(*args->current());
    // Get the first value
    if (!validateUnit(firstArgumentWithCalculation, FNumber, CSSStrictMode))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(parsedDouble(firstArgumentWithCalculation)) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; ++i) {
        CSSParserValue& operatorArgument = *args->next();
        if (operatorArgument.unit != CSSParserValue::Operator && operatorArgument.iValue != ',')
            return false;
        ValueWithCalculation argumentWithCalculation(*args->next());
        if (!validateUnit(argumentWithCalculation, FPercent, CSSStrictMode))
            return false;
        colorArray[i] = std::max<double>(0, std::min<double>(100, parsedDouble(argumentWithCalculation))) / 100.0; // needs to be value between 0 and 1.0
    }
    if (parseAlpha) {
        CSSParserValue& operatorArgument = *args->next();
        if (operatorArgument.unit != CSSParserValue::Operator && operatorArgument.iValue != ',')
            return false;
        ValueWithCalculation argumentWithCalculation(*args->next());
        if (!validateUnit(argumentWithCalculation, FNumber, CSSStrictMode))
            return false;
        colorArray[3] = std::max<double>(0, std::min<double>(1, parsedDouble(argumentWithCalculation)));
    }
    return true;
}

RefPtr<CSSPrimitiveValue> CSSParser::parseColor(CSSParserValue* value)
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(value ? *value : *m_valueList->current(), c))
        return nullptr;
    return CSSValuePool::singleton().createColorValue(c);
}

bool CSSParser::parseColorFromValue(CSSParserValue& value, RGBA32& c)
{
    if (inQuirksMode() && value.unit == CSSPrimitiveValue::CSS_NUMBER
        && value.fValue >= 0. && value.fValue < 1000000.) {
        String str = String::format("%06d", static_cast<int>((value.fValue+.5)));
        // FIXME: This should be strict parsing for SVG as well.
        if (!fastParseColor(c, str, inStrictMode()))
            return false;
    } else if (value.unit == CSSPrimitiveValue::CSS_PARSER_HEXCOLOR
        || value.unit == CSSPrimitiveValue::CSS_IDENT
        || (inQuirksMode() && value.unit == CSSPrimitiveValue::CSS_DIMENSION)) {
        if (!fastParseColor(c, value.string, inStrictMode() && value.unit == CSSPrimitiveValue::CSS_IDENT))
            return false;
    } else if (value.unit == CSSParserValue::Function
        && value.function->args
        && value.function->args->size() == 5 /* rgb + two commas */
        && equalLettersIgnoringASCIICase(value.function->name, "rgb(")) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        c = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else {
        if (value.unit == CSSParserValue::Function
            && value.function->args
            && value.function->args->size() == 7 /* rgba + three commas */
            && equalLettersIgnoringASCIICase(value.function->name, "rgba(")) {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            c = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value.unit == CSSParserValue::Function
            && value.function->args
            && value.function->args->size() == 5 /* hsl + two commas */
            && equalLettersIgnoringASCIICase(value.function->name, "hsl(")) {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value.unit == CSSParserValue::Function
            && value.function->args
            && value.function->args->size() == 7 /* hsla + three commas */
            && equalLettersIgnoringASCIICase(value.function->name, "hsla(")) {
            double colorValues[4];
            if (!parseHSLParameters(value, colorValues, true))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else
            return false;
    }

    return true;
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
struct ShadowParseContext {
    ShadowParseContext(CSSPropertyID prop, CSSParser& parser)
        : property(prop)
        , m_parser(parser)
        , allowX(true)
        , allowY(false)
        , allowBlur(false)
        , allowSpread(false)
        , allowColor(true)
        , allowStyle(prop == CSSPropertyWebkitBoxShadow || prop == CSSPropertyBoxShadow)
        , allowBreak(true)
    {
    }

    bool allowLength() { return allowX || allowY || allowBlur || allowSpread; }

    void commitValue()
    {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || spread || color || style) {
            if (!values)
                values = CSSValueList::createCommaSeparated();

            // Construct the current shadow value and add it to the list.
            values->append(CSSShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), style.release(), color.release()));
        }

        // Now reset for the next shadow value.
        x = nullptr;
        y = nullptr;
        blur = nullptr;
        spread = nullptr;
        style = nullptr;
        color = nullptr;

        allowX = true;
        allowColor = true;
        allowBreak = true;
        allowY = false;
        allowBlur = false;
        allowSpread = false;
        allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
    }

    void commitLength(CSSParser::ValueWithCalculation& valueWithCalculation)
    {
        RefPtr<CSSPrimitiveValue> primitiveValue = m_parser.createPrimitiveNumericValue(valueWithCalculation);

        if (allowX) {
            x = primitiveValue.release();
            allowX = false;
            allowY = true;
            allowColor = false;
            allowStyle = false;
            allowBreak = false;
        } else if (allowY) {
            y = primitiveValue.release();
            allowY = false;
            allowBlur = true;
            allowColor = true;
            allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
            allowBreak = true;
        } else if (allowBlur) {
            blur = primitiveValue.release();
            allowBlur = false;
            allowSpread = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
        } else if (allowSpread) {
            spread = primitiveValue.release();
            allowSpread = false;
        }
    }

    void commitColor(PassRefPtr<CSSPrimitiveValue> val)
    {
        color = val;
        allowColor = false;
        if (allowX) {
            allowStyle = false;
            allowBreak = false;
        } else {
            allowBlur = false;
            allowSpread = false;
            allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
        }
    }

    void commitStyle(CSSParserValue& value)
    {
        style = CSSValuePool::singleton().createIdentifierValue(value.id);
        allowStyle = false;
        if (allowX)
            allowBreak = false;
        else {
            allowBlur = false;
            allowSpread = false;
            allowColor = false;
        }
    }

    CSSPropertyID property;
    CSSParser& m_parser;

    RefPtr<CSSValueList> values;
    RefPtr<CSSPrimitiveValue> x;
    RefPtr<CSSPrimitiveValue> y;
    RefPtr<CSSPrimitiveValue> blur;
    RefPtr<CSSPrimitiveValue> spread;
    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowSpread;
    bool allowColor;
    bool allowStyle; // inset or not.
    bool allowBreak;
};

RefPtr<CSSValueList> CSSParser::parseShadow(CSSParserValueList& valueList, CSSPropertyID propId)
{
    ShadowParseContext context(propId, *this);
    CSSParserValue* value;
    while ((value = valueList.current())) {
        ValueWithCalculation valueWithCalculation(*value);
        // Check for a comma break first.
        if (value->unit == CSSParserValue::Operator) {
            if (value->iValue != ',' || !context.allowBreak) {
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return nullptr;
            }
            // -webkit-svg-shadow does not support multiple values.
            if (propId == CSSPropertyWebkitSvgShadow)
                return nullptr;
            // The value is good.  Commit it.
            context.commitValue();
        } else if (validateUnit(valueWithCalculation, FLength, CSSStrictMode)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return nullptr;

            // Blur radius must be non-negative.
            if (context.allowBlur && !validateUnit(valueWithCalculation, FLength | FNonNeg, CSSStrictMode))
                return nullptr;

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(valueWithCalculation);
        } else if (value->id == CSSValueInset) {
            if (!context.allowStyle)
                return nullptr;

            context.commitStyle(valueWithCalculation);
        } else {
            // The only other type of value that's ok is a color value.
            RefPtr<CSSPrimitiveValue> parsedColor;
            bool isColor = (isValidSystemColorValue(value->id) || value->id == CSSValueMenu
                || (value->id >= CSSValueWebkitFocusRingColor && value->id <= CSSValueWebkitText && inQuirksMode())
                || value->id == CSSValueCurrentcolor);
            if (isColor) {
                if (!context.allowColor)
                    return nullptr;
                parsedColor = CSSValuePool::singleton().createIdentifierValue(value->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColor(value);

            if (!parsedColor || !context.allowColor)
                return nullptr; // This value is not a color or length and is invalid or
                          // it is a color, but a color isn't allowed at this point.

            context.commitColor(parsedColor.release());
        }

        valueList.next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values && context.values->length())
            return context.values.release();
    }

    return nullptr;
}

bool CSSParser::parseReflect(CSSPropertyID propId, bool important)
{
    // box-reflect: <direction> <offset> <mask>

    // Direction comes first.
    CSSParserValue* value = m_valueList->current();
    RefPtr<CSSPrimitiveValue> direction;
    switch (value->id) {
        case CSSValueAbove:
        case CSSValueBelow:
        case CSSValueLeft:
        case CSSValueRight:
            direction = CSSValuePool::singleton().createIdentifierValue(value->id);
            break;
        default:
            return false;
    }

    // The offset comes next.
    value = m_valueList->next();
    RefPtr<CSSPrimitiveValue> offset;
    if (!value)
        offset = CSSValuePool::singleton().createValue(0, CSSPrimitiveValue::CSS_PX);
    else {
        ValueWithCalculation valueWithCalculation(*value);
        if (!validateUnit(valueWithCalculation, FLength | FPercent))
            return false;
        offset = createPrimitiveNumericValue(valueWithCalculation);
    }

    // Now for the mask.
    RefPtr<CSSValue> mask;
    value = m_valueList->next();
    if (value) {
        if (!parseBorderImage(propId, mask))
            return false;
    }

    addProperty(propId, CSSReflectValue::create(direction.release(), offset.release(), mask.release()), important);
    m_valueList->next();
    return true;
}

bool CSSParser::parseFlex(CSSParserValueList& args, bool important)
{
    if (!args.size() || args.size() > 3)
        return false;
    static const double unsetValue = -1;
    double flexGrow = unsetValue;
    double flexShrink = unsetValue;
    RefPtr<CSSPrimitiveValue> flexBasis;

    while (CSSParserValue* argument = args.current()) {
        ValueWithCalculation argumentWithCalculation(*argument);
        if (validateUnit(argumentWithCalculation, FNumber | FNonNeg)) {
            if (flexGrow == unsetValue)
                flexGrow = parsedDouble(argumentWithCalculation);
            else if (flexShrink == unsetValue)
                flexShrink = parsedDouble(argumentWithCalculation);
            else if (!parsedDouble(argumentWithCalculation)) {
                // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                flexBasis = CSSValuePool::singleton().createValue(0, CSSPrimitiveValue::CSS_PX);
            } else {
                // We only allow 3 numbers without units if the last value is 0. E.g., flex:1 1 1 is invalid.
                return false;
            }
        } else if (!flexBasis && (argumentWithCalculation.value().id == CSSValueAuto || validateUnit(argumentWithCalculation, FLength | FPercent | FNonNeg)))
            flexBasis = parseValidPrimitive(argumentWithCalculation.value().id, argumentWithCalculation);
        else {
            // Not a valid arg for flex.
            return false;
        }
        args.next();
    }

    if (flexGrow == unsetValue)
        flexGrow = 1;
    if (flexShrink == unsetValue)
        flexShrink = 1;
    if (!flexBasis)
        flexBasis = CSSValuePool::singleton().createValue(0, CSSPrimitiveValue::CSS_PX);

    addProperty(CSSPropertyFlexGrow, CSSValuePool::singleton().createValue(clampToFloat(flexGrow), CSSPrimitiveValue::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexShrink, CSSValuePool::singleton().createValue(clampToFloat(flexShrink), CSSPrimitiveValue::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexBasis, flexBasis, important);
    return true;
}

struct BorderImageParseContext {
    BorderImageParseContext()
    : m_canAdvance(false)
    , m_allowCommit(true)
    , m_allowImage(true)
    , m_allowImageSlice(true)
    , m_allowRepeat(true)
    , m_allowForwardSlashOperator(false)
    , m_requireWidth(false)
    , m_requireOutset(false)
    {}

    bool canAdvance() const { return m_canAdvance; }
    void setCanAdvance(bool canAdvance) { m_canAdvance = canAdvance; }

    bool allowCommit() const { return m_allowCommit; }
    bool allowImage() const { return m_allowImage; }
    bool allowImageSlice() const { return m_allowImageSlice; }
    bool allowRepeat() const { return m_allowRepeat; }
    bool allowForwardSlashOperator() const { return m_allowForwardSlashOperator; }

    bool requireWidth() const { return m_requireWidth; }
    bool requireOutset() const { return m_requireOutset; }

    void commitImage(PassRefPtr<CSSValue> image)
    {
        m_image = image;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImage = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowRepeat = !m_repeat;
    }
    void commitImageSlice(PassRefPtr<CSSBorderImageSliceValue> slice)
    {
        m_imageSlice = slice;
        m_canAdvance = true;
        m_allowCommit = m_allowForwardSlashOperator = true;
        m_allowImageSlice = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitForwardSlashOperator()
    {
        m_canAdvance = true;
        m_allowCommit = m_allowImage = m_allowImageSlice = m_allowRepeat = m_allowForwardSlashOperator = false;
        if (!m_borderSlice) {
            m_requireWidth = true;
            m_requireOutset = false;
        } else {
            m_requireOutset = true;
            m_requireWidth = false;
        }
    }
    void commitBorderWidth(PassRefPtr<CSSPrimitiveValue> slice)
    {
        m_borderSlice = slice;
        m_canAdvance = true;
        m_allowCommit = m_allowForwardSlashOperator = true;
        m_allowImageSlice = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitBorderOutset(PassRefPtr<CSSPrimitiveValue> outset)
    {
        m_outset = outset;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImageSlice = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitRepeat(PassRefPtr<CSSValue> repeat)
    {
        m_repeat = repeat;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowRepeat = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowImage = !m_image;
    }

    PassRefPtr<CSSValue> commitWebKitBorderImage()
    {
        return createBorderImageValue(m_image, m_imageSlice, m_borderSlice, m_outset, m_repeat);
    }

    void commitBorderImage(CSSParser& parser, bool important)
    {
        commitBorderImageProperty(CSSPropertyBorderImageSource, parser, m_image, important);
        commitBorderImageProperty(CSSPropertyBorderImageSlice, parser, m_imageSlice, important);
        commitBorderImageProperty(CSSPropertyBorderImageWidth, parser, m_borderSlice, important);
        commitBorderImageProperty(CSSPropertyBorderImageOutset, parser, m_outset, important);
        commitBorderImageProperty(CSSPropertyBorderImageRepeat, parser, m_repeat, important);
    }

    void commitBorderImageProperty(CSSPropertyID propId, CSSParser& parser, PassRefPtr<CSSValue> value, bool important)
    {
        if (value)
            parser.addProperty(propId, value, important);
        else
            parser.addProperty(propId, CSSValuePool::singleton().createImplicitInitialValue(), important, true);
    }

    bool m_canAdvance;

    bool m_allowCommit;
    bool m_allowImage;
    bool m_allowImageSlice;
    bool m_allowRepeat;
    bool m_allowForwardSlashOperator;

    bool m_requireWidth;
    bool m_requireOutset;

    RefPtr<CSSValue> m_image;
    RefPtr<CSSBorderImageSliceValue> m_imageSlice;
    RefPtr<CSSPrimitiveValue> m_borderSlice;
    RefPtr<CSSPrimitiveValue> m_outset;

    RefPtr<CSSValue> m_repeat;
};

bool CSSParser::parseBorderImage(CSSPropertyID propId, RefPtr<CSSValue>& result, bool important)
{
    ShorthandScope scope(this, propId);
    BorderImageParseContext context;
    while (CSSParserValue* currentValue = m_valueList->current()) {
        context.setCanAdvance(false);

        if (!context.canAdvance() && context.allowForwardSlashOperator() && isForwardSlashOperator(*currentValue))
            context.commitForwardSlashOperator();

        if (!context.canAdvance() && context.allowImage()) {
            if (currentValue->unit == CSSPrimitiveValue::CSS_URI)
                context.commitImage(CSSImageValue::create(completeURL(currentValue->string)));
            else if (isGeneratedImageValue(*currentValue)) {
                RefPtr<CSSValue> value;
                if (parseGeneratedImage(*m_valueList, value))
                    context.commitImage(value.release());
                else
                    return false;
#if ENABLE(CSS_IMAGE_SET)
            } else if (currentValue->unit == CSSParserValue::Function && equalLettersIgnoringASCIICase(currentValue->function->name, "-webkit-image-set(")) {
                RefPtr<CSSValue> value = parseImageSet();
                if (value)
                    context.commitImage(value.release());
                else
                    return false;
#endif
            } else if (currentValue->id == CSSValueNone)
                context.commitImage(CSSValuePool::singleton().createIdentifierValue(CSSValueNone));
        }

        if (!context.canAdvance() && context.allowImageSlice()) {
            RefPtr<CSSBorderImageSliceValue> imageSlice;
            if (parseBorderImageSlice(propId, imageSlice))
                context.commitImageSlice(imageSlice.release());
        }

        if (!context.canAdvance() && context.allowRepeat()) {
            RefPtr<CSSValue> repeat;
            if (parseBorderImageRepeat(repeat))
                context.commitRepeat(repeat.release());
        }

        if (!context.canAdvance() && context.requireWidth()) {
            RefPtr<CSSPrimitiveValue> borderSlice;
            if (parseBorderImageWidth(borderSlice))
                context.commitBorderWidth(borderSlice.release());
        }

        if (!context.canAdvance() && context.requireOutset()) {
            RefPtr<CSSPrimitiveValue> borderOutset;
            if (parseBorderImageOutset(borderOutset))
                context.commitBorderOutset(borderOutset.release());
        }

        if (!context.canAdvance())
            return false;

        m_valueList->next();
    }

    if (context.allowCommit()) {
        if (propId == CSSPropertyBorderImage)
            context.commitBorderImage(*this, important);
        else
            // Need to fully commit as a single value.
            result = context.commitWebKitBorderImage();
        return true;
    }

    return false;
}

static bool isBorderImageRepeatKeyword(int id)
{
    return id == CSSValueStretch || id == CSSValueRepeat || id == CSSValueSpace || id == CSSValueRound;
}

bool CSSParser::parseBorderImageRepeat(RefPtr<CSSValue>& result)
{
    RefPtr<CSSPrimitiveValue> firstValue;
    RefPtr<CSSPrimitiveValue> secondValue;
    CSSParserValue* val = m_valueList->current();
    if (!val)
        return false;
    if (isBorderImageRepeatKeyword(val->id))
        firstValue = CSSValuePool::singleton().createIdentifierValue(val->id);
    else
        return false;

    val = m_valueList->next();
    if (val) {
        if (isBorderImageRepeatKeyword(val->id))
            secondValue = CSSValuePool::singleton().createIdentifierValue(val->id);
        else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            // We need to rewind the value list, so that when its advanced we'll
            // end up back at this value.
            m_valueList->previous();
            secondValue = firstValue;
        }
    } else
        secondValue = firstValue;

    result = createPrimitiveValuePair(WTFMove(firstValue), WTFMove(secondValue));
    return true;
}

class BorderImageSliceParseContext {
public:
    BorderImageSliceParseContext(CSSParser& parser)
    : m_parser(parser)
    , m_allowNumber(true)
    , m_allowFill(true)
    , m_allowFinalCommit(false)
    , m_fill(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFill() const { return m_allowFill; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(CSSParser::ValueWithCalculation& valueWithCalculation)
    {
        RefPtr<CSSPrimitiveValue> primitiveValue = m_parser.createPrimitiveNumericValue(valueWithCalculation);
        if (!m_top)
            m_top = primitiveValue.release();
        else if (!m_right)
            m_right = primitiveValue.release();
        else if (!m_bottom)
            m_bottom = primitiveValue.release();
        else {
            ASSERT(!m_left);
            m_left = primitiveValue.release();
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void commitFill() { m_fill = true; m_allowFill = false; m_allowNumber = !m_top; }

    PassRefPtr<CSSBorderImageSliceValue> commitBorderImageSlice()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        // Now build a rect value to hold all four of our primitive values.
        auto quad = Quad::create();
        quad->setTop(m_top);
        quad->setRight(m_right);
        quad->setBottom(m_bottom);
        quad->setLeft(m_left);

        // Make our new border image value now.
        return CSSBorderImageSliceValue::create(CSSValuePool::singleton().createValue(WTFMove(quad)), m_fill);
    }

private:
    CSSParser& m_parser;

    bool m_allowNumber;
    bool m_allowFill;
    bool m_allowFinalCommit;

    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;

    bool m_fill;
};

bool CSSParser::parseBorderImageSlice(CSSPropertyID propId, RefPtr<CSSBorderImageSliceValue>& result)
{
    BorderImageSliceParseContext context(*this);
    CSSParserValue* value;
    while ((value = m_valueList->current())) {
        ValueWithCalculation valueWithCalculation(*value);
        // FIXME calc() http://webkit.org/b/16662 : calc is parsed but values are not created yet.
        if (context.allowNumber() && !isCalculation(valueWithCalculation) && validateUnit(valueWithCalculation, FInteger | FNonNeg | FPercent, CSSStrictMode)) {
            context.commitNumber(valueWithCalculation);
        } else if (context.allowFill() && value->id == CSSValueFill)
            context.commitFill();
        else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit()) {
                // We're going to successfully parse, but we don't want to consume this token.
                m_valueList->previous();
            }
            break;
        }
        m_valueList->next();
    }

    if (context.allowFinalCommit()) {
        // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect have to do a fill by default.
        // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image? Probably just have to leave them filling...
        if (propId == CSSPropertyWebkitBorderImage || propId == CSSPropertyWebkitMaskBoxImage || propId == CSSPropertyWebkitBoxReflect)
            context.commitFill();

        // Need to fully commit as a single value.
        result = context.commitBorderImageSlice();
        return true;
    }

    return false;
}

class BorderImageQuadParseContext {
public:
    BorderImageQuadParseContext(CSSParser& parser)
    : m_parser(parser)
    , m_allowNumber(true)
    , m_allowFinalCommit(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(CSSParser::ValueWithCalculation& valueWithCalculation)
    {
        RefPtr<CSSPrimitiveValue> primitiveValue;
        if (valueWithCalculation.value().id == CSSValueAuto)
            primitiveValue = CSSValuePool::singleton().createIdentifierValue(valueWithCalculation.value().id);
        else
            primitiveValue = m_parser.createPrimitiveNumericValue(valueWithCalculation);

        if (!m_top)
            m_top = primitiveValue.release();
        else if (!m_right)
            m_right = primitiveValue.release();
        else if (!m_bottom)
            m_bottom = primitiveValue.release();
        else {
            ASSERT(!m_left);
            m_left = primitiveValue.release();
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void setAllowFinalCommit() { m_allowFinalCommit = true; }
    void setTop(PassRefPtr<CSSPrimitiveValue> val) { m_top = val; }

    PassRefPtr<CSSPrimitiveValue> commitBorderImageQuad()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        // Now build a quad value to hold all four of our primitive values.
        auto quad = Quad::create();
        quad->setTop(m_top);
        quad->setRight(m_right);
        quad->setBottom(m_bottom);
        quad->setLeft(m_left);

        // Make our new value now.
        return CSSValuePool::singleton().createValue(WTFMove(quad));
    }

private:
    CSSParser& m_parser;

    bool m_allowNumber;
    bool m_allowFinalCommit;

    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;
};

bool CSSParser::parseBorderImageQuad(Units validUnits, RefPtr<CSSPrimitiveValue>& result)
{
    BorderImageQuadParseContext context(*this);
    CSSParserValue* value;
    while ((value = m_valueList->current())) {
        ValueWithCalculation valueWithCalculation(*value);
        if (context.allowNumber() && (validateUnit(valueWithCalculation, validUnits, CSSStrictMode) || value->id == CSSValueAuto)) {
            context.commitNumber(valueWithCalculation);
        } else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit())
                m_valueList->previous(); // The shorthand loop will advance back to this point.
            break;
        }
        m_valueList->next();
    }

    if (context.allowFinalCommit()) {
        // Need to fully commit as a single value.
        result = context.commitBorderImageQuad();
        return true;
    }
    return false;
}

bool CSSParser::parseBorderImageWidth(RefPtr<CSSPrimitiveValue>& result)
{
    return parseBorderImageQuad(FLength | FInteger | FNonNeg | FPercent, result);
}

bool CSSParser::parseBorderImageOutset(RefPtr<CSSPrimitiveValue>& result)
{
    return parseBorderImageQuad(FLength | FInteger | FNonNeg, result);
}

bool CSSParser::parseBorderRadius(CSSPropertyID propId, bool important)
{
    unsigned num = m_valueList->size();
    if (num > 9)
        return false;

    ShorthandScope scope(this, propId);
    RefPtr<CSSPrimitiveValue> radii[2][4];

    unsigned indexAfterSlash = 0;
    for (unsigned i = 0; i < num; ++i) {
        CSSParserValue& value = *m_valueList->valueAt(i);
        if (value.unit == CSSParserValue::Operator) {
            if (value.iValue != '/')
                return false;

            if (!i || indexAfterSlash || i + 1 == num || num > i + 5)
                return false;

            indexAfterSlash = i + 1;
            completeBorderRadii(radii[0]);
            continue;
        }

        if (i - indexAfterSlash >= 4)
            return false;

        ValueWithCalculation valueWithCalculation(value);
        if (!validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg))
            return false;

        RefPtr<CSSPrimitiveValue> radius = createPrimitiveNumericValue(valueWithCalculation);

        if (!indexAfterSlash) {
            radii[0][i] = radius;

            // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
            if (num == 2 && propId == CSSPropertyWebkitBorderRadius) {
                indexAfterSlash = 1;
                completeBorderRadii(radii[0]);
            }
        } else
            radii[1][i - indexAfterSlash] = radius.release();
    }

    if (!indexAfterSlash) {
        completeBorderRadii(radii[0]);
        for (unsigned i = 0; i < 4; ++i)
            radii[1][i] = radii[0][i];
    } else
        completeBorderRadii(radii[1]);

    ImplicitScope implicitScope(*this, PropertyImplicit);
    addProperty(CSSPropertyBorderTopLeftRadius, createPrimitiveValuePair(radii[0][0].release(), radii[1][0].release()), important);
    addProperty(CSSPropertyBorderTopRightRadius, createPrimitiveValuePair(radii[0][1].release(), radii[1][1].release()), important);
    addProperty(CSSPropertyBorderBottomRightRadius, createPrimitiveValuePair(radii[0][2].release(), radii[1][2].release()), important);
    addProperty(CSSPropertyBorderBottomLeftRadius, createPrimitiveValuePair(radii[0][3].release(), radii[1][3].release()), important);
    return true;
}

bool CSSParser::parseAspectRatio(bool important)
{
    unsigned num = m_valueList->size();
    if (num == 1) {
        CSSValueID valueId = m_valueList->valueAt(0)->id;
        if (valueId == CSSValueAuto || valueId == CSSValueFromDimensions || valueId == CSSValueFromIntrinsic) {
            addProperty(CSSPropertyWebkitAspectRatio, CSSValuePool::singleton().createIdentifierValue(valueId), important);
            return true;
        }
    }

    if (num != 3)
        return false;

    CSSParserValue& op = *m_valueList->valueAt(1);

    if (!isForwardSlashOperator(op))
        return false;

    ValueWithCalculation lvalueWithCalculation(*m_valueList->valueAt(0));
    ValueWithCalculation rvalueWithCalculation(*m_valueList->valueAt(2));
    if (!validateUnit(lvalueWithCalculation, FNumber | FNonNeg) || !validateUnit(rvalueWithCalculation, FNumber | FNonNeg))
        return false;

    // FIXME: This doesn't handle calculated values.
    if (!lvalueWithCalculation.value().fValue || !rvalueWithCalculation.value().fValue)
        return false;

    addProperty(CSSPropertyWebkitAspectRatio, CSSAspectRatioValue::create(narrowPrecisionToFloat(lvalueWithCalculation.value().fValue), narrowPrecisionToFloat(rvalueWithCalculation.value().fValue)), important);

    return true;
}

bool CSSParser::parseCounter(CSSPropertyID propId, int defaultValue, bool important)
{
    enum { ID, VAL } state = ID;

    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    RefPtr<CSSPrimitiveValue> counterName;

    while (true) {
        CSSParserValue* value = m_valueList->current();
        switch (state) {
            case ID:
                if (value && value->unit == CSSPrimitiveValue::CSS_IDENT) {
                    counterName = createPrimitiveStringValue(*value);
                    state = VAL;
                    m_valueList->next();
                    continue;
                }
                break;
            case VAL: {
                int i = defaultValue;
                if (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    i = clampToInteger(value->fValue);
                    m_valueList->next();
                }

                list->append(createPrimitiveValuePair(counterName.release(),
                    CSSValuePool::singleton().createValue(i, CSSPrimitiveValue::CSS_NUMBER)));
                state = ID;
                continue;
            }
        }
        break;
    }

    if (list->length() > 0) {
        addProperty(propId, list.release(), important);
        return true;
    }

    return false;
}

// This should go away once we drop support for -webkit-gradient
static PassRefPtr<CSSPrimitiveValue> parseDeprecatedGradientPoint(CSSParserValue& value, bool horizontal)
{
    RefPtr<CSSPrimitiveValue> result;
    if (value.unit == CSSPrimitiveValue::CSS_IDENT) {
        if ((equalLettersIgnoringASCIICase(value, "left") && horizontal)
            || (equalLettersIgnoringASCIICase(value, "top") && !horizontal))
            result = CSSValuePool::singleton().createValue(0., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if ((equalLettersIgnoringASCIICase(value, "right") && horizontal)
            || (equalLettersIgnoringASCIICase(value, "bottom") && !horizontal))
            result = CSSValuePool::singleton().createValue(100., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if (equalLettersIgnoringASCIICase(value, "center"))
            result = CSSValuePool::singleton().createValue(50., CSSPrimitiveValue::CSS_PERCENTAGE);
    } else if (value.unit == CSSPrimitiveValue::CSS_NUMBER || value.unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        result = CSSValuePool::singleton().createValue(value.fValue, static_cast<CSSPrimitiveValue::UnitTypes>(value.unit));
    return result;
}

static bool parseDeprecatedGradientColorStop(CSSParser& parser, CSSParserValue& value, CSSGradientColorStop& stop)
{
    if (value.unit != CSSParserValue::Function)
        return false;

    if (!equalLettersIgnoringASCIICase(value.function->name, "from(")
        && !equalLettersIgnoringASCIICase(value.function->name, "to(")
        && !equalLettersIgnoringASCIICase(value.function->name, "color-stop("))
        return false;

    CSSParserValueList* args = value.function->args.get();
    if (!args)
        return false;

    if (equalLettersIgnoringASCIICase(value.function->name, "from(")
        || equalLettersIgnoringASCIICase(value.function->name, "to(")) {
        // The "from" and "to" stops expect 1 argument.
        if (args->size() != 1)
            return false;

        if (equalLettersIgnoringASCIICase(value.function->name, "from("))
            stop.m_position = CSSValuePool::singleton().createValue(0, CSSPrimitiveValue::CSS_NUMBER);
        else
            stop.m_position = CSSValuePool::singleton().createValue(1, CSSPrimitiveValue::CSS_NUMBER);

        CSSValueID id = args->current()->id;
        if (id == CSSValueWebkitText || CSSParser::isValidSystemColorValue(id) || id == CSSValueMenu)
            stop.m_color = CSSValuePool::singleton().createIdentifierValue(id);
        else
            stop.m_color = parser.parseColor(args->current());
        if (!stop.m_color)
            return false;
    }

    // The "color-stop" function expects 3 arguments.
    if (equalLettersIgnoringASCIICase(value.function->name, "color-stop(")) {
        if (args->size() != 3)
            return false;

        CSSParserValue* stopArg = args->current();
        if (stopArg->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
            stop.m_position = CSSValuePool::singleton().createValue(stopArg->fValue / 100, CSSPrimitiveValue::CSS_NUMBER);
        else if (stopArg->unit == CSSPrimitiveValue::CSS_NUMBER)
            stop.m_position = CSSValuePool::singleton().createValue(stopArg->fValue, CSSPrimitiveValue::CSS_NUMBER);
        else
            return false;

        stopArg = args->next();
        if (stopArg->unit != CSSParserValue::Operator || stopArg->iValue != ',')
            return false;

        stopArg = args->next();
        CSSValueID id = stopArg->id;
        if (id == CSSValueWebkitText || CSSParser::isValidSystemColorValue(id) || id == CSSValueMenu)
            stop.m_color = CSSValuePool::singleton().createIdentifierValue(id);
        else
            stop.m_color = parser.parseColor(stopArg);
        if (!stop.m_color)
            return false;
    }

    return true;
}

bool CSSParser::parseDeprecatedGradient(CSSParserValueList& valueList, RefPtr<CSSValue>& gradient)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || args->size() == 0)
        return false;

    // The first argument is the gradient type.  It is an identifier.
    CSSGradientType gradientType;
    CSSParserValue* argument = args->current();
    if (!argument || argument->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;
    if (equalLettersIgnoringASCIICase(*argument, "linear"))
        gradientType = CSSDeprecatedLinearGradient;
    else if (equalLettersIgnoringASCIICase(*argument, "radial"))
        gradientType = CSSDeprecatedRadialGradient;
    else
        return false;

    RefPtr<CSSGradientValue> result;
    switch (gradientType) {
    case CSSDeprecatedLinearGradient:
        result = CSSLinearGradientValue::create(NonRepeating, gradientType);
        break;
    case CSSDeprecatedRadialGradient:
        result = CSSRadialGradientValue::create(NonRepeating, gradientType);
        break;
    default:
        // The rest of the gradient types shouldn't appear here.
        ASSERT_NOT_REACHED();
    }

    // Comma.
    argument = args->next();
    if (!isComma(argument))
        return false;

    // Next comes the starting point for the gradient as an x y pair.  There is no
    // comma between the x and the y values.
    // First X.  It can be left, right, number or percent.
    argument = args->next();
    if (!argument)
        return false;
    RefPtr<CSSPrimitiveValue> point = parseDeprecatedGradientPoint(*argument, true);
    if (!point)
        return false;
    result->setFirstX(point.release());

    // First Y.  It can be top, bottom, number or percent.
    argument = args->next();
    if (!argument)
        return false;
    point = parseDeprecatedGradientPoint(*argument, false);
    if (!point)
        return false;
    result->setFirstY(point.release());

    // Comma after the first point.
    argument = args->next();
    if (!isComma(argument))
        return false;

    // For radial gradients only, we now expect a numeric radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        argument = args->next();
        // FIXME: This does not handle calculation values.
        if (!argument || argument->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        ValueWithCalculation argumentWithCalculation(*argument);
        downcast<CSSRadialGradientValue>(*result).setFirstRadius(createPrimitiveNumericValue(argumentWithCalculation));

        // Comma after the first radius.
        argument = args->next();
        if (!isComma(argument))
            return false;
    }

    // Next is the ending point for the gradient as an x, y pair.
    // Second X.  It can be left, right, number or percent.
    argument = args->next();
    if (!argument)
        return false;
    point = parseDeprecatedGradientPoint(*argument, true);
    if (!point)
        return false;
    result->setSecondX(point.release());

    // Second Y.  It can be top, bottom, number or percent.
    argument = args->next();
    if (!argument)
        return false;
    point = parseDeprecatedGradientPoint(*argument, false);
    if (!point)
        return false;
    result->setSecondY(point.release());

    // For radial gradients only, we now expect the second radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        // Comma after the second point.
        argument = args->next();
        if (!isComma(argument))
            return false;

        argument = args->next();
        // FIXME: This does not handle calculation values.
        if (!argument || argument->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        ValueWithCalculation argumentWithCalculation(*argument);
        downcast<CSSRadialGradientValue>(*result).setSecondRadius(createPrimitiveNumericValue(argumentWithCalculation));
    }

    // We now will accept any number of stops (0 or more).
    argument = args->next();
    while (argument) {
        // Look for the comma before the next stop.
        if (!isComma(argument))
            return false;

        // Now examine the stop itself.
        argument = args->next();
        if (!argument)
            return false;

        // The function name needs to be one of "from", "to", or "color-stop."
        CSSGradientColorStop stop;
        if (!parseDeprecatedGradientColorStop(*this, *argument, stop))
            return false;
        result->addStop(stop);

        // Advance
        argument = args->next();
    }

    gradient = result.release();
    return true;
}

static PassRefPtr<CSSPrimitiveValue> valueFromSideKeyword(CSSParserValue& value, bool& isHorizontal)
{
    if (value.unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;

    switch (value.id) {
        case CSSValueLeft:
        case CSSValueRight:
            isHorizontal = true;
            break;
        case CSSValueTop:
        case CSSValueBottom:
            isHorizontal = false;
            break;
        default:
            return nullptr;
    }
    return CSSValuePool::singleton().createIdentifierValue(value.id);
}

static PassRefPtr<CSSPrimitiveValue> parseGradientColorOrKeyword(CSSParser& parser, CSSParserValue& value)
{
    CSSValueID id = value.id;
    if (id == CSSValueWebkitText || CSSParser::isValidSystemColorValue(id) || id == CSSValueMenu || id == CSSValueCurrentcolor)
        return CSSValuePool::singleton().createIdentifierValue(id);

    return parser.parseColor(&value);
}

bool CSSParser::parseDeprecatedLinearGradient(CSSParserValueList& valueList, RefPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSPrefixedLinearGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* argument = args->current();
    if (!argument)
        return false;
    ValueWithCalculation argumentWithCalculation(*argument);

    bool expectComma = false;
    // Look for angle.
    if (validateUnit(argumentWithCalculation, FAngle, CSSStrictMode)) {
        result->setAngle(createPrimitiveNumericValue(argumentWithCalculation));

        args->next();
        expectComma = true;
    } else {
        // Look one or two optional keywords that indicate a side or corner.
        RefPtr<CSSPrimitiveValue> startX, startY;

        RefPtr<CSSPrimitiveValue> location;
        bool isHorizontal = false;
        if ((location = valueFromSideKeyword(*argument, isHorizontal))) {
            if (isHorizontal)
                startX = location;
            else
                startY = location;

            if ((argument = args->next())) {
                if ((location = valueFromSideKeyword(*argument, isHorizontal))) {
                    if (isHorizontal) {
                        if (startX)
                            return false;
                        startX = location;
                    } else {
                        if (startY)
                            return false;
                        startY = location;
                    }

                    args->next();
                }
            }

            expectComma = true;
        }

        if (!startX && !startY)
            startY = CSSValuePool::singleton().createIdentifierValue(CSSValueTop);

        result->setFirstX(startX.release());
        result->setFirstY(startY.release());
    }

    if (!parseGradientColorStops(*args, *result, expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSParser::parseDeprecatedRadialGradient(CSSParserValueList& valueList, RefPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* argument = args->current();
    if (!argument)
        return false;

    bool expectComma = false;

    // Optional background-position
    RefPtr<CSSPrimitiveValue> centerX;
    RefPtr<CSSPrimitiveValue> centerY;
    // parse2ValuesFillPosition advances the args next pointer.
    parse2ValuesFillPosition(*args, centerX, centerY);
    argument = args->current();
    if (!argument)
        return false;

    if (centerX || centerY) {
        // Comma
        if (!isComma(argument))
            return false;

        argument = args->next();
        if (!argument)
            return false;
    }

    result->setFirstX(centerX);
    result->setSecondX(centerX);
    // CSS3 radial gradients always share the same start and end point.
    result->setFirstY(centerY);
    result->setSecondY(centerY);

    RefPtr<CSSPrimitiveValue> shapeValue;
    RefPtr<CSSPrimitiveValue> sizeValue;

    // Optional shape and/or size in any order.
    for (int i = 0; i < 2; ++i) {
        if (argument->unit != CSSPrimitiveValue::CSS_IDENT)
            break;

        bool foundValue = false;
        switch (argument->id) {
        case CSSValueCircle:
        case CSSValueEllipse:
            shapeValue = CSSValuePool::singleton().createIdentifierValue(argument->id);
            foundValue = true;
            break;
        case CSSValueClosestSide:
        case CSSValueClosestCorner:
        case CSSValueFarthestSide:
        case CSSValueFarthestCorner:
        case CSSValueContain:
        case CSSValueCover:
            sizeValue = CSSValuePool::singleton().createIdentifierValue(argument->id);
            foundValue = true;
            break;
        default:
            break;
        }

        if (foundValue) {
            argument = args->next();
            if (!argument)
                return false;

            expectComma = true;
        }
    }

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);

    // Or, two lengths or percentages
    RefPtr<CSSPrimitiveValue> horizontalSize;
    RefPtr<CSSPrimitiveValue> verticalSize;

    if (!shapeValue && !sizeValue) {
        ValueWithCalculation hSizeWithCalculation(*argument);
        if (validateUnit(hSizeWithCalculation, FLength | FPercent)) {
            horizontalSize = createPrimitiveNumericValue(hSizeWithCalculation);
            argument = args->next();
            if (!argument)
                return false;

            expectComma = true;
        }

        ValueWithCalculation vSizeWithCalculation(*argument);
        if (validateUnit(vSizeWithCalculation, FLength | FPercent)) {
            verticalSize = createPrimitiveNumericValue(vSizeWithCalculation);

            argument = args->next();
            if (!argument)
                return false;
            expectComma = true;
        }
    }

    // Must have neither or both.
    if (!horizontalSize != !verticalSize)
        return false;

    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    if (!parseGradientColorStops(*args, *result, expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSParser::parseLinearGradient(CSSParserValueList& valueList, RefPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSLinearGradient);

    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || !args->size())
        return false;

    if (!args->current())
        return false;

    ValueWithCalculation firstArgumentWithCalculation(*args->current());

    bool expectComma = false;
    // Look for angle.
    if (validateUnit(firstArgumentWithCalculation, FAngle, CSSStrictMode)) {
        result->setAngle(createPrimitiveNumericValue(firstArgumentWithCalculation));

        args->next();
        expectComma = true;
    } else if (firstArgumentWithCalculation.value().unit == CSSPrimitiveValue::CSS_IDENT && equalLettersIgnoringASCIICase(firstArgumentWithCalculation, "to")) {
        // to [ [left | right] || [top | bottom] ]
        CSSParserValue* nextArgument = args->next();
        if (!nextArgument)
            return false;

        RefPtr<CSSPrimitiveValue> endX, endY;
        RefPtr<CSSPrimitiveValue> location;
        bool isHorizontal = false;

        location = valueFromSideKeyword(*nextArgument, isHorizontal);
        if (!location)
            return false;

        if (isHorizontal)
            endX = location;
        else
            endY = location;

        nextArgument = args->next();
        if (!nextArgument)
            return false;

        location = valueFromSideKeyword(*nextArgument, isHorizontal);
        if (location) {
            if (isHorizontal) {
                if (endX)
                    return false;
                endX = location;
            } else {
                if (endY)
                    return false;
                endY = location;
            }

            args->next();
        }

        expectComma = true;
        result->setFirstX(endX.release());
        result->setFirstY(endY.release());
    }

    if (!parseGradientColorStops(*args, *result, expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSParser::parseRadialGradient(CSSParserValueList& valueList, RefPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSRadialGradient);

    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* argument = args->current();
    if (!argument)
        return false;

    bool expectComma = false;

    RefPtr<CSSPrimitiveValue> shapeValue;
    RefPtr<CSSPrimitiveValue> sizeValue;
    RefPtr<CSSPrimitiveValue> horizontalSize;
    RefPtr<CSSPrimitiveValue> verticalSize;

    // First part of grammar, the size/shape clause:
    // [ circle || <length> ] |
    // [ ellipse || [ <length> | <percentage> ]{2} ] |
    // [ [ circle | ellipse] || <size-keyword> ]
    for (int i = 0; i < 3; ++i) {
        ValueWithCalculation argumentWithCalculation(*argument);
        if (argument->unit == CSSPrimitiveValue::CSS_IDENT) {
            bool badIdent = false;
            switch (argument->id) {
            case CSSValueCircle:
            case CSSValueEllipse:
                if (shapeValue)
                    return false;
                shapeValue = CSSValuePool::singleton().createIdentifierValue(argument->id);
                break;
            case CSSValueClosestSide:
            case CSSValueClosestCorner:
            case CSSValueFarthestSide:
            case CSSValueFarthestCorner:
                if (sizeValue || horizontalSize)
                    return false;
                sizeValue = CSSValuePool::singleton().createIdentifierValue(argument->id);
                break;
            default:
                badIdent = true;
            }

            if (badIdent)
                break;

            argument = args->next();
            if (!argument)
                return false;
        } else if (validateUnit(argumentWithCalculation, FLength | FPercent)) {

            if (sizeValue || horizontalSize)
                return false;
            horizontalSize = createPrimitiveNumericValue(argumentWithCalculation);

            argument = args->next();
            if (!argument)
                return false;

            ValueWithCalculation vSizeWithCalculation(*argument);
            if (validateUnit(vSizeWithCalculation, FLength | FPercent)) {
                verticalSize = createPrimitiveNumericValue(vSizeWithCalculation);
                ++i;
                argument = args->next();
                if (!argument)
                    return false;
            }
        } else
            break;
    }

    // You can specify size as a keyword or a length/percentage, not both.
    if (sizeValue && horizontalSize)
        return false;
    // Circles must have 0 or 1 lengths.
    if (shapeValue && shapeValue->getValueID() == CSSValueCircle && verticalSize)
        return false;
    // Ellipses must have 0 or 2 length/percentages.
    if (shapeValue && shapeValue->getValueID() == CSSValueEllipse && horizontalSize && !verticalSize)
        return false;
    // If there's only one size, it must be a length.
    if (!verticalSize && horizontalSize && horizontalSize->isPercentage())
        return false;

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);
    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    // Second part of grammar, the center-position clause:
    // at <position>
    RefPtr<CSSPrimitiveValue> centerX;
    RefPtr<CSSPrimitiveValue> centerY;
    if (argument->unit == CSSPrimitiveValue::CSS_IDENT && equalLettersIgnoringASCIICase(*argument, "at")) {
        argument = args->next();
        if (!argument)
            return false;

        parseFillPosition(*args, centerX, centerY);
        if (!(centerX && centerY))
            return false;

        argument = args->current();
        if (!argument)
            return false;

        result->setFirstX(centerX);
        result->setFirstY(centerY);
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(centerX);
        result->setSecondY(centerY);
    }

    if (shapeValue || sizeValue || horizontalSize || centerX || centerY)
        expectComma = true;

    if (!parseGradientColorStops(*args, *result, expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSParser::parseGradientColorStops(CSSParserValueList& valueList, CSSGradientValue& gradient, bool expectComma)
{
    CSSParserValue* value = valueList.current();
    bool previousStopWasMidpoint = true;

    // Now look for color stops.
    while (value) {
        // Look for the comma before the next stop.
        if (expectComma) {
            if (!isComma(value))
                return false;

            value = valueList.next();
            if (!value)
                return false;
        }

        // <color-stop> = <color> [ <percentage> | <length> ]?
        CSSGradientColorStop stop;
        stop.m_color = parseGradientColorOrKeyword(*this, *value);
        if (!stop.m_color) {
            if (previousStopWasMidpoint) // 2 midpoints in a row is not allowed. This also catches starting with a midpoint.
                return false;

            stop.isMidpoint = true;
        } else
            value = valueList.next();

        previousStopWasMidpoint = stop.isMidpoint;

        if (value) {
            ValueWithCalculation valueWithCalculation(*value);
            if (validateUnit(valueWithCalculation, FLength | FPercent)) {
                stop.m_position = createPrimitiveNumericValue(valueWithCalculation);
                value = valueList.next();
            } else if (stop.isMidpoint)
                return false;
        }

        gradient.addStop(stop);
        expectComma = true;
    }

    // We can't end on a midpoint.
    if (previousStopWasMidpoint)
        return false;

    // Must have 2 or more stops to be valid.
    return gradient.stopCount() >= 2;
}

bool CSSParser::isGeneratedImageValue(CSSParserValue& value) const
{
    if (value.unit != CSSParserValue::Function)
        return false;

    return equalLettersIgnoringASCIICase(value.function->name, "-webkit-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-linear-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "linear-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-repeating-linear-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "repeating-linear-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-radial-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "radial-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-repeating-radial-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "repeating-radial-gradient(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-canvas(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-cross-fade(")
        || equalLettersIgnoringASCIICase(value.function->name, "filter(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-filter(")
        || equalLettersIgnoringASCIICase(value.function->name, "-webkit-named-image(");
}

bool CSSParser::parseGeneratedImage(CSSParserValueList& valueList, RefPtr<CSSValue>& value)
{
    CSSParserValue& parserValue = *valueList.current();

    if (parserValue.unit != CSSParserValue::Function)
        return false;

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-gradient("))
        return parseDeprecatedGradient(valueList, value);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-linear-gradient("))
        return parseDeprecatedLinearGradient(valueList, value, NonRepeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "linear-gradient("))
        return parseLinearGradient(valueList, value, NonRepeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-repeating-linear-gradient("))
        return parseDeprecatedLinearGradient(valueList, value, Repeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "repeating-linear-gradient("))
        return parseLinearGradient(valueList, value, Repeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-radial-gradient("))
        return parseDeprecatedRadialGradient(valueList, value, NonRepeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "radial-gradient("))
        return parseRadialGradient(valueList, value, NonRepeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-repeating-radial-gradient("))
        return parseDeprecatedRadialGradient(valueList, value, Repeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "repeating-radial-gradient("))
        return parseRadialGradient(valueList, value, Repeating);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-canvas("))
        return parseCanvas(valueList, value);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-cross-fade("))
        return parseCrossfade(valueList, value);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "filter(") || equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-filter("))
        return parseFilterImage(valueList, value);

    if (equalLettersIgnoringASCIICase(parserValue.function->name, "-webkit-named-image("))
        return parseNamedImage(valueList, value);

    return false;
}

bool CSSParser::parseFilterImage(CSSParserValueList& valueList, RefPtr<CSSValue>& filter)
{
    RefPtr<CSSFilterImageValue> result;

    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args)
        return false;
    CSSParserValue* value = args->current();
    RefPtr<CSSValue> imageValue;
    RefPtr<CSSValue> filterValue;

    if (!value)
        return false;

    // The first argument is the image. It is a fill image.
    if (!parseFillImage(*args, imageValue)) {
        if (value->unit == CSSPrimitiveValue::CSS_STRING)
            imageValue = CSSImageValue::create(completeURL(value->string));
        else
            return false;
    }

    value = args->next();

    // Skip a comma
    if (!isComma(value))
        return false;
    value = args->next();

    if (!value || !parseFilter(*args, filterValue))
        return false;
    value = args->next();

    result = CSSFilterImageValue::create(imageValue.releaseNonNull(), filterValue.releaseNonNull());

    filter = result;

    return true;
}

bool CSSParser::parseCrossfade(CSSParserValueList& valueList, RefPtr<CSSValue>& crossfade)
{
    RefPtr<CSSCrossfadeValue> result;

    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || args->size() != 5)
        return false;
    CSSParserValue* argument = args->current();
    RefPtr<CSSValue> fromImageValue;
    RefPtr<CSSValue> toImageValue;

    // The first argument is the "from" image. It is a fill image.
    if (!argument || !parseFillImage(*args, fromImageValue))
        return false;
    argument = args->next();

    // Skip a comma
    if (!isComma(argument))
        return false;
    argument = args->next();

    // The second argument is the "to" image. It is a fill image.
    if (!argument || !parseFillImage(*args, toImageValue))
        return false;
    argument = args->next();

    // Skip a comma
    if (!isComma(argument))
        return false;
    argument = args->next();

    // The third argument is the crossfade value. It is a percentage or a fractional number.
    RefPtr<CSSPrimitiveValue> percentage;
    if (!argument)
        return false;
    
    if (argument->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        percentage = CSSValuePool::singleton().createValue(clampTo<double>(argument->fValue / 100, 0, 1), CSSPrimitiveValue::CSS_NUMBER);
    else if (argument->unit == CSSPrimitiveValue::CSS_NUMBER)
        percentage = CSSValuePool::singleton().createValue(clampTo<double>(argument->fValue, 0, 1), CSSPrimitiveValue::CSS_NUMBER);
    else
        return false;

    result = CSSCrossfadeValue::create(fromImageValue, toImageValue);
    result->setPercentage(percentage);

    crossfade = result;

    return true;
}

bool CSSParser::parseCanvas(CSSParserValueList& valueList, RefPtr<CSSValue>& canvas)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || args->size() != 1)
        return false;

    // The first argument is the canvas name.  It is an identifier.
    CSSParserValue* value = args->current();
    if (!value || value->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;

    canvas = CSSCanvasValue::create(value->string);
    return true;
}

bool CSSParser::parseNamedImage(CSSParserValueList& valueList, RefPtr<CSSValue>& namedImage)
{
    CSSParserValueList* args = valueList.current()->function->args.get();
    if (!args || args->size() != 1)
        return false;

    // The only argument is the image name.
    CSSParserValue* value = args->current();
    if (!value || value->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;

    namedImage = CSSNamedImageValue::create(value->string);
    return true;
}

#if ENABLE(CSS_IMAGE_RESOLUTION)
RefPtr<CSSValue> CSSParser::parseImageResolution()
{
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool haveResolution = false;
    bool haveFromImage = false;
    bool haveSnap = false;

    CSSParserValue* value = m_valueList->current();
    while (value) {
        ValueWithCalculation valueWithCalculation(*value);
        if (!haveFromImage && value->id == CSSValueFromImage) {
            list->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            haveFromImage = true;
        } else if (!haveSnap && value->id == CSSValueSnap) {
            list->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            haveSnap = true;
        } else if (!haveResolution && validateUnit(valueWithCalculation, FResolution | FNonNeg) && value->fValue > 0) {
            list->append(createPrimitiveNumericValue(valueWithCalculation));
            haveResolution = true;
        } else
            return nullptr;
        value = m_valueList->next();
    }
    if (!list->length())
        return nullptr;
    if (!haveFromImage && !haveResolution)
        return nullptr;
    return list;
}
#endif

#if ENABLE(CSS_IMAGE_SET)
RefPtr<CSSValue> CSSParser::parseImageSet()
{
    CSSParserValue& value = *m_valueList->current();
    ASSERT(value.unit == CSSParserValue::Function);

    CSSParserValueList* functionArgs = value.function->args.get();
    if (!functionArgs || !functionArgs->size() || !functionArgs->current())
        return nullptr;

    RefPtr<CSSImageSetValue> imageSet = CSSImageSetValue::create();
    CSSParserValue* arg = functionArgs->current();
    while (arg) {
        if (arg->unit != CSSPrimitiveValue::CSS_URI)
            return nullptr;

        imageSet->append(CSSImageValue::create(completeURL(arg->string)));
        arg = functionArgs->next();
        if (!arg || arg->unit != CSSPrimitiveValue::CSS_DIMENSION)
            return nullptr;

        double imageScaleFactor = 0;
        const String& string = arg->string;
        unsigned length = string.length();
        if (!length)
            return nullptr;
        if (string.is8Bit()) {
            const LChar* start = string.characters8();
            parseDouble(start, start + length, 'x', imageScaleFactor);
        } else {
            const UChar* start = string.characters16();
            parseDouble(start, start + length, 'x', imageScaleFactor);
        }
        if (imageScaleFactor <= 0)
            return nullptr;
        imageSet->append(CSSValuePool::singleton().createValue(imageScaleFactor, CSSPrimitiveValue::CSS_NUMBER));

        // If there are no more arguments, we're done.
        arg = functionArgs->next();
        if (!arg)
            break;

        // If there are more arguments, they should be after a comma.
        if (!isComma(arg))
            return nullptr;

        // Skip the comma and move on to the next argument.
        arg = functionArgs->next();
    }

    return imageSet;
}
#endif

class TransformOperationInfo {
public:
    TransformOperationInfo(const CSSParserString& name)
        : m_type(WebKitCSSTransformValue::UnknownTransformOperation)
        , m_argCount(1)
        , m_allowSingleArgument(false)
        , m_unit(CSSParser::FUnknown)
    {
        const UChar* characters;
        unsigned nameLength = name.length();

        const unsigned longestNameLength = 12;
        UChar characterBuffer[longestNameLength];
        if (name.is8Bit()) {
            unsigned length = std::min(longestNameLength, nameLength);
            const LChar* characters8 = name.characters8();
            for (unsigned i = 0; i < length; ++i)
                characterBuffer[i] = characters8[i];
            characters = characterBuffer;
        } else
            characters = name.characters16();

        switch (nameLength) {
        case 5:
            // Valid name: skew(.
            if (((characters[0] == 's') || (characters[0] == 'S'))
                & ((characters[1] == 'k') || (characters[1] == 'K'))
                & ((characters[2] == 'e') || (characters[2] == 'E'))
                & ((characters[3] == 'w') || (characters[3] == 'W'))
                & (characters[4] == '(')) {
                m_unit = CSSParser::FAngle;
                m_type = WebKitCSSTransformValue::SkewTransformOperation;
                m_allowSingleArgument = true;
                m_argCount = 3;
            }
            break;
        case 6:
            // Valid names: skewx(, skewy(, scale(.
            if ((characters[1] == 'c') || (characters[1] == 'C')) {
                if (((characters[0] == 's') || (characters[0] == 'S'))
                    & ((characters[2] == 'a') || (characters[2] == 'A'))
                    & ((characters[3] == 'l') || (characters[3] == 'L'))
                    & ((characters[4] == 'e') || (characters[4] == 'E'))
                    & (characters[5] == '(')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::ScaleTransformOperation;
                    m_allowSingleArgument = true;
                    m_argCount = 3;
                }
            } else if (((characters[0] == 's') || (characters[0] == 'S'))
                       & ((characters[1] == 'k') || (characters[1] == 'K'))
                       & ((characters[2] == 'e') || (characters[2] == 'E'))
                       & ((characters[3] == 'w') || (characters[3] == 'W'))
                       & (characters[5] == '(')) {
                if ((characters[4] == 'x') || (characters[4] == 'X')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::SkewXTransformOperation;
                } else if ((characters[4] == 'y') || (characters[4] == 'Y')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::SkewYTransformOperation;
                }
            }
            break;
        case 7:
            // Valid names: matrix(, rotate(, scalex(, scaley(, scalez(.
            if ((characters[0] == 'm') || (characters[0] == 'M')) {
                if (((characters[1] == 'a') || (characters[1] == 'A'))
                    & ((characters[2] == 't') || (characters[2] == 'T'))
                    & ((characters[3] == 'r') || (characters[3] == 'R'))
                    & ((characters[4] == 'i') || (characters[4] == 'I'))
                    & ((characters[5] == 'x') || (characters[5] == 'X'))
                    & (characters[6] == '(')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::MatrixTransformOperation;
                    m_argCount = 11;
                }
            } else if ((characters[0] == 'r') || (characters[0] == 'R')) {
                if (((characters[1] == 'o') || (characters[1] == 'O'))
                    & ((characters[2] == 't') || (characters[2] == 'T'))
                    & ((characters[3] == 'a') || (characters[3] == 'A'))
                    & ((characters[4] == 't') || (characters[4] == 'T'))
                    & ((characters[5] == 'e') || (characters[5] == 'E'))
                    & (characters[6] == '(')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::RotateTransformOperation;
                }
            } else if (((characters[0] == 's') || (characters[0] == 'S'))
                       & ((characters[1] == 'c') || (characters[1] == 'C'))
                       & ((characters[2] == 'a') || (characters[2] == 'A'))
                       & ((characters[3] == 'l') || (characters[3] == 'L'))
                       & ((characters[4] == 'e') || (characters[4] == 'E'))
                       & (characters[6] == '(')) {
                if ((characters[5] == 'x') || (characters[5] == 'X')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::ScaleXTransformOperation;
                } else if ((characters[5] == 'y') || (characters[5] == 'Y')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::ScaleYTransformOperation;
                } else if ((characters[5] == 'z') || (characters[5] == 'Z')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::ScaleZTransformOperation;
                }
            }
            break;
        case 8:
            // Valid names: rotatex(, rotatey(, rotatez(, scale3d(.
            if ((characters[0] == 's') || (characters[0] == 'S')) {
                if (((characters[1] == 'c') || (characters[1] == 'C'))
                    & ((characters[2] == 'a') || (characters[2] == 'A'))
                    & ((characters[3] == 'l') || (characters[3] == 'L'))
                    & ((characters[4] == 'e') || (characters[4] == 'E'))
                    & (characters[5] == '3')
                    & ((characters[6] == 'd') || (characters[6] == 'D'))
                    & (characters[7] == '(')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::Scale3DTransformOperation;
                    m_argCount = 5;
                }
            } else if (((characters[0] == 'r') || (characters[0] == 'R'))
                       & ((characters[1] == 'o') || (characters[1] == 'O'))
                       & ((characters[2] == 't') || (characters[2] == 'T'))
                       & ((characters[3] == 'a') || (characters[3] == 'A'))
                       & ((characters[4] == 't') || (characters[4] == 'T'))
                       & ((characters[5] == 'e') || (characters[5] == 'E'))
                       & (characters[7] == '(')) {
                if ((characters[6] == 'x') || (characters[6] == 'X')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::RotateXTransformOperation;
                } else if ((characters[6] == 'y') || (characters[6] == 'Y')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::RotateYTransformOperation;
                } else if ((characters[6] == 'z') || (characters[6] == 'Z')) {
                    m_unit = CSSParser::FAngle;
                    m_type = WebKitCSSTransformValue::RotateZTransformOperation;
                }
            }
            break;
        case 9:
            // Valid names: matrix3d(, rotate3d(.
            if ((characters[0] == 'm') || (characters[0] == 'M')) {
                if (((characters[1] == 'a') || (characters[1] == 'A'))
                    & ((characters[2] == 't') || (characters[2] == 'T'))
                    & ((characters[3] == 'r') || (characters[3] == 'R'))
                    & ((characters[4] == 'i') || (characters[4] == 'I'))
                    & ((characters[5] == 'x') || (characters[5] == 'X'))
                    & (characters[6] == '3')
                    & ((characters[7] == 'd') || (characters[7] == 'D'))
                    & (characters[8] == '(')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::Matrix3DTransformOperation;
                    m_argCount = 31;
                }
            } else if (((characters[0] == 'r') || (characters[0] == 'R'))
                       & ((characters[1] == 'o') || (characters[1] == 'O'))
                       & ((characters[2] == 't') || (characters[2] == 'T'))
                       & ((characters[3] == 'a') || (characters[3] == 'A'))
                       & ((characters[4] == 't') || (characters[4] == 'T'))
                       & ((characters[5] == 'e') || (characters[5] == 'E'))
                       & (characters[6] == '3')
                       & ((characters[7] == 'd') || (characters[7] == 'D'))
                       & (characters[8] == '(')) {
                m_unit = CSSParser::FNumber;
                m_type = WebKitCSSTransformValue::Rotate3DTransformOperation;
                m_argCount = 7;
            }
            break;
        case 10:
            // Valid name: translate(.
            if (((characters[0] == 't') || (characters[0] == 'T'))
                & ((characters[1] == 'r') || (characters[1] == 'R'))
                & ((characters[2] == 'a') || (characters[2] == 'A'))
                & ((characters[3] == 'n') || (characters[3] == 'N'))
                & ((characters[4] == 's') || (characters[4] == 'S'))
                & ((characters[5] == 'l') || (characters[5] == 'L'))
                & ((characters[6] == 'a') || (characters[6] == 'A'))
                & ((characters[7] == 't') || (characters[7] == 'T'))
                & ((characters[8] == 'e') || (characters[8] == 'E'))
                & (characters[9] == '(')) {
                m_unit = CSSParser::FLength | CSSParser::FPercent;
                m_type = WebKitCSSTransformValue::TranslateTransformOperation;
                m_allowSingleArgument = true;
                m_argCount = 3;
            }
            break;
        case 11:
            // Valid names: translatex(, translatey(, translatez(.
            if (((characters[0] == 't') || (characters[0] == 'T'))
                & ((characters[1] == 'r') || (characters[1] == 'R'))
                & ((characters[2] == 'a') || (characters[2] == 'A'))
                & ((characters[3] == 'n') || (characters[3] == 'N'))
                & ((characters[4] == 's') || (characters[4] == 'S'))
                & ((characters[5] == 'l') || (characters[5] == 'L'))
                & ((characters[6] == 'a') || (characters[6] == 'A'))
                & ((characters[7] == 't') || (characters[7] == 'T'))
                & ((characters[8] == 'e') || (characters[8] == 'E'))
                & (characters[10] == '(')) {
                if ((characters[9] == 'x') || (characters[9] == 'X')) {
                    m_unit = CSSParser::FLength | CSSParser::FPercent;
                    m_type = WebKitCSSTransformValue::TranslateXTransformOperation;
                } else if ((characters[9] == 'y') || (characters[9] == 'Y')) {
                    m_unit = CSSParser::FLength | CSSParser::FPercent;
                    m_type = WebKitCSSTransformValue::TranslateYTransformOperation;
                } else if ((characters[9] == 'z') || (characters[9] == 'Z')) {
                    m_unit = CSSParser::FLength | CSSParser::FPercent;
                    m_type = WebKitCSSTransformValue::TranslateZTransformOperation;
                }
            }
            break;
        case 12:
            // Valid names: perspective(, translate3d(.
            if ((characters[0] == 'p') || (characters[0] == 'P')) {
                if (((characters[1] == 'e') || (characters[1] == 'E'))
                    & ((characters[2] == 'r') || (characters[2] == 'R'))
                    & ((characters[3] == 's') || (characters[3] == 'S'))
                    & ((characters[4] == 'p') || (characters[4] == 'P'))
                    & ((characters[5] == 'e') || (characters[5] == 'E'))
                    & ((characters[6] == 'c') || (characters[6] == 'C'))
                    & ((characters[7] == 't') || (characters[7] == 'T'))
                    & ((characters[8] == 'i') || (characters[8] == 'I'))
                    & ((characters[9] == 'v') || (characters[9] == 'V'))
                    & ((characters[10] == 'e') || (characters[10] == 'E'))
                    & (characters[11] == '(')) {
                    m_unit = CSSParser::FNumber;
                    m_type = WebKitCSSTransformValue::PerspectiveTransformOperation;
                }
            } else if (((characters[0] == 't') || (characters[0] == 'T'))
                       & ((characters[1] == 'r') || (characters[1] == 'R'))
                       & ((characters[2] == 'a') || (characters[2] == 'A'))
                       & ((characters[3] == 'n') || (characters[3] == 'N'))
                       & ((characters[4] == 's') || (characters[4] == 'S'))
                       & ((characters[5] == 'l') || (characters[5] == 'L'))
                       & ((characters[6] == 'a') || (characters[6] == 'A'))
                       & ((characters[7] == 't') || (characters[7] == 'T'))
                       & ((characters[8] == 'e') || (characters[8] == 'E'))
                       & (characters[9] == '3')
                       & ((characters[10] == 'd') || (characters[10] == 'D'))
                       & (characters[11] == '(')) {
                m_unit = CSSParser::FLength | CSSParser::FPercent;
                m_type = WebKitCSSTransformValue::Translate3DTransformOperation;
                m_argCount = 5;
            }
            break;
        } // end switch ()
    }

    WebKitCSSTransformValue::TransformOperationType type() const { return m_type; }
    unsigned argCount() const { return m_argCount; }
    CSSParser::Units unit() const { return m_unit; }

    bool unknown() const { return m_type == WebKitCSSTransformValue::UnknownTransformOperation; }
    bool hasCorrectArgCount(unsigned argCount) { return m_argCount == argCount || (m_allowSingleArgument && argCount == 1); }

private:
    WebKitCSSTransformValue::TransformOperationType m_type;
    unsigned m_argCount;
    bool m_allowSingleArgument;
    CSSParser::Units m_unit;
};

RefPtr<CSSValueList> CSSParser::parseTransform()
{
    if (!m_valueList)
        return nullptr;

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        RefPtr<CSSValue> parsedTransformValue = parseTransformValue(*value);
        if (!parsedTransformValue)
            return nullptr;

        list->append(parsedTransformValue.releaseNonNull());
    }

    return list;
}

RefPtr<CSSValue> CSSParser::parseTransformValue(CSSParserValue& value)
{
    if (value.unit != CSSParserValue::Function || !value.function)
        return nullptr;

    // Every primitive requires at least one argument.
    CSSParserValueList* args = value.function->args.get();
    if (!args)
        return nullptr;

    // See if the specified primitive is one we understand.
    TransformOperationInfo info(value.function->name);
    if (info.unknown())
        return nullptr;

    if (!info.hasCorrectArgCount(args->size()))
        return nullptr;

    // The transform is a list of functional primitives that specify transform operations.
    // We collect a list of WebKitCSSTransformValues, where each value specifies a single operation.

    // Create the new WebKitCSSTransformValue for this operation and add it to our list.
    RefPtr<WebKitCSSTransformValue> transformValue = WebKitCSSTransformValue::create(info.type());

    // Snag our values.
    CSSParserValue* argument = args->current();
    unsigned argNumber = 0;
    while (argument) {
        ValueWithCalculation argumentWithCalculation(*argument);
        CSSParser::Units unit = info.unit();

        if (info.type() == WebKitCSSTransformValue::Rotate3DTransformOperation && argNumber == 3) {
            // 4th param of rotate3d() is an angle rather than a bare number, validate it as such
            if (!validateUnit(argumentWithCalculation, FAngle, CSSStrictMode))
                return nullptr;
        } else if (info.type() == WebKitCSSTransformValue::Translate3DTransformOperation && argNumber == 2) {
            // 3rd param of translate3d() cannot be a percentage
            if (!validateUnit(argumentWithCalculation, FLength, CSSStrictMode))
                return nullptr;
        } else if (info.type() == WebKitCSSTransformValue::TranslateZTransformOperation && !argNumber) {
            // 1st param of translateZ() cannot be a percentage
            if (!validateUnit(argumentWithCalculation, FLength, CSSStrictMode))
                return nullptr;
        } else if (info.type() == WebKitCSSTransformValue::PerspectiveTransformOperation && !argNumber) {
            // 1st param of perspective() must be a non-negative number (deprecated) or length.
            if (!validateUnit(argumentWithCalculation, FNumber | FLength | FNonNeg, CSSStrictMode))
                return nullptr;
        } else if (!validateUnit(argumentWithCalculation, unit, CSSStrictMode))
            return nullptr;

        // Add the value to the current transform operation.
        transformValue->append(createPrimitiveNumericValue(argumentWithCalculation));

        argument = args->next();
        if (!argument)
            break;
        if (argument->unit != CSSParserValue::Operator || argument->iValue != ',')
            return nullptr;
        argument = args->next();

        ++argNumber;
    }

    return transformValue.release();
}

bool CSSParser::isBlendMode(CSSValueID valueID)
{
    return (valueID >= CSSValueMultiply && valueID <= CSSValueLuminosity)
        || valueID == CSSValueNormal
        || valueID == CSSValueOverlay;
}

bool CSSParser::isCompositeOperator(CSSValueID valueID)
{
    // FIXME: Add CSSValueDestination and CSSValueLighter when the Compositing spec updates.
    return valueID >= CSSValueClear && valueID <= CSSValueXor;
}

static void filterInfoForName(const CSSParserString& name, WebKitCSSFilterValue::FilterOperationType& filterType, unsigned& maximumArgumentCount)
{
    if (equalLettersIgnoringASCIICase(name, "grayscale("))
        filterType = WebKitCSSFilterValue::GrayscaleFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "sepia("))
        filterType = WebKitCSSFilterValue::SepiaFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "saturate("))
        filterType = WebKitCSSFilterValue::SaturateFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "hue-rotate("))
        filterType = WebKitCSSFilterValue::HueRotateFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "invert("))
        filterType = WebKitCSSFilterValue::InvertFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "opacity("))
        filterType = WebKitCSSFilterValue::OpacityFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "brightness("))
        filterType = WebKitCSSFilterValue::BrightnessFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "contrast("))
        filterType = WebKitCSSFilterValue::ContrastFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "blur("))
        filterType = WebKitCSSFilterValue::BlurFilterOperation;
    else if (equalLettersIgnoringASCIICase(name, "drop-shadow(")) {
        filterType = WebKitCSSFilterValue::DropShadowFilterOperation;
        maximumArgumentCount = 4;  // x-offset, y-offset, blur-radius, color -- spread and inset style not allowed.
    }
}

RefPtr<WebKitCSSFilterValue> CSSParser::parseBuiltinFilterArguments(CSSParserValueList& args, WebKitCSSFilterValue::FilterOperationType filterType)
{
    RefPtr<WebKitCSSFilterValue> filterValue = WebKitCSSFilterValue::create(filterType);

    switch (filterType) {    
    case WebKitCSSFilterValue::GrayscaleFilterOperation:
    case WebKitCSSFilterValue::SepiaFilterOperation:
    case WebKitCSSFilterValue::SaturateFilterOperation:
    case WebKitCSSFilterValue::InvertFilterOperation:
    case WebKitCSSFilterValue::OpacityFilterOperation:
    case WebKitCSSFilterValue::ContrastFilterOperation: {
        // One optional argument, 0-1 or 0%-100%, if missing use 100%.
        if (args.size() > 1)
            return nullptr;

        if (args.size()) {
            ValueWithCalculation argumentWithCalculation(*args.current());
            if (!validateUnit(argumentWithCalculation, FNumber | FPercent | FNonNeg, CSSStrictMode))
                return nullptr;

            auto primitiveValue = createPrimitiveNumericValue(argumentWithCalculation);

            // Saturate and Contrast allow values over 100%.
            if (filterType != WebKitCSSFilterValue::SaturateFilterOperation
                && filterType != WebKitCSSFilterValue::ContrastFilterOperation) {
                double maxAllowed = primitiveValue->primitiveType() == CSSPrimitiveValue::CSS_PERCENTAGE ? 100.0 : 1.0;
                if (primitiveValue->getDoubleValue() > maxAllowed)
                    return nullptr;
            }

            filterValue->append(WTFMove(primitiveValue));
        }
        break;
    }
    case WebKitCSSFilterValue::BrightnessFilterOperation: {
        // One optional argument, if missing use 100%.
        if (args.size() > 1)
            return nullptr;

        if (args.size()) {
            ValueWithCalculation argumentWithCalculation(*args.current());
            if (!validateUnit(argumentWithCalculation, FNumber | FPercent, CSSStrictMode))
                return nullptr;

            filterValue->append(createPrimitiveNumericValue(argumentWithCalculation));
        }
        break;
    }
    case WebKitCSSFilterValue::HueRotateFilterOperation: {
        // hue-rotate() takes one optional angle.
        if (args.size() > 1)
            return nullptr;
        
        if (args.size()) {
            ValueWithCalculation argumentWithCalculation(*args.current());
            if (!validateUnit(argumentWithCalculation, FAngle, CSSStrictMode))
                return nullptr;
        
            filterValue->append(createPrimitiveNumericValue(argumentWithCalculation));
        }
        break;
    }
    case WebKitCSSFilterValue::BlurFilterOperation: {
        // Blur takes a single length. Zero parameters are allowed.
        if (args.size() > 1)
            return nullptr;
        
        if (args.size()) {
            ValueWithCalculation argumentWithCalculation(*args.current());
            if (!validateUnit(argumentWithCalculation, FLength | FNonNeg, CSSStrictMode))
                return nullptr;

            filterValue->append(createPrimitiveNumericValue(argumentWithCalculation));
        }
        break;
    }
    case WebKitCSSFilterValue::DropShadowFilterOperation: {
        // drop-shadow() takes a single shadow.
        RefPtr<CSSValueList> shadowValueList = parseShadow(args, CSSPropertyFilter);
        if (!shadowValueList || shadowValueList->length() != 1)
            return nullptr;
        
        filterValue->append(*shadowValueList->itemWithoutBoundsCheck(0));
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return filterValue.release();
}

bool CSSParser::parseFilter(CSSParserValueList& valueList, RefPtr<CSSValue>& result)
{
    // The filter is a list of functional primitives that specify individual operations.
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (auto* value = valueList.current(); value; value = valueList.next()) {
        if (value->unit != CSSPrimitiveValue::CSS_URI && (value->unit != CSSParserValue::Function || !value->function))
            return false;

        WebKitCSSFilterValue::FilterOperationType filterType = WebKitCSSFilterValue::UnknownFilterOperation;

        // See if the specified primitive is one we understand.
        if (value->unit == CSSPrimitiveValue::CSS_URI) {
            RefPtr<WebKitCSSFilterValue> referenceFilterValue = WebKitCSSFilterValue::create(WebKitCSSFilterValue::ReferenceFilterOperation);
            referenceFilterValue->append(CSSPrimitiveValue::create(value->string, CSSPrimitiveValue::CSS_URI));
            list->append(referenceFilterValue.releaseNonNull());
        } else {
            const CSSParserString name = value->function->name;
            unsigned maximumArgumentCount = 1;

            filterInfoForName(name, filterType, maximumArgumentCount);

            if (filterType == WebKitCSSFilterValue::UnknownFilterOperation)
                return false;

            CSSParserValueList* args = value->function->args.get();
            if (!args)
                return false;

            RefPtr<WebKitCSSFilterValue> filterValue = parseBuiltinFilterArguments(*args, filterType);
            if (!filterValue)
                return false;
            
            list->append(filterValue.releaseNonNull());
        }
    }

    result = list;

    return true;
}

#if ENABLE(CSS_REGIONS)
static bool validFlowName(const String& flowName)
{
    return !(equalLettersIgnoringASCIICase(flowName, "auto")
        || equalLettersIgnoringASCIICase(flowName, "default")
        || equalLettersIgnoringASCIICase(flowName, "inherit")
        || equalLettersIgnoringASCIICase(flowName, "initial")
        || equalLettersIgnoringASCIICase(flowName, "none"));
}
#endif

bool CSSParser::cssRegionsEnabled() const
{
    return m_context.isCSSRegionsEnabled;
}

bool CSSParser::cssCompositingEnabled() const
{
    return m_context.isCSSCompositingEnabled;
}

#if ENABLE(CSS_REGIONS)

// none | <ident>
bool CSSParser::parseFlowThread(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertyWebkitFlowInto);
    ASSERT(cssRegionsEnabled());

    if (m_valueList->size() != 1)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;

    if (value->id == CSSValueNone) {
        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(value->id), important);
        return true;
    }

    String inputProperty = String(value->string);
    if (!inputProperty.isEmpty()) {
        if (!validFlowName(inputProperty))
            return false;
        addProperty(propId, CSSValuePool::singleton().createValue(inputProperty, CSSPrimitiveValue::CSS_STRING), important);
    } else
        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);

    return true;
}

// -webkit-flow-from: none | <ident>
bool CSSParser::parseRegionThread(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertyWebkitFlowFrom);
    ASSERT(cssRegionsEnabled());

    if (m_valueList->size() != 1)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;

    if (value->id == CSSValueNone)
        addProperty(propId, CSSValuePool::singleton().createIdentifierValue(value->id), important);
    else {
        String inputProperty = String(value->string);
        if (!inputProperty.isEmpty()) {
            if (!validFlowName(inputProperty))
                return false;
            addProperty(propId, CSSValuePool::singleton().createValue(inputProperty, CSSPrimitiveValue::CSS_STRING), important);
        } else
            addProperty(propId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
    }

    return true;
}
#endif

bool CSSParser::parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtr<CSSPrimitiveValue>& value, RefPtr<CSSPrimitiveValue>& value2, RefPtr<CSSValue>& value3)
{
    propId1 = propId;
    propId2 = propId;
    propId3 = propId;
    if (propId == CSSPropertyTransformOrigin) {
        propId1 = CSSPropertyTransformOriginX;
        propId2 = CSSPropertyTransformOriginY;
        propId3 = CSSPropertyTransformOriginZ;
    }

    switch (propId) {
    case CSSPropertyTransformOrigin:
        if (!parseTransformOriginShorthand(value, value2, value3))
            return false;
        // parseTransformOriginShorthand advances the m_valueList pointer
        break;
    case CSSPropertyTransformOriginX: {
        value = parsePositionX(*m_valueList);
        if (value)
            m_valueList->next();
        break;
    }
    case CSSPropertyTransformOriginY: {
        value = parsePositionY(*m_valueList);
        if (value)
            m_valueList->next();
        break;
    }
    case CSSPropertyTransformOriginZ: {
        ValueWithCalculation valueWithCalculation(*m_valueList->current());
        if (validateUnit(valueWithCalculation, FLength))
            value = createPrimitiveNumericValue(valueWithCalculation);
        if (value)
            m_valueList->next();
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return value;
}

bool CSSParser::parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtr<CSSPrimitiveValue>& value, RefPtr<CSSPrimitiveValue>& value2)
{
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyPerspectiveOrigin) {
        propId1 = CSSPropertyPerspectiveOriginX;
        propId2 = CSSPropertyPerspectiveOriginY;
    }

    switch (propId) {
    case CSSPropertyPerspectiveOrigin:
        if (m_valueList->size() > 2)
            return false;
        parse2ValuesFillPosition(*m_valueList, value, value2);
        break;
    case CSSPropertyPerspectiveOriginX: {
        value = parsePositionX(*m_valueList);
        if (value)
            m_valueList->next();
        break;
    }
    case CSSPropertyPerspectiveOriginY: {
        value = parsePositionY(*m_valueList);
        if (value)
            m_valueList->next();
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        return false;
    }

    return value;
}

void CSSParser::addTextDecorationProperty(CSSPropertyID propId, PassRefPtr<CSSValue> value, bool important)
{
    // The text-decoration-line property takes priority over text-decoration, unless the latter has important priority set.
    if (propId == CSSPropertyTextDecoration && !important && !inShorthand()) {
        for (unsigned i = 0; i < m_parsedProperties.size(); ++i) {
            if (m_parsedProperties[i].id() == CSSPropertyWebkitTextDecorationLine)
                return;
        }
    }
    addProperty(propId, value, important);
}

bool CSSParser::parseTextDecoration(CSSPropertyID propId, bool important)
{
    CSSParserValue* value = m_valueList->current();
    if (value && value->id == CSSValueNone) {
        addTextDecorationProperty(propId, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        m_valueList->next();
        return true;
    }

    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool isValid = true;
    while (isValid && value) {
        switch (value->id) {
        case CSSValueBlink:
        case CSSValueLineThrough:
        case CSSValueOverline:
        case CSSValueUnderline:
#if ENABLE(LETTERPRESS)
        case CSSValueWebkitLetterpress:
#endif
            list->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            break;
        default:
            isValid = false;
            break;
        }
        if (isValid)
            value = m_valueList->next();
    }

    // Values are either valid or in shorthand scope.
    if (list->length() && (isValid || inShorthand())) {
        addTextDecorationProperty(propId, list.release(), important);
        return true;
    }

    return false;
}

bool CSSParser::parseTextDecorationSkip(bool important)
{
    // The text-decoration-skip property has syntax "none | [ objects || spaces || ink || edges || box-decoration ]".
    // However, only 'none' and 'ink' are implemented yet, so we will parse syntax "none | ink" for now.
    CSSParserValue* value = m_valueList->current();
    do {
        switch (value->id) {
        case CSSValueNone:
        case CSSValueAuto:
        case CSSValueInk:
        case CSSValueObjects:
            addProperty(CSSPropertyWebkitTextDecorationSkip, CSSValuePool::singleton().createIdentifierValue(value->id), important);
            return true;
        default:
            break;
        }
    } while ((value = m_valueList->next()));
    return false;
}

bool CSSParser::parseTextUnderlinePosition(bool important)
{
    // The text-underline-position property has sintax "auto | alphabetic | [ under || [ left | right ] ]".
    // However, values 'left' and 'right' are not implemented yet, so we will parse sintax
    // "auto | alphabetic | under" for now.
    CSSParserValue& value = *m_valueList->current();
    switch (value.id) {
    case CSSValueAuto:
    case CSSValueAlphabetic:
    case CSSValueUnder:
        if (m_valueList->next())
            return false;

        addProperty(CSSPropertyWebkitTextUnderlinePosition, CSSValuePool::singleton().createIdentifierValue(value.id), important);
        return true;
    default:
        break;
    }
    return false;
}

bool CSSParser::parseTextEmphasisStyle(bool important)
{
    unsigned valueListSize = m_valueList->size();

    RefPtr<CSSPrimitiveValue> fill;
    RefPtr<CSSPrimitiveValue> shape;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            if (fill || shape || (valueListSize != 1 && !inShorthand()))
                return false;
            addProperty(CSSPropertyWebkitTextEmphasisStyle, createPrimitiveStringValue(*value), important);
            m_valueList->next();
            return true;
        }

        if (value->id == CSSValueNone) {
            if (fill || shape || (valueListSize != 1 && !inShorthand()))
                return false;
            addProperty(CSSPropertyWebkitTextEmphasisStyle, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
            m_valueList->next();
            return true;
        }

        if (value->id == CSSValueOpen || value->id == CSSValueFilled) {
            if (fill)
                return false;
            fill = CSSValuePool::singleton().createIdentifierValue(value->id);
        } else if (value->id == CSSValueDot || value->id == CSSValueCircle || value->id == CSSValueDoubleCircle || value->id == CSSValueTriangle || value->id == CSSValueSesame) {
            if (shape)
                return false;
            shape = CSSValuePool::singleton().createIdentifierValue(value->id);
        } else if (!inShorthand())
            return false;
        else
            break;
    }

    if (fill && shape) {
        RefPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(fill.releaseNonNull());
        parsedValues->append(shape.releaseNonNull());
        addProperty(CSSPropertyWebkitTextEmphasisStyle, parsedValues.release(), important);
        return true;
    }
    if (fill) {
        addProperty(CSSPropertyWebkitTextEmphasisStyle, fill.release(), important);
        return true;
    }
    if (shape) {
        addProperty(CSSPropertyWebkitTextEmphasisStyle, shape.release(), important);
        return true;
    }

    return false;
}

bool CSSParser::parseTextEmphasisPosition(bool important)
{
    bool foundOverOrUnder = false;
    CSSValueID overUnderValueID = CSSValueOver;
    bool foundLeftOrRight = false;
    CSSValueID leftRightValueID = CSSValueRight;
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        switch (value->id) {
        case CSSValueOver:
            if (foundOverOrUnder)
                return false;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueOver;
            break;
        case CSSValueUnder:
            if (foundOverOrUnder)
                return false;
            foundOverOrUnder = true;
            overUnderValueID = CSSValueUnder;
            break;
        case CSSValueLeft:
            if (foundLeftOrRight)
                return false;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueLeft;
            break;
        case CSSValueRight:
            if (foundLeftOrRight)
                return false;
            foundLeftOrRight = true;
            leftRightValueID = CSSValueRight;
            break;
        default:
            return false;
        }
    }
    if (!foundOverOrUnder)
        return false;
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    list->append(CSSValuePool::singleton().createIdentifierValue(overUnderValueID));
    if (foundLeftOrRight)
        list->append(CSSValuePool::singleton().createIdentifierValue(leftRightValueID));
    addProperty(CSSPropertyWebkitTextEmphasisPosition, list, important);
    return true;
}

RefPtr<CSSValue> CSSParser::parseTextIndent()
{
    // <length> | <percentage> | inherit  when CSS3_TEXT is disabled.
    // [ <length> | <percentage> ] && [ -webkit-hanging || -webkit-each-line ]? | inherit  when CSS3_TEXT is enabled.
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool hasLengthOrPercentage = false;
#if ENABLE(CSS3_TEXT)
    bool hasEachLine = false;
    bool hasHanging = false;
#endif

    CSSParserValue* value = m_valueList->current();
    while (value) {
        ValueWithCalculation valueWithCalculation(*value);
        if (!hasLengthOrPercentage && validateUnit(valueWithCalculation, FLength | FPercent)) {
            list->append(createPrimitiveNumericValue(valueWithCalculation));
            hasLengthOrPercentage = true;
        }
#if ENABLE(CSS3_TEXT)
        else if (!hasEachLine && value->id == CSSValueWebkitEachLine) {
            list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueWebkitEachLine));
            hasEachLine = true;
        } else if (!hasHanging && value->id == CSSValueWebkitHanging) {
            list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueWebkitHanging));
            hasHanging = true;
        }
#endif
        else
            return nullptr;

        value = m_valueList->next();
    }

    if (!hasLengthOrPercentage)
        return nullptr;

    return list.release();
}

bool CSSParser::parseHangingPunctuation(bool important)
{
    CSSParserValue* value = m_valueList->current();
    if (value && value->id == CSSValueNone) {
        addProperty(CSSPropertyHangingPunctuation, CSSValuePool::singleton().createIdentifierValue(CSSValueNone), important);
        m_valueList->next();
        return true;
    }
    
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool isValid = true;
    std::bitset<numCSSValueKeywords> seenValues;
    while (isValid && value) {
        if (seenValues[value->id]
            || (value->id == CSSValueAllowEnd && seenValues[CSSValueForceEnd])
            || (value->id == CSSValueForceEnd && seenValues[CSSValueAllowEnd])) {
            isValid = false;
            break;
        }
        switch (value->id) {
        case CSSValueAllowEnd:
        case CSSValueFirst:
        case CSSValueForceEnd:
        case CSSValueLast:
            list->append(CSSValuePool::singleton().createIdentifierValue(value->id));
            seenValues.set(value->id);
            break;
        default:
            isValid = false;
            break;
        }
        if (isValid)
            value = m_valueList->next();
    }
    
    // Values are either valid or in shorthand scope.
    if (list->length() && isValid) {
        addProperty(CSSPropertyHangingPunctuation, list.release(), important);
        return true;
    }
    
    return false;
}

bool CSSParser::parseLineBoxContain(bool important)
{
    LineBoxContain lineBoxContain = LineBoxContainNone;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->id == CSSValueBlock) {
            if (lineBoxContain & LineBoxContainBlock)
                return false;
            lineBoxContain |= LineBoxContainBlock;
        } else if (value->id == CSSValueInline) {
            if (lineBoxContain & LineBoxContainInline)
                return false;
            lineBoxContain |= LineBoxContainInline;
        } else if (value->id == CSSValueFont) {
            if (lineBoxContain & LineBoxContainFont)
                return false;
            lineBoxContain |= LineBoxContainFont;
        } else if (value->id == CSSValueGlyphs) {
            if (lineBoxContain & LineBoxContainGlyphs)
                return false;
            lineBoxContain |= LineBoxContainGlyphs;
        } else if (value->id == CSSValueReplaced) {
            if (lineBoxContain & LineBoxContainReplaced)
                return false;
            lineBoxContain |= LineBoxContainReplaced;
        } else if (value->id == CSSValueInlineBox) {
            if (lineBoxContain & LineBoxContainInlineBox)
                return false;
            lineBoxContain |= LineBoxContainInlineBox;
        } else if (value->id == CSSValueInitialLetter) {
            if (lineBoxContain & LineBoxContainInitialLetter)
                return false;
            lineBoxContain |= LineBoxContainInitialLetter;
        } else
            return false;
    }

    if (!lineBoxContain)
        return false;

    addProperty(CSSPropertyWebkitLineBoxContain, CSSLineBoxContainValue::create(lineBoxContain), important);
    return true;
}

bool CSSParser::parseFontFeatureTag(CSSValueList& settings)
{
    CSSParserValue* value = m_valueList->current();
    // Feature tag name comes first
    if (value->unit != CSSPrimitiveValue::CSS_STRING)
        return false;
    FontFeatureTag tag;
    if (value->string.length() != tag.size())
        return false;
    for (unsigned i = 0; i < tag.size(); ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = value->string[i];
        if (character < 0x20 || character > 0x7E)
            return false;
        tag[i] = toASCIILower(character);
    }

    int tagValue = 1;
    // Feature tag values could follow: <integer> | on | off
    value = m_valueList->next();
    if (value) {
        if (value->unit == CSSPrimitiveValue::CSS_NUMBER && value->isInt && value->fValue >= 0) {
            tagValue = clampToInteger(value->fValue);
            if (tagValue < 0)
                return false;
            m_valueList->next();
        } else if (value->id == CSSValueOn || value->id == CSSValueOff) {
            tagValue = value->id == CSSValueOn;
            m_valueList->next();
        }
    }
    settings.append(CSSFontFeatureValue::create(WTFMove(tag), tagValue));
    return true;
}

bool CSSParser::parseFontFeatureSettings(bool important)
{
    if (m_valueList->size() == 1 && m_valueList->current()->id == CSSValueNormal) {
        RefPtr<CSSPrimitiveValue> normalValue = CSSValuePool::singleton().createIdentifierValue(CSSValueNormal);
        m_valueList->next();
        addProperty(CSSPropertyFontFeatureSettings, normalValue.release(), important);
        return true;
    }

    RefPtr<CSSValueList> settings = CSSValueList::createCommaSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (!parseFontFeatureTag(*settings))
            return false;

        // If the list isn't parsed fully, the current value should be comma.
        value = m_valueList->current();
        if (value && !isComma(value))
            return false;
    }
    if (settings->length()) {
        addProperty(CSSPropertyFontFeatureSettings, settings.release(), important);
        return true;
    }
    return false;
}

bool CSSParser::parseFontVariantLigatures(bool important, bool unknownIsFailure, bool implicit)
{
    auto values = CSSValueList::createSpaceSeparated();
    FontVariantLigatures commonLigatures = FontVariantLigatures::Normal;
    FontVariantLigatures discretionaryLigatures = FontVariantLigatures::Normal;
    FontVariantLigatures historicalLigatures = FontVariantLigatures::Normal;
    FontVariantLigatures contextualAlternates = FontVariantLigatures::Normal;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        switch (value->id) {
        case CSSValueNoCommonLigatures:
            commonLigatures = FontVariantLigatures::No;
            break;
        case CSSValueCommonLigatures:
            commonLigatures = FontVariantLigatures::Yes;
            break;
        case CSSValueNoDiscretionaryLigatures:
            discretionaryLigatures = FontVariantLigatures::No;
            break;
        case CSSValueDiscretionaryLigatures:
            discretionaryLigatures = FontVariantLigatures::Yes;
            break;
        case CSSValueNoHistoricalLigatures:
            historicalLigatures = FontVariantLigatures::No;
            break;
        case CSSValueHistoricalLigatures:
            historicalLigatures = FontVariantLigatures::Yes;
            break;
        case CSSValueContextual:
            contextualAlternates = FontVariantLigatures::Yes;
            break;
        case CSSValueNoContextual:
            contextualAlternates = FontVariantLigatures::No;
            break;
        default:
            if (unknownIsFailure)
                return false;
            continue;
        }
    }

    switch (commonLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueCommonLigatures));
        break;
    case FontVariantLigatures::No:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoCommonLigatures));
        break;
    }

    switch (discretionaryLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiscretionaryLigatures));
        break;
    case FontVariantLigatures::No:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoDiscretionaryLigatures));
        break;
    }

    switch (historicalLigatures) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalLigatures));
        break;
    case FontVariantLigatures::No:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoHistoricalLigatures));
        break;
    }

    switch (contextualAlternates) {
    case FontVariantLigatures::Normal:
        break;
    case FontVariantLigatures::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueContextual));
        break;
    case FontVariantLigatures::No:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueNoContextual));
        break;
    }

    if (!values->length())
        return !unknownIsFailure;

    addProperty(CSSPropertyFontVariantLigatures, WTFMove(values), important, implicit);
    return true;
}

bool CSSParser::parseFontVariantNumeric(bool important, bool unknownIsFailure, bool implicit)
{
    auto values = CSSValueList::createSpaceSeparated();
    FontVariantNumericFigure figure = FontVariantNumericFigure::Normal;
    FontVariantNumericSpacing spacing = FontVariantNumericSpacing::Normal;
    FontVariantNumericFraction fraction = FontVariantNumericFraction::Normal;
    FontVariantNumericOrdinal ordinal = FontVariantNumericOrdinal::Normal;
    FontVariantNumericSlashedZero slashedZero = FontVariantNumericSlashedZero::Normal;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        switch (value->id) {
        case CSSValueLiningNums:
            figure = FontVariantNumericFigure::LiningNumbers;
            break;
        case CSSValueOldstyleNums:
            figure = FontVariantNumericFigure::OldStyleNumbers;
            break;
        case CSSValueProportionalNums:
            spacing = FontVariantNumericSpacing::ProportionalNumbers;
            break;
        case CSSValueTabularNums:
            spacing = FontVariantNumericSpacing::TabularNumbers;
            break;
        case CSSValueDiagonalFractions:
            fraction = FontVariantNumericFraction::DiagonalFractions;
            break;
        case CSSValueStackedFractions:
            fraction = FontVariantNumericFraction::StackedFractions;
            break;
        case CSSValueOrdinal:
            ordinal = FontVariantNumericOrdinal::Yes;
            break;
        case CSSValueSlashedZero:
            slashedZero = FontVariantNumericSlashedZero::Yes;
            break;
        default:
            if (unknownIsFailure)
                return false;
            continue;
        }
    }

    switch (figure) {
    case FontVariantNumericFigure::Normal:
        break;
    case FontVariantNumericFigure::LiningNumbers:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueLiningNums));
        break;
    case FontVariantNumericFigure::OldStyleNumbers:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueOldstyleNums));
        break;
    }

    switch (spacing) {
    case FontVariantNumericSpacing::Normal:
        break;
    case FontVariantNumericSpacing::ProportionalNumbers:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalNums));
        break;
    case FontVariantNumericSpacing::TabularNumbers:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTabularNums));
        break;
    }

    switch (fraction) {
    case FontVariantNumericFraction::Normal:
        break;
    case FontVariantNumericFraction::DiagonalFractions:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueDiagonalFractions));
        break;
    case FontVariantNumericFraction::StackedFractions:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueStackedFractions));
        break;
    }

    switch (ordinal) {
    case FontVariantNumericOrdinal::Normal:
        break;
    case FontVariantNumericOrdinal::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueOrdinal));
        break;
    }

    switch (slashedZero) {
    case FontVariantNumericSlashedZero::Normal:
        break;
    case FontVariantNumericSlashedZero::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSlashedZero));
        break;
    }

    if (!values->length())
        return !unknownIsFailure;

    addProperty(CSSPropertyFontVariantNumeric, WTFMove(values), important, implicit);
    return true;
}

bool CSSParser::parseFontVariantEastAsian(bool important, bool unknownIsFailure, bool implicit)
{
    auto values = CSSValueList::createSpaceSeparated();
    FontVariantEastAsianVariant variant = FontVariantEastAsianVariant::Normal;
    FontVariantEastAsianWidth width = FontVariantEastAsianWidth::Normal;
    FontVariantEastAsianRuby ruby = FontVariantEastAsianRuby::Normal;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        switch (value->id) {
        case CSSValueJis78:
            variant = FontVariantEastAsianVariant::Jis78;
            break;
        case CSSValueJis83:
            variant = FontVariantEastAsianVariant::Jis83;
            break;
        case CSSValueJis90:
            variant = FontVariantEastAsianVariant::Jis90;
            break;
        case CSSValueJis04:
            variant = FontVariantEastAsianVariant::Jis04;
            break;
        case CSSValueSimplified:
            variant = FontVariantEastAsianVariant::Simplified;
            break;
        case CSSValueTraditional:
            variant = FontVariantEastAsianVariant::Traditional;
            break;
        case CSSValueFullWidth:
            width = FontVariantEastAsianWidth::Full;
            break;
        case CSSValueProportionalWidth:
            width = FontVariantEastAsianWidth::Proportional;
            break;
        case CSSValueRuby:
            ruby = FontVariantEastAsianRuby::Yes;
            break;
        default:
            if (unknownIsFailure)
                return false;
            continue;
        }
    }

    switch (variant) {
    case FontVariantEastAsianVariant::Normal:
        break;
    case FontVariantEastAsianVariant::Jis78:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis78));
        break;
    case FontVariantEastAsianVariant::Jis83:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis83));
        break;
    case FontVariantEastAsianVariant::Jis90:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis90));
        break;
    case FontVariantEastAsianVariant::Jis04:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueJis04));
        break;
    case FontVariantEastAsianVariant::Simplified:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueSimplified));
        break;
    case FontVariantEastAsianVariant::Traditional:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueTraditional));
        break;
    }

    switch (width) {
    case FontVariantEastAsianWidth::Normal:
        break;
    case FontVariantEastAsianWidth::Full:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueFullWidth));
        break;
    case FontVariantEastAsianWidth::Proportional:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueProportionalWidth));
        break;
    }

    switch (ruby) {
    case FontVariantEastAsianRuby::Normal:
        break;
    case FontVariantEastAsianRuby::Yes:
        values->append(CSSValuePool::singleton().createIdentifierValue(CSSValueRuby));
    }

    if (!values->length())
        return !unknownIsFailure;

    addProperty(CSSPropertyFontVariantEastAsian, WTFMove(values), important, implicit);
    return true;
}

bool CSSParser::parseFontVariant(bool important)
{
    ShorthandScope scope(this, CSSPropertyFontVariant);
    if (!parseFontVariantLigatures(important, false, false))
        return false;
    m_valueList->setCurrentIndex(0);
    if (!parseFontVariantNumeric(important, false, false))
        return false;
    m_valueList->setCurrentIndex(0);
    if (!parseFontVariantEastAsian(important, false, false))
        return false;
    m_valueList->setCurrentIndex(0);

    FontVariantPosition position = FontVariantPosition::Normal;
    FontVariantCaps caps = FontVariantCaps::Normal;
    FontVariantAlternates alternates = FontVariantAlternates::Normal;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        switch (value->id) {
        case CSSValueNoCommonLigatures:
        case CSSValueCommonLigatures:
        case CSSValueNoDiscretionaryLigatures:
        case CSSValueDiscretionaryLigatures:
        case CSSValueNoHistoricalLigatures:
        case CSSValueHistoricalLigatures:
        case CSSValueContextual:
        case CSSValueNoContextual:
        case CSSValueLiningNums:
        case CSSValueOldstyleNums:
        case CSSValueProportionalNums:
        case CSSValueTabularNums:
        case CSSValueDiagonalFractions:
        case CSSValueStackedFractions:
        case CSSValueOrdinal:
        case CSSValueSlashedZero:
        case CSSValueJis78:
        case CSSValueJis83:
        case CSSValueJis90:
        case CSSValueJis04:
        case CSSValueSimplified:
        case CSSValueTraditional:
        case CSSValueFullWidth:
        case CSSValueProportionalWidth:
        case CSSValueRuby:
            break;
        case CSSValueSub:
            position = FontVariantPosition::Subscript;
            break;
        case CSSValueSuper:
            position = FontVariantPosition::Superscript;
            break;
        case CSSValueSmallCaps:
            caps = FontVariantCaps::Small;
            break;
        case CSSValueAllSmallCaps:
            caps = FontVariantCaps::AllSmall;
            break;
        case CSSValuePetiteCaps:
            caps = FontVariantCaps::Petite;
            break;
        case CSSValueAllPetiteCaps:
            caps = FontVariantCaps::AllPetite;
            break;
        case CSSValueUnicase:
            caps = FontVariantCaps::Unicase;
            break;
        case CSSValueTitlingCaps:
            caps = FontVariantCaps::Titling;
            break;
        case CSSValueHistoricalForms:
            alternates = FontVariantAlternates::HistoricalForms;
            break;
        default:
            return false;
        }
    }

    switch (position) {
    case FontVariantPosition::Normal:
        break;
    case FontVariantPosition::Subscript:
        addProperty(CSSPropertyFontVariantPosition, CSSValuePool::singleton().createIdentifierValue(CSSValueSub), important, false);
        break;
    case FontVariantPosition::Superscript:
        addProperty(CSSPropertyFontVariantPosition, CSSValuePool::singleton().createIdentifierValue(CSSValueSuper), important, false);
        break;
    }

    switch (caps) {
    case FontVariantCaps::Normal:
        break;
    case FontVariantCaps::Small:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueSmallCaps), important, false);
        break;
    case FontVariantCaps::AllSmall:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueAllSmallCaps), important, false);
        break;
    case FontVariantCaps::Petite:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValuePetiteCaps), important, false);
        break;
    case FontVariantCaps::AllPetite:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueAllPetiteCaps), important, false);
        break;
    case FontVariantCaps::Unicase:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueUnicase), important, false);
        break;
    case FontVariantCaps::Titling:
        addProperty(CSSPropertyFontVariantCaps, CSSValuePool::singleton().createIdentifierValue(CSSValueTitlingCaps), important, false);
        break;
    }

    switch (alternates) {
    case FontVariantAlternates::Normal:
        break;
    case FontVariantAlternates::HistoricalForms:
        addProperty(CSSPropertyFontVariantAlternates, CSSValuePool::singleton().createIdentifierValue(CSSValueHistoricalForms), important, false);
        break;
    }

    return true;
}

static inline bool isValidWillChangeAnimatableFeature(const CSSParserValue& value)
{
    if (value.id == CSSValueNone || value.id == CSSValueAuto || value.id == CSSValueAll)
        return false;

    if (valueIsCSSKeyword(value))
        return false;

    if (cssPropertyID(value.string) == CSSPropertyWillChange)
        return false;

    return true;
}

bool CSSParser::parseWillChange(bool important)
{
    RefPtr<CSSValueList> willChangePropertyValues = CSSValueList::createCommaSeparated();

    bool expectComma = false;
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (expectComma) {
            if (!isComma(value))
                return false;
            
            expectComma = false;
            continue;
        }

        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        if (!isValidWillChangeAnimatableFeature(*value))
            return false;

        RefPtr<CSSValue> cssValue;
        if (value->id == CSSValueScrollPosition || value->id == CSSValueContents)
            cssValue = CSSValuePool::singleton().createIdentifierValue(value->id);
        else {
            CSSPropertyID propertyID = cssPropertyID(value->string);
            if (propertyID != CSSPropertyInvalid)
                cssValue = CSSValuePool::singleton().createIdentifierValue(propertyID);
            else // This might be a property we don't support.
                cssValue = createPrimitiveStringValue(*value);
        }

        willChangePropertyValues->append(cssValue.releaseNonNull());
        expectComma = true;
    }

    addProperty(CSSPropertyWillChange, willChangePropertyValues.release(), important);
    return true;
}

RefPtr<CSSCalcValue> CSSParser::parseCalculation(CSSParserValue& value, CalculationPermittedValueRange range)
{
    ASSERT(isCalculation(value));
    
    CSSParserValueList* args = value.function->args.get();
    if (!args || !args->size())
        return nullptr;

    return CSSCalcValue::create(value.function->name, *args, range);
}

#define END_TOKEN 0

#include "CSSGrammar.h"

enum CharacterType {
    // Types for the main switch.

    // The first 4 types must be grouped together, as they
    // represent the allowed chars in an identifier.
    CharacterCaselessU,
    CharacterIdentifierStart,
    CharacterNumber,
    CharacterDash,

    CharacterOther,
    CharacterNull,
    CharacterWhiteSpace,
    CharacterEndConditionQuery,
    CharacterEndNthChild,
    CharacterQuote,
    CharacterExclamationMark,
    CharacterHashmark,
    CharacterDollar,
    CharacterAsterisk,
    CharacterPlus,
    CharacterDot,
    CharacterSlash,
    CharacterLess,
    CharacterAt,
    CharacterBackSlash,
    CharacterXor,
    CharacterVerticalBar,
    CharacterTilde,
};

// 128 ASCII codes
static const CharacterType typesOfASCIICharacters[128] = {
/*   0 - Null               */ CharacterNull,
/*   1 - Start of Heading   */ CharacterOther,
/*   2 - Start of Text      */ CharacterOther,
/*   3 - End of Text        */ CharacterOther,
/*   4 - End of Transm.     */ CharacterOther,
/*   5 - Enquiry            */ CharacterOther,
/*   6 - Acknowledgment     */ CharacterOther,
/*   7 - Bell               */ CharacterOther,
/*   8 - Back Space         */ CharacterOther,
/*   9 - Horizontal Tab     */ CharacterWhiteSpace,
/*  10 - Line Feed          */ CharacterWhiteSpace,
/*  11 - Vertical Tab       */ CharacterOther,
/*  12 - Form Feed          */ CharacterWhiteSpace,
/*  13 - Carriage Return    */ CharacterWhiteSpace,
/*  14 - Shift Out          */ CharacterOther,
/*  15 - Shift In           */ CharacterOther,
/*  16 - Data Line Escape   */ CharacterOther,
/*  17 - Device Control 1   */ CharacterOther,
/*  18 - Device Control 2   */ CharacterOther,
/*  19 - Device Control 3   */ CharacterOther,
/*  20 - Device Control 4   */ CharacterOther,
/*  21 - Negative Ack.      */ CharacterOther,
/*  22 - Synchronous Idle   */ CharacterOther,
/*  23 - End of Transmit    */ CharacterOther,
/*  24 - Cancel             */ CharacterOther,
/*  25 - End of Medium      */ CharacterOther,
/*  26 - Substitute         */ CharacterOther,
/*  27 - Escape             */ CharacterOther,
/*  28 - File Separator     */ CharacterOther,
/*  29 - Group Separator    */ CharacterOther,
/*  30 - Record Separator   */ CharacterOther,
/*  31 - Unit Separator     */ CharacterOther,
/*  32 - Space              */ CharacterWhiteSpace,
/*  33 - !                  */ CharacterExclamationMark,
/*  34 - "                  */ CharacterQuote,
/*  35 - #                  */ CharacterHashmark,
/*  36 - $                  */ CharacterDollar,
/*  37 - %                  */ CharacterOther,
/*  38 - &                  */ CharacterOther,
/*  39 - '                  */ CharacterQuote,
/*  40 - (                  */ CharacterOther,
/*  41 - )                  */ CharacterEndNthChild,
/*  42 - *                  */ CharacterAsterisk,
/*  43 - +                  */ CharacterPlus,
/*  44 - ,                  */ CharacterOther,
/*  45 - -                  */ CharacterDash,
/*  46 - .                  */ CharacterDot,
/*  47 - /                  */ CharacterSlash,
/*  48 - 0                  */ CharacterNumber,
/*  49 - 1                  */ CharacterNumber,
/*  50 - 2                  */ CharacterNumber,
/*  51 - 3                  */ CharacterNumber,
/*  52 - 4                  */ CharacterNumber,
/*  53 - 5                  */ CharacterNumber,
/*  54 - 6                  */ CharacterNumber,
/*  55 - 7                  */ CharacterNumber,
/*  56 - 8                  */ CharacterNumber,
/*  57 - 9                  */ CharacterNumber,
/*  58 - :                  */ CharacterOther,
/*  59 - ;                  */ CharacterEndConditionQuery,
/*  60 - <                  */ CharacterLess,
/*  61 - =                  */ CharacterOther,
/*  62 - >                  */ CharacterOther,
/*  63 - ?                  */ CharacterOther,
/*  64 - @                  */ CharacterAt,
/*  65 - A                  */ CharacterIdentifierStart,
/*  66 - B                  */ CharacterIdentifierStart,
/*  67 - C                  */ CharacterIdentifierStart,
/*  68 - D                  */ CharacterIdentifierStart,
/*  69 - E                  */ CharacterIdentifierStart,
/*  70 - F                  */ CharacterIdentifierStart,
/*  71 - G                  */ CharacterIdentifierStart,
/*  72 - H                  */ CharacterIdentifierStart,
/*  73 - I                  */ CharacterIdentifierStart,
/*  74 - J                  */ CharacterIdentifierStart,
/*  75 - K                  */ CharacterIdentifierStart,
/*  76 - L                  */ CharacterIdentifierStart,
/*  77 - M                  */ CharacterIdentifierStart,
/*  78 - N                  */ CharacterIdentifierStart,
/*  79 - O                  */ CharacterIdentifierStart,
/*  80 - P                  */ CharacterIdentifierStart,
/*  81 - Q                  */ CharacterIdentifierStart,
/*  82 - R                  */ CharacterIdentifierStart,
/*  83 - S                  */ CharacterIdentifierStart,
/*  84 - T                  */ CharacterIdentifierStart,
/*  85 - U                  */ CharacterCaselessU,
/*  86 - V                  */ CharacterIdentifierStart,
/*  87 - W                  */ CharacterIdentifierStart,
/*  88 - X                  */ CharacterIdentifierStart,
/*  89 - Y                  */ CharacterIdentifierStart,
/*  90 - Z                  */ CharacterIdentifierStart,
/*  91 - [                  */ CharacterOther,
/*  92 - \                  */ CharacterBackSlash,
/*  93 - ]                  */ CharacterOther,
/*  94 - ^                  */ CharacterXor,
/*  95 - _                  */ CharacterIdentifierStart,
/*  96 - `                  */ CharacterOther,
/*  97 - a                  */ CharacterIdentifierStart,
/*  98 - b                  */ CharacterIdentifierStart,
/*  99 - c                  */ CharacterIdentifierStart,
/* 100 - d                  */ CharacterIdentifierStart,
/* 101 - e                  */ CharacterIdentifierStart,
/* 102 - f                  */ CharacterIdentifierStart,
/* 103 - g                  */ CharacterIdentifierStart,
/* 104 - h                  */ CharacterIdentifierStart,
/* 105 - i                  */ CharacterIdentifierStart,
/* 106 - j                  */ CharacterIdentifierStart,
/* 107 - k                  */ CharacterIdentifierStart,
/* 108 - l                  */ CharacterIdentifierStart,
/* 109 - m                  */ CharacterIdentifierStart,
/* 110 - n                  */ CharacterIdentifierStart,
/* 111 - o                  */ CharacterIdentifierStart,
/* 112 - p                  */ CharacterIdentifierStart,
/* 113 - q                  */ CharacterIdentifierStart,
/* 114 - r                  */ CharacterIdentifierStart,
/* 115 - s                  */ CharacterIdentifierStart,
/* 116 - t                  */ CharacterIdentifierStart,
/* 117 - u                  */ CharacterCaselessU,
/* 118 - v                  */ CharacterIdentifierStart,
/* 119 - w                  */ CharacterIdentifierStart,
/* 120 - x                  */ CharacterIdentifierStart,
/* 121 - y                  */ CharacterIdentifierStart,
/* 122 - z                  */ CharacterIdentifierStart,
/* 123 - {                  */ CharacterEndConditionQuery,
/* 124 - |                  */ CharacterVerticalBar,
/* 125 - }                  */ CharacterOther,
/* 126 - ~                  */ CharacterTilde,
/* 127 - Delete             */ CharacterOther,
};

// Utility functions for the CSS tokenizer.

template <typename CharacterType>
static inline bool isCSSLetter(CharacterType character)
{
    return character >= 128 || typesOfASCIICharacters[character] <= CharacterDash;
}

template <typename CharacterType>
static inline bool isCSSEscape(CharacterType character)
{
    return character >= ' ' && character != 127;
}

template <typename CharacterType>
static inline bool isURILetter(CharacterType character)
{
    return (character >= '*' && character != 127) || (character >= '#' && character <= '&') || character == '!';
}

template <typename CharacterType>
static inline bool isIdentifierStartAfterDash(CharacterType* currentCharacter)
{
    return isASCIIAlpha(currentCharacter[0]) || currentCharacter[0] == '_' || currentCharacter[0] >= 128
        || (currentCharacter[0] == '\\' && isCSSEscape(currentCharacter[1]));
}

template <typename CharacterType>
static inline bool isCustomPropertyIdentifier(CharacterType* currentCharacter)
{
    return isASCIIAlpha(currentCharacter[0]) || currentCharacter[0] == '_' || currentCharacter[0] >= 128
        || (currentCharacter[0] == '\\' && isCSSEscape(currentCharacter[1]));
}

template <typename CharacterType>
static inline bool isEqualToCSSIdentifier(CharacterType* cssString, const char* constantString)
{
    // Compare an character memory data with a zero terminated string.
    do {
        // The input must be part of an identifier if constantChar or constString
        // contains '-'. Otherwise toASCIILowerUnchecked('\r') would be equal to '-'.
        ASSERT((*constantString >= 'a' && *constantString <= 'z') || *constantString == '-');
        ASSERT(*constantString != '-' || isCSSLetter(*cssString));
        if (toASCIILowerUnchecked(*cssString++) != (*constantString++))
            return false;
    } while (*constantString);
    return true;
}

template <typename CharacterType>
static inline bool isEqualToCSSCaseSensitiveIdentifier(CharacterType* string, const char* constantString)
{
    do {
        if (*string++ != *constantString++)
            return false;
    } while (*constantString);
    return true;
}

template <typename CharacterType>
static CharacterType* checkAndSkipEscape(CharacterType* currentCharacter)
{
    // Returns with 0, if escape check is failed. Otherwise
    // it returns with the following character.
    ASSERT(*currentCharacter == '\\');

    ++currentCharacter;
    if (!isCSSEscape(*currentCharacter))
        return nullptr;

    if (isASCIIHexDigit(*currentCharacter)) {
        int length = 6;

        do {
            ++currentCharacter;
        } while (isASCIIHexDigit(*currentCharacter) && --length);

        // Optional space after the escape sequence.
        if (isHTMLSpace(*currentCharacter))
            ++currentCharacter;
        return currentCharacter;
    }
    return currentCharacter + 1;
}

template <typename CharacterType>
static inline CharacterType* skipWhiteSpace(CharacterType* currentCharacter)
{
    while (isHTMLSpace(*currentCharacter))
        ++currentCharacter;
    return currentCharacter;
}

// Main CSS tokenizer functions.

template <>
LChar* CSSParserString::characters<LChar>() const { return characters8(); }

template <>
UChar* CSSParserString::characters<UChar>() const { return characters16(); }

template <>
inline LChar*& CSSParser::currentCharacter<LChar>()
{
    return m_currentCharacter8;
}

template <>
inline UChar*& CSSParser::currentCharacter<UChar>()
{
    return m_currentCharacter16;
}

UChar*& CSSParser::currentCharacter16()
{
    if (!m_currentCharacter16) {
        m_dataStart16 = std::make_unique<UChar[]>(m_length);
        m_currentCharacter16 = m_dataStart16.get();
    }

    return m_currentCharacter16;
}

template <>
inline LChar* CSSParser::tokenStart<LChar>()
{
    return m_tokenStart.ptr8;
}

template <>
inline UChar* CSSParser::tokenStart<UChar>()
{
    return m_tokenStart.ptr16;
}

CSSParser::Location CSSParser::currentLocation()
{
    Location location;
    location.lineNumber = m_tokenStartLineNumber;
    location.columnNumber = m_tokenStartColumnNumber;

    ASSERT(location.lineNumber >= 0);
    ASSERT(location.columnNumber >= 0);

    if (location.lineNumber == m_sheetStartLineNumber)
        location.columnNumber += m_sheetStartColumnNumber;

    if (is8BitSource())
        location.token.init(tokenStart<LChar>(), currentCharacter<LChar>() - tokenStart<LChar>());
    else
        location.token.init(tokenStart<UChar>(), currentCharacter<UChar>() - tokenStart<UChar>());

    return location;
}

template <typename CharacterType>
inline bool CSSParser::isIdentifierStart()
{
    // Check whether an identifier is started.
    return isIdentifierStartAfterDash((*currentCharacter<CharacterType>() != '-') ? currentCharacter<CharacterType>() : currentCharacter<CharacterType>() + 1);
}

template <typename CharacterType>
static inline CharacterType* checkAndSkipString(CharacterType* currentCharacter, int quote)
{
    // Returns with 0, if string check is failed. Otherwise
    // it returns with the following character. This is necessary
    // since we cannot revert escape sequences, thus strings
    // must be validated before parsing.
    while (true) {
        if (UNLIKELY(*currentCharacter == quote)) {
            // String parsing is successful.
            return currentCharacter + 1;
        }
        if (UNLIKELY(!*currentCharacter)) {
            // String parsing is successful up to end of input.
            return currentCharacter;
        }
        if (UNLIKELY(*currentCharacter <= '\r' && (*currentCharacter == '\n' || (*currentCharacter | 0x1) == '\r'))) {
            // String parsing is failed for character '\n', '\f' or '\r'.
            return nullptr;
        }

        if (LIKELY(currentCharacter[0] != '\\'))
            ++currentCharacter;
        else if (currentCharacter[1] == '\n' || currentCharacter[1] == '\f')
            currentCharacter += 2;
        else if (currentCharacter[1] == '\r')
            currentCharacter += currentCharacter[2] == '\n' ? 3 : 2;
        else {
            currentCharacter = checkAndSkipEscape(currentCharacter);
            if (!currentCharacter)
                return nullptr;
        }
    }
}

template <typename CharacterType>
unsigned CSSParser::parseEscape(CharacterType*& src)
{
    ASSERT(*src == '\\' && isCSSEscape(src[1]));

    unsigned unicode = 0;

    ++src;
    if (isASCIIHexDigit(*src)) {

        int length = 6;

        do {
            unicode = (unicode << 4) + toASCIIHexValue(*src++);
        } while (--length && isASCIIHexDigit(*src));

        if (unicode > UCHAR_MAX_VALUE)
            unicode = replacementCharacter;

        // Optional space after the escape sequence.
        if (isHTMLSpace(*src))
            ++src;

        return unicode;
    }

    return *currentCharacter<CharacterType>()++;
}

template <>
inline void CSSParser::UnicodeToChars<LChar>(LChar*& result, unsigned unicode)
{
    ASSERT(unicode <= 0xff);
    *result = unicode;

    ++result;
}

template <>
inline void CSSParser::UnicodeToChars<UChar>(UChar*& result, unsigned unicode)
{
    // Replace unicode with a surrogate pairs when it is bigger than 0xffff
    if (U16_LENGTH(unicode) == 2) {
        *result++ = U16_LEAD(unicode);
        *result = U16_TRAIL(unicode);
    } else
        *result = unicode;

    ++result;
}

template <typename SrcCharacterType, typename DestCharacterType>
inline bool CSSParser::parseIdentifierInternal(SrcCharacterType*& src, DestCharacterType*& result, bool& hasEscape)
{
    hasEscape = false;
    do {
        if (LIKELY(*src != '\\'))
            *result++ = *src++;
        else {
            hasEscape = true;
            SrcCharacterType* savedEscapeStart = src;
            unsigned unicode = parseEscape<SrcCharacterType>(src);
            if (unicode > 0xff && sizeof(DestCharacterType) == 1) {
                src = savedEscapeStart;
                return false;
            }
            UnicodeToChars(result, unicode);
        }
    } while (isCSSLetter(src[0]) || (src[0] == '\\' && isCSSEscape(src[1])));

    return true;
}

template <typename CharacterType>
inline void CSSParser::parseIdentifier(CharacterType*& result, CSSParserString& resultString, bool& hasEscape)
{
    CharacterType* start = currentCharacter<CharacterType>();
    if (UNLIKELY(!parseIdentifierInternal(currentCharacter<CharacterType>(), result, hasEscape))) {
        // Found an escape we couldn't handle with 8 bits, copy what has been recognized and continue
        ASSERT(is8BitSource());
        UChar*& result16 = currentCharacter16();
        UChar* start16 = result16;
        int i = 0;
        for (; i < result - start; ++i)
            result16[i] = start[i];

        result16 += i;

        parseIdentifierInternal(currentCharacter<CharacterType>(), result16, hasEscape);

        result += result16 - start16;
        resultString.init(start16, result16 - start16);

        return;
    }

    resultString.init(start, result - start);
}

template <typename SrcCharacterType, typename DestCharacterType>
inline bool CSSParser::parseStringInternal(SrcCharacterType*& src, DestCharacterType*& result, UChar quote)
{
    while (true) {
        if (UNLIKELY(*src == quote)) {
            // String parsing is done.
            ++src;
            return true;
        }
        if (UNLIKELY(!*src)) {
            // String parsing is done, but don't advance pointer if at the end of input.
            return true;
        }
        ASSERT(*src > '\r' || (*src < '\n' && *src) || *src == '\v');

        if (LIKELY(src[0] != '\\'))
            *result++ = *src++;
        else if (src[1] == '\n' || src[1] == '\f')
            src += 2;
        else if (src[1] == '\r')
            src += src[2] == '\n' ? 3 : 2;
        else {
            SrcCharacterType* savedEscapeStart = src;
            unsigned unicode = parseEscape<SrcCharacterType>(src);
            if (unicode > 0xff && sizeof(DestCharacterType) == 1) {
                src = savedEscapeStart;
                return false;
            }
            UnicodeToChars(result, unicode);
        }
    }

    return true;
}

template <typename CharacterType>
inline void CSSParser::parseString(CharacterType*& result, CSSParserString& resultString, UChar quote)
{
    CharacterType* start = currentCharacter<CharacterType>();

    if (UNLIKELY(!parseStringInternal(currentCharacter<CharacterType>(), result, quote))) {
        // Found an escape we couldn't handle with 8 bits, copy what has been recognized and continue
        ASSERT(is8BitSource());
        UChar*& result16 = currentCharacter16();
        UChar* start16 = result16;
        int i = 0;
        for (; i < result - start; ++i)
            result16[i] = start[i];

        result16 += i;

        parseStringInternal(currentCharacter<CharacterType>(), result16, quote);

        resultString.init(start16, result16 - start16);
        return;
    }

    resultString.init(start, result - start);
}

template <typename CharacterType>
inline bool CSSParser::findURI(CharacterType*& start, CharacterType*& end, UChar& quote)
{
    start = skipWhiteSpace(currentCharacter<CharacterType>());
    
    if (*start == '"' || *start == '\'') {
        quote = *start++;
        end = checkAndSkipString(start, quote);
        if (!end)
            return false;
    } else {
        quote = 0;
        end = start;
        while (isURILetter(*end)) {
            if (LIKELY(*end != '\\'))
                ++end;
            else {
                end = checkAndSkipEscape(end);
                if (!end)
                    return false;
            }
        }
    }

    end = skipWhiteSpace(end);
    if (*end != ')')
        return false;
    
    return true;
}

template <typename SrcCharacterType, typename DestCharacterType>
inline bool CSSParser::parseURIInternal(SrcCharacterType*& src, DestCharacterType*& dest, UChar quote)
{
    if (quote) {
        ASSERT(quote == '"' || quote == '\'');
        return parseStringInternal(src, dest, quote);
    }
    
    while (isURILetter(*src)) {
        if (LIKELY(*src != '\\'))
            *dest++ = *src++;
        else {
            unsigned unicode = parseEscape<SrcCharacterType>(src);
            if (unicode > 0xff && sizeof(SrcCharacterType) == 1)
                return false;
            UnicodeToChars(dest, unicode);
        }
    }

    return true;
}

template <typename CharacterType>
inline void CSSParser::parseURI(CSSParserString& string)
{
    CharacterType* uriStart;
    CharacterType* uriEnd;
    UChar quote;
    if (!findURI(uriStart, uriEnd, quote))
        return;
    
    CharacterType* dest = currentCharacter<CharacterType>() = uriStart;
    if (LIKELY(parseURIInternal(currentCharacter<CharacterType>(), dest, quote)))
        string.init(uriStart, dest - uriStart);
    else {
        // An escape sequence was encountered that can't be stored in 8 bits.
        // Reset the current character to the start of the URI and re-parse with
        // a 16-bit destination.
        ASSERT(is8BitSource());
        UChar* uriStart16 = currentCharacter16();
        currentCharacter<CharacterType>() = uriStart;
        bool result = parseURIInternal(currentCharacter<CharacterType>(), currentCharacter16(), quote);
        ASSERT_UNUSED(result, result);
        string.init(uriStart16, currentCharacter16() - uriStart16);
    }

    currentCharacter<CharacterType>() = uriEnd + 1;
    m_token = URI;
}

template <typename CharacterType>
inline bool CSSParser::parseUnicodeRange()
{
    CharacterType* character = currentCharacter<CharacterType>() + 1;
    int length = 6;
    ASSERT(*currentCharacter<CharacterType>() == '+');

    while (isASCIIHexDigit(*character) && length) {
        ++character;
        --length;
    }

    if (length && *character == '?') {
        // At most 5 hex digit followed by a question mark.
        do {
            ++character;
            --length;
        } while (*character == '?' && length);
        currentCharacter<CharacterType>() = character;
        return true;
    }

    if (length < 6) {
        // At least one hex digit.
        if (character[0] == '-' && isASCIIHexDigit(character[1])) {
            // Followed by a dash and a hex digit.
            ++character;
            length = 6;
            do {
                ++character;
            } while (--length && isASCIIHexDigit(*character));
        }
        currentCharacter<CharacterType>() = character;
        return true;
    }
    return false;
}

template <typename CharacterType>
bool CSSParser::parseNthChild()
{
    CharacterType* character = currentCharacter<CharacterType>();

    while (isASCIIDigit(*character))
        ++character;
    if (isASCIIAlphaCaselessEqual(*character, 'n')) {
        currentCharacter<CharacterType>() = character + 1;
        return true;
    }
    return false;
}

template <typename CharacterType>
bool CSSParser::parseNthChildExtra()
{
    CharacterType* character = skipWhiteSpace(currentCharacter<CharacterType>());
    if (*character != '+' && *character != '-')
        return false;

    character = skipWhiteSpace(character + 1);
    if (!isASCIIDigit(*character))
        return false;

    do {
        ++character;
    } while (isASCIIDigit(*character));

    currentCharacter<CharacterType>() = character;
    return true;
}

template <typename CharacterType>
inline bool CSSParser::detectFunctionTypeToken(int length)
{
    ASSERT(length > 0);
    CharacterType* name = tokenStart<CharacterType>();

    switch (length) {
    case 3:
        if (isASCIIAlphaCaselessEqual(name[0], 'n') && isASCIIAlphaCaselessEqual(name[1], 'o') && isASCIIAlphaCaselessEqual(name[2], 't')) {
            m_token = NOTFUNCTION;
            return true;
        }
        if (isASCIIAlphaCaselessEqual(name[0], 'u') && isASCIIAlphaCaselessEqual(name[1], 'r') && isASCIIAlphaCaselessEqual(name[2], 'l')) {
            m_token = URI;
            return true;
        }
        if (isASCIIAlphaCaselessEqual(name[0], 'v') && isASCIIAlphaCaselessEqual(name[1], 'a') && isASCIIAlphaCaselessEqual(name[2], 'r')) {
            m_token = VARFUNCTION;
            return true;
        }
#if ENABLE(VIDEO_TRACK)
        if (isASCIIAlphaCaselessEqual(name[0], 'c') && isASCIIAlphaCaselessEqual(name[1], 'u') && isASCIIAlphaCaselessEqual(name[2], 'e')) {
            m_token = CUEFUNCTION;
            return true;
        }
#endif
#if ENABLE(CSS_SELECTORS_LEVEL4)
        if (isASCIIAlphaCaselessEqual(name[0], 'd') && isASCIIAlphaCaselessEqual(name[1], 'i') && isASCIIAlphaCaselessEqual(name[2], 'r')) {
            m_token = DIRFUNCTION;
            return true;
        }
#endif
        return false;

    case 4:
        if (isEqualToCSSIdentifier(name, "calc")) {
            m_token = CALCFUNCTION;
            return true;
        }
        if (isEqualToCSSIdentifier(name, "lang")) {
            m_token = LANGFUNCTION;
            return true;
        }
#if ENABLE(CSS_SELECTORS_LEVEL4)
        if (isEqualToCSSIdentifier(name, "role")) {
            m_token = ROLEFUNCTION;
            return true;
        }
#endif
        return false;

    case 7:
        if (isEqualToCSSIdentifier(name, "matches")) {
            m_token = MATCHESFUNCTION;
            return true;
        }
#if ENABLE(SHADOW_DOM)
        if (isEqualToCSSIdentifier(name, "slotted")) {
            m_token = SLOTTEDFUNCTION;
            return true;
        }
#endif
        return false;

    case 9:
        if (isEqualToCSSIdentifier(name, "nth-child")) {
            m_token = NTHCHILDFUNCTIONS;
            m_parsingMode = NthChildMode;
            return true;
        }
        return false;

    case 11:
        if (isEqualToCSSIdentifier(name, "nth-of-type")) {
            m_parsingMode = NthChildMode;
            return true;
        }
        return false;

    case 14:
        if (isEqualToCSSIdentifier(name, "nth-last-child")) {
            m_token = NTHCHILDFUNCTIONS;
            m_parsingMode = NthChildMode;
            return true;
        }
        return false;

    case 16:
        if (isEqualToCSSIdentifier(name, "nth-last-of-type")) {
            m_parsingMode = NthChildMode;
            return true;
        }
        return false;
    }

    return false;
}

template <typename CharacterType>
inline void CSSParser::detectMediaQueryToken(int length)
{
    ASSERT(m_parsingMode == MediaQueryMode);
    CharacterType* name = tokenStart<CharacterType>();

    if (length == 3) {
        if (isASCIIAlphaCaselessEqual(name[0], 'a') && isASCIIAlphaCaselessEqual(name[1], 'n') && isASCIIAlphaCaselessEqual(name[2], 'd'))
            m_token = MEDIA_AND;
        else if (isASCIIAlphaCaselessEqual(name[0], 'n') && isASCIIAlphaCaselessEqual(name[1], 'o') && isASCIIAlphaCaselessEqual(name[2], 't'))
            m_token = MEDIA_NOT;
    } else if (length == 4) {
        if (isASCIIAlphaCaselessEqual(name[0], 'o') && isASCIIAlphaCaselessEqual(name[1], 'n')
                && isASCIIAlphaCaselessEqual(name[2], 'l') && isASCIIAlphaCaselessEqual(name[3], 'y'))
            m_token = MEDIA_ONLY;
    }
}

template <typename CharacterType>
inline void CSSParser::detectNumberToken(CharacterType* type, int length)
{
    ASSERT(length > 0);

    switch (toASCIILowerUnchecked(type[0])) {
    case 'c':
        if (length == 2 && isASCIIAlphaCaselessEqual(type[1], 'm'))
            m_token = CMS;
        else if (length == 2 && isASCIIAlphaCaselessEqual(type[1], 'h'))
            m_token = CHS;
        return;

    case 'd':
        if (length == 3 && isASCIIAlphaCaselessEqual(type[1], 'e') && isASCIIAlphaCaselessEqual(type[2], 'g'))
            m_token = DEGS;
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
        else if (length > 2 && isASCIIAlphaCaselessEqual(type[1], 'p')) {
            if (length == 4) {
                // There is a discussion about the name of this unit on www-style.
                // Keep this compile time guard in place until that is resolved.
                // http://lists.w3.org/Archives/Public/www-style/2012May/0915.html
                if (isASCIIAlphaCaselessEqual(type[2], 'p') && isASCIIAlphaCaselessEqual(type[3], 'x'))
                    m_token = DPPX;
                else if (isASCIIAlphaCaselessEqual(type[2], 'c') && isASCIIAlphaCaselessEqual(type[3], 'm'))
                    m_token = DPCM;
            } else if (length == 3 && isASCIIAlphaCaselessEqual(type[2], 'i'))
                    m_token = DPI;
        }
#endif
        return;

    case 'e':
        if (length == 2) {
            if (isASCIIAlphaCaselessEqual(type[1], 'm'))
                m_token = EMS;
            else if (isASCIIAlphaCaselessEqual(type[1], 'x'))
                m_token = EXS;
        }
        return;

    case 'f':
        if (length == 2 && isASCIIAlphaCaselessEqual(type[1], 'r'))
            m_token = FR;
        return;
    case 'g':
        if (length == 4 && isASCIIAlphaCaselessEqual(type[1], 'r')
                && isASCIIAlphaCaselessEqual(type[2], 'a') && isASCIIAlphaCaselessEqual(type[3], 'd'))
            m_token = GRADS;
        return;

    case 'h':
        if (length == 2 && isASCIIAlphaCaselessEqual(type[1], 'z'))
            m_token = HERTZ;
        return;

    case 'i':
        if (length == 2 && isASCIIAlphaCaselessEqual(type[1], 'n'))
            m_token = INS;
        return;

    case 'k':
        if (length == 3 && isASCIIAlphaCaselessEqual(type[1], 'h') && isASCIIAlphaCaselessEqual(type[2], 'z'))
            m_token = KHERTZ;
        return;

    case 'm':
        if (length == 2) {
            if (isASCIIAlphaCaselessEqual(type[1], 'm'))
                m_token = MMS;
            else if (isASCIIAlphaCaselessEqual(type[1], 's'))
                m_token = MSECS;
        }
        return;

    case 'p':
        if (length == 2) {
            if (isASCIIAlphaCaselessEqual(type[1], 'x'))
                m_token = PXS;
            else if (isASCIIAlphaCaselessEqual(type[1], 't'))
                m_token = PTS;
            else if (isASCIIAlphaCaselessEqual(type[1], 'c'))
                m_token = PCS;
        }
        return;

    case 'r':
        if (length == 3) {
            if (isASCIIAlphaCaselessEqual(type[1], 'a') && isASCIIAlphaCaselessEqual(type[2], 'd'))
                m_token = RADS;
            else if (isASCIIAlphaCaselessEqual(type[1], 'e') && isASCIIAlphaCaselessEqual(type[2], 'm'))
                m_token = REMS;
        }
        return;

    case 's':
        if (length == 1)
            m_token = SECS;
        return;

    case 't':
        if (length == 4 && isASCIIAlphaCaselessEqual(type[1], 'u')
                && isASCIIAlphaCaselessEqual(type[2], 'r') && isASCIIAlphaCaselessEqual(type[3], 'n'))
            m_token = TURNS;
        return;
    case 'v':
        if (length == 2) {
            if (isASCIIAlphaCaselessEqual(type[1], 'w'))
                m_token = VW;
            else if (isASCIIAlphaCaselessEqual(type[1], 'h'))
                m_token = VH;
        } else if (length == 4 && isASCIIAlphaCaselessEqual(type[1], 'm')) {
            if (isASCIIAlphaCaselessEqual(type[2], 'i') && isASCIIAlphaCaselessEqual(type[3], 'n'))
                m_token = VMIN;
            else if (isASCIIAlphaCaselessEqual(type[2], 'a') && isASCIIAlphaCaselessEqual(type[3], 'x'))
                m_token = VMAX;
        }
        return;

    default:
        if (type[0] == '_' && length == 5 && type[1] == '_' && isASCIIAlphaCaselessEqual(type[2], 'q')
                && isASCIIAlphaCaselessEqual(type[3], 'e') && isASCIIAlphaCaselessEqual(type[4], 'm'))
            m_token = QEMS;
        return;
    }
}

template <typename CharacterType>
inline void CSSParser::detectDashToken(int length)
{
    CharacterType* name = tokenStart<CharacterType>();

    if (length == 11) {
        if (isASCIIAlphaCaselessEqual(name[10], 'y') && isEqualToCSSIdentifier(name + 1, "webkit-an"))
            m_token = ANYFUNCTION;
        else if (isASCIIAlphaCaselessEqual(name[10], 'n') && isEqualToCSSIdentifier(name + 1, "webkit-mi"))
            m_token = MINFUNCTION;
        else if (isASCIIAlphaCaselessEqual(name[10], 'x') && isEqualToCSSIdentifier(name + 1, "webkit-ma"))
            m_token = MAXFUNCTION;
    } else if (length == 12 && isEqualToCSSIdentifier(name + 1, "webkit-calc"))
        m_token = CALCFUNCTION;
}

template <typename CharacterType>
inline void CSSParser::detectAtToken(int length, bool hasEscape)
{
    CharacterType* name = tokenStart<CharacterType>();
    ASSERT(name[0] == '@' && length >= 2);

    // charset, font-face, import, media, namespace, page, supports,
    // -webkit-keyframes, and -webkit-mediaquery are not affected by hasEscape.
    switch (toASCIILowerUnchecked(name[1])) {
    case 'b':
        if (hasEscape)
            return;

        switch (length) {
        case 12:
            if (isEqualToCSSIdentifier(name + 2, "ottom-left"))
                m_token = BOTTOMLEFT_SYM;
            return;

        case 13:
            if (isEqualToCSSIdentifier(name + 2, "ottom-right"))
                m_token = BOTTOMRIGHT_SYM;
            return;

        case 14:
            if (isEqualToCSSIdentifier(name + 2, "ottom-center"))
                m_token = BOTTOMCENTER_SYM;
            return;

        case 19:
            if (isEqualToCSSIdentifier(name + 2, "ottom-left-corner"))
                m_token = BOTTOMLEFTCORNER_SYM;
            return;

        case 20:
            if (isEqualToCSSIdentifier(name + 2, "ottom-right-corner"))
                m_token = BOTTOMRIGHTCORNER_SYM;
            return;
        }
        return;

    case 'c':
        if (length == 8 && isEqualToCSSIdentifier(name + 2, "harset"))
            m_token = CHARSET_SYM;
        return;

    case 'f':
        if (length == 10 && isEqualToCSSIdentifier(name + 2, "ont-face"))
            m_token = FONT_FACE_SYM;
        return;

    case 'i':
        if (length == 7 && isEqualToCSSIdentifier(name + 2, "mport")) {
            m_parsingMode = MediaQueryMode;
            m_token = IMPORT_SYM;
        }
        return;

    case 'k':
        if (length == 10 && isEqualToCSSIdentifier(name + 2, "eyframes"))
            m_token = KEYFRAMES_SYM;
        else if (length == 14 && !hasEscape && isEqualToCSSIdentifier(name + 2, "eyframe-rule"))
            m_token = KEYFRAME_RULE_SYM;
        return;

    case 'l':
        if (hasEscape)
            return;

        if (length == 9) {
            if (isEqualToCSSIdentifier(name + 2, "eft-top"))
                m_token = LEFTTOP_SYM;
        } else if (length == 12) {
            // Checking the last character first could further reduce the possibile cases.
            if (isASCIIAlphaCaselessEqual(name[11], 'e') && isEqualToCSSIdentifier(name + 2, "eft-middl"))
                m_token = LEFTMIDDLE_SYM;
            else if (isASCIIAlphaCaselessEqual(name[11], 'm') && isEqualToCSSIdentifier(name + 2, "eft-botto"))
                m_token = LEFTBOTTOM_SYM;
        }
        return;

    case 'm':
        if (length == 6 && isEqualToCSSIdentifier(name + 2, "edia")) {
            m_parsingMode = MediaQueryMode;
            m_token = MEDIA_SYM;
        }
        return;

    case 'n':
        if (length == 10 && isEqualToCSSIdentifier(name + 2, "amespace"))
            m_token = NAMESPACE_SYM;
        return;

    case 'p':
        if (length == 5 && isEqualToCSSIdentifier(name + 2, "age"))
            m_token = PAGE_SYM;
        return;

    case 'r':
        if (hasEscape)
            return;

        if (length == 10) {
            if (isEqualToCSSIdentifier(name + 2, "ight-top"))
                m_token = RIGHTTOP_SYM;
        } else if (length == 13) {
            // Checking the last character first could further reduce the possibile cases.
            if (isASCIIAlphaCaselessEqual(name[12], 'e') && isEqualToCSSIdentifier(name + 2, "ight-middl"))
                m_token = RIGHTMIDDLE_SYM;
            else if (isASCIIAlphaCaselessEqual(name[12], 'm') && isEqualToCSSIdentifier(name + 2, "ight-botto"))
                m_token = RIGHTBOTTOM_SYM;
        }
        return;

    case 's':
        if (length == 9 && isEqualToCSSIdentifier(name + 2, "upports")) {
            m_parsingMode = SupportsMode;
            m_token = SUPPORTS_SYM;
        }
        return;

    case 't':
        if (hasEscape)
            return;

        switch (length) {
        case 9:
            if (isEqualToCSSIdentifier(name + 2, "op-left"))
                m_token = TOPLEFT_SYM;
            return;

        case 10:
            if (isEqualToCSSIdentifier(name + 2, "op-right"))
                m_token = TOPRIGHT_SYM;
            return;

        case 11:
            if (isEqualToCSSIdentifier(name + 2, "op-center"))
                m_token = TOPCENTER_SYM;
            return;

        case 16:
            if (isEqualToCSSIdentifier(name + 2, "op-left-corner"))
                m_token = TOPLEFTCORNER_SYM;
            return;

        case 17:
            if (isEqualToCSSIdentifier(name + 2, "op-right-corner"))
                m_token = TOPRIGHTCORNER_SYM;
            return;
        }
        return;

    case '-':
        switch (length) {
        case 13:
            if (!hasEscape && isEqualToCSSIdentifier(name + 2, "webkit-rule"))
                m_token = WEBKIT_RULE_SYM;
            return;

        case 14:
            if (hasEscape)
                return;

            // Checking the last character first could further reduce the possibile cases.
            if (isASCIIAlphaCaselessEqual(name[13], 's') && isEqualToCSSIdentifier(name + 2, "webkit-decl"))
                m_token = WEBKIT_DECLS_SYM;
            else if (isASCIIAlphaCaselessEqual(name[13], 'e') && isEqualToCSSIdentifier(name + 2, "webkit-valu"))
                m_token = WEBKIT_VALUE_SYM;
            return;

        case 15:
            if (hasEscape)
                return;

#if ENABLE(CSS_REGIONS)
            if (isASCIIAlphaCaselessEqual(name[14], 'n') && isEqualToCSSIdentifier(name + 2, "webkit-regio"))
                m_token = WEBKIT_REGION_RULE_SYM;
#endif
            return;

        case 17:
            if (hasEscape)
                return;

            if (isASCIIAlphaCaselessEqual(name[16], 'r') && isEqualToCSSIdentifier(name + 2, "webkit-selecto"))
                m_token = WEBKIT_SELECTOR_SYM;
#if ENABLE(CSS_DEVICE_ADAPTATION)
            else if (isASCIIAlphaCaselessEqual(name[16], 't') && isEqualToCSSIdentifier(name + 2, "webkit-viewpor"))
                m_token = WEBKIT_VIEWPORT_RULE_SYM;
#endif
            return;

        case 18:
            if (isEqualToCSSIdentifier(name + 2, "webkit-keyframes"))
                m_token = KEYFRAMES_SYM;
            else if (isEqualToCSSIdentifier(name + 2, "webkit-sizesattr"))
                m_token = WEBKIT_SIZESATTR_SYM;
            return;

        case 19:
            if (isEqualToCSSIdentifier(name + 2, "webkit-mediaquery")) {
                m_parsingMode = MediaQueryMode;
                m_token = WEBKIT_MEDIAQUERY_SYM;
            }
            return;

        case 22:
            if (!hasEscape && isEqualToCSSIdentifier(name + 2, "webkit-keyframe-rule"))
                m_token = KEYFRAME_RULE_SYM;
            return;

        case 27:
            if (isEqualToCSSIdentifier(name + 2, "webkit-supports-condition")) {
                m_parsingMode = SupportsMode;
                m_token = WEBKIT_SUPPORTS_CONDITION_SYM;
            }
            return;
        }
    }
}

template <typename CharacterType>
inline void CSSParser::detectSupportsToken(int length)
{
    ASSERT(m_parsingMode == SupportsMode);
    CharacterType* name = tokenStart<CharacterType>();

    if (length == 2) {
        if (isASCIIAlphaCaselessEqual(name[0], 'o') && isASCIIAlphaCaselessEqual(name[1], 'r'))
            m_token = SUPPORTS_OR;
    } else if (length == 3) {
        if (isASCIIAlphaCaselessEqual(name[0], 'a') && isASCIIAlphaCaselessEqual(name[1], 'n') && isASCIIAlphaCaselessEqual(name[2], 'd'))
            m_token = SUPPORTS_AND;
        else if (isASCIIAlphaCaselessEqual(name[0], 'n') && isASCIIAlphaCaselessEqual(name[1], 'o') && isASCIIAlphaCaselessEqual(name[2], 't'))
            m_token = SUPPORTS_NOT;
    }
}

template <typename SrcCharacterType>
int CSSParser::realLex(void* yylvalWithoutType)
{
    YYSTYPE* yylval = static_cast<YYSTYPE*>(yylvalWithoutType);
    // Write pointer for the next character.
    SrcCharacterType* result;
    CSSParserString resultString;
    bool hasEscape;

    // The input buffer is terminated by a \0 character, so
    // it is safe to read one character ahead of a known non-null.
#ifndef NDEBUG
    // In debug we check with an ASSERT that the length is > 0 for string types.
    yylval->string.clear();
#endif

restartAfterComment:
    result = currentCharacter<SrcCharacterType>();
    setTokenStart(result);
    m_tokenStartLineNumber = m_lineNumber;
    m_tokenStartColumnNumber = tokenStartOffset() - m_columnOffsetForLine;
    m_token = *currentCharacter<SrcCharacterType>();
    ++currentCharacter<SrcCharacterType>();

    switch ((m_token <= 127) ? typesOfASCIICharacters[m_token] : CharacterIdentifierStart) {
    case CharacterCaselessU:
        if (UNLIKELY(*currentCharacter<SrcCharacterType>() == '+')) {
            if (parseUnicodeRange<SrcCharacterType>()) {
                m_token = UNICODERANGE;
                yylval->string.init(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
                break;
            }
        }
        FALLTHROUGH; // To CharacterIdentifierStart.

    case CharacterIdentifierStart:
        --currentCharacter<SrcCharacterType>();
        parseIdentifier(result, yylval->string, hasEscape);
        m_token = IDENT;

        if (UNLIKELY(*currentCharacter<SrcCharacterType>() == '(')) {
            if (m_parsingMode == SupportsMode && !hasEscape) {
                detectSupportsToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
                if (m_token != IDENT)
                    break;
            }
            m_token = FUNCTION;
            bool shouldSkipParenthesis = true;
            if (!hasEscape) {
                bool detected = detectFunctionTypeToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
                if (!detected && m_parsingMode == MediaQueryMode) {
                    // ... and(max-width: 480px) ... looks like a function, but in fact it is not,
                    // so run more detection code in the MediaQueryMode.
                    detectMediaQueryToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
                    shouldSkipParenthesis = false;
                }
            }

            if (LIKELY(shouldSkipParenthesis)) {
                ++currentCharacter<SrcCharacterType>();
                ++result;
                ++yylval->string.m_length;
            }

            if (token() == URI) {
                m_token = FUNCTION;
                // Check whether it is really an URI.
                if (yylval->string.is8Bit())
                    parseURI<LChar>(yylval->string);
                else
                    parseURI<UChar>(yylval->string);
            }
        } else if (UNLIKELY(m_parsingMode != NormalMode) && !hasEscape) {
            if (m_parsingMode == MediaQueryMode)
                detectMediaQueryToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
            else if (m_parsingMode == SupportsMode)
                detectSupportsToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
            else if (m_parsingMode == NthChildMode && isASCIIAlphaCaselessEqual(tokenStart<SrcCharacterType>()[0], 'n')) {
                if (result - tokenStart<SrcCharacterType>() == 1) {
                    // String "n" is IDENT but "n+1" is NTH.
                    if (parseNthChildExtra<SrcCharacterType>()) {
                        m_token = NTH;
                        yylval->string.m_length = currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>();
                    }
                } else if (result - tokenStart<SrcCharacterType>() >= 2 && tokenStart<SrcCharacterType>()[1] == '-') {
                    // String "n-" is IDENT but "n-1" is NTH.
                    // Set currentCharacter to '-' to continue parsing.
                    SrcCharacterType* nextCharacter = result;
                    currentCharacter<SrcCharacterType>() = tokenStart<SrcCharacterType>() + 1;
                    if (parseNthChildExtra<SrcCharacterType>()) {
                        m_token = NTH;
                        yylval->string.setLength(currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
                    } else {
                        // Revert the change to currentCharacter if unsuccessful.
                        currentCharacter<SrcCharacterType>() = nextCharacter;
                    }
                }
            }
        }
        if (m_parsingMode == NthChildMode && m_token == IDENT && yylval->string.length() == 2 && equalLettersIgnoringASCIICase(yylval->string, "of")) {
            m_parsingMode = NormalMode;
            m_token = NTHCHILDSELECTORSEPARATOR;
        }
        break;

    case CharacterDot:
        if (!isASCIIDigit(currentCharacter<SrcCharacterType>()[0]))
            break;
        FALLTHROUGH; // To CharacterNumber.

    case CharacterNumber: {
        bool dotSeen = (m_token == '.');

        while (true) {
            if (!isASCIIDigit(currentCharacter<SrcCharacterType>()[0])) {
                // Only one dot is allowed for a number,
                // and it must be followed by a digit.
                if (currentCharacter<SrcCharacterType>()[0] != '.' || dotSeen || !isASCIIDigit(currentCharacter<SrcCharacterType>()[1]))
                    break;
                dotSeen = true;
            }
            ++currentCharacter<SrcCharacterType>();
        }

        if (UNLIKELY(m_parsingMode == NthChildMode) && !dotSeen && isASCIIAlphaCaselessEqual(*currentCharacter<SrcCharacterType>(), 'n')) {
            // "[0-9]+n" is always an NthChild.
            ++currentCharacter<SrcCharacterType>();
            parseNthChildExtra<SrcCharacterType>();
            m_token = NTH;
            yylval->string.init(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
            break;
        }

        // Use SVG parser for numbers on SVG presentation attributes.
        if (m_context.mode == SVGAttributeMode) {
            // We need to take care of units like 'em' or 'ex'.
            SrcCharacterType* character = currentCharacter<SrcCharacterType>();
            if (isASCIIAlphaCaselessEqual(*character, 'e')) {
                ASSERT(character - tokenStart<SrcCharacterType>() > 0);
                ++character;
                if (*character == '-' || *character == '+' || isASCIIDigit(*character)) {
                    ++character;
                    while (isASCIIDigit(*character))
                        ++character;
                    // Use FLOATTOKEN if the string contains exponents.
                    dotSeen = true;
                    currentCharacter<SrcCharacterType>() = character;
                }
            }
            if (!parseSVGNumber(tokenStart<SrcCharacterType>(), character - tokenStart<SrcCharacterType>(), yylval->number))
                break;
        } else
            yylval->number = charactersToDouble(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
 
        // Type of the function.
        if (isIdentifierStart<SrcCharacterType>()) {
            SrcCharacterType* type = currentCharacter<SrcCharacterType>();
            result = currentCharacter<SrcCharacterType>();

            parseIdentifier(result, resultString, hasEscape);

            m_token = DIMEN;
            if (!hasEscape)
                detectNumberToken(type, currentCharacter<SrcCharacterType>() - type);

            if (m_token == DIMEN) {
                // The decoded number is overwritten, but this is intentional.
                yylval->string.init(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
            }
        } else if (*currentCharacter<SrcCharacterType>() == '%') {
            // Although the CSS grammar says {num}% we follow
            // webkit at the moment which uses {num}%+.
            do {
                ++currentCharacter<SrcCharacterType>();
            } while (*currentCharacter<SrcCharacterType>() == '%');
            m_token = PERCENTAGE;
        } else
            m_token = dotSeen ? FLOATTOKEN : INTEGER;
        break;
    }

    case CharacterDash:
        if (isIdentifierStartAfterDash(currentCharacter<SrcCharacterType>())) {
            --currentCharacter<SrcCharacterType>();
            parseIdentifier(result, resultString, hasEscape);
            m_token = IDENT;

            if (*currentCharacter<SrcCharacterType>() == '(') {
                m_token = FUNCTION;
                if (!hasEscape)
                    detectDashToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>());
                ++currentCharacter<SrcCharacterType>();
                ++result;
            } else if (UNLIKELY(m_parsingMode == NthChildMode) && !hasEscape && isASCIIAlphaCaselessEqual(tokenStart<SrcCharacterType>()[1], 'n')) {
                if (result - tokenStart<SrcCharacterType>() == 2) {
                    // String "-n" is IDENT but "-n+1" is NTH.
                    if (parseNthChildExtra<SrcCharacterType>()) {
                        m_token = NTH;
                        result = currentCharacter<SrcCharacterType>();
                    }
                } else if (result - tokenStart<SrcCharacterType>() >= 3 && tokenStart<SrcCharacterType>()[2] == '-') {
                    // String "-n-" is IDENT but "-n-1" is NTH.
                    // Set currentCharacter to second '-' of '-n-' to continue parsing.
                    SrcCharacterType* nextCharacter = result;
                    currentCharacter<SrcCharacterType>() = tokenStart<SrcCharacterType>() + 2;
                    if (parseNthChildExtra<SrcCharacterType>()) {
                        m_token = NTH;
                        result = currentCharacter<SrcCharacterType>();
                    } else {
                        // Revert the change to currentCharacter if unsuccessful.
                        currentCharacter<SrcCharacterType>() = nextCharacter;
                    }
                }
            }
            resultString.setLength(result - tokenStart<SrcCharacterType>());
            yylval->string = resultString;
        } else if (currentCharacter<SrcCharacterType>()[0] == '-' && currentCharacter<SrcCharacterType>()[1] == '>') {
            currentCharacter<SrcCharacterType>() += 2;
            m_token = SGML_CD;
        } else if (currentCharacter<SrcCharacterType>()[0] == '-') {
            --currentCharacter<SrcCharacterType>();
            parseIdentifier(result, resultString, hasEscape);
            m_token = CUSTOM_PROPERTY;
            yylval->string = resultString;
        } else if (UNLIKELY(m_parsingMode == NthChildMode)) {
            // "-[0-9]+n" is always an NthChild.
            if (parseNthChild<SrcCharacterType>()) {
                parseNthChildExtra<SrcCharacterType>();
                m_token = NTH;
                yylval->string.init(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
            }
        }
        break;

    case CharacterOther:
        // m_token is simply the current character.
        break;

    case CharacterNull:
        // Do not advance pointer at the end of input.
        --currentCharacter<SrcCharacterType>();
        break;

    case CharacterWhiteSpace:
        m_token = WHITESPACE;
        // Might start with a '\n'.
        --currentCharacter<SrcCharacterType>();
        do {
            if (*currentCharacter<SrcCharacterType>() == '\n') {
                ++m_lineNumber;
                m_columnOffsetForLine = currentCharacterOffset() + 1;
            }
            ++currentCharacter<SrcCharacterType>();
        } while (*currentCharacter<SrcCharacterType>() <= ' ' && (typesOfASCIICharacters[*currentCharacter<SrcCharacterType>()] == CharacterWhiteSpace));
        break;

    case CharacterEndConditionQuery: {
        bool isParsingCondition = m_parsingMode == MediaQueryMode || m_parsingMode == SupportsMode;
        if (isParsingCondition)
            m_parsingMode = NormalMode;
        break;
    }

    case CharacterEndNthChild:
        if (m_parsingMode == NthChildMode)
            m_parsingMode = NormalMode;
        break;

    case CharacterQuote:
        if (checkAndSkipString(currentCharacter<SrcCharacterType>(), m_token)) {
            ++result;
            parseString<SrcCharacterType>(result, yylval->string, m_token);
            m_token = STRING;
        }
        break;

    case CharacterExclamationMark: {
        SrcCharacterType* start = skipWhiteSpace(currentCharacter<SrcCharacterType>());
        if (isEqualToCSSIdentifier(start, "important")) {
            m_token = IMPORTANT_SYM;
            currentCharacter<SrcCharacterType>() = start + 9;
        }
        break;
    }

    case CharacterHashmark: {
        SrcCharacterType* start = currentCharacter<SrcCharacterType>();
        result = currentCharacter<SrcCharacterType>();

        if (isASCIIDigit(*currentCharacter<SrcCharacterType>())) {
            // This must be a valid hex number token.
            do {
                ++currentCharacter<SrcCharacterType>();
            } while (isASCIIHexDigit(*currentCharacter<SrcCharacterType>()));
            m_token = HEX;
            yylval->string.init(start, currentCharacter<SrcCharacterType>() - start);
        } else if (isIdentifierStart<SrcCharacterType>()) {
            m_token = IDSEL;
            parseIdentifier(result, yylval->string, hasEscape);
            if (!hasEscape) {
                // Check whether the identifier is also a valid hex number.
                SrcCharacterType* current = start;
                m_token = HEX;
                do {
                    if (!isASCIIHexDigit(*current)) {
                        m_token = IDSEL;
                        break;
                    }
                    ++current;
                } while (current < result);
            }
        }
        break;
    }

    case CharacterSlash:
        // Ignore comments. They are not even considered as white spaces.
        if (*currentCharacter<SrcCharacterType>() == '*') {
            ++currentCharacter<SrcCharacterType>();
            while (currentCharacter<SrcCharacterType>()[0] != '*' || currentCharacter<SrcCharacterType>()[1] != '/') {
                if (*currentCharacter<SrcCharacterType>() == '\n') {
                    ++m_lineNumber;
                    m_columnOffsetForLine = currentCharacterOffset() + 1;
                } else if (*currentCharacter<SrcCharacterType>() == '\0') {
                    // Unterminated comments are simply ignored.
                    currentCharacter<SrcCharacterType>() -= 2;
                    break;
                }
                ++currentCharacter<SrcCharacterType>();
            }
            currentCharacter<SrcCharacterType>() += 2;
            goto restartAfterComment;
        }
        break;

    case CharacterDollar:
        if (*currentCharacter<SrcCharacterType>() == '=') {
            ++currentCharacter<SrcCharacterType>();
            m_token = ENDSWITH;
        }
        break;

    case CharacterAsterisk:
        if (*currentCharacter<SrcCharacterType>() == '=') {
            ++currentCharacter<SrcCharacterType>();
            m_token = CONTAINS;
        }
        break;

    case CharacterPlus:
        if (UNLIKELY(m_parsingMode == NthChildMode)) {
            // Simplest case. "+[0-9]*n" is always NthChild.
            if (parseNthChild<SrcCharacterType>()) {
                parseNthChildExtra<SrcCharacterType>();
                m_token = NTH;
                yylval->string.init(tokenStart<SrcCharacterType>(), currentCharacter<SrcCharacterType>() - tokenStart<SrcCharacterType>());
            }
        }
        break;

    case CharacterLess:
        if (currentCharacter<SrcCharacterType>()[0] == '!' && currentCharacter<SrcCharacterType>()[1] == '-' && currentCharacter<SrcCharacterType>()[2] == '-') {
            currentCharacter<SrcCharacterType>() += 3;
            m_token = SGML_CD;
        }
        break;

    case CharacterAt:
        if (isIdentifierStart<SrcCharacterType>()) {
            m_token = ATKEYWORD;
            ++result;
            parseIdentifier(result, resultString, hasEscape);
            detectAtToken<SrcCharacterType>(result - tokenStart<SrcCharacterType>(), hasEscape);
        }
        break;

    case CharacterBackSlash:
        if (isCSSEscape(*currentCharacter<SrcCharacterType>())) {
            --currentCharacter<SrcCharacterType>();
            parseIdentifier(result, yylval->string, hasEscape);
            m_token = IDENT;
        }
        if (m_parsingMode == NthChildMode && m_token == IDENT && yylval->string.length() == 2 && equalLettersIgnoringASCIICase(yylval->string, "of")) {
            m_parsingMode = NormalMode;
            m_token = NTHCHILDSELECTORSEPARATOR;
        }
        break;

    case CharacterXor:
        if (*currentCharacter<SrcCharacterType>() == '=') {
            ++currentCharacter<SrcCharacterType>();
            m_token = BEGINSWITH;
        }
        break;

    case CharacterVerticalBar:
        if (*currentCharacter<SrcCharacterType>() == '=') {
            ++currentCharacter<SrcCharacterType>();
            m_token = DASHMATCH;
        }
        break;

    case CharacterTilde:
        if (*currentCharacter<SrcCharacterType>() == '=') {
            ++currentCharacter<SrcCharacterType>();
            m_token = INCLUDES;
        }
        break;

    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return token();
}

RefPtr<StyleRuleBase> CSSParser::createImportRule(const CSSParserString& url, PassRefPtr<MediaQuerySet> media)
{
    if (!media || !m_allowImportRules) {
        popRuleData();
        return nullptr;
    }
    RefPtr<StyleRuleImport> rule = StyleRuleImport::create(url, media);
    processAndAddNewRuleToSourceTreeIfNeeded();
    return rule;
}

RefPtr<StyleRuleBase> CSSParser::createMediaRule(PassRefPtr<MediaQuerySet> media, RuleList* rules)
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    RefPtr<StyleRuleMedia> rule;
    RuleList emptyRules;
    if (!media) {
        // To comply with w3c test suite expectation, create an empty media query
        // even when it is syntactically incorrect.
        rule = StyleRuleMedia::create(MediaQuerySet::create(), emptyRules);
    } else
        rule = StyleRuleMedia::create(media, rules ? *rules : emptyRules);
    processAndAddNewRuleToSourceTreeIfNeeded();
    return rule;
}

RefPtr<StyleRuleBase> CSSParser::createEmptyMediaRule(RuleList* rules)
{
    return createMediaRule(MediaQuerySet::create(), rules);
}

RefPtr<StyleRuleBase> CSSParser::createSupportsRule(bool conditionIsSupported, RuleList* rules)
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtr<CSSRuleSourceData> data = popSupportsRuleData();
    RefPtr<StyleRuleSupports> rule;
    String conditionText;
    unsigned conditionOffset = data->ruleHeaderRange.start + 9;
    unsigned conditionLength = data->ruleHeaderRange.length() - 9;

    if (is8BitSource())
        conditionText = String(m_dataStart8.get() + conditionOffset, conditionLength).stripWhiteSpace();
    else
        conditionText = String(m_dataStart16.get() + conditionOffset, conditionLength).stripWhiteSpace();

    if (rules)
        rule = StyleRuleSupports::create(conditionText, conditionIsSupported, *rules);
    else {
        RuleList emptyRules;
        rule = StyleRuleSupports::create(conditionText, conditionIsSupported, emptyRules);
    }

    processAndAddNewRuleToSourceTreeIfNeeded();

    return rule;
}

void CSSParser::markSupportsRuleHeaderStart()
{
    if (!m_supportsRuleDataStack)
        m_supportsRuleDataStack = std::make_unique<RuleSourceDataList>();

    RefPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(CSSRuleSourceData::SUPPORTS_RULE);
    data->ruleHeaderRange.start = tokenStartOffset();
    m_supportsRuleDataStack->append(data);
}

void CSSParser::markSupportsRuleHeaderEnd()
{
    ASSERT(m_supportsRuleDataStack && !m_supportsRuleDataStack->isEmpty());

    if (is8BitSource())
        m_supportsRuleDataStack->last()->ruleHeaderRange.end = tokenStart<LChar>() - m_dataStart8.get();
    else
        m_supportsRuleDataStack->last()->ruleHeaderRange.end = tokenStart<UChar>() - m_dataStart16.get();
}

RefPtr<CSSRuleSourceData> CSSParser::popSupportsRuleData()
{
    ASSERT(m_supportsRuleDataStack && !m_supportsRuleDataStack->isEmpty());
    RefPtr<CSSRuleSourceData> data = m_supportsRuleDataStack->last();
    m_supportsRuleDataStack->removeLast();
    return data;
}

void CSSParser::processAndAddNewRuleToSourceTreeIfNeeded()
{
    if (!isExtractingSourceData())
        return;
    markRuleBodyEnd();
    RefPtr<CSSRuleSourceData> rule = popRuleData();
    fixUnparsedPropertyRanges(rule.get());
    addNewRuleToSourceTree(rule.release());
}

void CSSParser::addNewRuleToSourceTree(PassRefPtr<CSSRuleSourceData> rule)
{
    // Precondition: (isExtractingSourceData()).
    if (!m_ruleSourceDataResult)
        return;
    if (m_currentRuleDataStack->isEmpty())
        m_ruleSourceDataResult->append(rule);
    else
        m_currentRuleDataStack->last()->childRules.append(rule);
}

PassRefPtr<CSSRuleSourceData> CSSParser::popRuleData()
{
    if (!m_ruleSourceDataResult)
        return nullptr;

    ASSERT(!m_currentRuleDataStack->isEmpty());
    m_currentRuleData = nullptr;
    RefPtr<CSSRuleSourceData> data = m_currentRuleDataStack->last();
    m_currentRuleDataStack->removeLast();
    return data.release();
}

void CSSParser::syntaxError(const Location& location, SyntaxErrorType error)
{
    if (!isLoggingErrors())
        return;

    StringBuilder builder;
    switch (error) {
    case PropertyDeclarationError:
        builder.appendLiteral("Invalid CSS property declaration at: ");
        break;
    default:
        builder.appendLiteral("Unexpected CSS token: ");
        break;
    }

    if (location.token.is8Bit())
        builder.append(location.token.characters8(), location.token.length());
    else
        builder.append(location.token.characters16(), location.token.length());

    logError(builder.toString(), location.lineNumber, location.columnNumber);

    m_ignoreErrorsInDeclaration = true;
}

bool CSSParser::isLoggingErrors()
{
    return m_logErrors && !m_ignoreErrorsInDeclaration;
}

void CSSParser::logError(const String& message, int lineNumber, int columnNumber)
{
    PageConsoleClient& console = m_styleSheet->singleOwnerDocument()->page()->console();
    console.addMessage(MessageSource::CSS, MessageLevel::Warning, message, m_styleSheet->baseURL().string(), lineNumber + 1, columnNumber + 1);
}

RefPtr<StyleRuleKeyframes> CSSParser::createKeyframesRule(const String& name, std::unique_ptr<Vector<RefPtr<StyleKeyframe>>> popKeyframes)
{
    std::unique_ptr<Vector<RefPtr<StyleKeyframe>>> keyframes = WTFMove(popKeyframes);
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    RefPtr<StyleRuleKeyframes> rule = StyleRuleKeyframes::create();
    for (size_t i = 0; i < keyframes->size(); ++i)
        rule->parserAppendKeyframe(keyframes->at(i));
    rule->setName(name);
    processAndAddNewRuleToSourceTreeIfNeeded();
    return rule;
}

RefPtr<StyleRuleBase> CSSParser::createStyleRule(Vector<std::unique_ptr<CSSParserSelector>>* selectors)
{
    RefPtr<StyleRule> rule;
    if (selectors) {
        m_allowImportRules = false;
        m_allowNamespaceDeclarations = false;
        rule = StyleRule::create(m_lastSelectorLineNumber, createStyleProperties());
        rule->parserAdoptSelectorVector(*selectors);
        processAndAddNewRuleToSourceTreeIfNeeded();
    } else
        popRuleData();
    clearProperties();
    return rule;
}

RefPtr<StyleRuleBase> CSSParser::createFontFaceRule()
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    for (unsigned i = 0; i < m_parsedProperties.size(); ++i) {
        CSSProperty& property = m_parsedProperties[i];
        if (property.id() == CSSPropertyFontFamily && (!is<CSSValueList>(*property.value()) || downcast<CSSValueList>(*property.value()).length() != 1)) {
            // Unlike font-family property, font-family descriptor in @font-face rule
            // has to be a value list with exactly one family name. It cannot have a
            // have 'initial' value and cannot 'inherit' from parent.
            // See http://dev.w3.org/csswg/css3-fonts/#font-family-desc
            clearProperties();
            popRuleData();
            return nullptr;
        }
    }
    RefPtr<StyleRuleFontFace> rule = StyleRuleFontFace::create(createStyleProperties());
    clearProperties();
    processAndAddNewRuleToSourceTreeIfNeeded();
    return rule;
}

void CSSParser::addNamespace(const AtomicString& prefix, const AtomicString& uri)
{
    if (!m_styleSheet || !m_allowNamespaceDeclarations)
        return;
    m_allowImportRules = false;
    m_styleSheet->parserAddNamespace(prefix, uri);
    if (prefix.isEmpty() && !uri.isNull())
        m_defaultNamespace = uri;
}

QualifiedName CSSParser::determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName)
{
    if (!m_styleSheet)
        return QualifiedName(prefix, localName, m_defaultNamespace);
    return QualifiedName(prefix, localName, m_styleSheet->determineNamespace(prefix));
}

void CSSParser::rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector& specifiers)
{
    if (m_defaultNamespace != starAtom || specifiers.isCustomPseudoElement())
        rewriteSpecifiersWithElementName(nullAtom, starAtom, specifiers, /*tagIsForNamespaceRule*/true);
}

void CSSParser::rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector& specifiers, bool tagIsForNamespaceRule)
{
    AtomicString determinedNamespace = namespacePrefix != nullAtom && m_styleSheet ? m_styleSheet->determineNamespace(namespacePrefix) : m_defaultNamespace;
    QualifiedName tag(namespacePrefix, elementName, determinedNamespace);

    if (!specifiers.isCustomPseudoElement()) {
        if (tag == anyQName())
            return;
        if (!specifiers.isPseudoElementCueFunction())
            specifiers.prependTagSelector(tag, tagIsForNamespaceRule);
        return;
    }

    CSSParserSelector* lastShadowDescendant = &specifiers;
    CSSParserSelector* history = &specifiers;
    while (history->tagHistory()) {
        history = history->tagHistory();
        if (history->isCustomPseudoElement() || history->hasShadowDescendant())
            lastShadowDescendant = history;
    }

    if (lastShadowDescendant->tagHistory()) {
        if (tag != anyQName())
            lastShadowDescendant->tagHistory()->prependTagSelector(tag, tagIsForNamespaceRule);
        return;
    }

    // For shadow-ID pseudo-elements to be correctly matched, the ShadowDescendant combinator has to be used.
    // We therefore create a new Selector with that combinator here in any case, even if matching any (host) element in any namespace (i.e. '*').
    lastShadowDescendant->setTagHistory(std::make_unique<CSSParserSelector>(tag));
    lastShadowDescendant->setRelation(CSSSelector::ShadowDescendant);
}

std::unique_ptr<CSSParserSelector> CSSParser::rewriteSpecifiers(std::unique_ptr<CSSParserSelector> specifiers, std::unique_ptr<CSSParserSelector> newSpecifier)
{
    if (newSpecifier->isCustomPseudoElement() || newSpecifier->isPseudoElementCueFunction()) {
        // Unknown pseudo element always goes at the top of selector chain.
        newSpecifier->appendTagHistory(CSSSelector::ShadowDescendant, WTFMove(specifiers));
        return newSpecifier;
    }
    if (specifiers->isCustomPseudoElement()) {
        // Specifiers for unknown pseudo element go right behind it in the chain.
        specifiers->insertTagHistory(CSSSelector::SubSelector, WTFMove(newSpecifier), CSSSelector::ShadowDescendant);
        return specifiers;
    }
    specifiers->appendTagHistory(CSSSelector::SubSelector, WTFMove(newSpecifier));
    return specifiers;
}

RefPtr<StyleRuleBase> CSSParser::createPageRule(std::unique_ptr<CSSParserSelector> pageSelector)
{
    // FIXME: Margin at-rules are ignored.
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    RefPtr<StyleRulePage> rule;
    if (pageSelector) {
        rule = StyleRulePage::create(createStyleProperties());
        Vector<std::unique_ptr<CSSParserSelector>> selectorVector;
        selectorVector.append(WTFMove(pageSelector));
        rule->parserAdoptSelectorVector(selectorVector);
        processAndAddNewRuleToSourceTreeIfNeeded();
    } else
        popRuleData();
    clearProperties();
    return rule;
}

std::unique_ptr<Vector<std::unique_ptr<CSSParserSelector>>> CSSParser::createSelectorVector()
{
    if (m_recycledSelectorVector) {
        m_recycledSelectorVector->shrink(0);
        return WTFMove(m_recycledSelectorVector);
    }
    return std::make_unique<Vector<std::unique_ptr<CSSParserSelector>>>();
}

void CSSParser::recycleSelectorVector(std::unique_ptr<Vector<std::unique_ptr<CSSParserSelector>>> vector)
{
    if (vector && !m_recycledSelectorVector)
        m_recycledSelectorVector = WTFMove(vector);
}

RefPtr<StyleRuleBase> CSSParser::createRegionRule(Vector<std::unique_ptr<CSSParserSelector>>* regionSelector, RuleList* rules)
{
    if (!cssRegionsEnabled() || !regionSelector || !rules) {
        popRuleData();
        return nullptr;
    }

    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtr<StyleRuleRegion> regionRule = StyleRuleRegion::create(regionSelector, *rules);

    if (isExtractingSourceData())
        addNewRuleToSourceTree(CSSRuleSourceData::createUnknown());

    return regionRule;
}

void CSSParser::createMarginAtRule(CSSSelector::MarginBoxType /* marginBox */)
{
    // FIXME: Implement margin at-rule here, using:
    //        - marginBox: margin box
    //        - m_parsedProperties: properties at [m_numParsedPropertiesBeforeMarginBox, m_parsedProperties.size()] are for this at-rule.
    // Don't forget to also update the action for page symbol in CSSGrammar.y such that margin at-rule data is cleared if page_selector is invalid.

    endDeclarationsForMarginBox();
}

void CSSParser::startDeclarationsForMarginBox()
{
    m_numParsedPropertiesBeforeMarginBox = m_parsedProperties.size();
}

void CSSParser::endDeclarationsForMarginBox()
{
    rollbackLastProperties(m_parsedProperties.size() - m_numParsedPropertiesBeforeMarginBox);
    m_numParsedPropertiesBeforeMarginBox = INVALID_NUM_PARSED_PROPERTIES;
}

RefPtr<StyleKeyframe> CSSParser::createKeyframe(CSSParserValueList& keys)
{
    // Create a key string from the passed keys
    StringBuilder keyString;
    for (unsigned i = 0; i < keys.size(); ++i) {
        // Just as per the comment below, we ignore keyframes with
        // invalid key values (plain numbers or unknown identifiers)
        // marked as CSSPrimitiveValue::CSS_UNKNOWN during parsing.
        if (keys.valueAt(i)->unit == CSSPrimitiveValue::CSS_UNKNOWN) {
            clearProperties();
            return nullptr;
        }

        ASSERT(keys.valueAt(i)->unit == CSSPrimitiveValue::CSS_NUMBER);
        float key = static_cast<float>(keys.valueAt(i)->fValue);
        if (key < 0 || key > 100) {
            // As per http://www.w3.org/TR/css3-animations/#keyframes,
            // "If a keyframe selector specifies negative percentage values
            // or values higher than 100%, then the keyframe will be ignored."
            clearProperties();
            return nullptr;
        }
        if (i != 0)
            keyString.append(',');
        keyString.appendNumber(key);
        keyString.append('%');
    }

    RefPtr<StyleKeyframe> keyframe = StyleKeyframe::create(createStyleProperties());
    keyframe->setKeyText(keyString.toString());

    clearProperties();

    return keyframe;
}

void CSSParser::invalidBlockHit()
{
    if (m_styleSheet && !m_hadSyntacticallyValidCSSRule)
        m_styleSheet->setHasSyntacticallyValidCSSHeader(false);
}

void CSSParser::updateLastSelectorLineAndPosition()
{
    m_lastSelectorLineNumber = m_lineNumber;
}

void CSSParser::updateLastMediaLine(MediaQuerySet* media)
{
    media->setLastLine(m_lineNumber);
}

template <typename CharacterType>
static inline void fixUnparsedProperties(const CharacterType* characters, CSSRuleSourceData* ruleData)
{
    Vector<CSSPropertySourceData>& propertyData = ruleData->styleSourceData->propertyData;
    unsigned size = propertyData.size();
    if (!size)
        return;

    unsigned styleStart = ruleData->ruleBodyRange.start;
    CSSPropertySourceData* nextData = &(propertyData.at(0));
    for (unsigned i = 0; i < size; ++i) {
        CSSPropertySourceData* currentData = nextData;
        nextData = i < size - 1 ? &(propertyData.at(i + 1)) : 0;

        if (currentData->parsedOk)
            continue;
        if (currentData->range.end > 0 && characters[styleStart + currentData->range.end - 1] == ';')
            continue;

        unsigned propertyEndInStyleSheet;
        if (!nextData)
            propertyEndInStyleSheet = ruleData->ruleBodyRange.end - 1;
        else
            propertyEndInStyleSheet = styleStart + nextData->range.start - 1;

        while (isHTMLSpace(characters[propertyEndInStyleSheet]))
            --propertyEndInStyleSheet;

        // propertyEndInStyleSheet points at the last property text character.
        unsigned newPropertyEnd = propertyEndInStyleSheet - styleStart + 1; // Exclusive of the last property text character.
        if (currentData->range.end != newPropertyEnd) {
            currentData->range.end = newPropertyEnd;
            unsigned valueStartInStyleSheet = styleStart + currentData->range.start + currentData->name.length();
            while (valueStartInStyleSheet < propertyEndInStyleSheet && characters[valueStartInStyleSheet] != ':')
                ++valueStartInStyleSheet;
            if (valueStartInStyleSheet < propertyEndInStyleSheet)
                ++valueStartInStyleSheet; // Shift past the ':'.
            while (valueStartInStyleSheet < propertyEndInStyleSheet && isHTMLSpace(characters[valueStartInStyleSheet]))
                ++valueStartInStyleSheet;
            // Need to exclude the trailing ';' from the property value.
            currentData->value = String(characters + valueStartInStyleSheet, propertyEndInStyleSheet - valueStartInStyleSheet + (characters[propertyEndInStyleSheet] == ';' ? 0 : 1));
        }
    }
}

void CSSParser::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData)
{
    if (!ruleData->styleSourceData)
        return;

    if (is8BitSource()) {
        fixUnparsedProperties<LChar>(m_dataStart8.get() + m_parsedTextPrefixLength, ruleData);
        return;
    }

    fixUnparsedProperties<UChar>(m_dataStart16.get() + m_parsedTextPrefixLength, ruleData);
}

void CSSParser::markRuleHeaderStart(CSSRuleSourceData::Type ruleType)
{
    if (!isExtractingSourceData())
        return;

    // Pop off data for a previous invalid rule.
    if (m_currentRuleData)
        m_currentRuleDataStack->removeLast();

    RefPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(ruleType);
    data->ruleHeaderRange.start = tokenStartOffset();
    m_currentRuleData = data;
    m_currentRuleDataStack->append(data.release());
}

template <typename CharacterType>
inline void CSSParser::setRuleHeaderEnd(const CharacterType* dataStart)
{
    CharacterType* listEnd = tokenStart<CharacterType>();
    while (listEnd > dataStart + 1) {
        if (isHTMLSpace(*(listEnd - 1)))
            --listEnd;
        else
            break;
    }

    m_currentRuleDataStack->last()->ruleHeaderRange.end = listEnd - dataStart;
}

void CSSParser::markRuleHeaderEnd()
{
    if (!isExtractingSourceData())
        return;
    ASSERT(!m_currentRuleDataStack->isEmpty());

    if (is8BitSource())
        setRuleHeaderEnd<LChar>(m_dataStart8.get());
    else
        setRuleHeaderEnd<UChar>(m_dataStart16.get());
}

void CSSParser::markSelectorStart()
{
    if (!isExtractingSourceData() || m_nestedSelectorLevel)
        return;
    ASSERT(!m_selectorRange.end);

    m_selectorRange.start = tokenStartOffset();
}

void CSSParser::markSelectorEnd()
{
    if (!isExtractingSourceData() || m_nestedSelectorLevel)
        return;
    ASSERT(!m_selectorRange.end);
    ASSERT(m_currentRuleDataStack->size());

    m_selectorRange.end = tokenStartOffset();
    m_currentRuleDataStack->last()->selectorRanges.append(m_selectorRange);
    m_selectorRange.start = 0;
    m_selectorRange.end = 0;
}

void CSSParser::markRuleBodyStart()
{
    if (!isExtractingSourceData())
        return;
    m_currentRuleData = nullptr;
    unsigned offset = tokenStartOffset();
    if (tokenStartChar() == '{')
        ++offset; // Skip the rule body opening brace.
    ASSERT(!m_currentRuleDataStack->isEmpty());
    m_currentRuleDataStack->last()->ruleBodyRange.start = offset;
}

void CSSParser::markRuleBodyEnd()
{
    // Precondition: (!isExtractingSourceData())
    unsigned offset = tokenStartOffset();
    ASSERT(!m_currentRuleDataStack->isEmpty());
    m_currentRuleDataStack->last()->ruleBodyRange.end = offset;
}

void CSSParser::markPropertyStart()
{
    m_ignoreErrorsInDeclaration = false;
    if (!isExtractingSourceData())
        return;
    if (m_currentRuleDataStack->isEmpty() || !m_currentRuleDataStack->last()->styleSourceData)
        return;

    m_propertyRange.start = tokenStartOffset();
}

void CSSParser::markPropertyEnd(bool isImportantFound, bool isPropertyParsed)
{
    if (!isExtractingSourceData())
        return;
    if (m_currentRuleDataStack->isEmpty() || !m_currentRuleDataStack->last()->styleSourceData)
        return;

    unsigned offset = tokenStartOffset();
    if (tokenStartChar() == ';') // Include semicolon into the property text.
        ++offset;
    m_propertyRange.end = offset;
    if (m_propertyRange.start != UINT_MAX && !m_currentRuleDataStack->isEmpty()) {
        // This stuff is only executed when the style data retrieval is requested by client.
        const unsigned start = m_propertyRange.start;
        const unsigned end = m_propertyRange.end;
        ASSERT_WITH_SECURITY_IMPLICATION(start < end);
        String propertyString;
        if (is8BitSource())
            propertyString = String(m_dataStart8.get() + start, end - start).stripWhiteSpace();
        else
            propertyString = String(m_dataStart16.get() + start, end - start).stripWhiteSpace();
        if (propertyString.endsWith(';'))
            propertyString = propertyString.left(propertyString.length() - 1);
        size_t colonIndex = propertyString.find(':');
        ASSERT(colonIndex != notFound);

        String name = propertyString.left(colonIndex).stripWhiteSpace();
        String value = propertyString.substring(colonIndex + 1, propertyString.length()).stripWhiteSpace();
        // The property range is relative to the declaration start offset.
        SourceRange& topRuleBodyRange = m_currentRuleDataStack->last()->ruleBodyRange;
        m_currentRuleDataStack->last()->styleSourceData->propertyData.append(
            CSSPropertySourceData(name, value, isImportantFound, isPropertyParsed, SourceRange(start - topRuleBodyRange.start, end - topRuleBodyRange.start)));
    }
    resetPropertyRange();
}

#if ENABLE(CSS_DEVICE_ADAPTATION)
PassRefPtr<StyleRuleBase> CSSParser::createViewportRule()
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtr<StyleRuleViewport> rule = StyleRuleViewport::create(createStyleProperties());
    clearProperties();

    processAndAddNewRuleToSourceTreeIfNeeded();

    return rule.release();
}

bool CSSParser::parseViewportProperty(CSSPropertyID propId, bool important)
{
    if (!m_valueList->current())
        return false;

    ValueWithCalculation valueWithCalculation(*m_valueList->current());

    CSSValueID id = valueWithCalculation.value().id;
    bool validPrimitive = false;

    switch (propId) {
    case CSSPropertyMinWidth: // auto | device-width | device-height | <length> | <percentage>
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
        if (id == CSSValueAuto || id == CSSValueDeviceWidth || id == CSSValueDeviceHeight)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FLength | FPercent | FNonNeg));
        break;
    case CSSPropertyWidth: // shorthand
        return parseViewportShorthand(propId, CSSPropertyMinWidth, CSSPropertyMaxWidth, important);
    case CSSPropertyHeight:
        return parseViewportShorthand(propId, CSSPropertyMinHeight, CSSPropertyMaxHeight, important);
    case CSSPropertyMinZoom: // auto | <number> | <percentage>
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validateUnit(valueWithCalculation, FNumber | FPercent | FNonNeg));
        break;
    case CSSPropertyUserZoom: // zoom | fixed
        if (id == CSSValueZoom || id == CSSValueFixed)
            validPrimitive = true;
        break;
    case CSSPropertyOrientation: // auto | portrait | landscape
        if (id == CSSValueAuto || id == CSSValuePortrait || id == CSSValueLandscape)
            validPrimitive = true;
    default:
        break;
    }

    RefPtr<CSSValue> parsedValue;
    if (validPrimitive) {
        parsedValue = parseValidPrimitive(id, valueWithCalculation);
        m_valueList->next();
    }

    if (parsedValue) {
        if (!m_valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue.release(), important);
            return true;
        }
    }

    return false;
}

bool CSSParser::parseViewportShorthand(CSSPropertyID propId, CSSPropertyID first, CSSPropertyID second, bool important)
{
    unsigned numValues = m_valueList->size();

    if (numValues > 2)
        return false;

    ShorthandScope scope(this, propId);

    if (!parseViewportProperty(first, important))
        return false;

    // If just one value is supplied, the second value
    // is implicitly initialized with the first value.
    if (numValues == 1)
        m_valueList->previous();

    return parseViewportProperty(second, important);
}
#endif

template <typename CharacterType>
static CSSPropertyID cssPropertyID(const CharacterType* propertyName, unsigned length)
{
    char buffer[maxCSSPropertyNameLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = propertyName[i];
        if (c == 0 || c >= 0x7F)
            return CSSPropertyInvalid; // illegal character
        buffer[i] = toASCIILower(c);
    }
    buffer[length] = '\0';

    const char* name = buffer;
    if (buffer[0] == '-') {
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled()
            && (hasPrefix(buffer, length, "-apple-") || hasPrefix(buffer, length, "-khtml-"))) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }
#endif
#if PLATFORM(IOS)
        cssPropertyNameIOSAliasing(buffer, name, length);
#endif
    }

    const Property* hashTableEntry = findProperty(name, length);
    return hashTableEntry ? static_cast<CSSPropertyID>(hashTableEntry->id) : CSSPropertyInvalid;
}

CSSPropertyID cssPropertyID(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}

CSSPropertyID cssPropertyID(const CSSParserString& string)
{
    unsigned length = string.length();

    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;
    
    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}

#if PLATFORM(IOS)
void cssPropertyNameIOSAliasing(const char* propertyName, const char*& propertyNameAlias, unsigned& newLength)
{
    if (!strcmp(propertyName, "-webkit-hyphenate-locale")) {
        // Worked in iOS 4.2.
        static const char* const webkitLocale = "-webkit-locale";
        propertyNameAlias = webkitLocale;
        newLength = strlen(webkitLocale);
    }
}
#endif

static bool isAppleLegacyCssValueKeyword(const char* valueKeyword, unsigned length)
{
    static const char* applePrefix = "-apple-";
    static const char* appleSystemPrefix = "-apple-system";
    static const char* appleWirelessPlaybackTargetActive = getValueName(CSSValueAppleWirelessPlaybackTargetActive);

    return hasPrefix(valueKeyword, length, applePrefix)
        && !hasPrefix(valueKeyword, length, appleSystemPrefix)
        && !WTF::equal(reinterpret_cast<const LChar*>(valueKeyword), reinterpret_cast<const LChar*>(appleWirelessPlaybackTargetActive), length);
}

template <typename CharacterType>
static CSSValueID cssValueKeywordID(const CharacterType* valueKeyword, unsigned length)
{
    char buffer[maxCSSValueKeywordLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = valueKeyword[i];
        if (c == 0 || c >= 0x7F)
            return CSSValueInvalid; // illegal keyword.
        buffer[i] = WTF::toASCIILower(c);
    }
    buffer[length] = '\0';

    if (buffer[0] == '-') {
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        // On iOS we don't want to change values starting with -apple-system to -webkit-system.
        // FIXME: Remove this mangling without breaking the web.
        if (isAppleLegacyCssValueKeyword(buffer, length) || hasPrefix(buffer, length, "-khtml-")) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }
    }

    const Value* hashTableEntry = findValue(buffer, length);
    return hashTableEntry ? static_cast<CSSValueID>(hashTableEntry->id) : CSSValueInvalid;
}

CSSValueID cssValueKeywordID(const CSSParserString& string)
{
    unsigned length = string.length();
    if (!length)
        return CSSValueInvalid;
    if (length > maxCSSValueKeywordLength)
        return CSSValueInvalid;

    return string.is8Bit() ? cssValueKeywordID(string.characters8(), length) : cssValueKeywordID(string.characters16(), length);
}

template <typename CharacterType>
static inline bool isCSSTokenizerIdentifier(const CharacterType* characters, unsigned length)
{
    const CharacterType* end = characters + length;

    // -?
    if (characters != end && characters[0] == '-')
        ++characters;

    // {nmstart}
    if (characters == end || !(characters[0] == '_' || characters[0] >= 128 || isASCIIAlpha(characters[0])))
        return false;
    ++characters;

    // {nmchar}*
    for (; characters != end; ++characters) {
        if (!(characters[0] == '_' || characters[0] == '-' || characters[0] >= 128 || isASCIIAlphanumeric(characters[0])))
            return false;
    }

    return true;
}

// "ident" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerIdentifier(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return false;

    if (string.is8Bit())
        return isCSSTokenizerIdentifier(string.characters8(), length);
    return isCSSTokenizerIdentifier(string.characters16(), length);
}

template <typename CharacterType>
static inline bool isCSSTokenizerURL(const CharacterType* characters, unsigned length)
{
    const CharacterType* end = characters + length;
    
    for (; characters != end; ++characters) {
        CharacterType c = characters[0];
        switch (c) {
            case '!':
            case '#':
            case '$':
            case '%':
            case '&':
                break;
            default:
                if (c < '*')
                    return false;
                if (c <= '~')
                    break;
                if (c < 128)
                    return false;
        }
    }
    
    return true;
}

// "url" from the CSS tokenizer, minus backslash-escape sequences
static bool isCSSTokenizerURL(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return true;

    if (string.is8Bit())
        return isCSSTokenizerURL(string.characters8(), length);
    return isCSSTokenizerURL(string.characters16(), length);
}


template <typename CharacterType>
static inline String quoteCSSStringInternal(const CharacterType* characters, unsigned length)
{
    // For efficiency, we first pre-calculate the length of the quoted string, then we build the actual one.
    // Please see below for the actual logic.
    unsigned quotedStringSize = 2; // Two quotes surrounding the entire string.
    bool afterEscape = false;
    for (unsigned i = 0; i < length; ++i) {
        CharacterType ch = characters[i];
        if (ch == '\\' || ch == '\'') {
            quotedStringSize += 2;
            afterEscape = false;
        } else if (ch < 0x20 || ch == 0x7F) {
            quotedStringSize += 2 + (ch >= 0x10);
            afterEscape = true;
        } else {
            quotedStringSize += 1 + (afterEscape && (isASCIIHexDigit(ch) || ch == ' '));
            afterEscape = false;
        }
    }

    StringBuffer<CharacterType> buffer(quotedStringSize);
    unsigned index = 0;
    buffer[index++] = '\'';
    afterEscape = false;
    for (unsigned i = 0; i < length; ++i) {
        CharacterType ch = characters[i];
        if (ch == '\\' || ch == '\'') {
            buffer[index++] = '\\';
            buffer[index++] = ch;
            afterEscape = false;
        } else if (ch < 0x20 || ch == 0x7F) { // Control characters.
            buffer[index++] = '\\';
            placeByteAsHexCompressIfPossible(ch, buffer, index, Lowercase);
            afterEscape = true;
        } else {
            // Space character may be required to separate backslash-escape sequence and normal characters.
            if (afterEscape && (isASCIIHexDigit(ch) || ch == ' '))
                buffer[index++] = ' ';
            buffer[index++] = ch;
            afterEscape = false;
        }
    }
    buffer[index++] = '\'';

    ASSERT(quotedStringSize == index);
    return String::adopt(buffer);
}

// We use single quotes for now because markup.cpp uses double quotes.
String quoteCSSString(const String& string)
{
    // This function expands each character to at most 3 characters ('\u0010' -> '\' '1' '0') as well as adds
    // 2 quote characters (before and after). Make sure the resulting size (3 * length + 2) will not overflow unsigned.

    unsigned length = string.length();

    if (!length)
        return String("\'\'");

    if (length > std::numeric_limits<unsigned>::max() / 3 - 2)
        return emptyString();

    if (string.is8Bit())
        return quoteCSSStringInternal(string.characters8(), length);
    return quoteCSSStringInternal(string.characters16(), length);
}

String quoteCSSStringIfNeeded(const String& string)
{
    return isCSSTokenizerIdentifier(string) ? string : quoteCSSString(string);
}

String quoteCSSURLIfNeeded(const String& string)
{
    return isCSSTokenizerURL(string) ? string : quoteCSSString(string);
}

bool isValidNthToken(const CSSParserString& token)
{
    // The tokenizer checks for the construct of an+b.
    // However, since the {ident} rule precedes the {nth} rule, some of those
    // tokens are identified as string literal. Furthermore we need to accept
    // "odd" and "even" which does not match to an+b.
    return equalLettersIgnoringASCIICase(token, "odd")
        || equalLettersIgnoringASCIICase(token, "even")
        || equalLettersIgnoringASCIICase(token, "n")
        || equalLettersIgnoringASCIICase(token, "-n");
}

}
