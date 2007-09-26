/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "CSSBorderImageValue.h"
#include "CSSCharsetRule.h"
#include "CSSCursorImageValue.h"
#include "CSSHelper.h"
#include "CSSImageValue.h"
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
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "Counter.h"
#include "DashboardRegion.h"
#include "Document.h"
#include "FontFamilyValue.h"
#include "FontValue.h"
#include "KURL.h"
#include "MediaList.h"
#include "MediaQueryExp.h"
#include "Pair.h"
#include "ShadowValue.h"

#define YYDEBUG 0

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse(void* parser);

using namespace std;
using namespace WTF;

namespace WebCore {

ValueList::~ValueList()
{
     size_t numValues = m_values.size();
     for (size_t i = 0; i < numValues; i++)
         if (m_values[i].unit == Value::Function)
             delete m_values[i].function;
}

namespace {
    class ShorthandScope {
    public:
        ShorthandScope(CSSParser* parser, int propId) : m_parser(parser)
        {
            if (!(m_parser->m_inParseShorthand++))
                m_parser->m_currentShorthand = propId;
        }
        ~ShorthandScope()
        {
            if (!(--m_parser->m_inParseShorthand))
                m_parser->m_currentShorthand = 0;
        }

    private:
        CSSParser* m_parser;
    };
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
    // Fast case for all-ASCII.
    UChar ored = 0;
    for (int i = 0; i < length; i++)
        ored |= characters[i];
    if (ored & ~0x7F)
        for (int i = 0; i < length; i++)
            characters[i] = Unicode::toLower(characters[i]);
    else
        for (int i = 0; i < length; i++)
            characters[i] = tolower(characters[i]);
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

RGBA32 CSSParser::parseColor(const String &string, bool strict)
{
    RGBA32 color = 0;
    RefPtr<CSSMutableStyleDeclaration>dummyStyleDeclaration = new CSSMutableStyleDeclaration;

    CSSParser parser(true);

    // First try creating a color specified by name or the "#" syntax.
    if (!parser.parseColor(string, color, strict)) {
    
        // Now try to create a color from the rgb() or rgba() syntax.
        if (parser.parseColor(dummyStyleDeclaration.get(), string)) {
            CSSValue* value = parser.parsedProperties[0]->value();
            if (value->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE) {
                CSSPrimitiveValue *primitiveValue = static_cast<CSSPrimitiveValue *>(value);
                color = primitiveValue->getRGBColorValue();
            }
        }
    }
    
    return color;
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
    if (numParsedProperties && parsedProperties[0]->m_id == CSS_PROP_COLOR)
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
        if (!b && ((unitflags & FLength) && (value->fValue == 0 || !strict))) {
            value->unit = CSSPrimitiveValue::CSS_PX;
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

    String str = domString(value->string);
    if (str == "em")
        return CSSPrimitiveValue::CSS_EMS;
    if (str == "ex")
        return CSSPrimitiveValue::CSS_EXS;
    if (str == "px")
        return CSSPrimitiveValue::CSS_PX;
    if (str == "cm")
        return CSSPrimitiveValue::CSS_CM;
    if (str == "mm")
        return CSSPrimitiveValue::CSS_MM;
    if (str == "in")
        return CSSPrimitiveValue::CSS_IN;
    if (str == "pt")
        return CSSPrimitiveValue::CSS_PT;
    if (str == "pc")
        return CSSPrimitiveValue::CSS_PC;
    if (str == "deg")
        return CSSPrimitiveValue::CSS_DEG;
    if (str == "rad")
        return CSSPrimitiveValue::CSS_RAD;
    if (str == "grad")
        return CSSPrimitiveValue::CSS_GRAD;
    if (str == "ms")
        return CSSPrimitiveValue::CSS_MS;
    if (str == "s")
        return CSSPrimitiveValue::CSS_S;
    if (str == "Hz")
        return CSSPrimitiveValue::CSS_HZ;
    if (str == "kHz")
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

    if (id == CSS_VAL_INHERIT) {
        if (num != 1)
            return false;
        addProperty(propId, new CSSInheritedValue(), important);
        return true;
    }
    else if (id == CSS_VAL_INITIAL) {
        if (num != 1)
            return false;
        addProperty(propId, new CSSInitialValue(false), important);
        return true;
    }

    bool valid_primitive = false;
    CSSValue *parsedValue = 0;

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

    case CSS_PROP_SIZE:                 // <length>{1,2} | auto | portrait | landscape | inherit
    case CSS_PROP_QUOTES:               // [<string> <string>]+ | none | inherit
        if (id)
            valid_primitive = true;
        break;
    case CSS_PROP_UNICODE_BIDI:         // normal | embed | bidi-override | inherit
        if (id == CSS_VAL_NORMAL ||
             id == CSS_VAL_EMBED ||
             id == CSS_VAL_BIDI_OVERRIDE)
            valid_primitive = true;
        break;

    case CSS_PROP_POSITION:             // static | relative | absolute | fixed | inherit
        if (id == CSS_VAL_STATIC ||
             id == CSS_VAL_RELATIVE ||
             id == CSS_VAL_ABSOLUTE ||
             id == CSS_VAL_FIXED)
            valid_primitive = true;
        break;

    case CSS_PROP_PAGE_BREAK_AFTER:     // auto | always | avoid | left | right | inherit
    case CSS_PROP_PAGE_BREAK_BEFORE:
    case CSS_PROP__WEBKIT_COLUMN_BREAK_AFTER:
    case CSS_PROP__WEBKIT_COLUMN_BREAK_BEFORE:
        if (id == CSS_VAL_AUTO ||
             id == CSS_VAL_ALWAYS ||
             id == CSS_VAL_AVOID ||
             id == CSS_VAL_LEFT ||
             id == CSS_VAL_RIGHT)
            valid_primitive = true;
        break;

    case CSS_PROP_PAGE_BREAK_INSIDE:    // avoid | auto | inherit
    case CSS_PROP__WEBKIT_COLUMN_BREAK_INSIDE:
        if (id == CSS_VAL_AUTO || id == CSS_VAL_AVOID)
            valid_primitive = true;
        break;

    case CSS_PROP_EMPTY_CELLS:          // show | hide | inherit
        if (id == CSS_VAL_SHOW ||
             id == CSS_VAL_HIDE)
            valid_primitive = true;
        break;

    case CSS_PROP_CONTENT:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
        // close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent(propId, important);
        break;

    case CSS_PROP_WHITE_SPACE:          // normal | pre | nowrap | inherit
        if (id == CSS_VAL_NORMAL ||
            id == CSS_VAL_PRE ||
            id == CSS_VAL_PRE_WRAP ||
            id == CSS_VAL_PRE_LINE ||
            id == CSS_VAL_NOWRAP)
            valid_primitive = true;
        break;

    case CSS_PROP_CLIP:                 // <shape> | auto | inherit
        if (id == CSS_VAL_AUTO)
            valid_primitive = true;
        else if (value->unit == Value::Function)
            return parseShape(propId, important);
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShorthand to work
     * correctly and allows optimization in WebCore::applyRule(..)
     */
    case CSS_PROP_CAPTION_SIDE:         // top | bottom | left | right | inherit
        if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
            id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM)
            valid_primitive = true;
        break;

    case CSS_PROP_BORDER_COLLAPSE:      // collapse | separate | inherit
        if (id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE)
            valid_primitive = true;
        break;

    case CSS_PROP_VISIBILITY:           // visible | hidden | collapse | inherit
        if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_COLLAPSE)
            valid_primitive = true;
        break;

    case CSS_PROP_OVERFLOW: {
        ShorthandScope scope(this, propId);
        if (num != 1 || !parseValue(CSS_PROP_OVERFLOW_X, important))
            return false;
        CSSValue* value = parsedProperties[numParsedProperties - 1]->value();
        addProperty(CSS_PROP_OVERFLOW_Y, value, important);
        return true;
    }
    case CSS_PROP_OVERFLOW_X:
    case CSS_PROP_OVERFLOW_Y:           // visible | hidden | scroll | auto | marquee | overlay | inherit
        if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_SCROLL || id == CSS_VAL_AUTO ||
            id == CSS_VAL_OVERLAY || id == CSS_VAL__WEBKIT_MARQUEE)
            valid_primitive = true;
        break;

