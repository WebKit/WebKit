/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
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
#include "CSSRuleList.h"
#include "CSSSelector.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSTransformValue.h"
#include "CSSUnicodeRangeValue.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "Counter.h"
#include "DashboardRegion.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "MediaList.h"
#include "MediaQueryExp.h"
#include "Pair.h"
#include "ShadowValue.h"
#include <kjs/dtoa.h>

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

static bool equal(const ParseString& a, const char* b)
{
    for (int i = 0; i < a.length; ++i) {
        if (!b[i])
            return false;
        if (a.characters[i] != b[i])
            return false;
    }
    return !b[a.length];
}

static bool equalIgnoringCase(const ParseString& a, const char* b)
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

ValueList::~ValueList()
{
     size_t numValues = m_values.size();
     for (size_t i = 0; i < numValues; i++)
         if (m_values[i].unit == Value::Function)
             delete m_values[i].function;
}

CSSParser* CSSParser::currentParser = 0;

CSSParser::CSSParser(bool strictParsing)
    : m_floatingMediaQuery(0)
    , m_floatingMediaQueryExp(0)
    , m_floatingMediaQueryExpList(0)
{
#ifdef CSS_DEBUG
    kdDebug(6080) << "CSSParser::CSSParser this=" << this << endl;
#endif
    strict = strictParsing;

    parsedProperties = (CSSProperty **)fastMalloc(32 * sizeof(CSSProperty *));
    numParsedProperties = 0;
    maxParsedProperties = 32;

    data = 0;
    valueList = 0;
    id = 0;
    important = false;
    m_inParseShorthand = 0;
    m_currentShorthand = 0;
    m_implicitShorthand = false;

    defaultNamespace = starAtom;
    
    yy_start = 1;

#if YYDEBUG > 0
    cssyydebug = 1;
#endif
}

CSSParser::~CSSParser()
{
    clearProperties();
    fastFree(parsedProperties);

    delete valueList;

    fastFree(data);

    if (m_floatingMediaQueryExpList) {
        deleteAllValues(*m_floatingMediaQueryExpList);
        delete m_floatingMediaQueryExpList;
    }
    delete m_floatingMediaQueryExp;
    delete m_floatingMediaQuery;
    deleteAllValues(m_floatingSelectors);
    deleteAllValues(m_floatingValueLists);
    deleteAllValues(m_floatingFunctions);
}

void ParseString::lower()
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

    if (data)
        fastFree(data);
    
    data = static_cast<UChar*>(fastMalloc(length * sizeof(UChar)));
    for (unsigned i = 0; i < strlen(prefix); i++)
        data[i] = prefix[i];
    
    memcpy(data + strlen(prefix), string.characters(), string.length() * sizeof(UChar));

    unsigned start = strlen(prefix) + string.length();
    unsigned end = start + strlen(suffix);
    for (unsigned i = start; i < end; i++)
        data[i] = suffix[i - start];

    data[length - 1] = 0;
    data[length - 2] = 0;

    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;
}

void CSSParser::parseSheet(CSSStyleSheet* sheet, const String& string)
{
    styleElement = sheet;
    defaultNamespace = starAtom; // Reset the default namespace.
    
    setupParser("", string, "");

    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;

    rule = 0;
}

PassRefPtr<CSSRule> CSSParser::parseRule(CSSStyleSheet *sheet, const String &string)
{
    styleElement = sheet;
    
    setupParser("@-webkit-rule{", string, "} ");

    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;

    return rule.release();
}

bool CSSParser::parseValue(CSSMutableStyleDeclaration *declaration, int _id, const String &string, bool _important)
{
    styleElement = declaration->stylesheet();

    setupParser("@-webkit-value{", string, "} ");

    id = _id;
    important = _important;
    
    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;
    
    rule = 0;

    bool ok = false;
    if (numParsedProperties) {
        ok = true;
        declaration->addParsedProperties(parsedProperties, numParsedProperties);
        clearProperties();
    }

    return ok;
}

// color will only be changed when string contains a valid css color, making it
// possible to set up a default color.
bool CSSParser::parseColor(RGBA32& color, const String &string, bool strict)
{
    color = 0;
    CSSParser parser(true);

    // First try creating a color specified by name or the "#" syntax.
    if (!parser.parseColor(string, color, strict)) {
        RefPtr<CSSMutableStyleDeclaration> dummyStyleDeclaration(new CSSMutableStyleDeclaration);

        // Now try to create a color from the rgb() or rgba() syntax.
        if (parser.parseColor(dummyStyleDeclaration.get(), string)) {
            CSSValue* value = parser.parsedProperties[0]->value();
            if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
                CSSPrimitiveValue *primitiveValue = static_cast<CSSPrimitiveValue *>(value);
                color = primitiveValue->getRGBColorValue();
            }
        } else
            return false;
    }

    return true;
}

bool CSSParser::parseColor(CSSMutableStyleDeclaration *declaration, const String &string)
{
    styleElement = declaration->stylesheet();

    setupParser("@-webkit-decls{color:", string, "} ");

    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;

    rule = 0;

    bool ok = false;
    if (numParsedProperties && parsedProperties[0]->m_id == CSSPropertyColor)
        ok = true;

    return ok;
}

bool CSSParser::parseDeclaration(CSSMutableStyleDeclaration *declaration, const String &string)
{
    styleElement = declaration->stylesheet();

    setupParser("@-webkit-decls{", string, "} ");

    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;

    rule = 0;

    bool ok = false;
    if (numParsedProperties) {
        ok = true;
        declaration->addParsedProperties(parsedProperties, numParsedProperties);
        clearProperties();
    }

    return ok;
}

bool CSSParser::parseMediaQuery(MediaList* queries, const String& string)
{
    if (string.isEmpty() || string.isNull()) {
        return true;
    }

    mediaQuery = 0;
    // can't use { because tokenizer state switches from mediaquery to initial state when it sees { token.
    // instead insert one " " (which is WHITESPACE in CSSGrammar.y)
    setupParser ("@-webkit-mediaquery ", string, "} ");

    CSSParser* old = currentParser;
    currentParser = this;
    cssyyparse(this);
    currentParser = old;

    bool ok = false;
    if (mediaQuery) {
        ok = true;
        queries->appendMediaQuery(mediaQuery);
        mediaQuery = 0;
    }

    return ok;
}


void CSSParser::addProperty(int propId, PassRefPtr<CSSValue> value, bool important)
{
    CSSProperty *prop = new CSSProperty(propId, value, important, m_currentShorthand, m_implicitShorthand);
    if (numParsedProperties >= maxParsedProperties) {
        maxParsedProperties += 32;
        parsedProperties = (CSSProperty **)fastRealloc(parsedProperties,
                                                       maxParsedProperties*sizeof(CSSProperty *));
    }
    parsedProperties[numParsedProperties++] = prop;
}

void CSSParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(numParsedProperties >= num);

    for (int i = 0; i < num; ++i)
        delete parsedProperties[--numParsedProperties];
}

void CSSParser::clearProperties()
{
    for (int i = 0; i < numParsedProperties; i++)
        delete parsedProperties[i];
    numParsedProperties = 0;
}

Document *CSSParser::document() const
{
    StyleBase *root = styleElement;
    Document *doc = 0;
    while (root->parent())
        root = root->parent();
    if (root->isCSSStyleSheet())
        doc = static_cast<CSSStyleSheet*>(root)->doc();
    return doc;
}

