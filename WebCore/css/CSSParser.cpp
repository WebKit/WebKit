/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#include "CString.h"
#include "CSSTimingFunctionValue.h"
#include "CSSBorderImageValue.h"
#include "CSSCanvasValue.h"
#include "CSSCharsetRule.h"
#include "CSSCursorImageValue.h"
#include "CSSHelper.h"
#include "CSSImageValue.h"
#include "CSSFontFaceRule.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSGradientValue.h"
#include "CSSImportRule.h"
#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSMediaRule.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSQuirkPrimitiveValue.h"
#include "CSSReflectValue.h"
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "CSSVariableDependentValue.h"
#include "CSSVariablesDeclaration.h"
#include "CSSVariablesRule.h"
#include "Counter.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "MediaList.h"
#include "MediaQueryExp.h"
#include "Pair.h"
#include "Rect.h"
#include "ShadowValue.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSTransformValue.h"
#include <wtf/dtoa.h>

#if ENABLE(DASHBOARD_SUPPORT)
#include "DashboardRegion.h"
#endif

#define YYDEBUG 0

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse(void* parser);

using namespace std;
using namespace WTF;

#include "CSSPropertyNames.cpp"
#include "CSSValueKeywords.c"

namespace WebCore {

static bool equal(const CSSParserString& a, const char* b)
{
    for (int i = 0; i < a.length; ++i) {
        if (!b[i])
            return false;
        if (a.characters[i] != b[i])
            return false;
    }
    return !b[a.length];
}

static bool equalIgnoringCase(const CSSParserString& a, const char* b)
{
    for (int i = 0; i < a.length; ++i) {
        if (!b[i])
            return false;
        ASSERT(!isASCIIUpper(b[i]));
        if (toASCIILower(a.characters[i]) != b[i])
            return false;
    }
    return !b[a.length];
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

CSSParser::CSSParser(bool strictParsing)
    : m_strict(strictParsing)
    , m_important(false)
    , m_id(0)
    , m_styleSheet(0)
    , m_mediaQuery(0)
    , m_valueList(0)
    , m_parsedProperties(static_cast<CSSProperty**>(fastMalloc(32 * sizeof(CSSProperty*))))
    , m_numParsedProperties(0)
    , m_maxParsedProperties(32)
    , m_inParseShorthand(0)
    , m_currentShorthand(0)
    , m_implicitShorthand(false)
    , m_hasFontFaceOnlyValues(false)
    , m_defaultNamespace(starAtom)
    , m_data(0)
    , yy_start(1)
    , m_floatingMediaQuery(0)
    , m_floatingMediaQueryExp(0)
    , m_floatingMediaQueryExpList(0)
{
#if YYDEBUG > 0
    cssyydebug = 1;
#endif
}

CSSParser::~CSSParser()
{
    clearProperties();
    fastFree(m_parsedProperties);

    clearVariables();
    
    delete m_valueList;

    fastFree(m_data);

    if (m_floatingMediaQueryExpList) {
        deleteAllValues(*m_floatingMediaQueryExpList);
        delete m_floatingMediaQueryExpList;
    }
    delete m_floatingMediaQueryExp;
    delete m_floatingMediaQuery;
    deleteAllValues(m_floatingSelectors);
    deleteAllValues(m_floatingValueLists);
    deleteAllValues(m_floatingFunctions);
    deleteAllValues(m_reusableSelectorVector);
}

void CSSParserString::lower()
{
    // FIXME: If we need Unicode lowercasing here, then we probably want the real kind
    // that can potentially change the length of the string rather than the character
    // by character kind. If we don't need Unicode lowercasing, it would be good to
    // simplify this function.

    if (charactersAreAllASCII(characters, length)) {
        // Fast case for all-ASCII.
        for (int i = 0; i < length; i++)
            characters[i] = toASCIILower(characters[i]);
    } else {
        for (int i = 0; i < length; i++)
            characters[i] = Unicode::toLower(characters[i]);
    }
}

void CSSParser::setupParser(const char* prefix, const String& string, const char* suffix)
{
    int length = string.length() + strlen(prefix) + strlen(suffix) + 2;

    fastFree(m_data);
    m_data = static_cast<UChar*>(fastMalloc(length * sizeof(UChar)));
    for (unsigned i = 0; i < strlen(prefix); i++)
        m_data[i] = prefix[i];
    
    memcpy(m_data + strlen(prefix), string.characters(), string.length() * sizeof(UChar));

    unsigned start = strlen(prefix) + string.length();
    unsigned end = start + strlen(suffix);
    for (unsigned i = start; i < end; i++)
        m_data[i] = suffix[i - start];

    m_data[length - 1] = 0;
    m_data[length - 2] = 0;

    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = m_data;
    yy_hold_char = *yy_c_buf_p;
}

void CSSParser::parseSheet(CSSStyleSheet* sheet, const String& string)
{
    m_styleSheet = sheet;
    m_defaultNamespace = starAtom; // Reset the default namespace.
    
    setupParser("", string, "");
    cssyyparse(this);
    m_rule = 0;
}

PassRefPtr<CSSRule> CSSParser::parseRule(CSSStyleSheet* sheet, const String& string)
{
    m_styleSheet = sheet;
    setupParser("@-webkit-rule{", string, "} ");
    cssyyparse(this);
    return m_rule.release();
}

PassRefPtr<CSSRule> CSSParser::parseKeyframeRule(CSSStyleSheet *sheet, const String &string)
{
    m_styleSheet = sheet;
    setupParser("@-webkit-keyframe-rule{ ", string, "} ");
    cssyyparse(this);
    return m_keyframe.release();
}

bool CSSParser::parseValue(CSSMutableStyleDeclaration* declaration, int id, const String& string, bool important)
{
    ASSERT(!declaration->stylesheet() || declaration->stylesheet()->isCSSStyleSheet());
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    setupParser("@-webkit-value{", string, "} ");

    m_id = id;
    m_important = important;
    
    cssyyparse(this);
    
    m_rule = 0;

    bool ok = false;
    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();
    if (m_numParsedProperties) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties, m_numParsedProperties);
        clearProperties();
    }

    return ok;
}

// color will only be changed when string contains a valid css color, making it
// possible to set up a default color.
bool CSSParser::parseColor(RGBA32& color, const String& string, bool strict)
{
    color = 0;
    CSSParser parser(true);

    // First try creating a color specified by name or the "#" syntax.
    if (!parser.parseColor(string, color, strict)) {
        RefPtr<CSSMutableStyleDeclaration> dummyStyleDeclaration = CSSMutableStyleDeclaration::create();

        // Now try to create a color from the rgb() or rgba() syntax.
        if (parser.parseColor(dummyStyleDeclaration.get(), string)) {
            CSSValue* value = parser.m_parsedProperties[0]->value();
            if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
                CSSPrimitiveValue* primitiveValue = static_cast<CSSPrimitiveValue*>(value);
                color = primitiveValue->getRGBColorValue();
            }
        } else
            return false;
    }

    return true;
}

bool CSSParser::parseColor(CSSMutableStyleDeclaration* declaration, const String& string)
{
    ASSERT(!declaration->stylesheet() || declaration->stylesheet()->isCSSStyleSheet());
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    setupParser("@-webkit-decls{color:", string, "} ");
    cssyyparse(this);
    m_rule = 0;

    return (m_numParsedProperties && m_parsedProperties[0]->m_id == CSSPropertyColor);
}

void CSSParser::parseSelector(const String& string, Document* doc, CSSSelectorList& selectorList)
{    
    RefPtr<CSSStyleSheet> dummyStyleSheet = CSSStyleSheet::create(doc);

    m_styleSheet = dummyStyleSheet.get();
    m_selectorListForParseSelector = &selectorList;

    setupParser("@-webkit-selector{", string, "}");

    cssyyparse(this);

    m_selectorListForParseSelector = 0;
}

bool CSSParser::parseDeclaration(CSSMutableStyleDeclaration* declaration, const String& string)
{
    ASSERT(!declaration->stylesheet() || declaration->stylesheet()->isCSSStyleSheet());
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    setupParser("@-webkit-decls{", string, "} ");
    cssyyparse(this);
    m_rule = 0;

    bool ok = false;
    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();
    if (m_numParsedProperties) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties, m_numParsedProperties);
        clearProperties();
    }

    return ok;
}

bool CSSParser::parseMediaQuery(MediaList* queries, const String& string)
{
    if (string.isEmpty())
        return true;

    m_mediaQuery = 0;
    // can't use { because tokenizer state switches from mediaquery to initial state when it sees { token.
    // instead insert one " " (which is WHITESPACE in CSSGrammar.y)
    setupParser ("@-webkit-mediaquery ", string, "} ");
    cssyyparse(this);

    bool ok = false;
    if (m_mediaQuery) {
        ok = true;
        queries->appendMediaQuery(m_mediaQuery);
        m_mediaQuery = 0;
    }

    return ok;
}


void CSSParser::addProperty(int propId, PassRefPtr<CSSValue> value, bool important)
{
    auto_ptr<CSSProperty> prop(new CSSProperty(propId, value, important, m_currentShorthand, m_implicitShorthand));
    if (m_numParsedProperties >= m_maxParsedProperties) {
        m_maxParsedProperties += 32;
        if (m_maxParsedProperties > UINT_MAX / sizeof(CSSProperty*))
            return;
        m_parsedProperties = static_cast<CSSProperty**>(fastRealloc(m_parsedProperties,
                                                       m_maxParsedProperties * sizeof(CSSProperty*)));
    }
    m_parsedProperties[m_numParsedProperties++] = prop.release();
}

void CSSParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(m_numParsedProperties >= static_cast<unsigned>(num));

    for (int i = 0; i < num; ++i)
        delete m_parsedProperties[--m_numParsedProperties];
}

void CSSParser::clearProperties()
{
    for (unsigned i = 0; i < m_numParsedProperties; i++)
        delete m_parsedProperties[i];
    m_numParsedProperties = 0;
    m_hasFontFaceOnlyValues = false;
}

Document* CSSParser::document() const
{
    StyleBase* root = m_styleSheet;
    Document* doc = 0;
    while (root && root->parent())
        root = root->parent();
    if (root && root->isCSSStyleSheet())
        doc = static_cast<CSSStyleSheet*>(root)->doc();
    return doc;
}

bool CSSParser::validUnit(CSSParserValue* value, Units unitflags, bool strict)
{
    if (unitflags & FNonNeg && value->fValue < 0)
        return false;

    bool b = false;
    switch(value->unit) {
    case CSSPrimitiveValue::CSS_NUMBER:
        b = (unitflags & FNumber);
        if (!b && ((unitflags & (FLength | FAngle | FTime)) && (value->fValue == 0 || !strict))) {
            value->unit = (unitflags & FLength) ? CSSPrimitiveValue::CSS_PX :
                          ((unitflags & FAngle) ? CSSPrimitiveValue::CSS_DEG : CSSPrimitiveValue::CSS_MS);
            b = true;
        }
        if (!b && (unitflags & FInteger) && value->isInt)
            b = true;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        b = (unitflags & FPercent);
        break;
    case CSSParserValue::Q_EMS:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
        b = (unitflags & FLength);
        break;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        b = (unitflags & FTime);
        break;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        b = (unitflags & FAngle);
        break;
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
        break;
    }
    return b;
}

static int unitFromString(CSSParserValue* value)
{
    if (value->unit != CSSPrimitiveValue::CSS_IDENT || value->id)
        return 0;

    if (equal(value->string, "em"))
        return CSSPrimitiveValue::CSS_EMS;
    if (equal(value->string, "ex"))
        return CSSPrimitiveValue::CSS_EXS;
    if (equal(value->string, "px"))
        return CSSPrimitiveValue::CSS_PX;
    if (equal(value->string, "cm"))
        return CSSPrimitiveValue::CSS_CM;
    if (equal(value->string, "mm"))
        return CSSPrimitiveValue::CSS_MM;
    if (equal(value->string, "in"))
        return CSSPrimitiveValue::CSS_IN;
    if (equal(value->string, "pt"))
        return CSSPrimitiveValue::CSS_PT;
    if (equal(value->string, "pc"))
        return CSSPrimitiveValue::CSS_PC;
    if (equal(value->string, "deg"))
        return CSSPrimitiveValue::CSS_DEG;
    if (equal(value->string, "rad"))
        return CSSPrimitiveValue::CSS_RAD;
    if (equal(value->string, "grad"))
        return CSSPrimitiveValue::CSS_GRAD;
    if (equal(value->string, "turn"))
        return CSSPrimitiveValue::CSS_TURN;
    if (equal(value->string, "ms"))
        return CSSPrimitiveValue::CSS_MS;
    if (equal(value->string, "s"))
        return CSSPrimitiveValue::CSS_S;
    if (equal(value->string, "Hz"))
        return CSSPrimitiveValue::CSS_HZ;
    if (equal(value->string, "kHz"))
        return CSSPrimitiveValue::CSS_KHZ;
    
    return 0;
}

void CSSParser::checkForOrphanedUnits()
{
    if (m_strict || inShorthand())
        return;
        
    // The purpose of this code is to implement the WinIE quirk that allows unit types to be separated from their numeric values
    // by whitespace, so e.g., width: 20 px instead of width:20px.  This is invalid CSS, so we don't do this in strict mode.
    CSSParserValue* numericVal = 0;
    unsigned size = m_valueList->size();
    for (unsigned i = 0; i < size; i++) {
        CSSParserValue* value = m_valueList->valueAt(i);

        if (numericVal) {
            // Change the unit type of the numeric val to match.
            int unit = unitFromString(value);
            if (unit) {
                numericVal->unit = unit;
                numericVal = 0;

                // Now delete the bogus unit value.
                m_valueList->deleteValueAt(i);
                i--; // We're safe even though |i| is unsigned, since we only hit this code if we had a previous numeric value (so |i| is always > 0 here).
                size--;
                continue;
            }
        }
        
        numericVal = (value->unit == CSSPrimitiveValue::CSS_NUMBER) ? value : 0;
    }
}