    case CSS_PROP_LIST_STYLE_POSITION:  // inside | outside | inherit
        if (id == CSS_VAL_INSIDE || id == CSS_VAL_OUTSIDE)
            valid_primitive = true;
        break;

    case CSS_PROP_LIST_STYLE_TYPE:
        // disc | circle | square | decimal | decimal-leading-zero | lower-roman |
        // upper-roman | lower-greek | lower-alpha | lower-latin | upper-alpha |
        // upper-latin | hebrew | armenian | georgian | cjk-ideographic | hiragana |
        // katakana | hiragana-iroha | katakana-iroha | none | inherit
        if ((id >= CSS_VAL_DISC && id <= CSS_VAL_KATAKANA_IROHA) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_DISPLAY:
        // inline | block | list-item | run-in | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | box | inline-box | none | inherit
        if ((id >= CSS_VAL_INLINE && id <= CSS_VAL__WEBKIT_INLINE_BOX) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_DIRECTION:            // ltr | rtl | inherit
        if (id == CSS_VAL_LTR || id == CSS_VAL_RTL)
            valid_primitive = true;
        break;

    case CSS_PROP_TEXT_TRANSFORM:       // capitalize | uppercase | lowercase | none | inherit
        if ((id >= CSS_VAL_CAPITALIZE && id <= CSS_VAL_LOWERCASE) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP_FLOAT:                // left | right | none | inherit + center for buggy CSS
        if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
             id == CSS_VAL_NONE || id == CSS_VAL_CENTER)
            valid_primitive = true;
        break;

    case CSS_PROP_CLEAR:                // none | left | right | both | inherit
        if (id == CSS_VAL_NONE || id == CSS_VAL_LEFT ||
             id == CSS_VAL_RIGHT|| id == CSS_VAL_BOTH)
            valid_primitive = true;
        break;

    case CSS_PROP_TEXT_ALIGN:
        // left | right | center | justify | webkit_left | webkit_right | webkit_center | <string> | inherit
        if ((id >= CSS_VAL__WEBKIT_AUTO && id <= CSS_VAL__WEBKIT_CENTER) ||
             value->unit == CSSPrimitiveValue::CSS_STRING)
            valid_primitive = true;
        break;

    case CSS_PROP_OUTLINE_STYLE:        // <border-style> | auto | inherit
        if (id == CSS_VAL_AUTO) {
            valid_primitive = true;
            break;
        } // Fall through!
    case CSS_PROP_BORDER_TOP_STYLE:     //// <border-style> | inherit
    case CSS_PROP_BORDER_RIGHT_STYLE:   //   Defined as:    none | hidden | dotted | dashed |
    case CSS_PROP_BORDER_BOTTOM_STYLE:  //   solid | double | groove | ridge | inset | outset
    case CSS_PROP_BORDER_LEFT_STYLE:
    case CSS_PROP__WEBKIT_COLUMN_RULE_STYLE:
        if (id >= CSS_VAL_NONE && id <= CSS_VAL_DOUBLE)
            valid_primitive = true;
        break;

    case CSS_PROP_FONT_WEIGHT:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 |
        // 500 | 600 | 700 | 800 | 900 | inherit
        if (id >= CSS_VAL_NORMAL && id <= CSS_VAL_900) {
            // Allready correct id
            valid_primitive = true;
        } else if (validUnit(value, FInteger|FNonNeg, false)) {
            int weight = (int)value->fValue;
            if ((weight % 100))
                break;
            weight /= 100;
            if (weight >= 1 && weight <= 9) {
                id = CSS_VAL_100 + weight - 1;
                valid_primitive = true;
            }
        }
        break;

    case CSS_PROP_BORDER_SPACING: {
        const int properties[2] = { CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING,
                                    CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING };
        if (num == 1) {
            ShorthandScope scope(this, CSS_PROP_BORDER_SPACING);
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = parsedProperties[numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSS_PROP_BORDER_SPACING);
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSS_PROP__WEBKIT_BORDER_HORIZONTAL_SPACING:
    case CSS_PROP__WEBKIT_BORDER_VERTICAL_SPACING:
        valid_primitive = validUnit(value, FLength|FNonNeg, strict);
        break;
    case CSS_PROP_SCROLLBAR_FACE_COLOR:         // IE5.5
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:       // IE5.5
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:    // IE5.5
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:      // IE5.5
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:   // IE5.5
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:        // IE5.5
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:        // IE5.5
        if (strict)
            break;
        /* nobreak */
    case CSS_PROP_OUTLINE_COLOR:        // <color> | invert | inherit
        // Outline color has "invert" as additional keyword.
        // Also, we want to allow the special focus color even in strict parsing mode.
        if (propId == CSS_PROP_OUTLINE_COLOR && (id == CSS_VAL_INVERT || id == CSS_VAL__WEBKIT_FOCUS_RING_COLOR)) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSS_PROP_BACKGROUND_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_TOP_COLOR:     // <color> | inherit
    case CSS_PROP_BORDER_RIGHT_COLOR:   // <color> | inherit
    case CSS_PROP_BORDER_BOTTOM_COLOR:  // <color> | inherit
    case CSS_PROP_BORDER_LEFT_COLOR:    // <color> | inherit
    case CSS_PROP_COLOR:                // <color> | inherit
    case CSS_PROP_TEXT_LINE_THROUGH_COLOR: // CSS3 text decoration colors
    case CSS_PROP_TEXT_UNDERLINE_COLOR:
    case CSS_PROP_TEXT_OVERLINE_COLOR:
    case CSS_PROP__WEBKIT_COLUMN_RULE_COLOR:
    case CSS_PROP__WEBKIT_TEXT_FILL_COLOR:
    case CSS_PROP__WEBKIT_TEXT_STROKE_COLOR:
        if (id == CSS_VAL__WEBKIT_TEXT)
            valid_primitive = true; // Always allow this, even when strict parsing is on,
                                    // since we use this in our UA sheets.
        else if (id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT || id == CSS_VAL_MENU ||
             (id >= CSS_VAL__WEBKIT_FOCUS_RING_COLOR && id < CSS_VAL__WEBKIT_TEXT && !strict)) {
            valid_primitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                valueList->next();
        }
        break;

    case CSS_PROP_CURSOR: {
        // [<uri>,]*  [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
        // nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | ew-resize | 
        // ns-resize | nesw-resize | nwse-resize | col-resize | row-resize | text | wait | help ] ] | inherit
        CSSValueList* list = 0;
        while (value && value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!list)
                list = new CSSValueList; 
            String uri = parseURL(domString(value->string));
            Vector<int> coords;
            value = valueList->next();
            while (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                coords.append(int(value->fValue));
                value = valueList->next();
            }
            IntPoint hotspot;
            int nrcoords = coords.size();
            if (nrcoords > 0 && nrcoords != 2) {
                if (strict) { // only support hotspot pairs in strict mode
                    delete list;
                    return false;
                }
            } else if(strict && nrcoords == 2)
                hotspot = IntPoint(coords[0], coords[1]);
            if (strict || coords.size() == 0) {
#if ENABLE(SVG)
                if (uri.startsWith("#"))
                    list->append(new CSSPrimitiveValue(uri, CSSPrimitiveValue::CSS_URI));
                else
#endif
                if (!uri.isEmpty()) {
                    list->append(new CSSCursorImageValue(
                                 String(KURL(styleElement->baseURL().deprecatedString(), uri.deprecatedString()).url()),
                                 hotspot, styleElement));
                }
            }
            if ((strict && !value) || (value && !(value->unit == Value::Operator && value->iValue == ','))) {
                delete list;
                return false;
            }
            value = valueList->next(); // comma
        }
        if (list) {
            if (!value) { // no value after url list (MSIE 5 compatibility)
                if (list->length() != 1) {
                    delete list;
                    return false;
                }
            } else if (!strict && value->id == CSS_VAL_HAND) // MSIE 5 compatibility :/
                list->append(new CSSPrimitiveValue(CSS_VAL_POINTER));
            else if (value && ((value->id >= CSS_VAL_AUTO && value->id <= CSS_VAL_ALL_SCROLL) || value->id == CSS_VAL_COPY || value->id == CSS_VAL_NONE))
                list->append(new CSSPrimitiveValue(value->id));
            valueList->next();
            parsedValue = list;
            break;
        }
        id = value->id;
        if (!strict && value->id == CSS_VAL_HAND) { // MSIE 5 compatibility :/
            id = CSS_VAL_POINTER;
            valid_primitive = true;
        } else if ((value->id >= CSS_VAL_AUTO && value->id <= CSS_VAL_ALL_SCROLL) || value->id == CSS_VAL_COPY || value->id == CSS_VAL_NONE)
            valid_primitive = true;
        break;
    }

    case CSS_PROP_BACKGROUND_ATTACHMENT:
    case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
    case CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE:
    case CSS_PROP_BACKGROUND_IMAGE:
    case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN:
    case CSS_PROP_BACKGROUND_POSITION:
    case CSS_PROP_BACKGROUND_POSITION_X:
    case CSS_PROP_BACKGROUND_POSITION_Y:
    case CSS_PROP__WEBKIT_BACKGROUND_SIZE:
    case CSS_PROP_BACKGROUND_REPEAT: {
        CSSValue *val1 = 0, *val2 = 0;
        int propId1, propId2;
        if (parseBackgroundProperty(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1, important);
            if (val2)
                addProperty(propId2, val2, important);
            return true;
        }
        return false;
    }
    case CSS_PROP_LIST_STYLE_IMAGE:     // <uri> | none | inherit
        if (id == CSS_VAL_NONE) {
            parsedValue = new CSSImageValue();
            valueList->next();
        }
        else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            // ### allow string in non strict mode?
            String uri = parseURL(domString(value->string));
            if (!uri.isEmpty()) {
                parsedValue = new CSSImageValue(
                    String(KURL(styleElement->baseURL().deprecatedString(), uri.deprecatedString()).url()),
                    styleElement);
                valueList->next();
            }
        }
        break;

    case CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH:
    case CSS_PROP_OUTLINE_WIDTH:        // <border-width> | inherit
    case CSS_PROP_BORDER_TOP_WIDTH:     //// <border-width> | inherit
    case CSS_PROP_BORDER_RIGHT_WIDTH:   //   Which is defined as
    case CSS_PROP_BORDER_BOTTOM_WIDTH:  //   thin | medium | thick | <length>
    case CSS_PROP_BORDER_LEFT_WIDTH:
    case CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH:
        if (id == CSS_VAL_THIN || id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, strict);
        break;

    case CSS_PROP_LETTER_SPACING:       // normal | <length> | inherit
    case CSS_PROP_WORD_SPACING:         // normal | <length> | inherit
        if (id == CSS_VAL_NORMAL)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, strict);
        break;

    case CSS_PROP_WORD_BREAK:          // normal | break-all | break-word (this is a custom extension)
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_BREAK_ALL || id == CSS_VAL_BREAK_WORD)
            valid_primitive = true;
        break;