bool CSSParser::validUnit(Value* value, Units unitflags, bool strict)
{
    if (unitflags & FNonNeg && value->fValue < 0)
        return false;

    bool b = false;
    switch(value->unit) {
    case CSSPrimitiveValue::CSS_NUMBER:
        b = (unitflags & FNumber);
        if (!b && ((unitflags & (FLength | FAngle)) && (value->fValue == 0 || !strict))) {
            value->unit = (unitflags & FLength) ? CSSPrimitiveValue::CSS_PX : CSSPrimitiveValue::CSS_DEG;
            b = true;
        }
        if (!b && (unitflags & FInteger) && value->isInt)
            b = true;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        b = (unitflags & FPercent);
        break;
    case Value::Q_EMS:
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

static int unitFromString(Value* value)
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
    if (strict || inShorthand())
        return;
        
    // The purpose of this code is to implement the WinIE quirk that allows unit types to be separated from their numeric values
    // by whitespace, so e.g., width: 20 px instead of width:20px.  This is invalid CSS, so we don't do this in strict mode.
    Value* numericVal = 0;
    unsigned size = valueList->size();
    for (unsigned i = 0; i < size; i++) {
        Value* value = valueList->valueAt(i);
        if (numericVal) {
            // Change the unit type of the numeric val to match.
            int unit = unitFromString(value);
            if (unit) {
                numericVal->unit = unit;
                numericVal = 0;

                // Now delete the bogus unit value.
                valueList->deleteValueAt(i);
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
    if (!valueList)
        return false;

    Value *value = valueList->current();

    if (!value)
        return false;

    int id = value->id;

    int num = inShorthand() ? 1 : valueList->size();

    if (id == CSSValueInherit) {
        if (num != 1)
            return false;
        addProperty(propId, new CSSInheritedValue(), important);
        return true;
    }
    else if (id == CSSValueInitial) {
        if (num != 1)
            return false;
        addProperty(propId, new CSSInitialValue(false), important);
        return true;
    }

    bool valid_primitive = false;
    RefPtr<CSSValue> parsedValue;

    // In quirks mode, we will look for units that have been incorrectly separated from the number they belong to
    // by a space.  We go ahead and associate the unit with the number even though it is invalid CSS.
    checkForOrphanedUnits();

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
        break;

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
        else if (value->unit == Value::Function)
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
        CSSValue* value = parsedProperties[numParsedProperties - 1]->value();
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

    case CSSPropertyOutlineStyle:        // <border-style> | auto | inherit
        if (id == CSSValueAuto) {
            valid_primitive = true;
            break;
        } // Fall through!
    case CSSPropertyBorderTopStyle:     //// <border-style> | inherit
    case CSSPropertyBorderRightStyle:   //   Defined as:    none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle:  //   solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitColumnRuleStyle:
        if (id >= CSSValueNone && id <= CSSValueDouble)
            valid_primitive = true;
        break;

    case CSSPropertyFontWeight:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | inherit
        if (id >= CSSValueNormal && id <= CSSValue900) {
            // Valid weight keyword
            valid_primitive = true;
        } else if (validUnit(value, FInteger | FNonNeg, false)) {
            int weight = static_cast<int>(value->fValue);
            if (weight % 100)
                break;
            weight /= 100;
            if (weight >= 1 && weight <= 9) {
                id = CSSValue100 + weight - 1;
                valid_primitive = true;
            }
        }
        break;

    case CSSPropertyBorderSpacing: {
        const int properties[2] = { CSSPropertyWebkitBorderHorizontalSpacing,
                                    CSSPropertyWebkitBorderVerticalSpacing };
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = parsedProperties[numParsedProperties-1]->value();
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
        valid_primitive = validUnit(value, FLength|FNonNeg, strict);
        break;
    case CSSPropertyScrollbarFaceColor:         // IE5.5
    case CSSPropertyScrollbarShadowColor:       // IE5.5
    case CSSPropertyScrollbarHighlightColor:    // IE5.5
    case CSSPropertyScrollbar3dlightColor:      // IE5.5
    case CSSPropertyScrollbarDarkshadowColor:   // IE5.5
    case CSSPropertyScrollbarTrackColor:        // IE5.5
    case CSSPropertyScrollbarArrowColor:        // IE5.5
        if (strict)
            break;
        /* nobreak */
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
        else if (id >= CSSValueAqua && id <= CSSValueWindowtext || id == CSSValueMenu ||
             (id >= CSSValueWebkitFocusRingColor && id < CSSValueWebkitText && !strict)) {
            valid_primitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                valueList->next();
        }
        break;

    case CSSPropertyCursor: {
        // [<uri>,]*  [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
        // nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | ew-resize | 
        // ns-resize | nesw-resize | nwse-resize | col-resize | row-resize | text | wait | help ] ] | inherit
        RefPtr<CSSValueList> list;
        while (value && value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!list)
                list = new CSSValueList; 
            String uri = parseURL(value->string);
            Vector<int> coords;
            value = valueList->next();
            while (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                coords.append(int(value->fValue));
                value = valueList->next();
            }
            IntPoint hotspot;
            int nrcoords = coords.size();
            if (nrcoords > 0 && nrcoords != 2) {
                if (strict) // only support hotspot pairs in strict mode
                    return false;
            } else if(strict && nrcoords == 2)
                hotspot = IntPoint(coords[0], coords[1]);
            if (strict || coords.size() == 0) {
                if (!uri.isEmpty()) {
                    list->append(new CSSCursorImageValue(KURL(styleElement->baseURL(), uri).string(),
                        hotspot, styleElement));
                }
            }
            if ((strict && !value) || (value && !(value->unit == Value::Operator && value->iValue == ',')))
                return false;
            value = valueList->next(); // comma
        }
        if (list) {
            if (!value) { // no value after url list (MSIE 5 compatibility)
                if (list->length() != 1)
                    return false;
            } else if (!strict && value->id == CSSValueHand) // MSIE 5 compatibility :/
                list->append(new CSSPrimitiveValue(CSSValuePointer));
            else if (value && ((value->id >= CSSValueAuto && value->id <= CSSValueAllScroll) || value->id == CSSValueCopy || value->id == CSSValueNone))
                list->append(new CSSPrimitiveValue(value->id));
            valueList->next();
            parsedValue = list.release();
            break;
        }
        id = value->id;
        if (!strict && value->id == CSSValueHand) { // MSIE 5 compatibility :/
            id = CSSValuePointer;
            valid_primitive = true;
        } else if ((value->id >= CSSValueAuto && value->id <= CSSValueAllScroll) || value->id == CSSValueCopy || value->id == CSSValueNone)
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
            parsedValue = new CSSImageValue();
            valueList->next();
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            // ### allow string in non strict mode?
            String uri = parseURL(value->string);
            if (!uri.isEmpty()) {
                parsedValue = new CSSImageValue(KURL(styleElement->baseURL(), uri).string(), styleElement);
                valueList->next();
            }
        } else if (value->unit == Value::Function && equalIgnoringCase(value->function->name, "-webkit-gradient(")) {
            if (parseGradient(parsedValue))
                valueList->next();
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
            valid_primitive = validUnit(value, FLength, strict);
        break;

    case CSSPropertyLetterSpacing:       // normal | <length> | inherit
    case CSSPropertyWordSpacing:         // normal | <length> | inherit
        if (id == CSSValueNormal)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, strict);
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
        valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
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
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, strict));
        break;

    case CSSPropertyFontSize:
        // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
        if (id >= CSSValueXxSmall && id <= CSSValueLarger)
            valid_primitive = true;
        else
            valid_primitive = (validUnit(value, FLength|FPercent, strict));
        break;

    case CSSPropertyFontStyle:           // normal | italic | oblique | inherit
        if (id == CSSValueNormal || id == CSSValueItalic || id == CSSValueOblique)
            valid_primitive = true;
        break;

    case CSSPropertyFontVariant:         // normal | small-caps | inherit
        if (id == CSSValueNormal || id == CSSValueSmallCaps)
            valid_primitive = true;
        break;

    case CSSPropertyVerticalAlign:
        // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if (id >= CSSValueBaseline && id <= CSSValueWebkitBaselineMiddle)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
        break;

    case CSSPropertyHeight:               // <length> | <percentage> | auto | inherit
    case CSSPropertyWidth:                // <length> | <percentage> | auto | inherit
        if (id == CSSValueAuto || id == CSSValueIntrinsic || id == CSSValueMinIntrinsic)
            valid_primitive = true;
        else
            // ### handle multilength case where we allow relative units
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, strict));
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
            valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
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
            valid_primitive = (!id && validUnit(value, FNumber|FLength|FPercent, strict));
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
            RefPtr<CSSValueList> list(new CSSValueList);
            bool is_valid = true;
            while(is_valid && value) {
                switch (value->id) {
                case CSSValueBlink:
                    break;
                case CSSValueUnderline:
                case CSSValueOverline:
                case CSSValueLineThrough:
                    list->append(new CSSPrimitiveValue(value->id));
                    break;
                default:
                    is_valid = false;
                }
                value = valueList->next();
            }
            if (list->length() && is_valid) {
                parsedValue = list.release();
                valueList->next();
            }
        }
        break;

    case CSSPropertyZoom:          // normal | reset | <number> | <percentage> | inherit
        if (id == CSSValueNormal || id == CSSValueReset)
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
            RefPtr<CSSValueList> values(new CSSValueList());
            Value* val;
            RefPtr<CSSValue> parsedValue;
            while ((val = valueList->current())) {
                if (val->unit == CSSPrimitiveValue::CSS_URI) {
                    String value = parseURL(val->string);
                    parsedValue = new CSSPrimitiveValue(KURL(styleElement->baseURL(), value).string(),
                        CSSPrimitiveValue::CSS_URI);
                }
                if (!parsedValue)
                    break;
                
                // FIXME: We can't use release() here since we might hit this path twice
                // but that logic seems wrong to me to begin with, we convert all non-uri values
                // into the last seen URI value!?
                // -webkit-binding: url(foo.xml), 1, 2; (if that were valid) is treated as:
                // -webkit-binding: url(foo.xml), url(foo.xml), url(foo.xml); !?
                values->append(parsedValue.get());
                valueList->next();
            }
            if (!values->length())
                return false;
            
            addProperty(propId, values.release(), important);
            valueList->next();
            return true;
        }