bool CSSParser::parseValue(int propId, bool important)
{
    if (!m_valueList)
        return false;

    CSSParserValue *value = m_valueList->current();

    if (!value)
        return false;

    int id = value->id;

    // In quirks mode, we will look for units that have been incorrectly separated from the number they belong to
    // by a space.  We go ahead and associate the unit with the number even though it is invalid CSS.
    checkForOrphanedUnits();
    
    int num = inShorthand() ? 1 : m_valueList->size();

    if (id == CSSValueInherit) {
        if (num != 1)
            return false;
        addProperty(propId, CSSInheritedValue::create(), important);
        return true;
    }
    else if (id == CSSValueInitial) {
        if (num != 1)
            return false;
        addProperty(propId, CSSInitialValue::createExplicit(), important);
        return true;
    }

    // If we have any variables, then we don't parse the list of values yet.  We add them to the declaration
    // as unresolved, and allow them to be parsed later.  The parse is considered "successful" for now, even though
    // it might ultimately fail once the variable has been resolved.
    if (!inShorthand() && checkForVariables(m_valueList)) {
        addUnresolvedProperty(propId, important);
        return true;
    }

    bool valid_primitive = false;
    RefPtr<CSSValue> parsedValue;

    switch (static_cast<CSSPropertyID>(propId)) {
        /* The comment to the left defines all valid value of this properties as defined
         * in CSS 2, Appendix F. Property index
         */

        /* All the CSS properties are not supported by the renderer at the moment.
         * Note that all the CSS2 Aural properties are only checked, if CSS_AURAL is defined
         * (see parseAuralValues). As we don't support them at all this seems reasonable.
         */

    case CSSPropertySize:                 // <length>{1,2} | auto | portrait | landscape | inherit
    case CSSPropertyQuotes:               // [<string> <string>]+ | none | inherit
        if (id)
            valid_primitive = true;
        break;
    case CSSPropertyUnicodeBidi:         // normal | embed | bidi-override | inherit
        if (id == CSSValueNormal ||
             id == CSSValueEmbed ||
             id == CSSValueBidiOverride)
            valid_primitive = true;
        break;

    case CSSPropertyPosition:             // static | relative | absolute | fixed | inherit
        if (id == CSSValueStatic ||
             id == CSSValueRelative ||
             id == CSSValueAbsolute ||
             id == CSSValueFixed)
            valid_primitive = true;
        break;

    case CSSPropertyPageBreakAfter:     // auto | always | avoid | left | right | inherit
    case CSSPropertyPageBreakBefore:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        if (id == CSSValueAuto ||
             id == CSSValueAlways ||
             id == CSSValueAvoid ||
             id == CSSValueLeft ||
             id == CSSValueRight)
            valid_primitive = true;
        break;

    case CSSPropertyPageBreakInside:    // avoid | auto | inherit
    case CSSPropertyWebkitColumnBreakInside:
        if (id == CSSValueAuto || id == CSSValueAvoid)
            valid_primitive = true;
        break;

    case CSSPropertyEmptyCells:          // show | hide | inherit
        if (id == CSSValueShow ||
             id == CSSValueHide)
            valid_primitive = true;
        break;

    case CSSPropertyContent:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
        // close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent(propId, important);

    case CSSPropertyWhiteSpace:          // normal | pre | nowrap | inherit
        if (id == CSSValueNormal ||
            id == CSSValuePre ||
            id == CSSValuePreWrap ||
            id == CSSValuePreLine ||
            id == CSSValueNowrap)
            valid_primitive = true;
        break;

    case CSSPropertyClip:                 // <shape> | auto | inherit
        if (id == CSSValueAuto)
            valid_primitive = true;
        else if (value->unit == CSSParserValue::Function)
            return parseShape(propId, important);
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShorthand to work
     * correctly and allows optimization in WebCore::applyRule(..)
     */
    case CSSPropertyCaptionSide:         // top | bottom | left | right | inherit
        if (id == CSSValueLeft || id == CSSValueRight ||
            id == CSSValueTop || id == CSSValueBottom)
            valid_primitive = true;
        break;

    case CSSPropertyBorderCollapse:      // collapse | separate | inherit
        if (id == CSSValueCollapse || id == CSSValueSeparate)
            valid_primitive = true;
        break;

    case CSSPropertyVisibility:           // visible | hidden | collapse | inherit
        if (id == CSSValueVisible || id == CSSValueHidden || id == CSSValueCollapse)
            valid_primitive = true;
        break;

    case CSSPropertyOverflow: {
        ShorthandScope scope(this, propId);
        if (num != 1 || !parseValue(CSSPropertyOverflowX, important))
            return false;
        CSSValue* value = m_parsedProperties[m_numParsedProperties - 1]->value();
        addProperty(CSSPropertyOverflowY, value, important);
        return true;
    }
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:           // visible | hidden | scroll | auto | marquee | overlay | inherit
        if (id == CSSValueVisible || id == CSSValueHidden || id == CSSValueScroll || id == CSSValueAuto ||
            id == CSSValueOverlay || id == CSSValueWebkitMarquee)
            valid_primitive = true;
        break;

    case CSSPropertyListStylePosition:  // inside | outside | inherit
        if (id == CSSValueInside || id == CSSValueOutside)
            valid_primitive = true;
        break;

    case CSSPropertyListStyleType:
        // disc | circle | square | decimal | decimal-leading-zero | lower-roman |
        // upper-roman | lower-greek | lower-alpha | lower-latin | upper-alpha |
        // upper-latin | hebrew | armenian | georgian | cjk-ideographic | hiragana |
        // katakana | hiragana-iroha | katakana-iroha | none | inherit
        if ((id >= CSSValueDisc && id <= CSSValueKatakanaIroha) || id == CSSValueNone)
            valid_primitive = true;
        break;

    case CSSPropertyDisplay:
        // inline | block | list-item | run-in | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | box | inline-box | none | inherit
        if ((id >= CSSValueInline && id <= CSSValueWebkitInlineBox) || id == CSSValueNone)
            valid_primitive = true;
        break;

    case CSSPropertyDirection:            // ltr | rtl | inherit
        if (id == CSSValueLtr || id == CSSValueRtl)
            valid_primitive = true;
        break;

    case CSSPropertyTextTransform:       // capitalize | uppercase | lowercase | none | inherit
        if ((id >= CSSValueCapitalize && id <= CSSValueLowercase) || id == CSSValueNone)
            valid_primitive = true;
        break;

    case CSSPropertyFloat:                // left | right | none | inherit + center for buggy CSS
        if (id == CSSValueLeft || id == CSSValueRight ||
             id == CSSValueNone || id == CSSValueCenter)
            valid_primitive = true;
        break;

    case CSSPropertyClear:                // none | left | right | both | inherit
        if (id == CSSValueNone || id == CSSValueLeft ||
             id == CSSValueRight|| id == CSSValueBoth)
            valid_primitive = true;
        break;

    case CSSPropertyTextAlign:
        // left | right | center | justify | webkit_left | webkit_right | webkit_center | start | end | <string> | inherit
        if ((id >= CSSValueWebkitAuto && id <= CSSValueWebkitCenter) || id == CSSValueStart || id == CSSValueEnd ||
             value->unit == CSSPrimitiveValue::CSS_STRING)
            valid_primitive = true;
        break;

    case CSSPropertyOutlineStyle:        // (<border-style> except hidden) | auto | inherit
        if (id == CSSValueAuto || id == CSSValueNone || (id >= CSSValueInset && id <= CSSValueDouble))
            valid_primitive = true;
        break; 
        
    case CSSPropertyBorderTopStyle:     //// <border-style> | inherit
    case CSSPropertyBorderRightStyle:   //   Defined as:    none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle:  //   solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitColumnRuleStyle:
        if (id >= CSSValueNone && id <= CSSValueDouble)
            valid_primitive = true;
        break;

    case CSSPropertyFontWeight:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | inherit
        return parseFontWeight(important);

    case CSSPropertyBorderSpacing: {
        const int properties[2] = { CSSPropertyWebkitBorderHorizontalSpacing,
                                    CSSPropertyWebkitBorderVerticalSpacing };
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = m_parsedProperties[m_numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        valid_primitive = validUnit(value, FLength|FNonNeg, m_strict);
        break;
    case CSSPropertyOutlineColor:        // <color> | invert | inherit
        // Outline color has "invert" as additional keyword.
        // Also, we want to allow the special focus color even in strict parsing mode.
        if (propId == CSSPropertyOutlineColor && (id == CSSValueInvert || id == CSSValueWebkitFocusRingColor)) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSSPropertyBackgroundColor:     // <color> | inherit
    case CSSPropertyBorderTopColor:     // <color> | inherit
    case CSSPropertyBorderRightColor:   // <color> | inherit
    case CSSPropertyBorderBottomColor:  // <color> | inherit
    case CSSPropertyBorderLeftColor:    // <color> | inherit
    case CSSPropertyColor:                // <color> | inherit
    case CSSPropertyTextLineThroughColor: // CSS3 text decoration colors
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        if (id == CSSValueWebkitText)
            valid_primitive = true; // Always allow this, even when strict parsing is on,
                                    // since we use this in our UA sheets.
        else if (id == CSSValueCurrentcolor)
            valid_primitive = true;
        else if ((id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu ||
             (id >= CSSValueWebkitFocusRingColor && id < CSSValueWebkitText && !m_strict)) {
            valid_primitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyCursor: {
        // [<uri>,]*  [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
        // nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | ew-resize | 
        // ns-resize | nesw-resize | nwse-resize | col-resize | row-resize | text | wait | help |
        // vertical-text | cell | context-menu | alias | copy | no-drop | not-allowed | -webkit-zoom-in
        // -webkit-zoom-in | -webkit-zoom-out | all-scroll | -webkit-grab | -webkit-grabbing ] ] | inherit
        RefPtr<CSSValueList> list;
        while (value && value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!list)
                list = CSSValueList::createCommaSeparated(); 
            String uri = parseURL(value->string);
            Vector<int> coords;
            value = m_valueList->next();
            while (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                coords.append(int(value->fValue));
                value = m_valueList->next();
            }
            IntPoint hotspot;
            int nrcoords = coords.size();
            if (nrcoords > 0 && nrcoords != 2) {
                if (m_strict) // only support hotspot pairs in strict mode
                    return false;
            } else if (m_strict && nrcoords == 2)
                hotspot = IntPoint(coords[0], coords[1]);
            if (m_strict || coords.size() == 0) {
                if (!uri.isNull() && m_styleSheet)
                    list->append(CSSCursorImageValue::create(m_styleSheet->completeURL(uri), hotspot));
            }
            if ((m_strict && !value) || (value && !(value->unit == CSSParserValue::Operator && value->iValue == ',')))
                return false;
            value = m_valueList->next(); // comma
        }
        if (list) {
            if (!value) { // no value after url list (MSIE 5 compatibility)
                if (list->length() != 1)
                    return false;
            } else if (!m_strict && value->id == CSSValueHand) // MSIE 5 compatibility :/
                list->append(CSSPrimitiveValue::createIdentifier(CSSValuePointer));
            else if (value && ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitGrabbing) || value->id == CSSValueCopy || value->id == CSSValueNone))
                list->append(CSSPrimitiveValue::createIdentifier(value->id));
            m_valueList->next();
            parsedValue = list.release();
            break;
        }
        id = value->id;
        if (!m_strict && value->id == CSSValueHand) { // MSIE 5 compatibility :/
            id = CSSValuePointer;
            valid_primitive = true;
        } else if ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitGrabbing) || value->id == CSSValueCopy || value->id == CSSValueNone)
            valid_primitive = true;
        break;
    }

    case CSSPropertyBackgroundAttachment:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskAttachment:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskRepeat: {
        RefPtr<CSSValue> val1;
        RefPtr<CSSValue> val2;
        int propId1, propId2;
        if (parseFillProperty(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyListStyleImage:     // <uri> | none | inherit
        if (id == CSSValueNone) {
            parsedValue = CSSImageValue::create();
            m_valueList->next();
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            // ### allow string in non strict mode?
            String uri = parseURL(value->string);
            if (!uri.isNull() && m_styleSheet) {
                parsedValue = CSSImageValue::create(m_styleSheet->completeURL(uri));
                m_valueList->next();
            }
        } else if (value->unit == CSSParserValue::Function && equalIgnoringCase(value->function->name, "-webkit-gradient(")) {
            if (parseGradient(parsedValue))
                m_valueList->next();
            else
                return false;
        }
        break;

    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyOutlineWidth:        // <border-width> | inherit
    case CSSPropertyBorderTopWidth:     //// <border-width> | inherit
    case CSSPropertyBorderRightWidth:   //   Which is defined as
    case CSSPropertyBorderBottomWidth:  //   thin | medium | thick | <length>
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyWebkitColumnRuleWidth:
        if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, m_strict);
        break;

    case CSSPropertyLetterSpacing:       // normal | <length> | inherit
    case CSSPropertyWordSpacing:         // normal | <length> | inherit
        if (id == CSSValueNormal)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, m_strict);
        break;

    case CSSPropertyWordBreak:          // normal | break-all | break-word (this is a custom extension)
        if (id == CSSValueNormal || id == CSSValueBreakAll || id == CSSValueBreakWord)
            valid_primitive = true;
        break;

    case CSSPropertyWordWrap:           // normal | break-word
        if (id == CSSValueNormal || id == CSSValueBreakWord)
            valid_primitive = true;
        break;

    case CSSPropertyTextIndent:          // <length> | <percentage> | inherit
    case CSSPropertyPaddingTop:          //// <padding-width> | inherit
    case CSSPropertyPaddingRight:        //   Which is defined as
    case CSSPropertyPaddingBottom:       //   <length> | <percentage>
    case CSSPropertyPaddingLeft:         ////
    case CSSPropertyWebkitPaddingStart:
        valid_primitive = (!id && validUnit(value, FLength|FPercent, m_strict));
        break;

    case CSSPropertyMaxHeight:           // <length> | <percentage> | none | inherit
    case CSSPropertyMaxWidth:            // <length> | <percentage> | none | inherit
        if (id == CSSValueNone || id == CSSValueIntrinsic || id == CSSValueMinIntrinsic) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSSPropertyMinHeight:           // <length> | <percentage> | inherit
    case CSSPropertyMinWidth:            // <length> | <percentage> | inherit
        if (id == CSSValueIntrinsic || id == CSSValueMinIntrinsic)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, m_strict));
        break;

    case CSSPropertyFontSize:
        // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
        if (id >= CSSValueXxSmall && id <= CSSValueLarger)
            valid_primitive = true;
        else
            valid_primitive = (validUnit(value, FLength|FPercent|FNonNeg, m_strict));
        break;

    case CSSPropertyFontStyle:           // normal | italic | oblique | inherit
        return parseFontStyle(important);

    case CSSPropertyFontVariant:         // normal | small-caps | inherit
        return parseFontVariant(important);

    case CSSPropertyVerticalAlign:
        // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if (id >= CSSValueBaseline && id <= CSSValueWebkitBaselineMiddle)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent, m_strict));
        break;

    case CSSPropertyHeight:               // <length> | <percentage> | auto | inherit
    case CSSPropertyWidth:                // <length> | <percentage> | auto | inherit
        if (id == CSSValueAuto || id == CSSValueIntrinsic || id == CSSValueMinIntrinsic)
            valid_primitive = true;
        else
            // ### handle multilength case where we allow relative units
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, m_strict));
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
        if (id == CSSValueAuto)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent, m_strict));
        break;

    case CSSPropertyZIndex:              // auto | <integer> | inherit
        if (id == CSSValueAuto) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSSPropertyOrphans:              // <integer> | inherit
    case CSSPropertyWidows:               // <integer> | inherit
        // ### not supported later on
        valid_primitive = (!id && validUnit(value, FInteger, false));
        break;

    case CSSPropertyLineHeight:          // normal | <number> | <length> | <percentage> | inherit
        if (id == CSSValueNormal)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FNumber|FLength|FPercent|FNonNeg, m_strict));
        break;
    case CSSPropertyCounterIncrement:    // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 1, important);
        valid_primitive = true;
        break;
     case CSSPropertyCounterReset:        // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 0, important);
        valid_primitive = true;
        break;
    case CSSPropertyFontFamily:
        // [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
        parsedValue = parseFontFamily();
        break;
    }

    case CSSPropertyTextDecoration:
    case CSSPropertyWebkitTextDecorationsInEffect:
        // none | [ underline || overline || line-through || blink ] | inherit
        if (id == CSSValueNone) {
            valid_primitive = true;
        } else {
            RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            bool is_valid = true;
            while(is_valid && value) {
                switch (value->id) {
                case CSSValueBlink:
                    break;
                case CSSValueUnderline:
                case CSSValueOverline:
                case CSSValueLineThrough:
                    list->append(CSSPrimitiveValue::createIdentifier(value->id));
                    break;
                default:
                    is_valid = false;
                }
                value = m_valueList->next();
            }
            if (list->length() && is_valid) {
                parsedValue = list.release();
                m_valueList->next();
            }
        }
        break;

    case CSSPropertyZoom:          // normal | reset | document | <number> | <percentage> | inherit
        if (id == CSSValueNormal || id == CSSValueReset || id == CSSValueDocument)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FNumber | FPercent | FNonNeg, true));
        break;
        
    case CSSPropertyTableLayout:         // auto | fixed | inherit
        if (id == CSSValueAuto || id == CSSValueFixed)
            valid_primitive = true;
        break;

    case CSSPropertySrc:  // Only used within @font-face, so cannot use inherit | initial or be !important.  This is a list of urls or local references.
        return parseFontFaceSrc();

    case CSSPropertyUnicodeRange:
        return parseFontFaceUnicodeRange();

    /* CSS3 properties */
    case CSSPropertyWebkitAppearance:
        if ((id >= CSSValueCheckbox && id <= CSSValueTextarea) || id == CSSValueNone)
            valid_primitive = true;
        break;

    case CSSPropertyWebkitBinding:
#if ENABLE(XBL)
        if (id == CSSValueNone)
            valid_primitive = true;
        else {
            RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
            CSSParserValue* val;
            RefPtr<CSSValue> parsedValue;
            while ((val = m_valueList->current())) {
                if (val->unit == CSSPrimitiveValue::CSS_URI && m_styleSheet) {
                    String value = parseURL(val->string);
                    parsedValue = CSSPrimitiveValue::create(m_styleSheet->completeURL(value), CSSPrimitiveValue::CSS_URI);
                }
                if (!parsedValue)
                    break;
                
                // FIXME: We can't use release() here since we might hit this path twice
                // but that logic seems wrong to me to begin with, we convert all non-uri values
                // into the last seen URI value!?
                // -webkit-binding: url(foo.xml), 1, 2; (if that were valid) is treated as:
                // -webkit-binding: url(foo.xml), url(foo.xml), url(foo.xml); !?
                values->append(parsedValue.get());
                m_valueList->next();
            }
            if (!values->length())
                return false;
            
            addProperty(propId, values.release(), important);
            m_valueList->next();
            return true;
        }