    case CSS_PROP_WORD_WRAP:           // normal | break-word
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_BREAK_WORD)
            valid_primitive = true;
        break;

    case CSS_PROP_TEXT_INDENT:          // <length> | <percentage> | inherit
    case CSS_PROP_PADDING_TOP:          //// <padding-width> | inherit
    case CSS_PROP_PADDING_RIGHT:        //   Which is defined as
    case CSS_PROP_PADDING_BOTTOM:       //   <length> | <percentage>
    case CSS_PROP_PADDING_LEFT:         ////
    case CSS_PROP__WEBKIT_PADDING_START:
        valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
        break;

    case CSS_PROP_MAX_HEIGHT:           // <length> | <percentage> | none | inherit
    case CSS_PROP_MAX_WIDTH:            // <length> | <percentage> | none | inherit
        if (id == CSS_VAL_NONE || id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSS_PROP_MIN_HEIGHT:           // <length> | <percentage> | inherit
    case CSS_PROP_MIN_WIDTH:            // <length> | <percentage> | inherit
        if (id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, strict));
        break;

    case CSS_PROP_FONT_SIZE:
        // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
        if (id >= CSS_VAL_XX_SMALL && id <= CSS_VAL_LARGER)
            valid_primitive = true;
        else
            valid_primitive = (validUnit(value, FLength|FPercent, strict));
        break;

    case CSS_PROP_FONT_STYLE:           // normal | italic | oblique | inherit
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE)
            valid_primitive = true;
        break;

    case CSS_PROP_FONT_VARIANT:         // normal | small-caps | inherit
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS)
            valid_primitive = true;
        break;

    case CSS_PROP_VERTICAL_ALIGN:
        // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if (id >= CSS_VAL_BASELINE && id <= CSS_VAL__WEBKIT_BASELINE_MIDDLE)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
        break;

    case CSS_PROP_HEIGHT:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_WIDTH:                // <length> | <percentage> | auto | inherit
        if (id == CSS_VAL_AUTO || id == CSS_VAL_INTRINSIC || id == CSS_VAL_MIN_INTRINSIC)
            valid_primitive = true;
        else
            // ### handle multilength case where we allow relative units
            valid_primitive = (!id && validUnit(value, FLength|FPercent|FNonNeg, strict));
        break;

    case CSS_PROP_BOTTOM:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_LEFT:                 // <length> | <percentage> | auto | inherit
    case CSS_PROP_RIGHT:                // <length> | <percentage> | auto | inherit
    case CSS_PROP_TOP:                  // <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_TOP:           //// <margin-width> | inherit
    case CSS_PROP_MARGIN_RIGHT:         //   Which is defined as
    case CSS_PROP_MARGIN_BOTTOM:        //   <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_LEFT:          ////
    case CSS_PROP__WEBKIT_MARGIN_START:
        if (id == CSS_VAL_AUTO)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FLength|FPercent, strict));
        break;

    case CSS_PROP_Z_INDEX:              // auto | <integer> | inherit
        if (id == CSS_VAL_AUTO) {
            valid_primitive = true;
            break;
        }
        /* nobreak */
    case CSS_PROP_ORPHANS:              // <integer> | inherit
    case CSS_PROP_WIDOWS:               // <integer> | inherit
        // ### not supported later on
        valid_primitive = (!id && validUnit(value, FInteger, false));
        break;

    case CSS_PROP_LINE_HEIGHT:          // normal | <number> | <length> | <percentage> | inherit
        if (id == CSS_VAL_NORMAL)
            valid_primitive = true;
        else
            valid_primitive = (!id && validUnit(value, FNumber|FLength|FPercent, strict));
        break;
    case CSS_PROP_COUNTER_INCREMENT:    // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSS_VAL_NONE)
            return parseCounter(propId, 1, important);
        valid_primitive = true;
        break;
     case CSS_PROP_COUNTER_RESET:        // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSS_VAL_NONE)
            return parseCounter(propId, 0, important);
        valid_primitive = true;
        break;
    case CSS_PROP_FONT_FAMILY:
        // [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
        parsedValue = parseFontFamily();
        break;
    }

    case CSS_PROP_TEXT_DECORATION:
    case CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT:
        // none | [ underline || overline || line-through || blink ] | inherit
        if (id == CSS_VAL_NONE) {
            valid_primitive = true;
        } else {
            CSSValueList *list = new CSSValueList;
            bool is_valid = true;
            while(is_valid && value) {
                switch (value->id) {
                case CSS_VAL_BLINK:
                    break;
                case CSS_VAL_UNDERLINE:
                case CSS_VAL_OVERLINE:
                case CSS_VAL_LINE_THROUGH:
                    list->append(new CSSPrimitiveValue(value->id));
                    break;
                default:
                    is_valid = false;
                }
                value = valueList->next();
            }
            if(list->length() && is_valid) {
                parsedValue = list;
                valueList->next();
            } else
                delete list;
        }
        break;

    case CSS_PROP_TABLE_LAYOUT:         // auto | fixed | inherit
        if (id == CSS_VAL_AUTO || id == CSS_VAL_FIXED)
            valid_primitive = true;
        break;

    /* CSS3 properties */
    case CSS_PROP__WEBKIT_APPEARANCE:
        if ((id >= CSS_VAL_CHECKBOX && id <= CSS_VAL_TEXTAREA) || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP__WEBKIT_BINDING:
#if ENABLE(XBL)
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else {
            CSSValueList* values = new CSSValueList();
            Value* val;
            CSSValue* parsedValue = 0;
            while ((val = valueList->current())) {
                if (val->unit == CSSPrimitiveValue::CSS_URI) {
                    String value = parseURL(domString(val->string));
                    parsedValue = new CSSPrimitiveValue(
                                    String(KURL(styleElement->baseURL().deprecatedString(), value.deprecatedString()).url()), 
                                    CSSPrimitiveValue::CSS_URI);
                } 
                
                if (parsedValue)
                    values->append(parsedValue);
                else
                    break;
                valueList->next();
            }
            if (values->length()) {
                addProperty(propId, values, important);
                valueList->next();
                return true;
            }
            delete values;
            return false;
        }
#endif
        break;
    case CSS_PROP__WEBKIT_BORDER_IMAGE:
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseBorderImage(propId, important);
        break;
    case CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS:
    case CSS_PROP__WEBKIT_BORDER_RADIUS: {
        if (num != 1 && num != 2)
            return false;
        valid_primitive = validUnit(value, FLength, strict);
        if (!valid_primitive)
            return false;
        CSSPrimitiveValue* parsedValue1 = new CSSPrimitiveValue(value->fValue,
                                                                        (CSSPrimitiveValue::UnitTypes)value->unit);
        CSSPrimitiveValue* parsedValue2 = parsedValue1;
        if (num == 2) {
            value = valueList->next();
            valid_primitive = validUnit(value, FLength, strict);
            if (!valid_primitive) {
                delete parsedValue1;
                return false;
            }
            parsedValue2 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
        }
        
        Pair* pair = new Pair(parsedValue1, parsedValue2);
        CSSPrimitiveValue* val = new CSSPrimitiveValue(pair);
        if (propId == CSS_PROP__WEBKIT_BORDER_RADIUS) {
            const int properties[4] = { CSS_PROP__WEBKIT_BORDER_TOP_RIGHT_RADIUS,
                                        CSS_PROP__WEBKIT_BORDER_TOP_LEFT_RADIUS,
                                        CSS_PROP__WEBKIT_BORDER_BOTTOM_LEFT_RADIUS,
                                        CSS_PROP__WEBKIT_BORDER_BOTTOM_RIGHT_RADIUS };
            for (int i = 0; i < 4; i++)
                addProperty(properties[i], val, important);
        } else
            addProperty(propId, val, important);
        return true;
    }
    case CSS_PROP_OUTLINE_OFFSET:
        valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSS_PROP_TEXT_SHADOW: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSS_PROP__WEBKIT_BOX_SHADOW:
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSS_PROP_OPACITY:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__WEBKIT_BOX_ALIGN:
        if (id == CSS_VAL_STRETCH || id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_BASELINE)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_BOX_DIRECTION:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_REVERSE)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_BOX_LINES:
        if (id == CSS_VAL_SINGLE || id == CSS_VAL_MULTIPLE)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_BOX_ORIENT:
        if (id == CSS_VAL_HORIZONTAL || id == CSS_VAL_VERTICAL ||
            id == CSS_VAL_INLINE_AXIS || id == CSS_VAL_BLOCK_AXIS)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_BOX_PACK:
        if (id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_JUSTIFY)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_BOX_FLEX:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__WEBKIT_BOX_FLEX_GROUP:
    case CSS_PROP__WEBKIT_BOX_ORDINAL_GROUP:
        valid_primitive = validUnit(value, FInteger|FNonNeg, true);
        break;
    case CSS_PROP__WEBKIT_BOX_SIZING:
        valid_primitive = id == CSS_VAL_BORDER_BOX || id == CSS_VAL_CONTENT_BOX;
        break;
    case CSS_PROP__WEBKIT_MARQUEE: {
        const int properties[5] = { CSS_PROP__WEBKIT_MARQUEE_DIRECTION, CSS_PROP__WEBKIT_MARQUEE_INCREMENT,
                                    CSS_PROP__WEBKIT_MARQUEE_REPETITION,
                                    CSS_PROP__WEBKIT_MARQUEE_STYLE, CSS_PROP__WEBKIT_MARQUEE_SPEED };
        return parseShorthand(propId, properties, 5, important);
    }
    case CSS_PROP__WEBKIT_MARQUEE_DIRECTION:
        if (id == CSS_VAL_FORWARDS || id == CSS_VAL_BACKWARDS || id == CSS_VAL_AHEAD ||
            id == CSS_VAL_REVERSE || id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT || id == CSS_VAL_DOWN ||
            id == CSS_VAL_UP || id == CSS_VAL_AUTO)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_MARQUEE_INCREMENT:
        if (id == CSS_VAL_SMALL || id == CSS_VAL_LARGE || id == CSS_VAL_MEDIUM)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength|FPercent, strict);
        break;
    case CSS_PROP__WEBKIT_MARQUEE_STYLE:
        if (id == CSS_VAL_NONE || id == CSS_VAL_SLIDE || id == CSS_VAL_SCROLL || id == CSS_VAL_ALTERNATE)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_MARQUEE_REPETITION:
        if (id == CSS_VAL_INFINITE)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FInteger|FNonNeg, strict);
        break;
    case CSS_PROP__WEBKIT_MARQUEE_SPEED:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_SLOW || id == CSS_VAL_FAST)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FTime|FInteger|FNonNeg, strict);
        break;
    case CSS_PROP__WEBKIT_USER_DRAG: // auto | none | element
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE || id == CSS_VAL_ELEMENT)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_USER_MODIFY: // read-only | read-write
        if (id == CSS_VAL_READ_ONLY || id == CSS_VAL_READ_WRITE || id == CSS_VAL_READ_WRITE_PLAINTEXT_ONLY)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_USER_SELECT: // auto | none | text
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE || id == CSS_VAL_TEXT)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_OVERFLOW: // clip | ellipsis
        if (id == CSS_VAL_CLIP || id == CSS_VAL_ELLIPSIS)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_MARGIN_COLLAPSE: {
        const int properties[2] = { CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE,
            CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE };
        if (num == 1) {
            ShorthandScope scope(this, CSS_PROP__WEBKIT_MARGIN_COLLAPSE);
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = parsedProperties[numParsedProperties-1]->value();
            addProperty(properties[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSS_PROP__WEBKIT_MARGIN_COLLAPSE);
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSS_PROP__WEBKIT_MARGIN_TOP_COLLAPSE:
    case CSS_PROP__WEBKIT_MARGIN_BOTTOM_COLLAPSE:
        if (id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE || id == CSS_VAL_DISCARD)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_MODE:
    case CSS_PROP_TEXT_OVERLINE_MODE:
    case CSS_PROP_TEXT_UNDERLINE_MODE:
        if (id == CSS_VAL_CONTINUOUS || id == CSS_VAL_SKIP_WHITE_SPACE)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_STYLE:
    case CSS_PROP_TEXT_OVERLINE_STYLE:
    case CSS_PROP_TEXT_UNDERLINE_STYLE:
        if (id == CSS_VAL_NONE || id == CSS_VAL_SOLID || id == CSS_VAL_DOUBLE ||
            id == CSS_VAL_DASHED || id == CSS_VAL_DOT_DASH || id == CSS_VAL_DOT_DOT_DASH ||
            id == CSS_VAL_WAVE)
            valid_primitive = true;
        break;
    case CSS_PROP_TEXT_LINE_THROUGH_WIDTH:
    case CSS_PROP_TEXT_OVERLINE_WIDTH:
    case CSS_PROP_TEXT_UNDERLINE_WIDTH:
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NORMAL || id == CSS_VAL_THIN ||
            id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
            valid_primitive = true;
        else
            valid_primitive = !id && validUnit(value, FNumber|FLength|FPercent, strict);
        break;
    case CSS_PROP_RESIZE: // none | both | horizontal | vertical | auto
        if (id == CSS_VAL_NONE || id == CSS_VAL_BOTH || id == CSS_VAL_HORIZONTAL || id == CSS_VAL_VERTICAL || id == CSS_VAL_AUTO)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_COLUMN_COUNT:
        if (id == CSS_VAL_AUTO)
            valid_primitive = true;
        else
            valid_primitive = !id && validUnit(value, FInteger | FNonNeg, false);
        break;
    case CSS_PROP__WEBKIT_COLUMN_GAP:         // normal | <length>
        if (id == CSS_VAL_NORMAL)
            valid_primitive = true;
        else
            valid_primitive = validUnit(value, FLength, strict);
        break;
    case CSS_PROP__WEBKIT_COLUMN_WIDTH:         // auto | <length>
        if (id == CSS_VAL_AUTO)
            valid_primitive = true;
        else // Always parse this property in strict mode, since it would be ambiguous otherwise when used in the 'columns' shorthand property.
            valid_primitive = validUnit(value, FLength, true);
        break;
    // End of CSS3 properties

    // Apple specific properties.  These will never be standardized and are purely to
    // support custom WebKit-based Apple applications.
    case CSS_PROP__WEBKIT_LINE_CLAMP:
        valid_primitive = (!id && validUnit(value, FPercent, false));
        break;
    case CSS_PROP__WEBKIT_TEXT_SIZE_ADJUST:
        if (id == CSS_VAL_AUTO || id == CSS_VAL_NONE)
            valid_primitive = true;
        break;
    case CSS_PROP__WEBKIT_RTL_ORDERING:
        if (id == CSS_VAL_LOGICAL || id == CSS_VAL_VISUAL)
            valid_primitive = true;
        break;
    
    case CSS_PROP__WEBKIT_FONT_SIZE_DELTA:           // <length>
        valid_primitive = validUnit(value, FLength, strict);
        break;

    case CSS_PROP__WEBKIT_NBSP_MODE:     // normal | space
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_SPACE)
            valid_primitive = true;
        break;

    case CSS_PROP__WEBKIT_LINE_BREAK:   // normal | after-white-space
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_AFTER_WHITE_SPACE)
            valid_primitive = true;
        break;

    case CSS_PROP__WEBKIT_MATCH_NEAREST_MAIL_BLOCKQUOTE_COLOR:   // normal | match
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_MATCH)
            valid_primitive = true;
        break;

    case CSS_PROP__WEBKIT_HIGHLIGHT:
        if (id == CSS_VAL_NONE || value->unit == CSSPrimitiveValue::CSS_STRING)
            valid_primitive = true;
        break;
    
    case CSS_PROP__WEBKIT_BORDER_FIT:
        if (id == CSS_VAL_BORDER || id == CSS_VAL_LINES)
            valid_primitive = true;
        break;
        
    case CSS_PROP__WEBKIT_TEXT_SECURITY:
        // disc | circle | square | none | inherit
        if (id == CSS_VAL_DISC || id == CSS_VAL_CIRCLE || id == CSS_VAL_SQUARE|| id == CSS_VAL_NONE)
            valid_primitive = true;
        break;

    case CSS_PROP__WEBKIT_DASHBOARD_REGION:                 // <dashboard-region> | <dashboard-region> 
        if (value->unit == Value::Function || id == CSS_VAL_NONE)
            return parseDashboardRegions(propId, important);
        break;
    // End Apple-specific properties

        /* shorthand properties */
    case CSS_PROP_BACKGROUND:
        // ['background-color' || 'background-image' || 'background-size' || 'background-repeat' ||
        // 'background-attachment' || 'background-position'] | inherit
        return parseBackgroundShorthand(important);
    case CSS_PROP_BORDER:
        // [ 'border-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
                                    CSS_PROP_BORDER_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_TOP:
        // [ 'border-top-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
                                    CSS_PROP_BORDER_TOP_COLOR};
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_RIGHT:
        // [ 'border-right-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_RIGHT_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_BOTTOM:
        // [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_LEFT:
        // [ 'border-left-width' || 'border-style' || <color> ] | inherit
    {
        const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
                                    CSS_PROP_BORDER_LEFT_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_OUTLINE:
        // [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
    {
        const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
                                    CSS_PROP_OUTLINE_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP_BORDER_COLOR:
        // <color>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
                                    CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_BORDER_WIDTH:
        // <border-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
                                    CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_BORDER_STYLE:
        // <border-style>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
                                    CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_MARGIN:
        // <margin-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
                                    CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_PADDING:
        // <padding-width>{1,4} | inherit
    {
        const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
                                    CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
        return parse4Values(propId, properties, important);
    }
    case CSS_PROP_FONT:
        // [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
        // 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
        if (id >= CSS_VAL_CAPTION && id <= CSS_VAL_STATUS_BAR)
            valid_primitive = true;
        else
            return parseFont(important);
        break;
    case CSS_PROP_LIST_STYLE:
    {
        const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
                                    CSS_PROP_LIST_STYLE_IMAGE };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP__WEBKIT_COLUMNS: {
        const int properties[2] = { CSS_PROP__WEBKIT_COLUMN_WIDTH, CSS_PROP__WEBKIT_COLUMN_COUNT };
        return parseShorthand(propId, properties, 2, important);
    }
    case CSS_PROP__WEBKIT_COLUMN_RULE: {
        const int properties[3] = { CSS_PROP__WEBKIT_COLUMN_RULE_WIDTH, CSS_PROP__WEBKIT_COLUMN_RULE_STYLE,
                                    CSS_PROP__WEBKIT_COLUMN_RULE_COLOR };
        return parseShorthand(propId, properties, 3, important);
    }
    case CSS_PROP__WEBKIT_TEXT_STROKE: {
        const int properties[2] = { CSS_PROP__WEBKIT_TEXT_STROKE_WIDTH, CSS_PROP__WEBKIT_TEXT_STROKE_COLOR };
        return parseShorthand(propId, properties, 2, important);
    }
    case CSS_PROP_INVALID:
        return false;
    case CSS_PROP_FONT_STRETCH:
    case CSS_PROP_PAGE:
    case CSS_PROP_TEXT_LINE_THROUGH:
    case CSS_PROP_TEXT_OVERLINE:
    case CSS_PROP_TEXT_UNDERLINE:
        return false;
    }

    if (valid_primitive) {
        if (id != 0)
            parsedValue = new CSSPrimitiveValue(id);
        else if (value->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = new CSSPrimitiveValue(domString(value->string), (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
            parsedValue = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else if (value->unit >= Value::Q_EMS)
            parsedValue = new CSSQuirkPrimitiveValue(value->fValue, CSSPrimitiveValue::CSS_EMS);
        valueList->next();
    }
    if (parsedValue) {
        if (!valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue, important);
            return true;
        }
        delete parsedValue;
    }
#if ENABLE(SVG)
    if (parseSVGValue(propId, important))
        return true;
#endif
    return false;
}

void CSSParser::addBackgroundValue(CSSValue*& lval, CSSValue* rval)
{
    if (lval) {
        if (lval->isValueList())
            static_cast<CSSValueList*>(lval)->append(rval);
        else {
            CSSValue* oldVal = lval;
            CSSValueList* list = new CSSValueList();
            lval = list;
            list->append(oldVal);
            list->append(rval);
        }
    }
    else
        lval = rval;
}

bool CSSParser::parseBackgroundShorthand(bool important)
{
    // Position must come before color in this array because a plain old "0" is a legal color
    // in quirks mode but it's usually the X coordinate of a position.
    // FIXME: Add CSS_PROP__KHTML_BACKGROUND_SIZE to the shorthand.
    const int properties[] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT, 
        CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION, CSS_PROP__WEBKIT_BACKGROUND_CLIP,
        CSS_PROP__WEBKIT_BACKGROUND_ORIGIN, CSS_PROP_BACKGROUND_COLOR };
    const int numProperties = sizeof(properties) / sizeof(properties[0]);
    
    ShorthandScope scope(this, CSS_PROP_BACKGROUND);

    bool parsedProperty[numProperties] = { false }; // compiler will repeat false as necessary
    CSSValue* values[numProperties] = { 0 }; // compiler will repeat 0 as necessary
    CSSValue* positionYValue = 0;
    int i;

    while (valueList->current()) {
        Value* val = valueList->current();
        if (val->unit == Value::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSS_PROP_BACKGROUND_COLOR && parsedProperty[i])
                    // Color is not allowed except as the last item in a list.  Reject the entire
                    // property.
                    goto fail;

                if (!parsedProperty[i] && properties[i] != CSS_PROP_BACKGROUND_COLOR) {
                    addBackgroundValue(values[i], new CSSInitialValue(true));
                    if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                        addBackgroundValue(positionYValue, new CSSInitialValue(true));
                }
                parsedProperty[i] = false;
            }
            if (!valueList->current())
                break;
        }
        
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                CSSValue *val1 = 0, *val2 = 0;
                int propId1, propId2;
                if (parseBackgroundProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addBackgroundValue(values[i], val1);
                    if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                        addBackgroundValue(positionYValue, val2);
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            goto fail;
    }
    
    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i]) {
            addBackgroundValue(values[i], new CSSInitialValue(true));
            if (properties[i] == CSS_PROP_BACKGROUND_POSITION)
                addBackgroundValue(positionYValue, new CSSInitialValue(true));
        }
    }
    
    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        if (properties[i] == CSS_PROP_BACKGROUND_POSITION) {
            addProperty(CSS_PROP_BACKGROUND_POSITION_X, values[i], important);
            addProperty(CSS_PROP_BACKGROUND_POSITION_Y, positionYValue, important);
        }
        else
            addProperty(properties[i], values[i], important);
    }
    
    return true;