#endif
        break;
    case CSSPropertyWebkitBorderImage:
        if (id == CSSValueNone)
            valid_primitive = true;
        else
            return parseBorderImage(propId, important);
        break;
    case CSSPropertyWebkitBorderTopRightRadius:
    case CSSPropertyWebkitBorderTopLeftRadius:
    case CSSPropertyWebkitBorderBottomLeftRadius:
    case CSSPropertyWebkitBorderBottomRightRadius:
    case CSSPropertyWebkitBorderRadius: {
        if (num != 1 && num != 2)
            return false;
        valid_primitive = validUnit(value, FLength, strict);
        if (!valid_primitive)
            return false;
        RefPtr<CSSPrimitiveValue> parsedValue1 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        RefPtr<CSSPrimitiveValue> parsedValue2;
        if (num == 2) {
            value = valueList->next();
            valid_primitive = validUnit(value, FLength, strict);
            if (!valid_primitive)
                return false;
            parsedValue2 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        } else
            parsedValue2 = parsedValue1;
        
        RefPtr<Pair> pair = new Pair(parsedValue1.release(), parsedValue2.release());
        RefPtr<CSSPrimitiveValue> val = new CSSPrimitiveValue(pair.release());
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
        valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyWebkitBoxShadow:
        if (id == CSSValueNone)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSSPropertyOpacity:
        valid_primitive = validUnit(value, FNumber, strict);
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
        valid_primitive = validUnit(value, FNumber, strict);
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
            valid_primitive = validUnit(value, FLength|FPercent, strict);
        break;
    case CSSPropertyWebkitMarqueeStyle:
        if (id == CSSValueNone || id == CSSValueSlide || id == CSSValueScroll || id == CSSValueAlternate)
            valid_primitive = true;
        break;
    case CSSPropertyWebkitMarqueeRepetition:
        if (id == CSSValueInfinite)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FInteger|FNonNeg, strict);
        break;
    case CSSPropertyWebkitMarqueeSpeed:
        if (id == CSSValueNormal || id == CSSValueSlow || id == CSSValueFast)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FTime|FInteger|FNonNeg, strict);
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
    case CSSPropertyWebkitTransformOriginY: {
        RefPtr<CSSValue> val1;
        RefPtr<CSSValue> val2;
        int propId1, propId2;
        if (parseTransformOrigin(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionRepeatCount:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitTransitionProperty: {
        RefPtr<CSSValue> val;
        if (parseTransitionProperty(propId, val)) {
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
            CSSValue* value = parsedProperties[numParsedProperties-1]->value();
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
            valid_primitive = !id && validUnit(value, FNumber|FLength|FPercent, strict);
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
            valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSSPropertyWebkitColumnWidth:         // auto | <length>
        if (id == CSSValueAuto)
            valid_primitive = true;
        else // Always parse this property in strict mode, since it would be ambiguous otherwise when used in the 'columns' shorthand property.
            valid_primitive = validUnit(value, FLength, true);
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
        valid_primitive = validUnit(value, FLength, strict);
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

    case CSSPropertyWebkitDashboardRegion:                 // <dashboard-region> | <dashboard-region> 
        if (value->unit == Value::Function || id == CSSValueNone)
            return parseDashboardRegions(propId, important);
        break;
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
    case CSSPropertyWebkitTransition:
        return parseTransitionShorthand(important);
    case CSSPropertyInvalid:
        return false;
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThrough:
    case CSSPropertyTextOverline:
    case CSSPropertyTextUnderline:
        return false;
#if ENABLE(SVG)
    default:
        return parseSVGValue(propId, important);
#endif
    }

    if (valid_primitive) {
        if (id != 0)
            parsedValue = new CSSPrimitiveValue(id);
        else if (value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = new CSSPrimitiveValue(value->string, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= Value::Q_EMS)
            parsedValue = new CSSQuirkPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_EMS);
        valueList->next();
    }
    if (parsedValue) {
        if (!valueList->current() || inShorthand()) {
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
            PassRefPtr<CSSValueList> list(new CSSValueList());
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

    while (valueList->current()) {
        Value* val = valueList->current();
        if (val->unit == Value::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSSPropertyBackgroundColor && parsedProperty[i])
                    // Color is not allowed except as the last item in a list for backgrounds.
                    // Reject the entire property.
                    return false;

                if (!parsedProperty[i] && properties[i] != CSSPropertyBackgroundColor) {
                    addFillValue(values[i], new CSSInitialValue(true));
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, new CSSInitialValue(true));
                }
                parsedProperty[i] = false;
            }
            if (!valueList->current())
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
            addFillValue(values[i], new CSSInitialValue(true));
            if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                addFillValue(positionYValue, new CSSInitialValue(true));
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

void CSSParser::addTransitionValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueList*>(lval.get())->append(rval);
        else {
            PassRefPtr<CSSValue> oldVal(lval.release());
            PassRefPtr<CSSValueList> list(new CSSValueList());
            list->append(oldVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

bool CSSParser::parseTransitionShorthand(bool important)
{
    const int properties[] = { CSSPropertyWebkitTransitionProperty, CSSPropertyWebkitTransitionDuration, 
                               CSSPropertyWebkitTransitionTimingFunction, CSSPropertyWebkitTransitionRepeatCount };
    const int numProperties = sizeof(properties) / sizeof(properties[0]);
    
    ShorthandScope scope(this, CSSPropertyWebkitTransition);

    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    RefPtr<CSSValue> values[numProperties];
    
    int i;
    while (valueList->current()) {
        Value* val = valueList->current();
        if (val->unit == Value::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addTransitionValue(values[i], new CSSInitialValue(true));
                parsedProperty[i] = false;
            }
            if (!valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtr<CSSValue> val;
                if (parseTransitionProperty(properties[i], val)) {
                    parsedProperty[i] = found = true;
                    addTransitionValue(values[i], val.release());
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
            addTransitionValue(values[i], new CSSInitialValue(true));
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

    while (valueList->current()) {
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
            addProperty(properties[i], new CSSInitialValue(true), important);
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
    
    int num = inShorthand() ? 1 : valueList->size();
    
    ShorthandScope scope(this, propId);

    // the order is top, right, bottom, left
    switch (num) {
        case 1: {
            if (!parseValue(properties[0], important))
                return false;
            CSSValue *value = parsedProperties[numParsedProperties-1]->value();
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
            CSSValue *value = parsedProperties[numParsedProperties-2]->value();
            m_implicitShorthand = true;
            addProperty(properties[2], value, important);
            value = parsedProperties[numParsedProperties-2]->value();
            addProperty(properties[3], value, important);
            m_implicitShorthand = false;
            break;
        }
        case 3: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) || !parseValue(properties[2], important))
                return false;
            CSSValue *value = parsedProperties[numParsedProperties-2]->value();
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
    RefPtr<CSSValueList> values = new CSSValueList;

    while (Value* val = valueList->current()) {
        RefPtr<CSSValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_URI) {
            // url
            String value = parseURL(val->string);
            parsedValue = new CSSImageValue(KURL(styleElement->baseURL(), value).string(), styleElement);
        } else if (val->unit == Value::Function) {
            // attr(X) | counter(X [,Y]) | counters(X, Y, [,Z]) | -webkit-gradient(...)
            ValueList* args = val->function->args;
            if (!args)
                return false;
            if (equalIgnoringCase(val->function->name, "attr(")) {
                if (args->size() != 1)
                    return false;
                Value* a = args->current();
                String attrName = a->string;
                if (document()->isHTMLDocument())
                    attrName = attrName.lower();
                parsedValue = new CSSPrimitiveValue(attrName, CSSPrimitiveValue::CSS_ATTR);
            } else if (equalIgnoringCase(val->function->name, "counter(")) {
                parsedValue = parseCounterContent(args, false);
                if (!parsedValue) return false;
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
            parsedValue = new CSSPrimitiveValue(val->string, CSSPrimitiveValue::CSS_STRING);
        }
        if (!parsedValue)
            break;
        values->append(parsedValue.release());
        valueList->next();
    }

    if (values->length()) {
        addProperty(propId, values.release(), important);
        valueList->next();
        return true;
    }

    return false;
}

PassRefPtr<CSSValue> CSSParser::parseBackgroundColor()
{
    int id = valueList->current()->id;
    if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu ||
        (id >= CSSValueGrey && id < CSSValueWebkitText && !strict))
       return new CSSPrimitiveValue(id);
    return parseColor();
}

bool CSSParser::parseFillImage(RefPtr<CSSValue>& value)
{
    if (valueList->current()->id == CSSValueNone) {
        value = new CSSImageValue();
        return true;
    }
    if (valueList->current()->unit == CSSPrimitiveValue::CSS_URI) {
        String uri = parseURL(valueList->current()->string);
        if (!uri.isEmpty())
            value = new CSSImageValue(KURL(styleElement->baseURL(), uri).string(), styleElement);
        return true;
    }
    if (valueList->current()->unit == Value::Function) {
        if (equalIgnoringCase(valueList->current()->function->name, "-webkit-gradient("))
            return parseGradient(value);
        if (equalIgnoringCase(valueList->current()->function->name, "-webkit-canvas("))
            return parseCanvas(value);
    }
    return false;
}

PassRefPtr<CSSValue> CSSParser::parseFillPositionXY(bool& xFound, bool& yFound)
{
    int id = valueList->current()->id;
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
        return new CSSPrimitiveValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent|FLength, strict))
        return new CSSPrimitiveValue(valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)valueList->current()->unit);
                
    return 0;
}

void CSSParser::parseFillPosition(RefPtr<CSSValue>& value1, RefPtr<CSSValue>& value2)
{
    Value* value = valueList->current();
    
    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    bool value1IsX = false, value1IsY = false;
    value1 = parseFillPositionXY(value1IsX, value1IsY);
    if (!value1)
        return;
    
    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    value = valueList->next();
    
    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (value && value->unit == Value::Operator && value->iValue == ',')
        value = 0;
    
    bool value2IsX = false, value2IsY = false;
    if (value) {
        value2 = parseFillPositionXY(value2IsX, value2IsY);
        if (value2)
            valueList->next();
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
        value2 = new CSSPrimitiveValue(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    if (value1IsY || value2IsX)
        value1.swap(value2);
}

PassRefPtr<CSSValue> CSSParser::parseFillSize()
{
    Value* value = valueList->current();
    CSSPrimitiveValue* parsedValue1;
    
    if (value->id == CSSValueAuto)
        parsedValue1 = new CSSPrimitiveValue(0, CSSPrimitiveValue::CSS_UNKNOWN);
    else {
        if (!validUnit(value, FLength|FPercent, strict))
            return 0;
        parsedValue1 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    }
    
    CSSPrimitiveValue* parsedValue2 = parsedValue1;
    if ((value = valueList->next())) {
        if (value->id == CSSValueAuto)
            parsedValue2 = new CSSPrimitiveValue(0, CSSPrimitiveValue::CSS_UNKNOWN);
        else {
            if (!validUnit(value, FLength|FPercent, strict)) {
                delete parsedValue1;
                return 0;
            }
            parsedValue2 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        }
    }
    
    Pair* pair = new Pair(parsedValue1, parsedValue2);
    return new CSSPrimitiveValue(pair);
}

bool CSSParser::parseFillProperty(int propId, int& propId1, int& propId2, 
                                  RefPtr<CSSValue>& retValue1, RefPtr<CSSValue>& retValue2)
{
    RefPtr<CSSValueList> values;
    RefPtr<CSSValueList> values2;
    Value* val;
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

    while ((val = valueList->current())) {
        RefPtr<CSSValue> currValue;
        RefPtr<CSSValue> currValue2;
        
        if (allowComma) {
            if (val->unit != Value::Operator || val->iValue != ',')
                return false;
            valueList->next();
            allowComma = false;
        } else {
            switch (propId) {
                case CSSPropertyBackgroundColor:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        valueList->next();
                    break;
                case CSSPropertyBackgroundAttachment:
                case CSSPropertyWebkitMaskAttachment:
                    if (val->id == CSSValueScroll || val->id == CSSValueFixed) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundImage:
                case CSSPropertyWebkitMaskImage:
                    if (parseFillImage(currValue))
                        valueList->next();
                    break;
                case CSSPropertyWebkitBackgroundClip:
                case CSSPropertyWebkitBackgroundOrigin:
                case CSSPropertyWebkitMaskClip:
                case CSSPropertyWebkitMaskOrigin:
                    if (val->id == CSSValueBorder || val->id == CSSValuePadding || val->id == CSSValueContent || val->id == CSSValueText) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundPosition:
                case CSSPropertyWebkitMaskPosition:
                    parseFillPosition(currValue, currValue2);
                    // unlike the other functions, parseFillPosition advances the valueList pointer
                    break;
                case CSSPropertyBackgroundPositionX:
                case CSSPropertyWebkitMaskPositionX: {
                    bool xFound = false, yFound = true;
                    currValue = parseFillPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSSPropertyBackgroundPositionY:
                case CSSPropertyWebkitMaskPositionY: {
                    bool xFound = true, yFound = false;
                    currValue = parseFillPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSSPropertyWebkitBackgroundComposite:
                case CSSPropertyWebkitMaskComposite:
                    if ((val->id >= CSSValueClear && val->id <= CSSValuePlusLighter) || val->id == CSSValueHighlight) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundRepeat:
                case CSSPropertyWebkitMaskRepeat:
                    if (val->id >= CSSValueRepeat && val->id <= CSSValueNoRepeat) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSSPropertyWebkitBackgroundSize:
                case CSSPropertyWebkitMaskSize:
                    currValue = parseFillSize();
                    if (currValue)
                        valueList->next();
                    break;
            }
            if (!currValue)
                return false;
            
            if (value && !values) {
                values = new CSSValueList();
                values->append(value.release());
            }
            
            if (value2 && !values2) {
                values2 = new CSSValueList();
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

PassRefPtr<CSSValue> CSSParser::parseTransitionDuration()
{
    Value* value = valueList->current();
    if (validUnit(value, FTime, strict))
        return new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    return 0;
}

PassRefPtr<CSSValue> CSSParser::parseTransitionRepeatCount()
{
    Value* value = valueList->current();
    if (value->id == CSSValueInfinite)
        return new CSSPrimitiveValue(value->id);
    if (validUnit(value, FInteger|FNonNeg, strict))
        return new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    return 0;
}

bool CSSParser::parseTimingFunctionValue(ValueList*& args, double& result)
{
    Value* v = args->current();
    if (!validUnit(v, FNumber, strict))
        return false;
    result = v->fValue;
    if (result < 0 || result > 1.0)
        return false;
    v = args->next();
    if (!v)
        // The last number in the function has no comma after it, so we're done.
        return true;
    if (v->unit != Value::Operator && v->iValue != ',')
        return false;
    v = args->next();
    return true;
}

PassRefPtr<CSSValue> CSSParser::parseTransitionTimingFunction()
{
    Value* value = valueList->current();
    if (value->id == CSSValueEase || value->id == CSSValueLinear || value->id == CSSValueEaseIn || value->id == CSSValueEaseOut || value->id == CSSValueEaseInOut)
        return new CSSPrimitiveValue(value->id);
    
    // We must be a function.
    if (value->unit != Value::Function)
        return 0;
    
    // The only timing function we accept for now is a cubic bezier function.  4 points must be specified.
    ValueList* args = value->function->args;
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

    return new CSSTimingFunctionValue(x1, y1, x2, y2);
}

PassRefPtr<CSSValue> CSSParser::parseTransitionProperty()
{
    Value* value = valueList->current();
    if (value->unit != CSSPrimitiveValue::CSS_IDENT)
        return 0;
    int result = cssPropertyID(value->string);
    if (result)
        return new CSSPrimitiveValue(result);
    if (equalIgnoringCase(value->string, "all"))
        return new CSSPrimitiveValue(cAnimateAll);
    if (equalIgnoringCase(value->string, "none"))
        return new CSSPrimitiveValue(cAnimateNone);
    return 0;
}

bool CSSParser::parseTransitionProperty(int propId, RefPtr<CSSValue>& result)
{
    RefPtr<CSSValueList> values;
    Value* val;
    RefPtr<CSSValue> value;
    bool allowComma = false;
    
    result = 0;

    while ((val = valueList->current())) {
        RefPtr<CSSValue> currValue;
        if (allowComma) {
            if (val->unit != Value::Operator || val->iValue != ',')
                return false;
            valueList->next();
            allowComma = false;
        }
        else {
            switch (propId) {
                case CSSPropertyWebkitTransitionDuration:
                    currValue = parseTransitionDuration();
                    if (currValue)
                        valueList->next();
                    break;
                case CSSPropertyWebkitTransitionRepeatCount:
                    currValue = parseTransitionRepeatCount();
                    if (currValue)
                        valueList->next();
                    break;
                case CSSPropertyWebkitTransitionTimingFunction:
                    currValue = parseTransitionTimingFunction();
                    if (currValue)
                        valueList->next();
                    break;
                case CSSPropertyWebkitTransitionProperty:
                    currValue = parseTransitionProperty();
                    if (currValue)
                        valueList->next();
                    break;
            }
            
            if (!currValue)
                return false;
            
            if (value && !values) {
                values = new CSSValueList();
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

#define DASHBOARD_REGION_NUM_PARAMETERS  6
#define DASHBOARD_REGION_SHORT_NUM_PARAMETERS  2

static Value *skipCommaInDashboardRegion (ValueList *args)
{
    if (args->size() == (DASHBOARD_REGION_NUM_PARAMETERS*2-1) ||
         args->size() == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
        Value *current = args->current();
        if (current->unit == Value::Operator && current->iValue == ',')
            return args->next();
    }
    return args->current();
}

bool CSSParser::parseDashboardRegions(int propId, bool important)
{
    bool valid = true;
    
    Value *value = valueList->current();

    if (value->id == CSSValueNone) {
        if (valueList->next())
            return false;
        addProperty(propId, new CSSPrimitiveValue(value->id), important);
        return valid;
    }
        
    RefPtr<DashboardRegion> firstRegion = new DashboardRegion;
    DashboardRegion* region = 0;

    while (value) {
        if (region == 0) {
            region = firstRegion.get();
        } else {
            RefPtr<DashboardRegion> nextRegion = new DashboardRegion();
            region->m_next = nextRegion;
            region = nextRegion.get();
        }
        
        if (value->unit != Value::Function) {
            valid = false;
            break;
        }
            
        // Commas count as values, so allow:
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // dashboard-region(label, type, t, r, b, l) or dashboard-region(label type t r b l)
        // also allow
        // dashboard-region(label, type) or dashboard-region(label type)
        // dashboard-region(label, type) or dashboard-region(label type)
        ValueList* args = value->function->args;
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
        Value* arg = args->current();
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
            CSSPrimitiveValue *amount = new CSSPrimitiveValue(CSSValueInvalid);
                
            region->setTop(amount);
            region->setRight(amount);
            region->setBottom(amount);
            region->setLeft(amount);
        }
        else {
            // Next four arguments must be offset numbers
            int i;
            for (i = 0; i < 4; i++) {
                arg = args->next();
                arg = skipCommaInDashboardRegion (args);

                valid = arg->id == CSSValueAuto || validUnit(arg, FLength, strict);
                if (!valid)
                    break;
                    
                CSSPrimitiveValue *amount = arg->id == CSSValueAuto ?
                    new CSSPrimitiveValue(CSSValueAuto) :
                    new CSSPrimitiveValue(arg->fValue, (CSSPrimitiveValue::UnitTypes) arg->unit);
                    
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

        value = valueList->next();
    }

    if (valid)
        addProperty(propId, new CSSPrimitiveValue(firstRegion.release()), important);
        
    return valid;
}

PassRefPtr<CSSValue> CSSParser::parseCounterContent(ValueList* args, bool counters)
{
    unsigned numArgs = args->size();
    if (counters && numArgs != 3 && numArgs != 5)
        return 0;
    if (!counters && numArgs != 1 && numArgs != 3)
        return 0;
    
    Value* i = args->current();
    RefPtr<CSSPrimitiveValue> identifier = new CSSPrimitiveValue(i->string, CSSPrimitiveValue::CSS_STRING);

    RefPtr<CSSPrimitiveValue> separator;
    if (!counters)
        separator = new CSSPrimitiveValue(String(), CSSPrimitiveValue::CSS_STRING);
    else {
        i = args->next();
        if (i->unit != Value::Operator || i->iValue != ',')
            return 0;
        
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_STRING)
            return 0;
        
        separator = new CSSPrimitiveValue(i->string, (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    RefPtr<CSSPrimitiveValue> listStyle;
    i = args->next();
    if (!i) // Make the list style default decimal
        listStyle = new CSSPrimitiveValue(CSSValueDecimal - CSSValueDisc, CSSPrimitiveValue::CSS_NUMBER);
    else {
        if (i->unit != Value::Operator || i->iValue != ',')
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

        listStyle = new CSSPrimitiveValue(ls, (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    return new CSSPrimitiveValue(new Counter(identifier.release(), listStyle.release(), separator.release()));
}

bool CSSParser::parseShape(int propId, bool important)
{
    Value* value = valueList->current();
    ValueList* args = value->function->args;
    if (!equalIgnoringCase(value->function->name, "rect(") || !args)
        return false;

    // rect(t, r, b, l) || rect(t r b l)
    if (args->size() != 4 && args->size() != 7)
        return false;
    Rect *rect = new Rect();
    bool valid = true;
    int i = 0;
    Value *a = args->current();
    while (a) {
        valid = a->id == CSSValueAuto || validUnit(a, FLength, strict);
        if (!valid)
            break;
        CSSPrimitiveValue *length = a->id == CSSValueAuto ?
            new CSSPrimitiveValue(CSSValueAuto) :
            new CSSPrimitiveValue(a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit);
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
            if (a->unit == Value::Operator && a->iValue == ',') {
                a = args->next();
            } else {
                valid = false;
                break;
            }
        }
        i++;
    }
    if (valid) {
        addProperty(propId, new CSSPrimitiveValue(rect), important);
        valueList->next();
        return true;
    }
    delete rect;
    return false;
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSParser::parseFont(bool important)
{
    bool valid = true;
    Value *value = valueList->current();
    RefPtr<FontValue> font = new FontValue;
    // optional font-style, font-variant and font-weight
    while (value) {
        int id = value->id;
        if (id) {
            if (id == CSSValueNormal) {
                // do nothing, it's the inital value for all three
            } else if (id == CSSValueItalic || id == CSSValueOblique) {
                if (font->style)
                    return false;
                font->style = new CSSPrimitiveValue(id);
            } else if (id == CSSValueSmallCaps) {
                if (font->variant)
                    return false;
                font->variant = new CSSPrimitiveValue(id);
            } else if (id >= CSSValueBold && id <= CSSValueLighter) {
                if (font->weight)
                    return false;
                font->weight = new CSSPrimitiveValue(id);
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
                font->weight = new CSSPrimitiveValue(val);
            else
                valid = false;
        } else {
            valid = false;
        }
        if (!valid)
            break;
        value = valueList->next();
    }
    if (!value)
        return false;

    // set undefined values to default
    if (!font->style)
        font->style = new CSSPrimitiveValue(CSSValueNormal);
    if (!font->variant)
        font->variant = new CSSPrimitiveValue(CSSValueNormal);
    if (!font->weight)
        font->weight = new CSSPrimitiveValue(CSSValueNormal);

    // now a font size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (value->id >= CSSValueXxSmall && value->id <= CSSValueLarger)
        font->size = new CSSPrimitiveValue(value->id);
    else if (validUnit(value, FLength|FPercent, strict))
        font->size = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
    value = valueList->next();
    if (!font->size || !value)
        return false;

    if (value->unit == Value::Operator && value->iValue == '/') {
        // line-height
        value = valueList->next();
        if (!value)
            return false;
        if (value->id == CSSValueNormal) {
            // default value, nothing to do
        } else if (validUnit(value, FNumber|FLength|FPercent, strict))
            font->lineHeight = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else
            return false;
        value = valueList->next();
        if (!value)
            return false;
    }
    
    if (!font->lineHeight)
        font->lineHeight = new CSSPrimitiveValue(CSSValueNormal);

    // font family must come now
    font->family = parseFontFamily();

    if (valueList->current() || !font->family)
        return false;

    addProperty(CSSPropertyFont, font.release(), important);
    return true;
}

PassRefPtr<CSSValueList> CSSParser::parseFontFamily()
{
    CSSValueList* list = new CSSValueList;
    Value* value = valueList->current();
    FontFamilyValue* currFamily = 0;
    while (value) {
        Value* nextValue = valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == Value::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSSValueSerif && nextValue->id <= CSSValueWebkitBody) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id >= CSSValueSerif && value->id <= CSSValueWebkitBody) {
            if (currFamily)
                currFamily->appendSpaceSeparated(value->string.characters, value->string.length);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new CSSPrimitiveValue(value->id));
            else
                list->append(currFamily = new FontFamilyValue(value->string));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFamily = 0;
            list->append(new FontFamilyValue(value->string));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (currFamily)
                currFamily->appendSpaceSeparated(value->string.characters, value->string.length);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new FontFamilyValue(value->string));
            else
                list->append(currFamily = new FontFamilyValue(value->string));
        }
        else {
            break;
        }
        
        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = valueList->next();
            currFamily = 0;
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }
    if (!list->length()) {
        delete list;
        list = 0;
    }
    return list;
}

bool CSSParser::parseFontFaceSrc()
{
    RefPtr<CSSValueList> values(new CSSValueList());
    Value* val;
    bool expectComma = false;
    bool allowFormat = false;
    bool failed = false;
    RefPtr<CSSFontFaceSrcValue> uriValue;
    while ((val = valueList->current())) {
        RefPtr<CSSFontFaceSrcValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_URI && !expectComma) {
            String value = parseURL(val->string);
            parsedValue = new CSSFontFaceSrcValue(KURL(styleElement->baseURL(), value).string(), false);
            uriValue = parsedValue;
            allowFormat = true;
            expectComma = true;
        } else if (val->unit == Value::Function) {
            // There are two allowed functions: local() and format().             
            ValueList* args = val->function->args;
            if (args && args->size() == 1) {
                if (equalIgnoringCase(val->function->name, "local(") && !expectComma) {
                    expectComma = true;
                    allowFormat = false;
                    Value* a = args->current();
                    uriValue.clear();
                    parsedValue = new CSSFontFaceSrcValue(a->string, true);
                } else if (equalIgnoringCase(val->function->name, "format(") && allowFormat && uriValue) {
                    expectComma = true;
                    allowFormat = false;
                    uriValue->setFormat(args->current()->string);
                    uriValue.clear();
                    valueList->next();
                    continue;
                }
            }
        } else if (val->unit == Value::Operator && val->iValue == ',' && expectComma) {
            expectComma = false;
            allowFormat = false;
            uriValue.clear();
            valueList->next();
            continue;
        }
    
        if (parsedValue)
            values->append(parsedValue.release());
        else {
            failed = true;
            break;
        }
        valueList->next();
    }
    
    if (values->length() && !failed) {
        addProperty(CSSPropertySrc, values.release(), important);
        valueList->next();
        return true;
    }

    return false;
}

bool CSSParser::parseFontFaceUnicodeRange()
{
    CSSValueList* values = new CSSValueList();
    Value* currentValue;
    bool failed = false;
    while ((currentValue = valueList->current())) {
        if (valueList->current()->unit != CSSPrimitiveValue::CSS_UNICODE_RANGE) {
            failed = true;
            break;
        }

        String rangeString = valueList->current()->string;
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
        values->append(new CSSUnicodeRangeValue(from, to));
        valueList->next();
    }
    if (failed || !values->length()) {
        delete values;
        return false;
    }
    addProperty(CSSPropertyUnicodeRange, values, important);
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

bool CSSParser::parseColorParameters(Value* value, int* colorArray, bool parseAlpha)
{
    ValueList* args = value->function->args;
    Value* v = args->current();
    // Get the first value
    if (!validUnit(v, FInteger | FPercent, true))
        return false;
    colorArray[0] = static_cast<int>(v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256.0 / 100.0 : 1.0));
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FInteger | FPercent, true))
            return false;
        colorArray[i] = static_cast<int>(v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256.0 / 100.0 : 1.0));
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = static_cast<int>(max(0.0, min(1.0, v->fValue)) * 255);
    }
    return true;
}

// CSS3 sepcification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSParser::parseHSLParameters(Value* value, double* colorArray, bool parseAlpha)
{
    ValueList* args = value->function->args;
    Value* v = args->current();
    // Get the first value
    if (!validUnit(v, FNumber, true))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(v->fValue) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FPercent, true))
            return false;
        colorArray[i] = max(0.0, min(100.0, v->fValue)) / 100.0; // needs to be value between 0 and 1.0
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != Value::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, true))
            return false;
        colorArray[3] = max(0.0, min(1.0, v->fValue));
    }
    return true;
}

PassRefPtr<CSSPrimitiveValue> CSSParser::parseColor(Value* value)
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(value ? value : valueList->current(), c))
        return 0;
    return new CSSPrimitiveValue(c);
}

bool CSSParser::parseColorFromValue(Value* value, RGBA32& c, bool svg)
{
    if (!strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
        value->fValue >= 0. && value->fValue < 1000000.) {
        String str = String::format("%06d", (int)(value->fValue+.5));
        if (!CSSParser::parseColor(str, c, strict))
            return false;
    } else if (value->unit == CSSPrimitiveValue::CSS_RGBCOLOR ||
                value->unit == CSSPrimitiveValue::CSS_IDENT ||
                (!strict && value->unit == CSSPrimitiveValue::CSS_DIMENSION)) {
        if (!CSSParser::parseColor(value->string, c, strict && value->unit == CSSPrimitiveValue::CSS_IDENT))
            return false;
    } else if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 5 /* rgb + two commas */ &&
                equalIgnoringCase(value->function->name, "rgb(")) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        c = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else if (!svg) {
        if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* rgba + three commas */ &&
                equalIgnoringCase(value->function->name, "rgba(")) {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            c = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value->unit == Value::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 5 /* hsl + two commas */ &&
                    equalIgnoringCase(value->function->name, "hsl(")) {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value->unit == Value::Function &&
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
                values = new CSSValueList();
            
            // Construct the current shadow value and add it to the list.
            values->append(new ShadowValue(x.release(), y.release(), blur.release(), color.release()));
        }
        
        // Now reset for the next shadow value.
        x = y = blur = color = 0;
        allowX = allowColor = allowBreak = true;
        allowY = allowBlur = false;  
    }

    void commitLength(Value* v) {
        RefPtr<CSSPrimitiveValue> val = new CSSPrimitiveValue(v->fValue, (CSSPrimitiveValue::UnitTypes)v->unit);

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
    Value* val;
    while ((val = valueList->current())) {
        // Check for a comma break first.
        if (val->unit == Value::Operator) {
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
            bool isColor = (val->id >= CSSValueAqua && val->id <= CSSValueWindowtext || val->id == CSSValueMenu ||
                            (val->id >= CSSValueWebkitFocusRingColor && val->id <= CSSValueWebkitText && !strict));
            if (isColor) {
                if (!context.allowColor)
                    return false;
                parsedColor = new CSSPrimitiveValue(val->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColor(val);

            if (!parsedColor || !context.allowColor)
                return false; // This value is not a color or length and is invalid or
                              // it is a color, but a color isn't allowed at this point.
            
            context.commitColor(parsedColor.release());
        }
        
        valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values->length()) {
            addProperty(propId, context.values.release(), important);
            valueList->next();
            return true;
        }
    }
    
    return false;
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
    void commitNumber(Value* v) {
        PassRefPtr<CSSPrimitiveValue> val = new CSSPrimitiveValue(v->fValue, (CSSPrimitiveValue::UnitTypes)v->unit);
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
    void commitWidth(Value* val) {
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
    void commitBorderImage(CSSParser* p, int propId, bool important) {
        // We need to clone and repeat values for any omissions.
        if (!m_right) {
            m_right = new CSSPrimitiveValue(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_bottom = new CSSPrimitiveValue(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_left = new CSSPrimitiveValue(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
        }
        if (!m_bottom) {
            m_bottom = new CSSPrimitiveValue(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
            m_left = new CSSPrimitiveValue(m_right->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_right->primitiveType());
        }
        if (!m_left)
             m_left = new CSSPrimitiveValue(m_top->getDoubleValue(), (CSSPrimitiveValue::UnitTypes)m_top->primitiveType());
             
        // Now build a rect value to hold all four of our primitive values.
        RefPtr<Rect> rect = new Rect;
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

        // Make our new border image value now and add it as the result.
        CSSBorderImageValue* borderImage = new CSSBorderImageValue(m_image, rect.release(), m_horizontalRule, m_verticalRule);
        p->addProperty(propId, borderImage, important);
            
        // Now we have to deal with the border widths.  The best way to deal with these is to actually put these values into a value
        // list and then make our parsing machinery do the parsing.
        if (m_borderTop) {
            ValueList newList;
            newList.addValue(*m_borderTop);
            if (m_borderRight)
                newList.addValue(*m_borderRight);
            if (m_borderBottom)
                newList.addValue(*m_borderBottom);
            if (m_borderLeft)
                newList.addValue(*m_borderLeft);
            p->valueList = &newList;
            p->parseValue(CSSPropertyBorderWidth, important);
            p->valueList = 0;
        }
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
    
    Value* m_borderTop;
    Value* m_borderRight;
    Value* m_borderBottom;
    Value* m_borderLeft;
    
    int m_horizontalRule;
    int m_verticalRule;
};

bool CSSParser::parseBorderImage(int propId, bool important)
{
    // Look for an image initially.  If the first value is not a URI, then we're done.
    BorderImageParseContext context;
    Value* val = valueList->current();
    if (val->unit == CSSPrimitiveValue::CSS_URI) {        
        String uri = parseURL(val->string);
        if (uri.isEmpty())
            return false;
        context.commitImage(new CSSImageValue(KURL(styleElement->baseURL(), uri).string(), styleElement));
    } else if (val->unit == Value::Function) {
        RefPtr<CSSValue> value;
        if ((equalIgnoringCase(val->function->name, "-webkit-gradient(") && parseGradient(value)) ||
            (equalIgnoringCase(val->function->name, "-webkit-canvas(") && parseCanvas(value)))
            context.commitImage(value);
        else
            return false;
    } else
        return false;

    while ((val = valueList->next())) {
        if (context.allowNumber() && validUnit(val, FInteger|FNonNeg|FPercent, true)) {
            context.commitNumber(val);
        } else if (context.allowSlash() && val->unit == Value::Operator && val->iValue == '/') {
            context.commitSlash();
        } else if (context.allowWidth() &&
            (val->id == CSSValueThin || val->id == CSSValueMedium || val->id == CSSValueThick || validUnit(val, FLength, strict))) {
            context.commitWidth(val);
        } else if (context.allowRule() &&
            (val->id == CSSValueStretch || val->id == CSSValueRound || val->id == CSSValueRepeat)) {
            context.commitRule(val->id);
        } else {
            // Something invalid was encountered.
            return false;
        }
    }
    
    if (context.allowBreak()) {
        // Need to fully commit as a single value.
        context.commitBorderImage(this, propId, important);
        return true;
    }
    
    return false;
}

bool CSSParser::parseCounter(int propId, int defaultValue, bool important)
{
    enum { ID, VAL } state = ID;

    RefPtr<CSSValueList> list = new CSSValueList;
    RefPtr<CSSPrimitiveValue> counterName;
    
    while (true) {
        Value* val = valueList->current();
        switch (state) {
            case ID:
                if (val && val->unit == CSSPrimitiveValue::CSS_IDENT) {
                    counterName = new CSSPrimitiveValue(val->string, CSSPrimitiveValue::CSS_STRING);
                    state = VAL;
                    valueList->next();
                    continue;
                }
                break;
            case VAL: {
                int i = defaultValue;
                if (val && val->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    i = (int)val->fValue;
                    valueList->next();
                }

                list->append(new CSSPrimitiveValue(new Pair(counterName.release(),
                    new CSSPrimitiveValue(i, CSSPrimitiveValue::CSS_NUMBER))));
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

static PassRefPtr<CSSPrimitiveValue> parseGradientPoint(Value* a, bool horizontal)
{
    RefPtr<CSSPrimitiveValue> result;
    if (a->unit == CSSPrimitiveValue::CSS_IDENT) {
        if ((equalIgnoringCase(a->string, "left") && horizontal) || 
            (equalIgnoringCase(a->string, "top") && !horizontal))
            result = new CSSPrimitiveValue(0., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if ((equalIgnoringCase(a->string, "right") && horizontal) ||
                 (equalIgnoringCase(a->string, "bottom") && !horizontal))
            result = new CSSPrimitiveValue(100., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if (equalIgnoringCase(a->string, "center"))
            result = new CSSPrimitiveValue(50., CSSPrimitiveValue::CSS_PERCENTAGE);
    } else if (a->unit == CSSPrimitiveValue::CSS_NUMBER || a->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        result = new CSSPrimitiveValue(a->fValue, (CSSPrimitiveValue::UnitTypes)a->unit);
    return result;
}

bool parseGradientColorStop(CSSParser* p, Value* a, CSSGradientColorStop& stop)
{
    if (a->unit != Value::Function)
        return false;
    
    if (!equalIgnoringCase(a->function->name, "from(") &&
        !equalIgnoringCase(a->function->name, "to(") &&
        !equalIgnoringCase(a->function->name, "color-stop("))
        return false;
    
    ValueList* args = a->function->args;
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
        
        stop.m_color = p->parseColor(args->current());
        if (!stop.m_color)
            return false;
    }
    
    // The "color-stop" function expects 3 arguments.
    if (equalIgnoringCase(a->function->name, "color-stop(")) {
        if (args->size() != 3)
            return false;
        
        Value* stopArg = args->current();
        if (stopArg->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
            stop.m_stop = (float)stopArg->fValue / 100.f;
        else if (stopArg->unit == CSSPrimitiveValue::CSS_NUMBER)
            stop.m_stop = (float)stopArg->fValue;
        else
            return false;

        stopArg = args->next();
        if (stopArg->unit != Value::Operator || stopArg->iValue != ',')
            return false;
            
        stopArg = args->next();
        stop.m_color = p->parseColor(stopArg);
        if (!stop.m_color)
            return false;
    }

    return true;
}

bool CSSParser::parseGradient(RefPtr<CSSValue>& gradient)
{
    RefPtr<CSSGradientValue> result = new CSSGradientValue;
    
    // Walk the arguments.
    ValueList* args = valueList->current()->function->args;
    if (!args || args->size() == 0)
        return false;
    
    // The first argument is the gradient type.  It is an identifier.
    Value* a = args->current();
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
    if (!a || a->unit != Value::Operator || a->iValue != ',')
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
    if (!a || a->unit != Value::Operator || a->iValue != ',')
        return false;
            
    // For radial gradients only, we now expect a numeric radius.
    if (result->type() == CSSRadialGradient) {
        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        result->setFirstRadius(new CSSPrimitiveValue(a->fValue, CSSPrimitiveValue::CSS_NUMBER));
        
        // Comma after the first radius.
        a = args->next();
        if (!a || a->unit != Value::Operator || a->iValue != ',')
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
        if (!a || a->unit != Value::Operator || a->iValue != ',')
            return false;
        
        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        result->setSecondRadius(new CSSPrimitiveValue(a->fValue, CSSPrimitiveValue::CSS_NUMBER));
    }

    // We now will accept any number of stops (0 or more).
    a = args->next();
    while (a) {
        // Look for the comma before the next stop.
        if (a->unit != Value::Operator || a->iValue != ',')
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
    
    gradient = result;
    return true;
}

bool CSSParser::parseCanvas(RefPtr<CSSValue>& canvas)
{
    RefPtr<CSSCanvasValue> result = new CSSCanvasValue;
    
    // Walk the arguments.
    ValueList* args = valueList->current()->function->args;
    if (!args || args->size() != 1)
        return false;
    
    // The first argument is the canvas name.  It is an identifier.
    Value* a = args->current();
    if (!a || a->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;
    result->setName(a->string);
    canvas = result;
    return true;
}

class TransformOperationInfo {
public:
    TransformOperationInfo(const ParseString& name)
    : m_type(CSSTransformValue::UnknownTransformOperation)
    , m_argCount(1)
    , m_allowSingleArgument(false)
    , m_unit(CSSParser::FUnknown)
    {
        if (equalIgnoringCase(name, "scale(") || equalIgnoringCase(name, "scalex(") || equalIgnoringCase(name, "scaley(")) {
            m_unit = CSSParser::FNumber;
            if (equalIgnoringCase(name, "scale("))
                m_type = CSSTransformValue::ScaleTransformOperation;
            else if (equalIgnoringCase(name, "scalex("))
                m_type = CSSTransformValue::ScaleXTransformOperation;
            else
                m_type = CSSTransformValue::ScaleYTransformOperation;
        } else if (equalIgnoringCase(name, "rotate(")) {
            m_type = CSSTransformValue::RotateTransformOperation;
            m_unit = CSSParser::FAngle;
        } else if (equalIgnoringCase(name, "skew(") || equalIgnoringCase(name, "skewx(") || equalIgnoringCase(name, "skewy(")) {
            m_unit = CSSParser::FAngle;
            if (equalIgnoringCase(name, "skew("))
                m_type = CSSTransformValue::SkewTransformOperation;
            else if (equalIgnoringCase(name, "skewx("))
                m_type = CSSTransformValue::SkewXTransformOperation;
            else
                m_type = CSSTransformValue::SkewYTransformOperation;
        } else if (equalIgnoringCase(name, "translate(") || equalIgnoringCase(name, "translatex(") || equalIgnoringCase(name, "translatey(")) {
            m_unit = CSSParser::FLength | CSSParser::FPercent;
            if (equalIgnoringCase(name, "translate("))
                m_type = CSSTransformValue::TranslateTransformOperation;
            else if (equalIgnoringCase(name, "translatex("))
                m_type = CSSTransformValue::TranslateXTransformOperation;
            else
                m_type = CSSTransformValue::TranslateYTransformOperation;
        } else if (equalIgnoringCase(name, "matrix(")) {
            m_type = CSSTransformValue::MatrixTransformOperation;
            m_argCount = 11;
            m_unit = CSSParser::FNumber;
        }
        
        if (equalIgnoringCase(name, "scale(") || equalIgnoringCase(name, "skew(") || equalIgnoringCase(name, "translate(")) {
            m_allowSingleArgument = true;
            m_argCount = 3;
        }
    }
    
    CSSTransformValue::TransformOperationType type() const { return m_type; }
    unsigned argCount() const { return m_argCount; }
    CSSParser::Units unit() const { return m_unit; }

    bool unknown() const { return m_type == CSSTransformValue::UnknownTransformOperation; }
    bool hasCorrectArgCount(unsigned argCount) { return m_argCount == argCount || (m_allowSingleArgument && argCount == 1); }

private:
    CSSTransformValue::TransformOperationType m_type;
    unsigned m_argCount;
    bool m_allowSingleArgument;
    CSSParser::Units m_unit;
};

PassRefPtr<CSSValue> CSSParser::parseTransform()
{
    if (!valueList)
        return 0;

    // The transform is a list of functional primitives that specify transform operations.  We collect a list
    // of CSSTransformValues, where each value specifies a single operation.
    RefPtr<CSSValueList> list = new CSSValueList;
    for (Value* value = valueList->current(); value; value = valueList->next()) {
        if (value->unit != Value::Function || !value->function)
            return 0;
        
        // Every primitive requires at least one argument.
        ValueList* args = value->function->args;
        if (!args)
            return 0;
        
        // See if the specified primitive is one we understand.
        TransformOperationInfo info(value->function->name);
        if (info.unknown())
            return 0;
       
        if (!info.hasCorrectArgCount(args->size()))
            return 0;

        // Create the new CSSTransformValue for this operation and add it to our list.
        CSSTransformValue* transformValue = new CSSTransformValue(info.type());
        list->append(transformValue);

        // Snag our values.
        Value* a = args->current();
        unsigned argNumber = 0;
        while (a) {
            CSSParser::Units unit = info.unit();

            if (!validUnit(a, unit, true))
                return 0;
            
            // Add the value to the current transform operation.
            transformValue->addValue(new CSSPrimitiveValue(a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit));

            a = args->next();
            if (!a)
                break;
            if (a->unit != Value::Operator || a->iValue != ',')
                return 0;
            a = args->next();
            
            argNumber++;
        }
    }
    
    return list.release();
}

bool CSSParser::parseTransformOrigin(int propId, int& propId1, int& propId2, RefPtr<CSSValue>& value, RefPtr<CSSValue>& value2)
{
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyWebkitTransformOrigin) {
        propId1 = CSSPropertyWebkitTransformOriginX;
        propId2 = CSSPropertyWebkitTransformOriginY;
    }

    switch (propId) {
        case CSSPropertyWebkitTransformOrigin:
            parseFillPosition(value, value2);
            // Unlike the other functions, parseFillPosition advances the valueList pointer
            break;
        case CSSPropertyWebkitTransformOriginX: {
            bool xFound = false, yFound = true;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                valueList->next();
            break;
        }
        case CSSPropertyWebkitTransformOriginY: {
            bool xFound = true, yFound = false;
            value = parseFillPositionXY(xFound, yFound);
            if (value)
                valueList->next();
            break;
        }
    }
    
    return value;
}

#ifdef CSS_DEBUG

static inline int yyerror(const char *str)
{
    kdDebug(6080) << "CSS parse error " << str << endl;
    return 1;
}

#else

static inline int yyerror(const char*) { return 1; }

#endif

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
        yylval->string.characters = t;
        yylval->string.length = length;
        break;

    case IMPORT_SYM:
    case PAGE_SYM:
    case MEDIA_SYM:
    case FONT_FACE_SYM:
    case CHARSET_SYM:
    case NAMESPACE_SYM:

    case IMPORTANT_SYM:
        break;

    case QEMS:
        length--;
    case GRADS:
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

ValueList* CSSParser::createFloatingValueList()
{
    ValueList* list = new ValueList;
    m_floatingValueLists.add(list);
    return list;
}

ValueList* CSSParser::sinkFloatingValueList(ValueList* list)
{
    if (list) {
        ASSERT(m_floatingValueLists.contains(list));
        m_floatingValueLists.remove(list);
    }
    return list;
}

Function* CSSParser::createFloatingFunction()
{
    Function* function = new Function;
    m_floatingFunctions.add(function);
    return function;
}

Function* CSSParser::sinkFloatingFunction(Function* function)
{
    if (function) {
        ASSERT(m_floatingFunctions.contains(function));
        m_floatingFunctions.remove(function);
    }
    return function;
}

Value& CSSParser::sinkFloatingValue(Value& value)
{
    if (value.unit == Value::Function) {
        ASSERT(m_floatingFunctions.contains(value.function));
        m_floatingFunctions.remove(value.function);
    }
    return value;
}

MediaQueryExp* CSSParser::createFloatingMediaQueryExp(const AtomicString& mediaFeature, ValueList* values)
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
    MediaList* list = new MediaList;
    m_parsedStyleObjects.append(list);
    return list;
}

CSSRule* CSSParser::createCharsetRule(const ParseString& charset)
{
    if (!styleElement)
        return 0;
    if (!styleElement->isCSSStyleSheet())
        return 0;
    CSSCharsetRule* rule = new CSSCharsetRule(styleElement, charset);
    m_parsedStyleObjects.append(rule);
    return rule;
}

CSSRule* CSSParser::createImportRule(const ParseString& url, MediaList* media)
{
    if (!media)
        return 0;
    if (!styleElement)
        return 0;
    if (!styleElement->isCSSStyleSheet())
        return 0;
    CSSImportRule* rule = new CSSImportRule(styleElement, url, media);
    m_parsedStyleObjects.append(rule);
    return rule;
}

CSSRule* CSSParser::createMediaRule(MediaList* media, CSSRuleList* rules)
{
    if (!media)
        return 0;
    if (!rules)
        return 0;
    if (!styleElement)
        return 0;
    if (!styleElement->isCSSStyleSheet())
        return 0;
    CSSMediaRule* rule = new CSSMediaRule(styleElement, media, rules);
    m_parsedStyleObjects.append(rule);
    return rule;
}

CSSRuleList* CSSParser::createRuleList()
{
    CSSRuleList* list = new CSSRuleList;
    m_parsedRuleLists.append(list);
    return list;
}

CSSRule* CSSParser::createStyleRule(CSSSelector* selector)
{
    CSSStyleRule* rule = 0;
    if (selector) {
        rule = new CSSStyleRule(styleElement);
        m_parsedStyleObjects.append(rule);
        rule->setSelector(sinkFloatingSelector(selector));
        rule->setDeclaration(new CSSMutableStyleDeclaration(rule, parsedProperties, numParsedProperties));
    }
    clearProperties();
    return rule;
}

CSSRule* CSSParser::createFontFaceRule()
{
    CSSFontFaceRule* rule = new CSSFontFaceRule(styleElement);
    m_parsedStyleObjects.append(rule);
    rule->setDeclaration(new CSSMutableStyleDeclaration(rule, parsedProperties, numParsedProperties));
    clearProperties();
    return rule;
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

int cssPropertyID(const ParseString& string)
{
    return cssPropertyID(string.characters, string.length);
}

int cssValueKeywordID(const ParseString& string)
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