#endif
        break;
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
        if (id == CSSValueNone)
            valid_primitive = true;
        else {
            RefPtr<CSSValue> result;
            if (parseBorderImage(propId, important, result)) {
                addProperty(propId, result, important);
                return true;
            }
        }
        break;
    case CSSPropertyWebkitBorderTopRightRadius:
    case CSSPropertyWebkitBorderTopLeftRadius:
    case CSSPropertyWebkitBorderBottomLeftRadius:
    case CSSPropertyWebkitBorderBottomRightRadius:
    case CSSPropertyWebkitBorderRadius: {
        if (num != 1 && num != 2)
            return false;
        valid_primitive = validUnit(value, FLength, m_strict);
        if (!valid_primitive)
            return false;
        RefPtr<CSSPrimitiveValue> parsedValue1 = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        RefPtr<CSSPrimitiveValue> parsedValue2;
        if (num == 2) {
            value = m_valueList->next();
            valid_primitive = validUnit(value, FLength, m_strict);
            if (!valid_primitive)
                return false;
            parsedValue2 = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        } else
            parsedValue2 = parsedValue1;
        
        RefPtr<Pair> pair = Pair::create(parsedValue1.release(), parsedValue2.release());
        RefPtr<CSSPrimitiveValue> val = CSSPrimitiveValue::create(pair.release());
        if (propId == CSSPropertyWebkitBorderRadius) {
            const int properties[4] = { CSSPropertyWebkitBorderTopRightRadius,
                                        CSSPropertyWebkitBorderTopLeftRadius,
                                        CSSPropertyWebkitBorderBottomLeftRadius,
                                        CSSPropertyWebkitBorderBottomRightRadius };
            for (int i = 0; i < 4; i++)
                addProperty(properties[i], val.get(), important);
        } else
            addProperty(propId, val.release(), important);
        return true;
    }
    case CSSPropertyOutlineOffset:
        valid_primitive = validUnit(value, FLength, m_strict);
        break;
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyWebkitBoxShadow:
        if (id == CSSValueNone)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSSPropertyWebkitBoxReflect:
        if (id == CSSValueNone)
            valid_primitive = true;
        else
            return parseReflect(propId, important);
        break;
    case CSSPropertyOpacity:
        valid_primitive = validUnit(value, FNumber, m_strict);
        break;
    case CSSPropertyWebkitBoxAlign:
        if (id == CSSValueStretch || id == CSSValueStart || id == CSSValueEnd ||
            id == CSSValueCenter || id == CSSValueBaseline)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBoxDirection:
        if (id == CSSValueNormal || id == CSSValueReverse)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBoxLines:
        if (id == CSSValueSingle || id == CSSValueMultiple)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBoxOrient:
        if (id == CSSValueHorizontal || id == CSSValueVertical ||
            id == CSSValueInlineAxis || id == CSSValueBlockAxis)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBoxPack:
        if (id == CSSValueStart || id == CSSValueEnd ||
            id == CSSValueCenter || id == CSSValueJustify)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBoxFlex:
        valid_primitive = validUnit(value, FNumber, m_strict);
        break;
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxOrdinalGroup:
        valid_primitive = validUnit(value, FInteger|FNonNeg, true);
        break;
    case CSSPropertyWebkitBoxSizing:
        valid_primitive = id == CSSValueBorderBox || id == CSSValueContentBox;
        break;
    case CSSPropertyWebkitMarquee: {
        const int properties[5] = { CSSPropertyWebkitMarqueeDirection, CSSPropertyWebkitMarqueeIncrement,
                                    CSSPropertyWebkitMarqueeRepetition,
                                    CSSPropertyWebkitMarqueeStyle, CSSPropertyWebkitMarqueeSpeed };
        return parseShorthand(propId, properties, 5, important);
    }
    case CSSPropertyWebkitMarqueeDirection:
        if (id == CSSValueForwards || id == CSSValueBackwards || id == CSSValueAhead ||
            id == CSSValueReverse || id == CSSValueLeft || id == CSSValueRight || id == CSSValueDown ||
            id == CSSValueUp || id == CSSValueAuto)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitMarqueeIncrement:
        if (id == CSSValueSmall || id == CSSValueLarge || id == CSSValueMedium)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, m_strict);
        break;
    case CSSPropertyWebkitMarqueeStyle:
        if (id == CSSValueNone || id == CSSValueSlide || id == CSSValueScroll || id == CSSValueAlternate)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitMarqueeRepetition:
        if (id == CSSValueInfinite)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FInteger|FNonNeg, m_strict);
        break;
    case CSSPropertyWebkitMarqueeSpeed:
        if (id == CSSValueNormal || id == CSSValueSlow || id == CSSValueFast)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FTime|FInteger|FNonNeg, m_strict);
        break;
    case CSSPropertyWebkitUserDrag: // auto | none | element
        if (id == CSSValueAuto || id == CSSValueNone || id == CSSValueElement)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitUserModify: // read-only | read-write
        if (id == CSSValueReadOnly || id == CSSValueReadWrite || id == CSSValueReadWritePlaintextOnly)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitUserSelect: // auto | none | text
        if (id == CSSValueAuto || id == CSSValueNone || id == CSSValueText)
            valid_primitive = true;
        break;
    case CSSPropertyTextOverflow: // clip | ellipsis
        if (id == CSSValueClip || id == CSSValueEllipsis)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitTransform:
        if (id == CSSValueNone)
            valid_primitive = true;
        else {
            PassRefPtr<CSSValue> val = parseTransform();
            if (val) {
                addProperty(propId, val, important);
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ: {
        RefPtr<CSSValue> val1;
        RefPtr<CSSValue> val2;
        RefPtr<CSSValue> val3;
        int propId1, propId2, propId3;
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
    case CSSPropertyWebkitTransformStyle:
        if (value->id == CSSValueFlat || value->id == CSSValuePreserve3d)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitBackfaceVisibility:
        if (value->id == CSSValueVisible || value->id == CSSValueHidden)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitPerspective:
        if (id == CSSValueNone)
            valid_primitive = true;
        else {
            // Accepting valueless numbers is a quirk of the -webkit prefixed version of the property.
            if (validUnit(value, FNumber|FLength|FNonNeg, m_strict)) {
                RefPtr<CSSValue> val = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
                if (val) {
                    addProperty(propId, val.release(), important);
                    return true;
                }
                return false;
            }
        }
        break;
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY: {
        RefPtr<CSSValue> val1;
        RefPtr<CSSValue> val2;
        int propId1, propId2;
        if (parsePerspectiveOrigin(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitTransitionProperty: {
        RefPtr<CSSValue> val;
        if (parseAnimationProperty(propId, val)) {
            addProperty(propId, val.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitMarginCollapse: {
        const int properties[2] = { CSSPropertyWebkitMarginTopCollapse,
            CSSPropertyWebkitMarginBottomCollapse };
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = m_parsedProperties[m_numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
        if (id == CSSValueCollapse || id == CSSValueSeparate || id == CSSValueDiscard)
            valid_primitive = true;
        break;
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextUnderlineMode:
        if (id == CSSValueContinuous || id == CSSValueSkipWhiteSpace)
            valid_primitive = true;
        break;
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextUnderlineStyle:
        if (id == CSSValueNone || id == CSSValueSolid || id == CSSValueDouble ||
            id == CSSValueDashed || id == CSSValueDotDash || id == CSSValueDotDotDash ||
            id == CSSValueWave)
            valid_primitive = true;
        break;
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineWidth:
        if (id == CSSValueAuto || id == CSSValueNormal || id == CSSValueThin ||
            id == CSSValueMedium || id == CSSValueThick)
            valid_primitive = true;
        else
            valid_primitive = !id && validUnit(value, FNumber|FLength|FPercent, m_strict);
        break;
    case CSSPropertyResize: // none | both | horizontal | vertical | auto
        if (id == CSSValueNone || id == CSSValueBoth || id == CSSValueHorizontal || id == CSSValueVertical || id == CSSValueAuto)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitColumnCount:
        if (id == CSSValueAuto)
            valid_primitive = true;
        else
            valid_primitive = !id && validUnit(value, FInteger | FNonNeg, false);
        break;
    case CSSPropertyWebkitColumnGap:         // normal | <length>
        if (id == CSSValueNormal)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength | FNonNeg, m_strict);
        break;
    case CSSPropertyWebkitColumnWidth:         // auto | <length>
        if (id == CSSValueAuto)
            valid_primitive = true;
        else // Always parse this property in strict mode, since it would be ambiguous otherwise when used in the 'columns' shorthand property.
            valid_primitive = validUnit(value, FLength, true);
        break;
    case CSSPropertyPointerEvents:
        // none | visiblePainted | visibleFill | visibleStroke | visible |
        // painted | fill | stroke | auto | all | inherit
        if (id == CSSValueVisible || id == CSSValueNone || id == CSSValueAll || id == CSSValueAuto ||
            (id >= CSSValueVisiblepainted && id <= CSSValueStroke))
            valid_primitive = true;
        break;
            
    // End of CSS3 properties

    // Apple specific properties.  These will never be standardized and are purely to
    // support custom WebKit-based Apple applications.
    case CSSPropertyWebkitLineClamp:
        valid_primitive = (!id && validUnit(value, FPercent, false));
        break;
    case CSSPropertyWebkitTextSizeAdjust:
        if (id == CSSValueAuto || id == CSSValueNone)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitRtlOrdering:
        if (id == CSSValueLogical || id == CSSValueVisual)
            valid_primitive = true;
        break;
    
    case CSSPropertyWebkitFontSizeDelta:           // <length>
        valid_primitive = validUnit(value, FLength, m_strict);
        break;

    case CSSPropertyWebkitNbspMode:     // normal | space
        if (id == CSSValueNormal || id == CSSValueSpace)
            valid_primitive = true;
        break;

    case CSSPropertyWebkitLineBreak:   // normal | after-white-space
        if (id == CSSValueNormal || id == CSSValueAfterWhiteSpace)
            valid_primitive = true;
        break;

    case CSSPropertyWebkitMatchNearestMailBlockquoteColor:   // normal | match
        if (id == CSSValueNormal || id == CSSValueMatch)
            valid_primitive = true;
        break;

    case CSSPropertyWebkitHighlight:
        if (id == CSSValueNone || value->unit == CSSPrimitiveValue::CSS_STRING)
            valid_primitive = true;
        break;
    
    case CSSPropertyWebkitBorderFit:
        if (id == CSSValueBorder || id == CSSValueLines)
            valid_primitive = true;
        break;
        
    case CSSPropertyWebkitTextSecurity:
        // disc | circle | square | none | inherit
        if (id == CSSValueDisc || id == CSSValueCircle || id == CSSValueSquare|| id == CSSValueNone)
            valid_primitive = true;
        break;

#if ENABLE(DASHBOARD_SUPPORT)
    case CSSPropertyWebkitDashboardRegion:                 // <dashboard-region> | <dashboard-region> 
        if (value->unit == CSSParserValue::Function || id == CSSValueNone)
            return parseDashboardRegions(propId, important);
        break;
#endif
    // End Apple-specific properties

        /* shorthand properties */
    case CSSPropertyBackground: {
        // Position must come before color in this array because a plain old "0" is a legal color
        // in quirks mode but it's usually the X coordinate of a position.
        // FIXME: Add CSSPropertyWebkitBackgroundSize to the shorthand.
        const int properties[] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat, 
                                   CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyWebkitBackgroundClip,
                                   CSSPropertyWebkitBackgroundOrigin, CSSPropertyBackgroundColor };
        return parseFillShorthand(propId, properties, 7, important);
    }
    case CSSPropertyWebkitMask: {
        const int properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat, 
                                   CSSPropertyWebkitMaskAttachment, CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskClip,
                                   CSSPropertyWebkitMaskOrigin };
        return parseFillShorthand(propId, properties, 6, important);
    }
    case CSSPropertyBorder:
        // [ 'border-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSSPropertyBorderWidth, CSSPropertyBorderStyle,
                                    CSSPropertyBorderColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyBorderTop:
        // [ 'border-top-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSSPropertyBorderTopWidth, CSSPropertyBorderTopStyle,
                                    CSSPropertyBorderTopColor};
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyBorderRight:
        // [ 'border-right-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSSPropertyBorderRightWidth, CSSPropertyBorderRightStyle,
                                    CSSPropertyBorderRightColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyBorderBottom:
        // [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSSPropertyBorderBottomWidth, CSSPropertyBorderBottomStyle,
                                    CSSPropertyBorderBottomColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyBorderLeft:
        // [ 'border-left-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSSPropertyBorderLeftWidth, CSSPropertyBorderLeftStyle,
                                    CSSPropertyBorderLeftColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyOutline:
        // [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
    {
        const int properties[3] = { CSSPropertyOutlineWidth, CSSPropertyOutlineStyle,
                                    CSSPropertyOutlineColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyBorderColor:
        // <color>{1,4} | inherit
    {
        const int properties[4] = { CSSPropertyBorderTopColor, CSSPropertyBorderRightColor,
                                    CSSPropertyBorderBottomColor, CSSPropertyBorderLeftColor };
        return parse4Values(propId, properties, important);
    }
    case CSSPropertyBorderWidth:
        // <border-width>{1,4} | inherit
    {
        const int properties[4] = { CSSPropertyBorderTopWidth, CSSPropertyBorderRightWidth,
                                    CSSPropertyBorderBottomWidth, CSSPropertyBorderLeftWidth };
        return parse4Values(propId, properties, important);
    }
    case CSSPropertyBorderStyle:
        // <border-style>{1,4} | inherit
    {
        const int properties[4] = { CSSPropertyBorderTopStyle, CSSPropertyBorderRightStyle,
                                    CSSPropertyBorderBottomStyle, CSSPropertyBorderLeftStyle };
        return parse4Values(propId, properties, important);
    }
    case CSSPropertyMargin:
        // <margin-width>{1,4} | inherit
    {
        const int properties[4] = { CSSPropertyMarginTop, CSSPropertyMarginRight,
                                    CSSPropertyMarginBottom, CSSPropertyMarginLeft };
        return parse4Values(propId, properties, important);
    }
    case CSSPropertyPadding:
        // <padding-width>{1,4} | inherit
    {
        const int properties[4] = { CSSPropertyPaddingTop, CSSPropertyPaddingRight,
                                    CSSPropertyPaddingBottom, CSSPropertyPaddingLeft };
        return parse4Values(propId, properties, important);
    }
    case CSSPropertyFont:
        // [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
        // 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
        if (id >= CSSValueCaption && id <= CSSValueStatusBar)
            valid_primitive = true;
        else
            return parseFont(important);
        break;
    case CSSPropertyListStyle:
    {
        const int properties[3] = { CSSPropertyListStyleType, CSSPropertyListStylePosition,
                                    CSSPropertyListStyleImage };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyWebkitColumns: {
        const int properties[2] = { CSSPropertyWebkitColumnWidth, CSSPropertyWebkitColumnCount };
        return parseShorthand(propId, properties, 2, important);
    }
    case CSSPropertyWebkitColumnRule: {
        const int properties[3] = { CSSPropertyWebkitColumnRuleWidth, CSSPropertyWebkitColumnRuleStyle,
                                    CSSPropertyWebkitColumnRuleColor };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSSPropertyWebkitTextStroke: {
        const int properties[2] = { CSSPropertyWebkitTextStrokeWidth, CSSPropertyWebkitTextStrokeColor };
        return parseShorthand(propId, properties, 2, important);
    }
    case CSSPropertyWebkitAnimation:
        return parseAnimationShorthand(important);
    case CSSPropertyWebkitTransition:
        return parseTransitionShorthand(important);
    case CSSPropertyInvalid:
        return false;
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextOverline:
    case CSSPropertyTextUnderline:
    case CSSPropertyWebkitVariableDeclarationBlock:
        return false;
#if ENABLE(SVG)
    default:
        return parseSVGValue(propId, important);
#endif
    }

    if (valid_primitive) {
        if (id != 0)
            parsedValue = CSSPrimitiveValue::createIdentifier(id);
        else if (value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = CSSPrimitiveValue::create(value->string, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSParserValue::Q_EMS)
            parsedValue = CSSQuirkPrimitiveValue::create(value->fValue, CSSPrimitiveValue::CSS_EMS);
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

void CSSParser::addFillValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueList*>(lval.get())->append(rval);
        else {
            PassRefPtr<CSSValue> oldlVal(lval.release());
            PassRefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(oldlVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

const int cMaxFillProperties = 7;

bool CSSParser::parseFillShorthand(int propId, const int* properties, int numProperties, bool important)
{
    ASSERT(numProperties <= cMaxFillProperties);
    if (numProperties > cMaxFillProperties)
        return false;

    ShorthandScope scope(this, propId);

    bool parsedProperty[cMaxFillProperties] = { false };
    RefPtr<CSSValue> values[cMaxFillProperties];
    RefPtr<CSSValue> positionYValue;
    int i;

    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSSPropertyBackgroundColor && parsedProperty[i])
                    // Color is not allowed except as the last item in a list for backgrounds.
                    // Reject the entire property.
                    return false;

                if (!parsedProperty[i] && properties[i] != CSSPropertyBackgroundColor) {
                    addFillValue(values[i], CSSInitialValue::createImplicit());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, CSSInitialValue::createImplicit());
                }
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val1;
                RefPtr<CSSValue> val2;
                int propId1, propId2;
                if (parseFillProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addFillValue(values[i], val1.release());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, val2.release());
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }
    
    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i]) {
            addFillValue(values[i], CSSInitialValue::createImplicit());
            if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                addFillValue(positionYValue, CSSInitialValue::createImplicit());
        }
    }
    
    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        if (properties[i] == CSSPropertyBackgroundPosition) {
            addProperty(CSSPropertyBackgroundPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskPosition) {
            addProperty(CSSPropertyWebkitMaskPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyWebkitMaskPosition once
            addProperty(CSSPropertyWebkitMaskPositionY, positionYValue.release(), important);
        } else
            addProperty(properties[i], values[i].release(), important);
    }
    
    return true;
}

void CSSParser::addAnimationValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueList*>(lval.get())->append(rval);
        else {
            PassRefPtr<CSSValue> oldVal(lval.release());
            PassRefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(oldVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

bool CSSParser::parseAnimationShorthand(bool important)
{
    const int properties[] = {  CSSPropertyWebkitAnimationName,
                                CSSPropertyWebkitAnimationDuration,
                                CSSPropertyWebkitAnimationTimingFunction,
                                CSSPropertyWebkitAnimationDelay,
                                CSSPropertyWebkitAnimationIterationCount,
                                CSSPropertyWebkitAnimationDirection };
    const int numProperties = sizeof(properties) / sizeof(properties[0]);
    
    ShorthandScope scope(this, CSSPropertyWebkitAnimation);

    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    RefPtr<CSSValue> values[numProperties];
    
    int i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], CSSInitialValue::createImplicit());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val;
                if (parseAnimationProperty(properties[i], val)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.release());
                }
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
            addAnimationValue(values[i], CSSInitialValue::createImplicit());
    }
    
    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++)
        addProperty(properties[i], values[i].release(), important);
    
    return true;
}

bool CSSParser::parseTransitionShorthand(bool important)
{
    const int properties[] = { CSSPropertyWebkitTransitionProperty,
                               CSSPropertyWebkitTransitionDuration,
                               CSSPropertyWebkitTransitionTimingFunction,
                               CSSPropertyWebkitTransitionDelay };
    const int numProperties = sizeof(properties) / sizeof(properties[0]);
    
    ShorthandScope scope(this, CSSPropertyWebkitTransition);

    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    RefPtr<CSSValue> values[numProperties];
    
    int i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], CSSInitialValue::createImplicit());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val;
                if (parseAnimationProperty(properties[i], val)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.release());
                }
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
            addAnimationValue(values[i], CSSInitialValue::createImplicit());
    }
    
    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++)
        addProperty(properties[i], values[i].release(), important);
    
    return true;
}

bool CSSParser::parseShorthand(int propId, const int *properties, int numProperties, bool important)
{
    // We try to match as many properties as possible
    // We set up an array of booleans to mark which property has been found,
    // and we try to search for properties until it makes no longer any sense.
    ShorthandScope scope(this, propId);

    bool found = false;
    bool fnd[6]; // Trust me ;)
    for (int i = 0; i < numProperties; i++)
        fnd[i] = false;

    while (m_valueList->current()) {
        found = false;
        for (int propIndex = 0; !found && propIndex < numProperties; ++propIndex) {
            if (!fnd[propIndex]) {
                if (parseValue(properties[propIndex], important))
                    fnd[propIndex] = found = true;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }
    
    // Fill in any remaining properties with the initial value.
    m_implicitShorthand = true;
    for (int i = 0; i < numProperties; ++i) {
        if (!fnd[i])
            addProperty(properties[i], CSSInitialValue::createImplicit(), important);
    }
    m_implicitShorthand = false;

    return true;
}

bool CSSParser::parse4Values(int propId, const int *properties,  bool important)
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */
    
    int num = inShorthand() ? 1 : m_valueList->size();
    
    ShorthandScope scope(this, propId);

    // the order is top, right, bottom, left
    switch (num) {
        case 1: {
            if (!parseValue(properties[0], important))
                return false;
            CSSValue *value = m_parsedProperties[m_numParsedProperties-1]->value();
            m_implicitShorthand = true;
            addProperty(properties[1], value, important);
            addProperty(properties[2], value, important);
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 2: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            CSSValue *value = m_parsedProperties[m_numParsedProperties-2]->value();
            m_implicitShorthand = true;
            addProperty(properties[2], value, important);
            value = m_parsedProperties[m_numParsedProperties-2]->value();
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 3: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) || !parseValue(properties[2], important))
                return false;
            CSSValue *value = m_parsedProperties[m_numParsedProperties-2]->value();
            m_implicitShorthand = true;
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
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

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSParser::parseContent(int propId, bool important)
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    while (CSSParserValue* val = m_valueList->current()) {
        RefPtr<CSSValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_URI && m_styleSheet) {
            // url
            String value = parseURL(val->string);
            parsedValue = CSSImageValue::create(m_styleSheet->completeURL(value));
        } else if (val->unit == CSSParserValue::Function) {
            // attr(X) | counter(X [,Y]) | counters(X, Y, [,Z]) | -webkit-gradient(...)
            CSSParserValueList* args = val->function->args;
            if (!args)
                return false;
            if (equalIgnoringCase(val->function->name, "attr(")) {
                parsedValue = parseAttr(args);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "counter(")) {
                parsedValue = parseCounterContent(args, false);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "counters(")) {
                parsedValue = parseCounterContent(args, true);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "-webkit-gradient(")) {
                if (!parseGradient(parsedValue))
                    return false;
            } else if (equalIgnoringCase(val->function->name, "-webkit-canvas(")) {
                if (!parseCanvas(parsedValue))
                    return false;
            } else
                return false;
        } else if (val->unit == CSSPrimitiveValue::CSS_IDENT) {
            // open-quote
            // close-quote
            // no-open-quote
            // no-close-quote
            // FIXME: These are not yet implemented (http://bugs.webkit.org/show_bug.cgi?id=6503).
        } else if (val->unit == CSSPrimitiveValue::CSS_STRING) {
            parsedValue = CSSPrimitiveValue::create(val->string, CSSPrimitiveValue::CSS_STRING);
        }
        if (!parsedValue)
            break;
        values->append(parsedValue.release());
        m_valueList->next();
    }

    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }

    return false;
}