fail:
    for (int k = 0; k < numProperties; k++)
        delete values[k];
    delete positionYValue;
    return false;
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
            String value = parseURL(domString(val->string));
            parsedValue = new CSSImageValue(
                String(KURL(styleElement->baseURL().deprecatedString(), value.deprecatedString()).url()), styleElement);
        } else if (val->unit == Value::Function) {
            // attr(X) | counter(X [,Y]) | counters(X, Y, [,Z])
            ValueList *args = val->function->args;
            String fname = domString(val->function->name).lower();
            if (!args)
                return false;
            if (fname == "attr(") {
                if (args->size() != 1)
                    return false;
                Value* a = args->current();
                String attrName = domString(a->string);
                if (document()->isHTMLDocument())
                    attrName = attrName.lower();
                parsedValue = new CSSPrimitiveValue(attrName, CSSPrimitiveValue::CSS_ATTR);
            } else if (fname == "counter(") {
                parsedValue = parseCounterContent(args, false);
                if (!parsedValue) return false;
            } else if (fname == "counters(") {
                parsedValue = parseCounterContent(args, true);
                if (!parsedValue)
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
            parsedValue = new CSSPrimitiveValue(domString(val->string), CSSPrimitiveValue::CSS_STRING);
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

CSSValue* CSSParser::parseBackgroundColor()
{
    int id = valueList->current()->id;
    if (id == CSS_VAL__WEBKIT_TEXT || (id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT) || id == CSS_VAL_MENU ||
        (id >= CSS_VAL_GREY && id < CSS_VAL__WEBKIT_TEXT && !strict))
       return new CSSPrimitiveValue(id);
    return parseColor();
}

bool CSSParser::parseBackgroundImage(CSSValue*& value)
{
    if (valueList->current()->id == CSS_VAL_NONE) {
        value = new CSSImageValue();
        return true;
    }
    if (valueList->current()->unit == CSSPrimitiveValue::CSS_URI) {
        String uri = parseURL(domString(valueList->current()->string));
        if (!uri.isEmpty())
            value = new CSSImageValue(String(KURL(styleElement->baseURL().deprecatedString(), uri.deprecatedString()).url()), 
                                         styleElement);
        return true;
    }
    return false;
}

CSSValue* CSSParser::parseBackgroundPositionXY(bool& xFound, bool& yFound)
{
    int id = valueList->current()->id;
    if (id == CSS_VAL_LEFT || id == CSS_VAL_TOP || id == CSS_VAL_RIGHT || id == CSS_VAL_BOTTOM || id == CSS_VAL_CENTER) {
        int percent = 0;
        if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT) {
            if (xFound)
                return 0;
            xFound = true;
            if (id == CSS_VAL_RIGHT)
                percent = 100;
        }
        else if (id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM) {
            if (yFound)
                return 0;
            yFound = true;
            if (id == CSS_VAL_BOTTOM)
                percent = 100;
        }
        else if (id == CSS_VAL_CENTER)
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
        return new CSSPrimitiveValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent|FLength, strict))
        return new CSSPrimitiveValue(valueList->current()->fValue,
                                         (CSSPrimitiveValue::UnitTypes)valueList->current()->unit);
                
    return 0;
}

void CSSParser::parseBackgroundPosition(CSSValue*& value1, CSSValue*& value2)
{
    value1 = value2 = 0;
    Value* value = valueList->current();
    
    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    bool value1IsX = false, value1IsY = false;
    value1 = parseBackgroundPositionXY(value1IsX, value1IsY);
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
        value2 = parseBackgroundPositionXY(value2IsX, value2IsY);
        if (value2)
            valueList->next();
        else {
            if (!inShorthand()) {
                delete value1;
                value1 = 0;
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

    if (value1IsY || value2IsX) {
        // Swap our two values.
        CSSValue* val = value2;
        value2 = value1;
        value1 = val;
    }
}

CSSValue* CSSParser::parseBackgroundSize()
{
    Value* value = valueList->current();
    CSSPrimitiveValue* parsedValue1;
    
    if (value->id == CSS_VAL_AUTO)
        parsedValue1 = new CSSPrimitiveValue(0, CSSPrimitiveValue::CSS_UNKNOWN);
    else {
        if (!validUnit(value, FLength|FPercent, strict))
            return 0;
        parsedValue1 = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes)value->unit);
    }
    
    CSSPrimitiveValue* parsedValue2 = parsedValue1;
    if ((value = valueList->next())) {
        if (value->id == CSS_VAL_AUTO)
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

bool CSSParser::parseBackgroundProperty(int propId, int& propId1, int& propId2, 
                                        CSSValue*& retValue1, CSSValue*& retValue2)
{
    CSSValueList *values = 0, *values2 = 0;
    Value* val;
    CSSValue *value = 0, *value2 = 0;
    bool allowComma = false;
    
    retValue1 = retValue2 = 0;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSS_PROP_BACKGROUND_POSITION) {
        propId1 = CSS_PROP_BACKGROUND_POSITION_X;
        propId2 = CSS_PROP_BACKGROUND_POSITION_Y;
    }

    while ((val = valueList->current())) {
        CSSValue *currValue = 0, *currValue2 = 0;
        if (allowComma) {
            if (val->unit != Value::Operator || val->iValue != ',')
                goto failed;
            valueList->next();
            allowComma = false;
        } else {
            switch (propId) {
                case CSS_PROP_BACKGROUND_ATTACHMENT:
                    if (val->id == CSS_VAL_SCROLL || val->id == CSS_VAL_FIXED) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_COLOR:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        valueList->next();
                    break;
                case CSS_PROP_BACKGROUND_IMAGE:
                    if (parseBackgroundImage(currValue))
                        valueList->next();
                    break;
                case CSS_PROP__WEBKIT_BACKGROUND_CLIP:
                case CSS_PROP__WEBKIT_BACKGROUND_ORIGIN:
                    if (val->id == CSS_VAL_BORDER || val->id == CSS_VAL_PADDING || val->id == CSS_VAL_CONTENT) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_POSITION:
                    parseBackgroundPosition(currValue, currValue2);
                    // unlike the other functions, parseBackgroundPosition advances the valueList pointer
                    break;
                case CSS_PROP_BACKGROUND_POSITION_X: {
                    bool xFound = false, yFound = true;
                    currValue = parseBackgroundPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSS_PROP_BACKGROUND_POSITION_Y: {
                    bool xFound = true, yFound = false;
                    currValue = parseBackgroundPositionXY(xFound, yFound);
                    if (currValue)
                        valueList->next();
                    break;
                }
                case CSS_PROP__WEBKIT_BACKGROUND_COMPOSITE:
                    if ((val->id >= CSS_VAL_CLEAR && val->id <= CSS_VAL_PLUS_LIGHTER) || val->id == CSS_VAL_HIGHLIGHT) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP_BACKGROUND_REPEAT:
                    if (val->id >= CSS_VAL_REPEAT && val->id <= CSS_VAL_NO_REPEAT) {
                        currValue = new CSSPrimitiveValue(val->id);
                        valueList->next();
                    }
                    break;
                case CSS_PROP__WEBKIT_BACKGROUND_SIZE:
                    currValue = parseBackgroundSize();
                    if (currValue)
                        valueList->next();
                    break;
            }
            if (!currValue)
                goto failed;
            
            if (value && !values) {
                values = new CSSValueList();
                values->append(value);
                value = 0;
            }
            
            if (value2 && !values2) {
                values2 = new CSSValueList();
                values2->append(value2);
                value2 = 0;
            }
            
            if (values)
                values->append(currValue);
            else
                value = currValue;
            if (currValue2) {
                if (values2)
                    values2->append(currValue2);
                else
                    value2 = currValue2;
            }
            allowComma = true;
        }
        
        // When parsing the 'background' shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;
    }

    if (values && values->length()) {
        retValue1 = values;
        if (values2 && values2->length())
            retValue2 = values2;
        return true;
    }
    if (value) {
        retValue1 = value;
        retValue2 = value2;
        return true;
    }

failed:
    delete values; delete values2;
    delete value; delete value2;
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

    if (value->id == CSS_VAL_NONE) {
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
        String fname = domString(value->function->name).lower();
        if (fname != "dashboard-region(" || !args) {
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
            
        region->m_label = domString(arg->string);

        // Second arg is a type.
        arg = args->next();
        arg = skipCommaInDashboardRegion (args);
        if (arg->unit != CSSPrimitiveValue::CSS_IDENT) {
            valid = false;
            break;
        }

        String geometryType = domString(arg->string).lower();
        if (geometryType == "circle")
            region->m_isCircle = true;
        else if (geometryType == "rectangle")
            region->m_isRectangle = true;
        else {
            valid = false;
            break;
        }
            
        region->m_geometryType = domString(arg->string);

        if (numArgs == DASHBOARD_REGION_SHORT_NUM_PARAMETERS || numArgs == (DASHBOARD_REGION_SHORT_NUM_PARAMETERS*2-1)) {
            // This originally used CSS_VAL_INVALID by accident. It might be more logical to use something else.
            CSSPrimitiveValue *amount = new CSSPrimitiveValue(CSS_VAL_INVALID);
                
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

                valid = arg->id == CSS_VAL_AUTO || validUnit(arg, FLength, strict);
                if (!valid)
                    break;
                    
                CSSPrimitiveValue *amount = arg->id == CSS_VAL_AUTO ?
                    new CSSPrimitiveValue(CSS_VAL_AUTO) :
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
    RefPtr<CSSPrimitiveValue> identifier = new CSSPrimitiveValue(domString(i->string),
        CSSPrimitiveValue::CSS_STRING);

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
        
        separator = new CSSPrimitiveValue(domString(i->string), (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    RefPtr<CSSPrimitiveValue> listStyle;
    i = args->next();
    if (!i) // Make the list style default decimal
        listStyle = new CSSPrimitiveValue(CSS_VAL_DECIMAL - CSS_VAL_DISC, CSSPrimitiveValue::CSS_NUMBER);
    else {
        if (i->unit != Value::Operator || i->iValue != ',')
            return 0;
        
        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_IDENT)
            return 0;
        
        short ls = 0;
        if (i->id == CSS_VAL_NONE)
            ls = CSS_VAL_KATAKANA_IROHA - CSS_VAL_DISC + 1;
        else if (i->id >= CSS_VAL_DISC && i->id <= CSS_VAL_KATAKANA_IROHA)
            ls = i->id - CSS_VAL_DISC;
        else
            return 0;

        listStyle = new CSSPrimitiveValue(ls, (CSSPrimitiveValue::UnitTypes) i->unit);
    }

    return new CSSPrimitiveValue(new Counter(identifier.release(), listStyle.release(), separator.release()));
}

bool CSSParser::parseShape(int propId, bool important)
{
    Value *value = valueList->current();
    ValueList *args = value->function->args;
    String fname = domString(value->function->name).lower();
    if (fname != "rect(" || !args)
        return false;

    // rect(t, r, b, l) || rect(t r b l)
    if (args->size() != 4 && args->size() != 7)
        return false;
    Rect *rect = new Rect();
    bool valid = true;
    int i = 0;
    Value *a = args->current();
    while (a) {
        valid = a->id == CSS_VAL_AUTO || validUnit(a, FLength, strict);
        if (!valid)
            break;
        CSSPrimitiveValue *length = a->id == CSS_VAL_AUTO ?
            new CSSPrimitiveValue(CSS_VAL_AUTO) :
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
    FontValue *font = new FontValue;
    // optional font-style, font-variant and font-weight
    while (value) {
        int id = value->id;
        if (id) {
            if (id == CSS_VAL_NORMAL) {
                // do nothing, it's the inital value for all three
            } else if (id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE) {
                if (font->style)
                    goto invalid;
                font->style = new CSSPrimitiveValue(id);
            } else if (id == CSS_VAL_SMALL_CAPS) {
                if (font->variant)
                    goto invalid;
                font->variant = new CSSPrimitiveValue(id);
            } else if (id >= CSS_VAL_BOLD && id <= CSS_VAL_LIGHTER) {
                if (font->weight)
                    goto invalid;
                font->weight = new CSSPrimitiveValue(id);
            } else {
                valid = false;
            }
        } else if (!font->weight && validUnit(value, FInteger|FNonNeg, true)) {
            int weight = (int)value->fValue;
            int val = 0;
            if (weight == 100)
                val = CSS_VAL_100;
            else if (weight == 200)
                val = CSS_VAL_200;
            else if (weight == 300)
                val = CSS_VAL_300;
            else if (weight == 400)
                val = CSS_VAL_400;
            else if (weight == 500)
                val = CSS_VAL_500;
            else if (weight == 600)
                val = CSS_VAL_600;
            else if (weight == 700)
                val = CSS_VAL_700;
            else if (weight == 800)
                val = CSS_VAL_800;
            else if (weight == 900)
                val = CSS_VAL_900;

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
        goto invalid;

    // set undefined values to default
    if (!font->style)
        font->style = new CSSPrimitiveValue(CSS_VAL_NORMAL);
    if (!font->variant)
        font->variant = new CSSPrimitiveValue(CSS_VAL_NORMAL);
    if (!font->weight)
        font->weight = new CSSPrimitiveValue(CSS_VAL_NORMAL);

    // now a font size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (value->id >= CSS_VAL_XX_SMALL && value->id <= CSS_VAL_LARGER)
        font->size = new CSSPrimitiveValue(value->id);
    else if (validUnit(value, FLength|FPercent, strict))
        font->size = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
    value = valueList->next();
    if (!font->size || !value)
        goto invalid;

    if (value->unit == Value::Operator && value->iValue == '/') {
        // line-height
        value = valueList->next();
        if (!value)
            goto invalid;
        if (value->id == CSS_VAL_NORMAL) {
            // default value, nothing to do
        } else if (validUnit(value, FNumber|FLength|FPercent, strict))
            font->lineHeight = new CSSPrimitiveValue(value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit);
        else
            goto invalid;
        value = valueList->next();
        if (!value)
            goto invalid;
    }
    
    if (!font->lineHeight)
        font->lineHeight = new CSSPrimitiveValue(CSS_VAL_NORMAL);

    // font family must come now
    font->family = parseFontFamily();

    if (valueList->current() || !font->family)
        goto invalid;

    addProperty(CSS_PROP_FONT, font, important);
    return true;

 invalid:
    delete font;
    return false;
}

CSSValueList* CSSParser::parseFontFamily()
{
    CSSValueList* list = new CSSValueList;
    Value* value = valueList->current();
    FontFamilyValue* currFamily = 0;
    while (value) {
        Value* nextValue = valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == Value::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSS_VAL_SERIF && nextValue->id <= CSS_VAL__WEBKIT_BODY) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id >= CSS_VAL_SERIF && value->id <= CSS_VAL__WEBKIT_BODY) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += deprecatedString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new CSSPrimitiveValue(value->id));
            else
                list->append(currFamily = new FontFamilyValue(deprecatedString(value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFamily = 0;
            list->append(new FontFamilyValue(deprecatedString(value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += deprecatedString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new FontFamilyValue(deprecatedString(value->string)));
            else
                list->append(currFamily = new FontFamilyValue(deprecatedString(value->string)));
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

bool CSSParser::parseColor(const String &name, RGBA32& rgb, bool strict)
{
    if (!strict && Color::parseHexColor(name, rgb))
        return true;

    // try a little harder
    Color tc;
    tc.setNamedColor(name.lower());
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

CSSPrimitiveValue *CSSParser::parseColor(Value* value)
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
        if (!CSSParser::parseColor(domString(value->string), c, strict && value->unit == CSSPrimitiveValue::CSS_IDENT))
            return false;
    } else if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 5 /* rgb + two commas */ &&
                domString(value->function->name).lower() == "rgb(") {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        c = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else if (!svg) {
        if (value->unit == Value::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* rgba + three commas */ &&
                domString(value->function->name).lower() == "rgba(") {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            c = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value->unit == Value::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 5 /* hsl + two commas */ &&
                    domString(value->function->name).lower() == "hsl(") {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value->unit == Value::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 7 /* hsla + three commas */ &&
                    domString(value->function->name).lower() == "hsla(") {
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
    :values(0), x(0), y(0), blur(0), color(0),
     allowX(true), allowY(false), allowBlur(false), allowColor(true),
     allowBreak(true)
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

    void commitColor(CSSPrimitiveValue* val) {
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
            CSSPrimitiveValue* parsedColor = 0;
            bool isColor = (val->id >= CSS_VAL_AQUA && val->id <= CSS_VAL_WINDOWTEXT || val->id == CSS_VAL_MENU ||
                            (val->id >= CSS_VAL__WEBKIT_FOCUS_RING_COLOR && val->id <= CSS_VAL__WEBKIT_TEXT && !strict));
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
            
            context.commitColor(parsedColor);
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
    :m_allowBreak(false), m_allowNumber(false), m_allowSlash(false), m_allowWidth(false),
     m_allowRule(false), m_image(0), m_top(0), m_right(0), m_bottom(0), m_left(0), m_borderTop(0), m_borderRight(0), m_borderBottom(0),
     m_borderLeft(0), m_horizontalRule(0), m_verticalRule(0)
    {}
    
    bool allowBreak() const { return m_allowBreak; }
    bool allowNumber() const { return m_allowNumber; }
    bool allowSlash() const { return m_allowSlash; }
    bool allowWidth() const { return m_allowWidth; }
    bool allowRule() const { return m_allowRule; }

    void commitImage(CSSImageValue* image) { m_image = image; m_allowNumber = true; }
    void commitNumber(Value* v) {
        CSSPrimitiveValue* val = new CSSPrimitiveValue(v->fValue,
                                                               (CSSPrimitiveValue::UnitTypes)v->unit);
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
            m_borderTop.set(val);
        else if (!m_borderRight)
            m_borderRight.set(val);
        else if (!m_borderBottom)
            m_borderBottom.set(val);
        else {
            ASSERT(!m_borderLeft);
            m_borderLeft.set(val);
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
        Rect* rect = new Rect;
        rect->setTop(m_top); rect->setRight(m_right); rect->setBottom(m_bottom); rect->setLeft(m_left);

        // Fill in STRETCH as the default if it wasn't specified.
        if (!m_horizontalRule)
            m_horizontalRule = CSS_VAL_STRETCH;
            
        // The vertical rule should match the horizontal rule if unspecified.
        if (!m_verticalRule)
            m_verticalRule = m_horizontalRule;

        // Make our new border image value now and add it as the result.
        CSSBorderImageValue* borderImage = new CSSBorderImageValue(m_image, rect, m_horizontalRule, m_verticalRule);
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
            p->parseValue(CSS_PROP_BORDER_WIDTH, important);
            p->valueList = 0;
        }
    }
    
    bool m_allowBreak;
    bool m_allowNumber;
    bool m_allowSlash;
    bool m_allowWidth;
    bool m_allowRule;
    
    RefPtr<CSSImageValue> m_image;
    
    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;
    
    OwnPtr<Value> m_borderTop;
    OwnPtr<Value> m_borderRight;
    OwnPtr<Value> m_borderBottom;
    OwnPtr<Value> m_borderLeft;
    
    int m_horizontalRule;
    int m_verticalRule;
};

bool CSSParser::parseBorderImage(int propId, bool important)
{
    // Look for an image initially.  If the first value is not a URI, then we're done.
    BorderImageParseContext context;
    Value* val = valueList->current();
    if (val->unit != CSSPrimitiveValue::CSS_URI)
        return false;
        
    String uri = parseURL(domString(val->string));
    if (uri.isEmpty())
        return false;
    
    context.commitImage(new CSSImageValue(String(KURL(styleElement->baseURL().deprecatedString(), uri.deprecatedString()).url()),
                                                             styleElement));
    while ((val = valueList->next())) {
        if (context.allowNumber() && validUnit(val, FInteger|FNonNeg|FPercent, true)) {
            context.commitNumber(val);
        } else if (context.allowSlash() && val->unit == Value::Operator && val->iValue == '/') {
            context.commitSlash();
        } else if (context.allowWidth() &&
            (val->id == CSS_VAL_THIN || val->id == CSS_VAL_MEDIUM || val->id == CSS_VAL_THICK || validUnit(val, FLength, strict))) {
            context.commitWidth(val);
        } else if (context.allowRule() &&
            (val->id == CSS_VAL_STRETCH || val->id == CSS_VAL_ROUND || val->id == CSS_VAL_REPEAT)) {
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
                    counterName = new CSSPrimitiveValue(domString(val->string), CSSPrimitiveValue::CSS_STRING);
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
    case FLOAT:
    case INTEGER:
        yylval->val = DeprecatedString((DeprecatedChar*)t, length).toFloat();
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
    CSSCharsetRule* rule = new CSSCharsetRule(styleElement, domString(charset));
    m_parsedStyleObjects.append(rule);
    return rule;
}

CSSRule* CSSParser::createImportRule(const ParseString& URL, MediaList* media)
{
    if (!media)
        return 0;
    if (!styleElement)
        return 0;
    if (!styleElement->isCSSStyleSheet())
        return 0;
    CSSImportRule* rule = new CSSImportRule(styleElement, domString(URL), media);
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

DeprecatedString deprecatedString(const ParseString& ps)
{
    return DeprecatedString(reinterpret_cast<const DeprecatedChar*>(ps.characters), ps.length);
}

#define YY_DECL int CSSParser::lex()
#define yyconst const
typedef int yy_state_type;
typedef unsigned YY_CHAR;
// this line makes sure we treat all Unicode chars correctly.
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
// The line below is needed to build the tokenizer with conditon stack.
// The macro is used in the tokenizer grammar with lines containing
// BEGIN(mediaqueries) and BEGIN(initial). yy_start acts as index to
// tokenizer transition table, and 'mediaqueries' and 'initial' are
// offset multipliers that specify which transitions are active
// in the tokenizer during in each condition (tokenizer state)
#define BEGIN yy_start = 1 + 2 *

#include "tokenizer.cpp"

}