PassRefPtr<CSSValue> CSSParser::parseAttr(CSSParserValueList* args)
{
    if (args->size() != 1)
        return 0;

    CSSParserValue* a = args->current();

    if (a->unit != CSSPrimitiveValue::CSS_IDENT)
        return 0;

    String attrName = a->string;
    // CSS allows identifiers with "-" at the start, like "-webkit-mask-image".
    // But HTML attribute names can't have those characters, and we should not
    // even parse them inside attr().
    if (attrName[0] == '-')
        return 0;

    if (document()->isHTMLDocument())
        attrName = attrName.lower();
    
    return CSSPrimitiveValue::create(attrName, CSSPrimitiveValue::CSS_ATTR);
}

PassRefPtr<CSSValue> CSSParser::parseBackgroundColor()
{
    int id = m_valueList->current()->id;
    if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu || id == CSSValueCurrentcolor ||
        (id >= CSSValueGrey && id < CSSValueWebkitText && !m_strict))
       return CSSPrimitiveValue::createIdentifier(id);
    return parseColor();
}

bool CSSParser::parseFillImage(RefPtr<CSSValue>& value)
{
    if (m_valueList->current()->id == CSSValueNone) {
        value = CSSImageValue::create();
        return true;
    }
    if (m_valueList->current()->unit == CSSPrimitiveValue::CSS_URI) {
        String uri = parseURL(m_valueList->current()->string);
        if (!uri.isNull() && m_styleSheet)
            value = CSSImageValue::create(m_styleSheet->completeURL(uri));
        return true;
    }

    if (m_valueList->current()->unit == CSSParserValue::Function) {
        if (equalIgnoringCase(m_valueList->current()->function->name, "-webkit-gradient("))
            return parseGradient(value);
        if (equalIgnoringCase(m_valueList->current()->function->name, "-webkit-canvas("))
            return parseCanvas(value);
    }
    return false;
}

PassRefPtr<CSSValue> CSSParser::parseFillPositionXY(bool& xFound, bool& yFound)
{
    int id = m_valueList->current()->id;
    if (id == CSSValueLeft || id == CSSValueTop || id == CSSValueRight || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueLeft || id == CSSValueRight) {
            if (xFound)
                return 0;
            xFound = true;
            if (id == CSSValueRight)
                percent = 100;
        }
        else if (id == CSSValueTop || id == CSSValueBottom) {
            if (yFound)
                return 0;
            yFound = true;
            if (id == CSSValueBottom)
                percent = 100;
        }
        else if (id == CSSValueCenter)
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
        return CSSPrimitiveValue::create(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(m_valueList->current(), FPercent|FLength, m_strict))
        return CSSPrimitiveValue::create(m_valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)m_valueList->current()->unit);
                
    return 0;
}

void CSSParser::parseFillPosition(RefPtr<CSSValue>& value1, RefPtr<CSSValue>& value2)
{
    CSSParserValue* value = m_valueList->current();
    
    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    bool value1IsX = false, value1IsY = false;
    value1 = parseFillPositionXY(value1IsX, value1IsY);
    if (!value1)
        return;
    
    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    value = m_valueList->next();
    
    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && value->unit == CSSParserValue::Operator && value->iValue == ',')
        value = 0;
    
    bool value2IsX = false, value2IsY = false;
    if (value) {
        value2 = parseFillPositionXY(value2IsX, value2IsY);
        if (value2)
            m_valueList->next();
        else {
            if (!inShorthand()) {
                value1.clear();
                return;
            }
        }
    }
    
    if (!value2)
        // Only one value was specified.  If that value was not a keyword, then it sets the x position, and the y position
        // is simply 50%.  This is our default.
        // For keywords, the keyword was either an x-keyword (left/right), a y-keyword (top/bottom), or an ambiguous keyword (center).
        // For left/right/center, the default of 50% in the y is still correct.
        value2 = CSSPrimitiveValue::create(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    if (value1IsY || value2IsX)
        value1.swap(value2);
}

PassRefPtr<CSSValue> CSSParser::parseFillSize()
{
    CSSParserValue* value = m_valueList->current();
    RefPtr<CSSPrimitiveValue> parsedValue1;
    
    if (value->id == CSSValueAuto)
        parsedValue1 = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_UNKNOWN);
    else {
        if (!validUnit(value, FLength|FPercent, m_strict))
            return 0;
        parsedValue1 = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    }
    
    RefPtr<CSSPrimitiveValue> parsedValue2 = parsedValue1;
    if ((value = m_valueList->next())) {
        if (value->id == CSSValueAuto)
            parsedValue2 = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_UNKNOWN);
        else {
            if (!validUnit(value, FLength|FPercent, m_strict))
                return 0;
            parsedValue2 = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        }
    }
    
    return CSSPrimitiveValue::create(Pair::create(parsedValue1.release(), parsedValue2.release()));
}

bool CSSParser::parseFillProperty(int propId, int& propId1, int& propId2, 
                                  RefPtr<CSSValue>& retValue1, RefPtr<CSSValue>& retValue2)
{
    RefPtr<CSSValueList> values;
    RefPtr<CSSValueList> values2;
    CSSParserValue* val;
    RefPtr<CSSValue> value;
    RefPtr<CSSValue> value2;
    
    bool allowComma = false;
    
    retValue1 = retValue2 = 0;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyBackgroundPosition) {
        propId1 = CSSPropertyBackgroundPositionX;
        propId2 = CSSPropertyBackgroundPositionY;
    } else if (propId == CSSPropertyWebkitMaskPosition) {
        propId1 = CSSPropertyWebkitMaskPositionX;
        propId2 = CSSPropertyWebkitMaskPositionY;
    }

    while ((val = m_valueList->current())) {
        RefPtr<CSSValue> currValue;
        RefPtr<CSSValue> currValue2;
        
        if (allowComma) {
            if (val->unit != CSSParserValue::Operator || val->iValue != ',')
                return false;
            m_valueList->next();
            allowComma = false;
        } else {
            switch (propId) {
                case CSSPropertyBackgroundColor:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyBackgroundAttachment:
                case CSSPropertyWebkitMaskAttachment:
                    if (val->id == CSSValueScroll || val->id == CSSValueFixed) {
                        currValue = CSSPrimitiveValue::createIdentifier(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundImage:
                case CSSPropertyWebkitMaskImage:
                    if (parseFillImage(currValue))
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitBackgroundClip:
                case CSSPropertyWebkitBackgroundOrigin:
                case CSSPropertyWebkitMaskClip:
                case CSSPropertyWebkitMaskOrigin:
                    // The first three values here are deprecated and should not be allowed to apply when we drop the -webkit-
                    // from the property names.
                    if (val->id == CSSValueBorder || val->id == CSSValuePadding || val->id == CSSValueContent ||
                        val->id == CSSValueBorderBox || val->id == CSSValuePaddingBox || val->id == CSSValueContentBox || val->id == CSSValueText) {
                        currValue = CSSPrimitiveValue::createIdentifier(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundPosition:
                case CSSPropertyWebkitMaskPosition:
                    parseFillPosition(currValue, currValue2);
                    // parseFillPosition advances the m_valueList pointer
                    break;
                case CSSPropertyBackgroundPositionX:
                case CSSPropertyWebkitMaskPositionX: {
                    bool xFound = false, yFound = true;
                    currValue = parseFillPositionXY(xFound, yFound);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyBackgroundPositionY:
                case CSSPropertyWebkitMaskPositionY: {
                    bool xFound = true, yFound = false;
                    currValue = parseFillPositionXY(xFound, yFound);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyWebkitBackgroundComposite:
                case CSSPropertyWebkitMaskComposite:
                    if ((val->id >= CSSValueClear && val->id <= CSSValuePlusLighter) || val->id == CSSValueHighlight) {
                        currValue = CSSPrimitiveValue::createIdentifier(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundRepeat:
                case CSSPropertyWebkitMaskRepeat:
                    if (val->id >= CSSValueRepeat && val->id <= CSSValueNoRepeat) {
                        currValue = CSSPrimitiveValue::createIdentifier(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyWebkitBackgroundSize:
                case CSSPropertyWebkitMaskSize:
                    currValue = parseFillSize();
                    if (currValue)
                        m_valueList->next();
                    break;
            }
            if (!currValue)
                return false;
            
            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.release());
            }
            
            if (value2 && !values2) {
                values2 = CSSValueList::createCommaSeparated();
                values2->append(value2.release());
            }
            
            if (values)
                values->append(currValue.release());
            else
                value = currValue.release();
            if (currValue2) {
                if (values2)
                    values2->append(currValue2.release());
                else
                    value2 = currValue2.release();
            }
            allowComma = true;
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

PassRefPtr<CSSValue> CSSParser::parseAnimationDelay()
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FTime, m_strict))
        return CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationDirection()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNormal || value->id == CSSValueAlternate)
        return CSSPrimitiveValue::createIdentifier(value->id);
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationDuration()
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FTime|FNonNeg, m_strict))
        return CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationIterationCount()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueInfinite)
        return CSSPrimitiveValue::createIdentifier(value->id);
    if (validUnit(value, FInteger|FNonNeg, m_strict))
        return CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationName()
{
    CSSParserValue* value = m_valueList->current();
    if (value->unit == CSSPrimitiveValue::CSS_STRING || value->unit == CSSPrimitiveValue::CSS_IDENT) {
        if (value->id == CSSValueNone || (value->unit == CSSPrimitiveValue::CSS_STRING && equalIgnoringCase(value->string, "none"))) {
            return CSSPrimitiveValue::createIdentifier(CSSValueNone);
        } else {
            return CSSPrimitiveValue::create(value->string, CSSPrimitiveValue::CSS_STRING);
        }
    }
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationProperty()
{
    CSSParserValue* value = m_valueList->current();
    if (value->unit != CSSPrimitiveValue::CSS_IDENT)
        return 0;
    int result = cssPropertyID(value->string);
    if (result)
        return CSSPrimitiveValue::createIdentifier(result);
    if (equalIgnoringCase(value->string, "all"))
        return CSSPrimitiveValue::createIdentifier(CSSValueAll);
    if (equalIgnoringCase(value->string, "none"))
        return CSSPrimitiveValue::createIdentifier(CSSValueNone);
    return 0;
}

void CSSParser::parseTransformOriginShorthand(RefPtr<CSSValue>& value1, RefPtr<CSSValue>& value2, RefPtr<CSSValue>& value3)
{
    parseFillPosition(value1, value2);

    // now get z
    if (m_valueList->current() && validUnit(m_valueList->current(), FLength, m_strict))
        value3 = CSSPrimitiveValue::create(m_valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)m_valueList->current()->unit);
    if (value3)
        m_valueList->next();
}

bool CSSParser::parseTimingFunctionValue(CSSParserValueList*& args, double& result)
{
    CSSParserValue* v = args->current();
    if (!validUnit(v, FNumber, m_strict))
        return false;
    result = v->fValue;
    if (result < 0 || result > 1.0)
        return false;
    v = args->next();
    if (!v)
        // The last number in the function has no comma after it, so we're done.
        return true;
    if (v->unit != CSSParserValue::Operator && v->iValue != ',')
        return false;
    v = args->next();
    return true;
}

PassRefPtr<CSSValue> CSSParser::parseAnimationTimingFunction()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueEase || value->id == CSSValueLinear || value->id == CSSValueEaseIn || value->id == CSSValueEaseOut || value->id == CSSValueEaseInOut)
        return CSSPrimitiveValue::createIdentifier(value->id);
    
    // We must be a function.
    if (value->unit != CSSParserValue::Function)
        return 0;
    
    // The only timing function we accept for now is a cubic bezier function.  4 points must be specified.
    CSSParserValueList* args = value->function->args;
    if (!equalIgnoringCase(value->function->name, "cubic-bezier(") || !args || args->size() != 7)
        return 0;

    // There are two points specified.  The values must be between 0 and 1.
    double x1, y1, x2, y2;

    if (!parseTimingFunctionValue(args, x1))
        return 0;
    if (!parseTimingFunctionValue(args, y1))
        return 0;
    if (!parseTimingFunctionValue(args, x2))
        return 0;
    if (!parseTimingFunctionValue(args, y2))
        return 0;

    return CSSTimingFunctionValue::create(x1, y1, x2, y2);
}

bool CSSParser::parseAnimationProperty(int propId, RefPtr<CSSValue>& result)
{
    RefPtr<CSSValueList> values;
    CSSParserValue* val;
    RefPtr<CSSValue> value;
    bool allowComma = false;
    
    result = 0;

    while ((val = m_valueList->current())) {
        RefPtr<CSSValue> currValue;
        if (allowComma) {
            if (val->unit != CSSParserValue::Operator || val->iValue != ',')
                return false;
            m_valueList->next();
            allowComma = false;
        }
        else {
            switch (propId) {
                case CSSPropertyWebkitAnimationDelay:
                case CSSPropertyWebkitTransitionDelay:
                    currValue = parseAnimationDelay();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitAnimationDirection:
                    currValue = parseAnimationDirection();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitAnimationDuration:
                case CSSPropertyWebkitTransitionDuration:
                    currValue = parseAnimationDuration();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitAnimationIterationCount:
                    currValue = parseAnimationIterationCount();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitAnimationName:
                    currValue = parseAnimationName();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitTransitionProperty:
                    currValue = parseAnimationProperty();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitAnimationTimingFunction:
                case CSSPropertyWebkitTransitionTimingFunction:
                    currValue = parseAnimationTimingFunction();
                    if (currValue)
                        m_valueList->next();
                    break;
            }
            
            if (!currValue)
                return false;
            
            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.release());
            }
            
            if (values)
                values->append(currValue.release());
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



#if ENABLE(DASHBOARD_SUPPORT)

#define DASHBOARD_REGION_NUM_PARAMETERS  6
#define DASHBOARD_REGION_SHORT_NUM_PARAMETERS  2

static CSSParserValue* skipCommaInDashboardRegion (CSSParserValueList *args)
{
    if (args->size() == (DASHBOARD_REGION_NUM_PARAMETERS*2-1) ||
         args->size() == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
        CSSParserValue* current = args->current();
        if (current->unit == CSSParserValue::Operator && current->iValue == ',')
            return args->next();
    }
    return args->current();
}

bool CSSParser::parseDashboardRegions(int propId, bool important)
{
    bool valid = true;
    
    CSSParserValue* value = m_valueList->current();

    if (value->id == CSSValueNone) {
        if (m_valueList->next())
            return false;
        addProperty(propId, CSSPrimitiveValue::createIdentifier(value->id), important);
        return valid;
    }
        
    RefPtr<DashboardRegion> firstRegion = DashboardRegion::create();
    DashboardRegion* region = 0;

    while (value) {
        if (region == 0) {
            region = firstRegion.get();
        } else {
            RefPtr<DashboardRegion> nextRegion = DashboardRegion::create();
            region->m_next = nextRegion;
            region = nextRegion.get();
        }
        
        if (value->unit != CSSParserValue::Function) {
            valid = false;
            break;
        }
            
        // Commas count as values, so allow:
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // also allow
        // dashboard-region(label, type) or dashboard-region(label type)
        // dashboard-region(label, type) or dashboard-region(label type)
        CSSParserValueList* args = value->function->args;
        if (!equalIgnoringCase(value->function->name, "dashboard-region(") || !args) {
            valid = false;
            break;
        }
        
        int numArgs = args->size();
        if ((numArgs != DASHBOARD_REGION_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_NUM_PARAMETERS*2-1)) &&
            (numArgs != DASHBOARD_REGION_SHORT_NUM_PARAMETERS && numArgs != (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1))){
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
        arg = skipCommaInDashboardRegion (args);
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }

        if (equalIgnoringCase(arg->string, "circle"))
            region->m_isCircle = true;
        else if (equalIgnoringCase(arg->string, "rectangle"))
            region->m_isRectangle = true;
        else {
            valid = false;
            break;
        }
            
        region->m_geometryType = arg->string;

        if (numArgs == DASHBOARD_REGION_SHORT_NUM_PARAMETERS || numArgs == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
            // This originally used CSSValueInvalid by accident. It might be more logical to use something else.
            RefPtr<CSSPrimitiveValue> amount = CSSPrimitiveValue::createIdentifier(CSSValueInvalid);
                
            region->setTop(amount);
            region->setRight(amount);
            region->setBottom(amount);
            region->setLeft(amount);
        } else {
            // Next four arguments must be offset numbers
            int i;
            for (i = 0; i < 4; i++) {
                arg = args->next();
                arg = skipCommaInDashboardRegion (args);

                valid = arg->id == CSSValueAuto || validUnit(arg, FLength, m_strict);
                if (!valid)
                    break;
                    
                RefPtr<CSSPrimitiveValue> amount = arg->id == CSSValueAuto ?
                    CSSPrimitiveValue::createIdentifier(CSSValueAuto) :
                    CSSPrimitiveValue::create(arg->fValue, (CSSPrimitiveValue::UnitTypes) arg->unit);
                    
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
        addProperty(propId, CSSPrimitiveValue::create(firstRegion.release()), important);
        
    return valid;
}

#endif /* ENABLE(DASHBOARD_SUPPORT) */

PassRefPtr<CSSValue> CSSParser::parseCounterContent(CSSParserValueList* args, bool counters)
{
    unsigned numArgs = args->size();
    if (counters && numArgs != 3 && numArgs != 5)
        return 0;
    if (!counters && numArgs != 1 && numArgs != 3)
        return 0;
    
    CSSParserValue* i = args->current();
    if (i->unit != CSSPrimitiveValue::CSS_IDENT)
        return 0;
    RefPtr<CSSPrimitiveValue> identifier = CSSPrimitiveValue::create(i->string, CSSPrimitiveValue::CSS_STRING);

    RefPtr<CSSPrimitiveValue> separator;
    if (!counters)
        separator = CSSPrimitiveValue::create(String(), CSSPrimitiveValue::CSS_STRING);
    else {
        i = args->next();
        if (i->unit != CSSParserValue::Operator || i->iValue != ',')
            return 0;
        
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_STRING)
            return 0;
        
        separator = CSSPrimitiveValue::create(i->string, (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    RefPtr<CSSPrimitiveValue> listStyle;
    i = args->next();
    if (!i) // Make the list style default decimal
        listStyle = CSSPrimitiveValue::create(CSSValueDecimal - CSSValueDisc, CSSPrimitiveValue::CSS_NUMBER);
    else {
        if (i->unit != CSSParserValue::Operator || i->iValue != ',')
            return 0;
        
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_IDENT)
            return 0;
        
        short ls = 0;
        if (i->id == CSSValueNone)
            ls = CSSValueKatakanaIroha - CSSValueDisc + 1;
        else if (i->id >= CSSValueDisc && i->id <= CSSValueKatakanaIroha)
            ls = i->id - CSSValueDisc;
        else
            return 0;

        listStyle = CSSPrimitiveValue::create(ls, (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    return CSSPrimitiveValue::create(Counter::create(identifier.release(), listStyle.release(), separator.release()));
}

bool CSSParser::parseShape(int propId, bool important)
{
    CSSParserValue* value = m_valueList->current();
    CSSParserValueList* args = value->function->args;
    
    if (!equalIgnoringCase(value->function->name, "rect(") || !args)
        return false;

    // rect(t, r, b, l) || rect(t r b l)
    if (args->size() != 4 && args->size() != 7)
        return false;
    RefPtr<Rect> rect = Rect::create();
    bool valid = true;
    int i = 0;
    CSSParserValue* a = args->current();
    while (a) {
        valid = a->id == CSSValueAuto || validUnit(a, FLength, m_strict);
        if (!valid)
            break;
        RefPtr<CSSPrimitiveValue> length = a->id == CSSValueAuto ?
            CSSPrimitiveValue::createIdentifier(CSSValueAuto) :
            CSSPrimitiveValue::create(a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit);
        if (i == 0)
            rect->setTop(length);
        else if (i == 1)
            rect->setRight(length);
        else if (i == 2)
            rect->setBottom(length);
        else
            rect->setLeft(length);
        a = args->next();
        if (a && args->size() == 7) {
            if (a->unit == CSSParserValue::Operator && a->iValue == ',') {
                a = args->next();
            } else {
                valid = false;
                break;
            }
        }
        i++;
    }
    if (valid) {
        addProperty(propId, CSSPrimitiveValue::create(rect.release()), important);
        m_valueList->next();
        return true;
    }
    return false;
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSParser::parseFont(bool important)
{
    bool valid = true;
    CSSParserValue *value = m_valueList->current();
    RefPtr<FontValue> font = FontValue::create();
    // optional font-style, font-variant and font-weight
    while (value) {
        int id = value->id;
        if (id) {
            if (id == CSSValueNormal) {
                // do nothing, it's the inital value for all three
            } else if (id == CSSValueItalic || id == CSSValueOblique) {
                if (font->style)
                    return false;
                font->style = CSSPrimitiveValue::createIdentifier(id);
            } else if (id == CSSValueSmallCaps) {
                if (font->variant)
                    return false;
                font->variant = CSSPrimitiveValue::createIdentifier(id);
            } else if (id >= CSSValueBold && id <= CSSValueLighter) {
                if (font->weight)
                    return false;
                font->weight = CSSPrimitiveValue::createIdentifier(id);
            } else {
                valid = false;
            }
        } else if (!font->weight && validUnit(value, FInteger|FNonNeg, true)) {
            int weight = (int)value->fValue;
            int val = 0;
            if (weight == 100)
                val = CSSValue100;
            else if (weight == 200)
                val = CSSValue200;
            else if (weight == 300)
                val = CSSValue300;
            else if (weight == 400)
                val = CSSValue400;
            else if (weight == 500)
                val = CSSValue500;
            else if (weight == 600)
                val = CSSValue600;
            else if (weight == 700)
                val = CSSValue700;
            else if (weight == 800)
                val = CSSValue800;
            else if (weight == 900)
                val = CSSValue900;

            if (val)
                font->weight = CSSPrimitiveValue::createIdentifier(val);
            else
                valid = false;
        } else {
            valid = false;
        }
        if (!valid)
            break;
        value = m_valueList->next();
    }
    if (!value)
        return false;

    // set undefined values to default
    if (!font->style)
        font->style = CSSPrimitiveValue::createIdentifier(CSSValueNormal);
    if (!font->variant)
        font->variant = CSSPrimitiveValue::createIdentifier(CSSValueNormal);
    if (!font->weight)
        font->weight = CSSPrimitiveValue::createIdentifier(CSSValueNormal);

    // now a font size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (value->id >= CSSValueXxSmall && value->id <= CSSValueLarger)
        font->size = CSSPrimitiveValue::createIdentifier(value->id);
    else if (validUnit(value, FLength|FPercent|FNonNeg, m_strict))
        font->size = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
    value = m_valueList->next();
    if (!font->size || !value)
        return false;

    if (value->unit == CSSParserValue::Operator && value->iValue == '/') {
        // line-height
        value = m_valueList->next();
        if (!value)
            return false;
        if (value->id == CSSValueNormal) {
            // default value, nothing to do
        } else if (validUnit(value, FNumber|FLength|FPercent|FNonNeg, m_strict))
            font->lineHeight = CSSPrimitiveValue::create(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else
            return false;
        value = m_valueList->next();
        if (!value)
            return false;
    }
    
    if (!font->lineHeight)
        font->lineHeight = CSSPrimitiveValue::createIdentifier(CSSValueNormal);

    // font family must come now
    font->family = parseFontFamily();

    if (m_valueList->current() || !font->family)
        return false;

    addProperty(CSSPropertyFont, font.release(), important);
    return true;
}

PassRefPtr<CSSValueList> CSSParser::parseFontFamily()
{
    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    CSSParserValue* value = m_valueList->current();

    FontFamilyValue* currFamily = 0;
    while (value) {
        CSSParserValue* nextValue = m_valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == CSSParserValue::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSSValueSerif && nextValue->id <= CSSValueWebkitBody) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id >= CSSValueSerif && value->id <= CSSValueWebkitBody) {
            if (currFamily)
                currFamily->appendSpaceSeparated(value->string.characters, value->string.length);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(CSSPrimitiveValue::createIdentifier(value->id));
            else {
                RefPtr<FontFamilyValue> newFamily = FontFamilyValue::create(value->string);
                currFamily = newFamily.get();
                list->append(newFamily.release());
            }
        } else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFamily = 0;
            list->append(FontFamilyValue::create(value->string));
        } else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (currFamily)
                currFamily->appendSpaceSeparated(value->string.characters, value->string.length);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(FontFamilyValue::create(value->string));
            else {
                RefPtr<FontFamilyValue> newFamily = FontFamilyValue::create(value->string);
                currFamily = newFamily.get();
                list->append(newFamily.release());
            }
        } else {
            break;
        }
        
        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = m_valueList->next();
            currFamily = 0;
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }
    if (!list->length())
        list = 0;
    return list.release();
}

bool CSSParser::parseFontStyle(bool important)
{
    RefPtr<CSSValueList> values;
    if (m_valueList->size() > 1)
        values = CSSValueList::createCommaSeparated();
    CSSParserValue* val;
    bool expectComma = false;
    while ((val = m_valueList->current())) {
        RefPtr<CSSPrimitiveValue> parsedValue;
        if (!expectComma) {
            expectComma = true;
            if (val->id == CSSValueNormal || val->id == CSSValueItalic || val->id == CSSValueOblique)
                parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
            else if (val->id == CSSValueAll && !values) {
                // 'all' is only allowed in @font-face and with no other values. Make a value list to
                // indicate that we are in the @font-face case.
                values = CSSValueList::createCommaSeparated();
                parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
            }
        } else if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            expectComma = false;
            m_valueList->next();
            continue;
        }

        if (!parsedValue)
            return false;

        m_valueList->next();

        if (values)
            values->append(parsedValue.release());
        else {
            addProperty(CSSPropertyFontStyle, parsedValue.release(), important);
            return true;
        }
    }

    if (values && values->length()) {
        m_hasFontFaceOnlyValues = true;
        addProperty(CSSPropertyFontStyle, values.release(), important);
        return true;
    }

    return false;
}

bool CSSParser::parseFontVariant(bool important)
{
    RefPtr<CSSValueList> values;
    if (m_valueList->size() > 1)
        values = CSSValueList::createCommaSeparated();
    CSSParserValue* val;
    bool expectComma = false;
    while ((val = m_valueList->current())) {
        RefPtr<CSSPrimitiveValue> parsedValue;
        if (!expectComma) {
            expectComma = true;
            if (val->id == CSSValueNormal || val->id == CSSValueSmallCaps)
                parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
            else if (val->id == CSSValueAll && !values) {
                // 'all' is only allowed in @font-face and with no other values. Make a value list to
                // indicate that we are in the @font-face case.
                values = CSSValueList::createCommaSeparated();
                parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
            }
        } else if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            expectComma = false;
            m_valueList->next();
            continue;
        }

        if (!parsedValue)
            return false;

        m_valueList->next();

        if (values)
            values->append(parsedValue.release());
        else {
            addProperty(CSSPropertyFontVariant, parsedValue.release(), important);
            return true;
        }
    }

    if (values && values->length()) {
        m_hasFontFaceOnlyValues = true;
        addProperty(CSSPropertyFontVariant, values.release(), important);
        return true;
    }

    return false;
}

bool CSSParser::parseFontWeight(bool important)
{
    RefPtr<CSSValueList> values;
    if (m_valueList->size() > 1)
        values = CSSValueList::createCommaSeparated();
    CSSParserValue* val;
    bool expectComma = false;
    while ((val = m_valueList->current())) {
        RefPtr<CSSPrimitiveValue> parsedValue;
        if (!expectComma) {
            expectComma = true;
            if (val->unit == CSSPrimitiveValue::CSS_IDENT) {
                if (val->id >= CSSValueNormal && val->id <= CSSValue900)
                    parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
                else if (val->id == CSSValueAll && !values) {
                    // 'all' is only allowed in @font-face and with no other values. Make a value list to
                    // indicate that we are in the @font-face case.
                    values = CSSValueList::createCommaSeparated();
                    parsedValue = CSSPrimitiveValue::createIdentifier(val->id);
                }
            } else if (validUnit(val, FInteger | FNonNeg, false)) {
                int weight = static_cast<int>(val->fValue);
                if (!(weight % 100) && weight >= 100 && weight <= 900)
                    parsedValue = CSSPrimitiveValue::createIdentifier(CSSValue100 + weight / 100 - 1);
            }
        } else if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            expectComma = false;
            m_valueList->next();
            continue;
        }

        if (!parsedValue)
            return false;

        m_valueList->next();

        if (values)
            values->append(parsedValue.release());
        else {
            addProperty(CSSPropertyFontWeight, parsedValue.release(), important);
            return true;
        }
    }

    if (values && values->length()) {
        m_hasFontFaceOnlyValues = true;
        addProperty(CSSPropertyFontWeight, values.release(), important);
        return true;
    }

    return false;
}

bool CSSParser::parseFontFaceSrc()
{
    RefPtr<CSSValueList> values(CSSValueList::createCommaSeparated());
    CSSParserValue* val;
    bool expectComma = false;
    bool allowFormat = false;
    bool failed = false;
    RefPtr<CSSFontFaceSrcValue> uriValue;
    while ((val = m_valueList->current())) {
        RefPtr<CSSFontFaceSrcValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_URI && !expectComma && m_styleSheet) {
            String value = parseURL(val->string);
            parsedValue = CSSFontFaceSrcValue::create(m_styleSheet->completeURL(value));
            uriValue = parsedValue;
            allowFormat = true;
            expectComma = true;
        } else if (val->unit == CSSParserValue::Function) {
            // There are two allowed functions: local() and format().             
            CSSParserValueList* args = val->function->args;
            if (args && args->size() == 1) {
                if (equalIgnoringCase(val->function->name, "local(") && !expectComma) {
                    expectComma = true;
                    allowFormat = false;
                    CSSParserValue* a = args->current();
                    uriValue.clear();
                    parsedValue = CSSFontFaceSrcValue::createLocal(a->string);
                } else if (equalIgnoringCase(val->function->name, "format(") && allowFormat && uriValue) {
                    expectComma = true;
                    allowFormat = false;
                    uriValue->setFormat(args->current()->string);
                    uriValue.clear();
                    m_valueList->next();
                    continue;
                }
            }
        } else if (val->unit == CSSParserValue::Operator && val->iValue == ',' && expectComma) {
            expectComma = false;
            allowFormat = false;
            uriValue.clear();
            m_valueList->next();
            continue;
        }
    
        if (parsedValue)
            values->append(parsedValue.release());
        else {
            failed = true;
            break;
        }
        m_valueList->next();
    }
    
    if (values->length() && !failed) {
        addProperty(CSSPropertySrc, values.release(), m_important);
        m_valueList->next();
        return true;
    }

    return false;
}

bool CSSParser::parseFontFaceUnicodeRange()
{
    RefPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    CSSParserValue* currentValue;
    bool failed = false;
    while ((currentValue = m_valueList->current())) {
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
        values->append(CSSUnicodeRangeValue::create(from, to));
        m_valueList->next();
    }
    if (failed || !values->length())
        return false;
    addProperty(CSSPropertyUnicodeRange, values.release(), m_important);
    return true;
}

bool CSSParser::parseColor(const String &name, RGBA32& rgb, bool strict)
{
    if (!strict && Color::parseHexColor(name, rgb))
        return true;

    // try a little harder
    Color tc;
    tc.setNamedColor(name);
    if (tc.isValid()) {
        rgb = tc.rgb();
        return true;
    }

    return false;
}

bool CSSParser::parseColorParameters(CSSParserValue* value, int* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args;
    CSSParserValue* v = args->current();
    Units unitType = FUnknown;
    // Get the first value and its type
    if (validUnit(v, FInteger, true))
        unitType = FInteger;
    else if (validUnit(v, FPercent, true))
        unitType = FPercent;
    else
        return false;
    colorArray[0] = static_cast<int>(v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256.0 / 100.0 : 1.0));
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, unitType, true))
            return false;
        colorArray[i] = static_cast<int>(v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256.0 / 100.0 : 1.0));
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = static_cast<int>(max(0.0, min(1.0, v->fValue)) * 255);
    }
    return true;
}

// The CSS3 specification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSParser::parseHSLParameters(CSSParserValue* value, double* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args;
    CSSParserValue* v = args->current();
    // Get the first value
    if (!validUnit(v, FNumber, true))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(v->fValue) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FPercent, true))
            return false;
        colorArray[i] = max(0.0, min(100.0, v->fValue)) / 100.0; // needs to be value between 0 and 1.0
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = max(0.0, min(1.0, v->fValue));
    }
    return true;
}

PassRefPtr<CSSPrimitiveValue> CSSParser::parseColor(CSSParserValue* value)
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(value ? value : m_valueList->current(), c))
        return 0;
    return CSSPrimitiveValue::createColor(c);
}

bool CSSParser::parseColorFromValue(CSSParserValue* value, RGBA32& c, bool svg)
{
    if (!m_strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
        value->fValue >= 0. && value->fValue < 1000000.) {
        String str = String::format("%06d", (int)(value->fValue+.5));
        if (!CSSParser::parseColor(str, c, m_strict))
            return false;
    } else if (value->unit == CSSPrimitiveValue::CSS_PARSER_HEXCOLOR ||
                value->unit == CSSPrimitiveValue::CSS_IDENT ||
                (!m_strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION)) {
        if (!CSSParser::parseColor(value->string, c, m_strict && value->unit == CSSPrimitiveValue::CSS_IDENT))
            return false;
    } else if (value->unit == CSSParserValue::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 5 /* rgb + two commas */ &&
                equalIgnoringCase(value->function->name, "rgb(")) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        c = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else if (!svg) {
        if (value->unit == CSSParserValue::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* rgba + three commas */ &&
                equalIgnoringCase(value->function->name, "rgba(")) {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            c = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value->unit == CSSParserValue::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 5 /* hsl + two commas */ &&
                    equalIgnoringCase(value->function->name, "hsl(")) {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value->unit == CSSParserValue::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 7 /* hsla + three commas */ &&
                    equalIgnoringCase(value->function->name, "hsla(")) {
            double colorValues[4];
            if (!parseHSLParameters(value, colorValues, true))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else
            return false;
    } else
        return false;

    return true;
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
struct ShadowParseContext {
    ShadowParseContext()
    : allowX(true)
    , allowY(false)
    , allowBlur(false)
    , allowColor(true)
    , allowBreak(true)
    {}

    bool allowLength() { return allowX || allowY || allowBlur; }

    void commitValue() {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || color) {
            if (!values)
                values = CSSValueList::createCommaSeparated();
            
            // Construct the current shadow value and add it to the list.
            values->append(ShadowValue::create(x.release(), y.release(), blur.release(), color.release()));
        }
        
        // Now reset for the next shadow value.
        x = y = blur = color = 0;
        allowX = allowColor = allowBreak = true;
        allowY = allowBlur = false;  
    }

    void commitLength(CSSParserValue* v) {
        RefPtr<CSSPrimitiveValue> val = CSSPrimitiveValue::create(v->fValue, (CSSPrimitiveValue::UnitTypes)v->unit);

        if (allowX) {
            x = val.release();
            allowX = false; allowY = true; allowColor = false; allowBreak = false;
        }
        else if (allowY) {
            y = val.release();
            allowY = false; allowBlur = true; allowColor = true; allowBreak = true;
        }
        else if (allowBlur) {
            blur = val.release();
            allowBlur = false;
        }
    }

    void commitColor(PassRefPtr<CSSPrimitiveValue> val) {
        color = val;
        allowColor = false;
        if (allowX)
            allowBreak = false;
        else
            allowBlur = false;
    }
    
    RefPtr<CSSValueList> values;
    RefPtr<CSSPrimitiveValue> x;
    RefPtr<CSSPrimitiveValue> y;
    RefPtr<CSSPrimitiveValue> blur;
    RefPtr<CSSPrimitiveValue> color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowColor;
    bool allowBreak;
};

bool CSSParser::parseShadow(int propId, bool important)
{
    ShadowParseContext context;
    CSSParserValue* val;
    while ((val = m_valueList->current())) {
        // Check for a comma break first.
        if (val->unit == CSSParserValue::Operator) {
            if (val->iValue != ',' || !context.allowBreak)
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return false;
            
            // The value is good.  Commit it.
            context.commitValue();
        }
        // Check to see if we're a length.
        else if (validUnit(val, FLength, true)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return false;

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(val);
        }
        else {
            // The only other type of value that's ok is a color value.
            RefPtr<CSSPrimitiveValue> parsedColor;
            bool isColor = ((val->id >= CSSValueAqua && val->id <= CSSValueWindowtext) || val->id == CSSValueMenu ||
                            (val->id >= CSSValueWebkitFocusRingColor && val->id <= CSSValueWebkitText && !m_strict));
            if (isColor) {
                if (!context.allowColor)
                    return false;
                parsedColor = CSSPrimitiveValue::createIdentifier(val->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColor(val);

            if (!parsedColor || !context.allowColor)
                return false; // This value is not a color or length and is invalid or
                              // it is a color, but a color isn't allowed at this point.
            
            context.commitColor(parsedColor.release());
        }
        
        m_valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values->length()) {
            addProperty(propId, context.values.release(), important);
            m_valueList->next();
            return true;
        }
    }
    
    return false;
}

bool CSSParser::parseReflect(int propId, bool important)
{
    // box-reflect: <direction> <offset> <mask>
    
    // Direction comes first.
    CSSParserValue* val = m_valueList->current();
    CSSReflectionDirection direction;
    switch (val->id) {
        case CSSValueAbove:
            direction = ReflectionAbove;
            break;
        case CSSValueBelow:
            direction = ReflectionBelow;
            break;
        case CSSValueLeft:
            direction = ReflectionLeft;
            break;
        case CSSValueRight:
            direction = ReflectionRight;
            break;
        default:
            return false;
    }

    // The offset comes next.
    val = m_valueList->next();
    RefPtr<CSSPrimitiveValue> offset;
    if (!val)
        offset = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_PX);
    else {
        if (!validUnit(val, FLength | FPercent, m_strict))
            return false;
        offset = CSSPrimitiveValue::create(val->fValue, static_cast<CSSPrimitiveValue::UnitTypes>(val->unit));
    }

    // Now for the mask.
    RefPtr<CSSValue> mask;
    val = m_valueList->next();
    if (val) {
        if (!parseBorderImage(propId, important, mask))
            return false;
    }

    RefPtr<CSSReflectValue> reflectValue = CSSReflectValue::create(direction, offset.release(), mask.release());
    addProperty(propId, reflectValue.release(), important);
    m_valueList->next();
    return true;
}

struct BorderImageParseContext
{
    BorderImageParseContext()
    : m_allowBreak(false)
    , m_allowNumber(false)
    , m_allowSlash(false)
    , m_allowWidth(false)
    , m_allowRule(false)
    , m_borderTop(0)
    , m_borderRight(0)
    , m_borderBottom(0)
    , m_borderLeft(0)
    , m_horizontalRule(0)
    , m_verticalRule(0)
    {}
    
    bool allowBreak() const { return m_allowBreak; }
    bool allowNumber() const { return m_allowNumber; }
    bool allowSlash() const { return m_allowSlash; }
    bool allowWidth() const { return m_allowWidth; }
    bool allowRule() const { return m_allowRule; }

    void commitImage(PassRefPtr<CSSValue> image) { m_image = image; m_allowNumber = true; }
    void commitNumber(CSSParserValue* v) {
        PassRefPtr<CSSPrimitiveValue> val = CSSPrimitiveValue::create(v->fValue, (CSSPrimitiveValue::UnitTypes)v->unit);
        if (!m_top)
            m_top = val;
        else if (!m_right)
            m_right = val;
        else if (!m_bottom)
            m_bottom = val;
        else {
            ASSERT(!m_left);
            m_left = val;
        }
        
        m_allowBreak = m_allowSlash = m_allowRule = true;
        m_allowNumber = !m_left;
    }
    void commitSlash() { m_allowBreak = m_allowSlash = m_allowNumber = false; m_allowWidth = true; }
    void commitWidth(CSSParserValue* val) {
        if (!m_borderTop)
            m_borderTop = val;
        else if (!m_borderRight)
            m_borderRight = val;
        else if (!m_borderBottom)
            m_borderBottom = val;
        else {
            ASSERT(!m_borderLeft);
            m_borderLeft = val;
        }

        m_allowBreak = m_allowRule = true;
        m_allowWidth = !m_borderLeft;
    }
    void commitRule(int keyword) {
        if (!m_horizontalRule)
            m_horizontalRule = keyword;
        else if (!m_verticalRule)
            m_verticalRule = keyword;
        m_allowRule = !m_verticalRule;
    }
    PassRefPtr<CSSValue> commitBorderImage(CSSParser* p, bool important) {
        // We need to clone and repeat values for any omissions.
        if (!m_right) {
            m_right = CSSPrimitiveValue::create(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_bottom = CSSPrimitiveValue::create(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_left = CSSPrimitiveValue::create(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
        }
        if (!m_bottom) {
            m_bottom = CSSPrimitiveValue::create(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_left = CSSPrimitiveValue::create(m_right->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_right->primitiveType());
        }
        if (!m_left)
             m_left = CSSPrimitiveValue::create(m_right->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_right->primitiveType());
             
        // Now build a rect value to hold all four of our primitive values.
        RefPtr<Rect> rect = Rect::create();
        rect->setTop(m_top);
        rect->setRight(m_right);
        rect->setBottom(m_bottom);
        rect->setLeft(m_left);

        // Fill in STRETCH as the default if it wasn't specified.
        if (!m_horizontalRule)
            m_horizontalRule = CSSValueStretch;
            
        // The vertical rule should match the horizontal rule if unspecified.
        if (!m_verticalRule)
            m_verticalRule = m_horizontalRule;

        // Now we have to deal with the border widths.  The best way to deal with these is to actually put these values into a value
        // list and then make our parsing machinery do the parsing.
        if (m_borderTop) {
            CSSParserValueList newList;
            newList.addValue(*m_borderTop);
            if (m_borderRight)
                newList.addValue(*m_borderRight);
            if (m_borderBottom)
                newList.addValue(*m_borderBottom);
            if (m_borderLeft)
                newList.addValue(*m_borderLeft);
            CSSParserValueList* oldList = p->m_valueList;
            p->m_valueList = &newList;
            p->parseValue(CSSPropertyBorderWidth, important);
            p->m_valueList = oldList;
        }

        // Make our new border image value now.
        return CSSBorderImageValue::create(m_image, rect.release(), m_horizontalRule, m_verticalRule);
    }
    
    bool m_allowBreak;
    bool m_allowNumber;
    bool m_allowSlash;
    bool m_allowWidth;
    bool m_allowRule;
    
    RefPtr<CSSValue> m_image;

    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;
    
    CSSParserValue* m_borderTop;
    CSSParserValue* m_borderRight;
    CSSParserValue* m_borderBottom;
    CSSParserValue* m_borderLeft;
    
    int m_horizontalRule;
    int m_verticalRule;
};

bool CSSParser::parseBorderImage(int propId, bool important, RefPtr<CSSValue>& result)
{
    // Look for an image initially.  If the first value is not a URI, then we're done.
    BorderImageParseContext context;
    CSSParserValue* val = m_valueList->current();
    if (val->unit == CSSPrimitiveValue::CSS_URI && m_styleSheet) {        
        String uri = parseURL(val->string);
        if (uri.isNull())
            return false;
        context.commitImage(CSSImageValue::create(m_styleSheet->completeURL(uri)));
    } else if (val->unit == CSSParserValue::Function) {
        RefPtr<CSSValue> value;
        if ((equalIgnoringCase(val->function->name, "-webkit-gradient(") && parseGradient(value)) ||
            (equalIgnoringCase(val->function->name, "-webkit-canvas(") && parseCanvas(value)))
            context.commitImage(value);
        else
            return false;
    } else
        return false;

    while ((val = m_valueList->next())) {
        if (context.allowNumber() && validUnit(val, FInteger|FNonNeg|FPercent, true)) {
            context.commitNumber(val);
        } else if (propId == CSSPropertyWebkitBorderImage && context.allowSlash() && val->unit == CSSParserValue::Operator && val->iValue == '/') {
            context.commitSlash();
        } else if (context.allowWidth() &&
            (val->id == CSSValueThin || val->id == CSSValueMedium || val->id == CSSValueThick || validUnit(val, FLength, m_strict))) {
            context.commitWidth(val);
        } else if (context.allowRule() &&
            (val->id == CSSValueStretch || val->id == CSSValueRound || val->id == CSSValueRepeat)) {
            context.commitRule(val->id);
        } else {
            // Something invalid was encountered.
            return false;
        }
    }
    
    if (context.allowNumber() && propId != CSSPropertyWebkitBorderImage) {
        // Allow the slices to be omitted for images that don't fit to a border.  We just set the slices to be 0.
        context.m_top = CSSPrimitiveValue::create(0, CSSPrimitiveValue::CSS_NUMBER);
        context.m_allowBreak = true;
    }

    if (context.allowBreak()) {
        // Need to fully commit as a single value.
        result = context.commitBorderImage(this, important);
        return true;
    }
    
    return false;
}

bool CSSParser::parseCounter(int propId, int defaultValue, bool important)
{
    enum { ID, VAL } state = ID;

    RefPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    RefPtr<CSSPrimitiveValue> counterName;
    
    while (true) {
        CSSParserValue* val = m_valueList->current();
        switch (state) {
            case ID:
                if (val && val->unit == CSSPrimitiveValue::CSS_IDENT) {
                    counterName = CSSPrimitiveValue::create(val->string, CSSPrimitiveValue::CSS_STRING);
                    state = VAL;
                    m_valueList->next();
                    continue;
                }
                break;
            case VAL: {
                int i = defaultValue;
                if (val && val->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    i = (int)val->fValue;
                    m_valueList->next();
                }

                list->append(CSSPrimitiveValue::create(Pair::create(counterName.release(),
                    CSSPrimitiveValue::create(i, CSSPrimitiveValue::CSS_NUMBER))));
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

static PassRefPtr<CSSPrimitiveValue> parseGradientPoint(CSSParserValue* a, bool horizontal)
{
    RefPtr<CSSPrimitiveValue> result;
    if (a->unit == CSSPrimitiveValue::CSS_IDENT) {
        if ((equalIgnoringCase(a->string, "left") && horizontal) || 
            (equalIgnoringCase(a->string, "top") && !horizontal))
            result = CSSPrimitiveValue::create(0., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if ((equalIgnoringCase(a->string, "right") && horizontal) ||
                 (equalIgnoringCase(a->string, "bottom") && !horizontal))
            result = CSSPrimitiveValue::create(100., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if (equalIgnoringCase(a->string, "center"))
            result = CSSPrimitiveValue::create(50., CSSPrimitiveValue::CSS_PERCENTAGE);
    } else if (a->unit == CSSPrimitiveValue::CSS_NUMBER || a->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        result = CSSPrimitiveValue::create(a->fValue, (CSSPrimitiveValue::UnitTypes)a->unit);
    return result;
}

static bool parseGradientColorStop(CSSParser* p, CSSParserValue* a, CSSGradientColorStop& stop)
{
    if (a->unit != CSSParserValue::Function)
        return false;
    
    if (!equalIgnoringCase(a->function->name, "from(") &&
        !equalIgnoringCase(a->function->name, "to(") &&
        !equalIgnoringCase(a->function->name, "color-stop("))
        return false;
    
    CSSParserValueList* args = a->function->args;
    if (!args)
        return false;
    
    if (equalIgnoringCase(a->function->name, "from(") || 
        equalIgnoringCase(a->function->name, "to(")) {
        // The "from" and "to" stops expect 1 argument.
        if (args->size() != 1)
            return false;
        
        if (equalIgnoringCase(a->function->name, "from("))
            stop.m_stop = 0.f;
        else
            stop.m_stop = 1.f;
        
        int id = args->current()->id;
        if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu)
            stop.m_color = CSSPrimitiveValue::createIdentifier(id);
        else
            stop.m_color = p->parseColor(args->current());
        if (!stop.m_color)
            return false;
    }
    
    // The "color-stop" function expects 3 arguments.
    if (equalIgnoringCase(a->function->name, "color-stop(")) {
        if (args->size() != 3)
            return false;
        
        CSSParserValue* stopArg = args->current();
        if (stopArg->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
            stop.m_stop = (float)stopArg->fValue / 100.f;
        else if (stopArg->unit == CSSPrimitiveValue::CSS_NUMBER)
            stop.m_stop = (float)stopArg->fValue;
        else
            return false;

        stopArg = args->next();
        if (stopArg->unit != CSSParserValue::Operator || stopArg->iValue != ',')
            return false;
            
        stopArg = args->next();
        int id = stopArg->id;
        if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu)
            stop.m_color = CSSPrimitiveValue::createIdentifier(id);
        else
            stop.m_color = p->parseColor(stopArg);
        if (!stop.m_color)
            return false;
    }

    return true;
}

bool CSSParser::parseGradient(RefPtr<CSSValue>& gradient)
{
    RefPtr<CSSGradientValue> result = CSSGradientValue::create();
    
    // Walk the arguments.
    CSSParserValueList* args = m_valueList->current()->function->args;
    if (!args || args->size() == 0)
        return false;
    
    // The first argument is the gradient type.  It is an identifier.
    CSSParserValue* a = args->current();
    if (!a || a->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;
    if (equalIgnoringCase(a->string, "linear"))
        result->setType(CSSLinearGradient);
    else if (equalIgnoringCase(a->string, "radial"))
        result->setType(CSSRadialGradient);
    else
        return false;
    
    // Comma.
    a = args->next();
    if (!a || a->unit != CSSParserValue::Operator || a->iValue != ',')
        return false;
    
    // Next comes the starting point for the gradient as an x y pair.  There is no
    // comma between the x and the y values.
    // First X.  It can be left, right, number or percent.
    a = args->next();
    if (!a)
        return false;
    RefPtr<CSSPrimitiveValue> point = parseGradientPoint(a, true);
    if (!point)
        return false;
    result->setFirstX(point.release());
    
    // First Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseGradientPoint(a, false);
    if (!point)
        return false;
    result->setFirstY(point.release());
    
    // Comma after the first point.
    a = args->next();
    if (!a || a->unit != CSSParserValue::Operator || a->iValue != ',')
        return false;
            
    // For radial gradients only, we now expect a numeric radius.
    if (result->type() == CSSRadialGradient) {
        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        result->setFirstRadius(CSSPrimitiveValue::create(a->fValue, CSSPrimitiveValue::CSS_NUMBER));
        
        // Comma after the first radius.
        a = args->next();
        if (!a || a->unit != CSSParserValue::Operator || a->iValue != ',')
            return false;
    }
    
    // Next is the ending point for the gradient as an x, y pair.
    // Second X.  It can be left, right, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseGradientPoint(a, true);
    if (!point)
        return false;
    result->setSecondX(point.release());
    
    // Second Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseGradientPoint(a, false);
    if (!point)
        return false;
    result->setSecondY(point.release());

    // For radial gradients only, we now expect the second radius.
    if (result->type() == CSSRadialGradient) {
        // Comma after the second point.
        a = args->next();
        if (!a || a->unit != CSSParserValue::Operator || a->iValue != ',')
            return false;
        
        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        result->setSecondRadius(CSSPrimitiveValue::create(a->fValue, CSSPrimitiveValue::CSS_NUMBER));
    }

    // We now will accept any number of stops (0 or more).
    a = args->next();
    while (a) {
        // Look for the comma before the next stop.
        if (a->unit != CSSParserValue::Operator || a->iValue != ',')
            return false;
        
        // Now examine the stop itself.
        a = args->next();
        if (!a)
            return false;
        
        // The function name needs to be one of "from", "to", or "color-stop."
        CSSGradientColorStop stop;
        if (!parseGradientColorStop(this, a, stop))
            return false;
        result->addStop(stop);
        
        // Advance
        a = args->next();
    }
    
    gradient = result.release();
    return true;
}

bool CSSParser::parseCanvas(RefPtr<CSSValue>& canvas)
{
    RefPtr<CSSCanvasValue> result = CSSCanvasValue::create();
    
    // Walk the arguments.
    CSSParserValueList* args = m_valueList->current()->function->args;
    if (!args || args->size() != 1)
        return false;
    
    // The first argument is the canvas name.  It is an identifier.
    CSSParserValue* a = args->current();
    if (!a || a->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;
    result->setName(a->string);
    canvas = result;
    return true;
}

class TransformOperationInfo {
public:
    TransformOperationInfo(const CSSParserString& name)
    : m_type(WebKitCSSTransformValue::UnknownTransformOperation)
    , m_argCount(1)
    , m_allowSingleArgument(false)
    , m_unit(CSSParser::FUnknown)
    {
        if (equalIgnoringCase(name, "scale(") || equalIgnoringCase(name, "scalex(") || equalIgnoringCase(name, "scaley(") || equalIgnoringCase(name, "scalez(")) {
            m_unit = CSSParser::FNumber;
            if (equalIgnoringCase(name, "scale("))
                m_type = WebKitCSSTransformValue::ScaleTransformOperation;
            else if (equalIgnoringCase(name, "scalex("))
                m_type = WebKitCSSTransformValue::ScaleXTransformOperation;
            else if (equalIgnoringCase(name, "scaley("))
                m_type = WebKitCSSTransformValue::ScaleYTransformOperation;
            else
                m_type = WebKitCSSTransformValue::ScaleZTransformOperation;
        } else if (equalIgnoringCase(name, "scale3d(")) {
            m_type = WebKitCSSTransformValue::Scale3DTransformOperation;
            m_argCount = 5;
            m_unit = CSSParser::FNumber;
        } else if (equalIgnoringCase(name, "rotate(")) {
            m_type = WebKitCSSTransformValue::RotateTransformOperation;
            m_unit = CSSParser::FAngle;
        } else if (equalIgnoringCase(name, "rotatex(") ||
                   equalIgnoringCase(name, "rotatey(") ||
                   equalIgnoringCase(name, "rotatez(")) {
            m_unit = CSSParser::FAngle;
            if (equalIgnoringCase(name, "rotatex("))
                m_type = WebKitCSSTransformValue::RotateXTransformOperation;
            else if (equalIgnoringCase(name, "rotatey("))
                m_type = WebKitCSSTransformValue::RotateYTransformOperation;
            else
                m_type = WebKitCSSTransformValue::RotateZTransformOperation;
        } else if (equalIgnoringCase(name, "rotate3d(")) {
            m_type = WebKitCSSTransformValue::Rotate3DTransformOperation;
            m_argCount = 7;
            m_unit = CSSParser::FNumber;
        } else if (equalIgnoringCase(name, "skew(") || equalIgnoringCase(name, "skewx(") || equalIgnoringCase(name, "skewy(")) {
            m_unit = CSSParser::FAngle;
            if (equalIgnoringCase(name, "skew("))
                m_type = WebKitCSSTransformValue::SkewTransformOperation;
            else if (equalIgnoringCase(name, "skewx("))
                m_type = WebKitCSSTransformValue::SkewXTransformOperation;
            else
                m_type = WebKitCSSTransformValue::SkewYTransformOperation;
        } else if (equalIgnoringCase(name, "translate(") || equalIgnoringCase(name, "translatex(") || equalIgnoringCase(name, "translatey(") || equalIgnoringCase(name, "translatez(")) {
            m_unit = CSSParser::FLength | CSSParser::FPercent;
            if (equalIgnoringCase(name, "translate("))
                m_type = WebKitCSSTransformValue::TranslateTransformOperation;
            else if (equalIgnoringCase(name, "translatex("))
                m_type = WebKitCSSTransformValue::TranslateXTransformOperation;
            else if (equalIgnoringCase(name, "translatey("))
                m_type = WebKitCSSTransformValue::TranslateYTransformOperation;
            else
                m_type = WebKitCSSTransformValue::TranslateZTransformOperation;
        } else if (equalIgnoringCase(name, "translate3d(")) {
            m_type = WebKitCSSTransformValue::Translate3DTransformOperation;
            m_argCount = 5;
            m_unit = CSSParser::FLength | CSSParser::FPercent;
        } else if (equalIgnoringCase(name, "matrix(")) {
            m_type = WebKitCSSTransformValue::MatrixTransformOperation;
            m_argCount = 11;
            m_unit = CSSParser::FNumber;
        } else if (equalIgnoringCase(name, "matrix3d(")) {
            m_type = WebKitCSSTransformValue::Matrix3DTransformOperation;
            m_argCount = 31;
            m_unit = CSSParser::FNumber;
        } else if (equalIgnoringCase(name, "perspective(")) {
            m_type = WebKitCSSTransformValue::PerspectiveTransformOperation;
            m_unit = CSSParser::FNumber;
        }

        if (equalIgnoringCase(name, "scale(") || equalIgnoringCase(name, "skew(") || equalIgnoringCase(name, "translate(")) {
            m_allowSingleArgument = true;
            m_argCount = 3;
        }
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

PassRefPtr<CSSValueList> CSSParser::parseTransform()
{
    if (!m_valueList)
        return 0;

    // The transform is a list of functional primitives that specify transform operations.
    // We collect a list of WebKitCSSTransformValues, where each value specifies a single operation.
    RefPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSParserValue::Function || !value->function)
            return 0;
        
        // Every primitive requires at least one argument.
        CSSParserValueList* args = value->function->args;
        if (!args)
            return 0;
        
        // See if the specified primitive is one we understand.
        TransformOperationInfo info(value->function->name);
        if (info.unknown())
            return 0;
       
        if (!info.hasCorrectArgCount(args->size()))
            return 0;

        // Create the new WebKitCSSTransformValue for this operation and add it to our list.
        RefPtr<WebKitCSSTransformValue> transformValue = WebKitCSSTransformValue::create(info.type());
        list->append(transformValue);

        // Snag our values.
        CSSParserValue* a = args->current();
        unsigned argNumber = 0;
        while (a) {
            CSSParser::Units unit = info.unit();

            // 4th param of rotate3d() is an angle rather than a bare number, validate it as such
            if (info.type() == WebKitCSSTransformValue::Rotate3DTransformOperation && argNumber == 3) {
                if (!validUnit(a, FAngle, true))
                    return 0;
            } else if (!validUnit(a, unit, true))
                return 0;
            
            // Add the value to the current transform operation.
            transformValue->append(CSSPrimitiveValue::create(a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit));

            a = args->next();
            if (!a)
                break;
            if (a->unit != CSSParserValue::Operator || a->iValue != ',')
                return 0;
            a = args->next();
            
            argNumber++;
        }
    }
    
    return list.release();
}

bool CSSParser::parseTransformOrigin(int propId, int& propId1, int& propId2, int& propId3, RefPtr<CSSValue>& value, RefPtr<CSSValue>& value2, RefPtr<CSSValue>& value3)
{
    propId1 = propId;
    propId2 = propId;
    propId3 = propId;
    if (propId == CSSPropertyWebkitTransformOrigin) {
        propId1 = CSSPropertyWebkitTransformOriginX;
        propId2 = CSSPropertyWebkitTransformOriginY;
        propId3 = CSSPropertyWebkitTransformOriginZ;
    }

    switch (propId) {
        case CSSPropertyWebkitTransformOrigin:
            parseTransformOriginShorthand(value, value2, value3);
            // parseTransformOriginShorthand advances the m_valueList pointer
            break;
        case CSSPropertyWebkitTransformOriginX: {
            bool xFound = false, yFound = true;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitTransformOriginY: {
            bool xFound = true, yFound = false;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitTransformOriginZ: {
            if (validUnit(m_valueList->current(), FLength, m_strict))
            value = CSSPrimitiveValue::create(m_valueList->current()->fValue, (CSSPrimitiveValue::UnitTypes)m_valueList->current()->unit);
            if (value)
                m_valueList->next();
            break;
        }
    }
    
    return value;
}

bool CSSParser::parsePerspectiveOrigin(int propId, int& propId1, int& propId2, RefPtr<CSSValue>& value, RefPtr<CSSValue>& value2)
{
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyWebkitPerspectiveOrigin) {
        propId1 = CSSPropertyWebkitPerspectiveOriginX;
        propId2 = CSSPropertyWebkitPerspectiveOriginY;
    }

    switch (propId) {
        case CSSPropertyWebkitPerspectiveOrigin:
            parseFillPosition(value, value2);
            break;
        case CSSPropertyWebkitPerspectiveOriginX: {
            bool xFound = false, yFound = true;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitPerspectiveOriginY: {
            bool xFound = true, yFound = false;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                m_valueList->next();
            break;
        }
    }
    
    return value;
}

static inline int yyerror(const char*) { return 1; }

#define END_TOKEN 0

#include "CSSGrammar.h"

int CSSParser::lex(void* yylvalWithoutType)
{
    YYSTYPE* yylval = static_cast<YYSTYPE*>(yylvalWithoutType);
    int token = lex();
    int length;
    UChar* t = text(&length);

    switch(token) {
    case WHITESPACE:
    case SGML_CD:
    case INCLUDES:
    case DASHMATCH:
        break;

    case URI:
    case STRING:
    case IDENT:
    case NTH:
    case HEX:
    case IDSEL:
    case DIMEN:
    case UNICODERANGE:
    case FUNCTION:
    case NOTFUNCTION:
    case VARCALL:
        yylval->string.characters = t;
        yylval->string.length = length;
        break;

    case IMPORT_SYM:
    case PAGE_SYM:
    case MEDIA_SYM:
    case FONT_FACE_SYM:
    case CHARSET_SYM:
    case NAMESPACE_SYM:
    case WEBKIT_KEYFRAMES_SYM:

    case IMPORTANT_SYM:
        break;

    case QEMS:
        length--;
    case GRADS:
    case TURNS:
        length--;
    case DEGS:
    case RADS:
    case KHERZ:
        length--;
    case MSECS:
    case HERZ:
    case EMS:
    case EXS:
    case PXS:
    case CMS:
    case MMS:
    case INS:
    case PTS:
    case PCS:
        length--;
    case SECS:
    case PERCENTAGE:
        length--;
    case FLOATTOKEN:
    case INTEGER:
        yylval->number = charactersToDouble(t, length);
        break;

    default:
        break;
    }

    return token;
}

static inline int toHex(char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    if ('A' <= c && c<= 'F')
        return c - 'A' + 10;
    return 0;
}

UChar* CSSParser::text(int *length)
{
    UChar* start = yytext;
    int l = yyleng;
    switch(yyTok) {
    case STRING:
        l--;
        /* nobreak */
    case HEX:
    case IDSEL:
        start++;
        l--;
        break;
    case URI:
        // "url("{w}{string}{w}")"
        // "url("{w}{url}{w}")"

        // strip "url(" and ")"
        start += 4;
        l -= 5;
        // strip {w}
        while (l &&
                (*start == ' ' || *start == '\t' || *start == '\r' ||
                 *start == '\n' || *start == '\f')) {
            start++; l--;
        }
        if (*start == '"' || *start == '\'') {
            start++; l--;
        }
        while (l &&
                (start[l-1] == ' ' || start[l-1] == '\t' || start[l-1] == '\r' ||
                 start[l-1] == '\n' || start[l-1] == '\f')) {
            l--;
        }
        if (l && (start[l-1] == '\"' || start[l-1] == '\''))
             l--;

        break;
    case VARCALL:
        // "-webkit-var("{w}{ident}{w}")"
        // strip "-webkit-var(" and ")"
        start += 12;
        l -= 13;
        // strip {w}
        while (l &&
                (*start == ' ' || *start == '\t' || *start == '\r' ||
                 *start == '\n' || *start == '\f')) {
            start++; l--;
        }
        while (l &&
                (start[l-1] == ' ' || start[l-1] == '\t' || start[l-1] == '\r' ||
                 start[l-1] == '\n' || start[l-1] == '\f')) {
            l--;
        }
    default:
        break;
    }

    // process escapes
    UChar* out = start;
    UChar* escape = 0;

    for (int i = 0; i < l; i++) {
        UChar* current = start + i;
        if (escape == current - 1) {
            if ((*current >= '0' && *current <= '9') ||
                 (*current >= 'a' && *current <= 'f') ||
                 (*current >= 'A' && *current <= 'F'))
                continue;
            if (yyTok == STRING &&
                 (*current == '\n' || *current == '\r' || *current == '\f')) {
                // ### handle \r\n case
                if (*current != '\r')
                    escape = 0;
                continue;
            }
            // in all other cases copy the char to output
            // ###
            *out++ = *current;
            escape = 0;
            continue;
        }
        if (escape == current - 2 && yyTok == STRING &&
             *(current-1) == '\r' && *current == '\n') {
            escape = 0;
            continue;
        }
        if (escape > current - 7 &&
             ((*current >= '0' && *current <= '9') ||
               (*current >= 'a' && *current <= 'f') ||
               (*current >= 'A' && *current <= 'F')))
            continue;
        if (escape) {
            // add escaped char
            unsigned uc = 0;
            escape++;
            while (escape < current) {
                uc *= 16;
                uc += toHex(*escape);
                escape++;
            }
            // can't handle chars outside ucs2
            if (uc > 0xffff)
                uc = 0xfffd;
            *out++ = uc;
            escape = 0;
            if (*current == ' ' ||
                 *current == '\t' ||
                 *current == '\r' ||
                 *current == '\n' ||
                 *current == '\f')
                continue;
        }
        if (!escape && *current == '\\') {
            escape = current;
            continue;
        }
        *out++ = *current;
    }
    if (escape) {
        // add escaped char
        unsigned uc = 0;
        escape++;
        while (escape < start+l) {
            uc *= 16;
            uc += toHex(*escape);
            escape++;
        }
        // can't handle chars outside ucs2
        if (uc > 0xffff)
            uc = 0xfffd;
        *out++ = uc;
    }
    
    *length = out - start;
    return start;
}

CSSSelector* CSSParser::createFloatingSelector()
{
    CSSSelector* selector = new CSSSelector;
    m_floatingSelectors.add(selector);
    return selector;
}

CSSSelector* CSSParser::sinkFloatingSelector(CSSSelector* selector)
{
    if (selector) {
        ASSERT(m_floatingSelectors.contains(selector));
        m_floatingSelectors.remove(selector);
    }
    return selector;
}

CSSParserValueList* CSSParser::createFloatingValueList()
{
    CSSParserValueList* list = new CSSParserValueList;
    m_floatingValueLists.add(list);
    return list;
}

CSSParserValueList* CSSParser::sinkFloatingValueList(CSSParserValueList* list)
{
    if (list) {
        ASSERT(m_floatingValueLists.contains(list));
        m_floatingValueLists.remove(list);
    }
    return list;
}

CSSParserFunction* CSSParser::createFloatingFunction()
{
    CSSParserFunction* function = new CSSParserFunction;
    m_floatingFunctions.add(function);
    return function;
}

CSSParserFunction* CSSParser::sinkFloatingFunction(CSSParserFunction* function)
{
    if (function) {
        ASSERT(m_floatingFunctions.contains(function));
        m_floatingFunctions.remove(function);
    }
    return function;
}

CSSParserValue& CSSParser::sinkFloatingValue(CSSParserValue& value)
{
    if (value.unit == CSSParserValue::Function) {
        ASSERT(m_floatingFunctions.contains(value.function));
        m_floatingFunctions.remove(value.function);
    }
    return value;
}

MediaQueryExp* CSSParser::createFloatingMediaQueryExp(const AtomicString& mediaFeature, CSSParserValueList* values)
{
    delete m_floatingMediaQueryExp;
    m_floatingMediaQueryExp = new MediaQueryExp(mediaFeature, values);
    return m_floatingMediaQueryExp;
}

MediaQueryExp* CSSParser::sinkFloatingMediaQueryExp(MediaQueryExp* e)
{
    ASSERT(e == m_floatingMediaQueryExp);
    m_floatingMediaQueryExp = 0;
    return e;
}

Vector<MediaQueryExp*>* CSSParser::createFloatingMediaQueryExpList()
{
    if (m_floatingMediaQueryExpList) {
        deleteAllValues(*m_floatingMediaQueryExpList);
        delete m_floatingMediaQueryExpList;
    }
    m_floatingMediaQueryExpList = new Vector<MediaQueryExp*>;
    return m_floatingMediaQueryExpList;
}

Vector<MediaQueryExp*>* CSSParser::sinkFloatingMediaQueryExpList(Vector<MediaQueryExp*>* l)
{
    ASSERT(l == m_floatingMediaQueryExpList);
    m_floatingMediaQueryExpList = 0;
    return l;
}

MediaQuery* CSSParser::createFloatingMediaQuery(MediaQuery::Restrictor r, const String& mediaType, Vector<MediaQueryExp*>* exprs)
{
    delete m_floatingMediaQuery;
    m_floatingMediaQuery = new MediaQuery(r, mediaType, exprs);
    return m_floatingMediaQuery;
}

MediaQuery* CSSParser::createFloatingMediaQuery(Vector<MediaQueryExp*>* exprs)
{
    return createFloatingMediaQuery(MediaQuery::None, "all", exprs);
}

MediaQuery* CSSParser::sinkFloatingMediaQuery(MediaQuery* mq)
{
    ASSERT(mq == m_floatingMediaQuery);
    m_floatingMediaQuery = 0;
    return mq;
}

MediaList* CSSParser::createMediaList()
{
    RefPtr<MediaList> list = MediaList::create();
    MediaList* result = list.get();
    m_parsedStyleObjects.append(list.release());
    return result;
}

CSSRule* CSSParser::createCharsetRule(const CSSParserString& charset)
{
    if (!m_styleSheet)
        return 0;
    RefPtr<CSSCharsetRule> rule = CSSCharsetRule::create(m_styleSheet, charset);
    CSSCharsetRule* result = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return result;
}

CSSRule* CSSParser::createImportRule(const CSSParserString& url, MediaList* media)
{
    if (!media || !m_styleSheet)
        return 0;
    RefPtr<CSSImportRule> rule = CSSImportRule::create(m_styleSheet, url, media);
    CSSImportRule* result = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return result;
}

CSSRule* CSSParser::createMediaRule(MediaList* media, CSSRuleList* rules)
{
    if (!media || !rules || !m_styleSheet)
        return 0;
    RefPtr<CSSMediaRule> rule = CSSMediaRule::create(m_styleSheet, media, rules);
    CSSMediaRule* result = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return result;
}

CSSRuleList* CSSParser::createRuleList()
{
    RefPtr<CSSRuleList> list = CSSRuleList::create();
    CSSRuleList* listPtr = list.get();
    
    m_parsedRuleLists.append(list.release());
    return listPtr;
}

WebKitCSSKeyframesRule* CSSParser::createKeyframesRule()
{
    RefPtr<WebKitCSSKeyframesRule> rule = WebKitCSSKeyframesRule::create(m_styleSheet);
    WebKitCSSKeyframesRule* rulePtr = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return rulePtr;
}

CSSRule* CSSParser::createStyleRule(Vector<CSSSelector*>* selectors)
{
    CSSStyleRule* result = 0;
    if (selectors) {
        RefPtr<CSSStyleRule> rule = CSSStyleRule::create(m_styleSheet);
        rule->adoptSelectorVector(*selectors);
        if (m_hasFontFaceOnlyValues)
            deleteFontFaceOnlyValues();
        rule->setDeclaration(CSSMutableStyleDeclaration::create(rule.get(), m_parsedProperties, m_numParsedProperties));
        result = rule.get();
        m_parsedStyleObjects.append(rule.release());
    }
    clearProperties();
    return result;
}

CSSRule* CSSParser::createFontFaceRule()
{
    RefPtr<CSSFontFaceRule> rule = CSSFontFaceRule::create(m_styleSheet);
    for (unsigned i = 0; i < m_numParsedProperties; ++i) {
        CSSProperty* property = m_parsedProperties[i];
        int id = property->id();
        if ((id == CSSPropertyFontWeight || id == CSSPropertyFontStyle || id == CSSPropertyFontVariant) && property->value()->isPrimitiveValue()) {
            RefPtr<CSSValue> value = property->m_value.release();
            property->m_value = CSSValueList::createCommaSeparated();
            static_cast<CSSValueList*>(property->m_value.get())->append(value.release());
        }
    }
    rule->setDeclaration(CSSMutableStyleDeclaration::create(rule.get(), m_parsedProperties, m_numParsedProperties));
    clearProperties();
    CSSFontFaceRule* result = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return result;
}

#if !ENABLE(CSS_VARIABLES)

CSSRule* CSSParser::createVariablesRule(MediaList*, bool)
{
    return 0;
}

bool CSSParser::addVariable(const CSSParserString&, CSSParserValueList*)
{
    return false;
}

bool CSSParser::addVariableDeclarationBlock(const CSSParserString&)
{
    return false;
}

#else

CSSRule* CSSParser::createVariablesRule(MediaList* mediaList, bool variablesKeyword)
{
    RefPtr<CSSVariablesRule> rule = CSSVariablesRule::create(m_styleSheet, mediaList, variablesKeyword);
    rule->setDeclaration(CSSVariablesDeclaration::create(rule.get(), m_variableNames, m_variableValues));
    clearVariables();    
    CSSRule* result = rule.get();
    m_parsedStyleObjects.append(rule.release());
    return result;
}

bool CSSParser::addVariable(const CSSParserString& name, CSSParserValueList* valueList)
{
    if (checkForVariables(valueList)) {
        delete valueList;
        return false;
    }
    m_variableNames.append(String(name));
    m_variableValues.append(CSSValueList::createFromParserValueList(valueList));
    return true;
}

bool CSSParser::addVariableDeclarationBlock(const CSSParserString&)
{
// FIXME: Disabling declarations as variable values for now since they no longer have a common base class with CSSValues.
#if 0
    m_variableNames.append(String(name));
    m_variableValues.append(CSSMutableStyleDeclaration::create(0, m_parsedProperties, m_numParsedProperties));
    clearProperties();
#endif
    return true;
}

#endif

void CSSParser::clearVariables()
{
    m_variableNames.clear();
    m_variableValues.clear();
}

bool CSSParser::parseVariable(CSSVariablesDeclaration* declaration, const String& variableName, const String& variableValue)
{
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    String nameValuePair = variableName + ": ";
    nameValuePair += variableValue;

    setupParser("@-webkit-variables-decls{", nameValuePair, "} ");
    cssyyparse(this);
    m_rule = 0;

    bool ok = false;
    if (m_variableNames.size()) {
        ok = true;
        declaration->addParsedVariable(variableName, m_variableValues[0]);
    } 
    
    clearVariables();

    return ok;
}

void CSSParser::parsePropertyWithResolvedVariables(int propId, bool isImportant, CSSMutableStyleDeclaration* declaration, CSSParserValueList* list)
{
    m_valueList = list;
    m_styleSheet = static_cast<CSSStyleSheet*>(declaration->stylesheet());

    if (parseValue(propId, isImportant))
        declaration->addParsedProperties(m_parsedProperties, m_numParsedProperties);
        
    clearProperties();
    m_valueList = 0;
}

bool CSSParser::checkForVariables(CSSParserValueList* valueList)
{
    if (!valueList || !valueList->containsVariables())
        return false;

    bool hasVariables = false;
    for (unsigned i = 0; i < valueList->size(); ++i) {
        if (valueList->valueAt(i)->isVariable()) {
            hasVariables = true;
            break;
        } 
        
        if (valueList->valueAt(i)->unit == CSSParserValue::Function && checkForVariables(valueList->valueAt(i)->function->args)) {
            hasVariables = true;
            break;
        }
    }

    return hasVariables;
}

void CSSParser::addUnresolvedProperty(int propId, bool important)
{
    RefPtr<CSSVariableDependentValue> val = CSSVariableDependentValue::create(CSSValueList::createFromParserValueList(m_valueList));
    addProperty(propId, val.release(), important);
}

void CSSParser::deleteFontFaceOnlyValues()
{
    ASSERT(m_hasFontFaceOnlyValues);
    int deletedProperties = 0;

    for (unsigned i = 0; i < m_numParsedProperties; ++i) {
        CSSProperty* property = m_parsedProperties[i];
        int id = property->id();
        if ((id == CSSPropertyFontWeight || id == CSSPropertyFontStyle || id == CSSPropertyFontVariant) && property->value()->isValueList()) {
            delete property;
            deletedProperties++;
        } else if (deletedProperties)
            m_parsedProperties[i - deletedProperties] = m_parsedProperties[i];
    }

    m_numParsedProperties -= deletedProperties;
}

WebKitCSSKeyframeRule* CSSParser::createKeyframeRule(CSSParserValueList* keys)
{
    // Create a key string from the passed keys
    String keyString;
    for (unsigned i = 0; i < keys->size(); ++i) {
        float key = (float) keys->valueAt(i)->fValue;
        if (i != 0)
            keyString += ",";
        keyString += String::number(key);
        keyString += "%";
    }
    
    RefPtr<WebKitCSSKeyframeRule> keyframe = WebKitCSSKeyframeRule::create(m_styleSheet);
    keyframe->setKeyText(keyString);
    keyframe->setDeclaration(CSSMutableStyleDeclaration::create(0, m_parsedProperties, m_numParsedProperties));
    
    clearProperties();

    WebKitCSSKeyframeRule* keyframePtr = keyframe.get();
    m_parsedStyleObjects.append(keyframe.release());
    return keyframePtr;
}

static int cssPropertyID(const UChar* propertyName, unsigned length)
{
    if (!length)
        return 0;
    if (length > maxCSSPropertyNameLength)
        return 0;

    char buffer[maxCSSPropertyNameLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        UChar c = propertyName[i];
        if (c == 0 || c >= 0x7F)
            return 0; // illegal character
        buffer[i] = toASCIILower(c);
    }
    buffer[length] = '\0';

    const char* name = buffer;
    if (buffer[0] == '-') {
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        if (hasPrefix(buffer, length, "-apple-") || hasPrefix(buffer, length, "-khtml-")) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }

        // Honor -webkit-opacity as a synonym for opacity.
        // This was the only syntax that worked in Safari 1.1, and may be in use on some websites and widgets.
        if (strcmp(buffer, "-webkit-opacity") == 0) {
            const char * const opacity = "opacity";
            name = opacity;
            length = strlen(opacity);
        }
    }

    const props* hashTableEntry = findProp(name, length);
    return hashTableEntry ? hashTableEntry->id : 0;
}

int cssPropertyID(const String& string)
{
    return cssPropertyID(string.characters(), string.length());
}

int cssPropertyID(const CSSParserString& string)
{
    return cssPropertyID(string.characters, string.length);
}

int cssValueKeywordID(const CSSParserString& string)
{
    unsigned length = string.length;
    if (!length)
        return 0;
    if (length > maxCSSValueKeywordLength)
        return 0;

    char buffer[maxCSSValueKeywordLength + 1 + 1]; // 1 to turn "apple"/"khtml" into "webkit", 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        UChar c = string.characters[i];
        if (c == 0 || c >= 0x7F)
            return 0; // illegal character
        buffer[i] = WTF::toASCIILower(c);
    }
    buffer[length] = '\0';

    if (buffer[0] == '-') {
        // If the prefix is -apple- or -khtml-, change it to -webkit-.
        // This makes the string one character longer.
        if (hasPrefix(buffer, length, "-apple-") || hasPrefix(buffer, length, "-khtml-")) {
            memmove(buffer + 7, buffer + 6, length + 1 - 6);
            memcpy(buffer, "-webkit", 7);
            ++length;
        }
    }

    const css_value* hashTableEntry = findValue(buffer, length);
    return hashTableEntry ? hashTableEntry->id : 0;
}

#define YY_DECL int CSSParser::lex()
#define yyconst const
typedef int yy_state_type;
typedef unsigned YY_CHAR;
// The following line makes sure we treat non-Latin-1 Unicode characters correctly.
#define YY_SC_TO_UI(c) (c > 0xff ? 0xff : c)
#define YY_DO_BEFORE_ACTION \
        yytext = yy_bp; \
        yyleng = (int) (yy_cp - yy_bp); \
        yy_hold_char = *yy_cp; \
        *yy_cp = 0; \
        yy_c_buf_p = yy_cp;
#define YY_BREAK break;
#define ECHO
#define YY_RULE_SETUP
#define INITIAL 0
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
#define yyterminate() yyTok = END_TOKEN; return yyTok
#define YY_FATAL_ERROR(a)
// The following line is needed to build the tokenizer with a condition stack.
// The macro is used in the tokenizer grammar with lines containing
// BEGIN(mediaqueries) and BEGIN(initial). yy_start acts as index to
// tokenizer transition table, and 'mediaqueries' and 'initial' are
// offset multipliers that specify which transitions are active
// in the tokenizer during in each condition (tokenizer state).
#define BEGIN yy_start = 1 + 2 *

#include "tokenizer.cpp"

}
