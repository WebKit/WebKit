/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 *
 * $Id$
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#define CSS_DEBUG
// #define TOKEN_DEBUG
#define YYDEBUG 0

#include <kdebug.h>
#include <kurl.h>

#include "cssparser.h"
#include "css_valueimpl.h"
#include "css_ruleimpl.h"
#include "css_stylesheetimpl.h"
#include "cssproperties.h"
#include "cssvalues.h"
#include "misc/helper.h"
#include "csshelper.h"
using namespace DOM;

#include <stdlib.h>

#if APPLE_CHANGES
void qFatal ( const char * msg ) {}
#endif

ValueList::ValueList()
{
    values = (Value *) malloc( 16 * sizeof ( Value ) );
    numValues = 0;
    currentValue = 0;
    maxValues = 16;
}

ValueList::~ValueList()
{
    for ( int i = 0; i < numValues; i++ ) {
#ifdef CSS_DEBUG
	kdDebug( 6080 ) << "       value: (unit=" << values[i].unit <<")"<< endl;
#endif
	if ( values[i].unit == Value::Function )
	    delete values[i].function;
    }
    free( values );
}

void ValueList::addValue( const Value &val )
{
    if ( numValues >= maxValues ) {
	maxValues += 16;
	values = (Value *) realloc( values, maxValues*sizeof( Value ) );
    }
    values[numValues++] = val;
}


using namespace DOM;

#if YYDEBUG > 0
extern int cssyydebug;
#endif

extern int cssyyparse( void * parser );

CSSParser *CSSParser::currentParser = 0;

CSSParser::CSSParser( bool strictParsing )
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::CSSParser this=" << this << endl;
#endif
    strict = strictParsing;

    parsedProperties = (CSSProperty **) malloc( 32 * sizeof( CSSProperty * ) );
    numParsedProperties = 0;
    maxParsedProperties = 32;

    valueList = 0;
    rule = 0;
    id = 0;
    important = false;
    nonCSSHint = false;
    inParseShortHand = false;
    yy_start = 1;

#if YYDEBUG > 0
    cssyydebug = 1;
#endif

}

CSSParser::~CSSParser()
{
    if ( numParsedProperties )
	clearProperties();
    free( parsedProperties );

    delete valueList;

#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::~CSSParser this=" << this << endl;
#endif

    free( data );

}

void CSSParser::parseSheet( CSSStyleSheetImpl *sheet, const DOMString &string )
{
    styleElement = sheet;

    int length = string.length() + 3;
    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    memcpy( data, string.unicode(), string.length()*sizeof( unsigned short) );
    data[length-1] = 0;
    data[length-2] = 0;
    data[length-3] = ' ';
    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;

#ifdef CSS_DEBUG
    kdDebug( 6080 ) << ">>>>>>> start parsing style sheet" << endl;
#endif
    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "<<<<<<< done parsing style sheet" << endl;
#endif

    delete rule;
    rule = 0;
}

CSSRuleImpl *CSSParser::parseRule( DOM::CSSStyleSheetImpl *sheet, const DOM::DOMString &string )
{
    styleElement = sheet;
    
    const char khtml_rule[] = "@-khtml-rule{";
    int length = string.length() + 4 + strlen(khtml_rule);
    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    for ( unsigned int i = 0; i < strlen(khtml_rule); i++ )
	data[i] = khtml_rule[i];
    memcpy( data + strlen( khtml_rule ), string.unicode(), string.length()*sizeof( unsigned short) );
    // qDebug("parse string = '%s'", QConstString( (const QChar *)data, length ).string().latin1() );
    data[length-1] = 0;
    data[length-2] = 0;
    data[length-3] = ' ';
    data[length-4] = '}';
    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;

    CSSRuleImpl *result = rule;
    rule = 0;
    
    return result;
}

bool CSSParser::parseValue( DOM::CSSStyleDeclarationImpl *declaration, int _id, const DOM::DOMString &string,
			    bool _important, bool _nonCSSHint )
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::parseValue: id=" << _id << " important=" << _important
		    << " nonCSSHint=" << _nonCSSHint << " value='" << string.string() << "'" << endl;
#endif

    styleElement = declaration->stylesheet();

    const char khtml_value[] = "@-khtml-value{";
    int length = string.length() + 4 + strlen(khtml_value);
    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    for ( unsigned int i = 0; i < strlen(khtml_value); i++ )
	data[i] = khtml_value[i];
    memcpy( data + strlen( khtml_value ), string.unicode(), string.length()*sizeof( unsigned short) );
    data[length-1] = 0;
    data[length-2] = 0;
    data[length-3] = ' ';
    data[length-4] = '}';
    // qDebug("parse string = '%s'", QConstString( (const QChar *)data, length ).string().latin1() );
    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;

    id = _id;
    important = _important;
    nonCSSHint = _nonCSSHint;

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;
    
    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
	ok = true;
	for ( int i = 0; i < numParsedProperties; i++ ) {
	    declaration->removeProperty(parsedProperties[i]->m_id, nonCSSHint);
	    declaration->values()->append( parsedProperties[i] );
	}
	numParsedProperties = 0;
    }

    return ok;
}

bool CSSParser::parseDeclaration( DOM::CSSStyleDeclarationImpl *declaration, const DOM::DOMString &string,
				  bool _nonCSSHint )
{
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSSParser::parseDeclaration: nonCSSHint=" << nonCSSHint
		    << " value='" << string.string() << "'" << endl;
#endif

    styleElement = declaration->stylesheet();

    const char khtml_decls[] = "@-khtml-decls{";
    int length = string.length() + 4 + strlen(khtml_decls);
    data = (unsigned short *)malloc( length *sizeof( unsigned short ) );
    for ( unsigned int i = 0; i < strlen(khtml_decls); i++ )
	data[i] = khtml_decls[i];
    memcpy( data + strlen( khtml_decls ), string.unicode(), string.length()*sizeof( unsigned short) );
    data[length-1] = 0;
    data[length-2] = 0;
    data[length-3] = ' ';
    data[length-4] = '}';
    // qDebug("parse string = '%s'", QConstString( (const QChar *)data, length ).string().latin1() );
    yy_hold_char = 0;
    yyleng = 0;
    yytext = yy_c_buf_p = data;
    yy_hold_char = *yy_c_buf_p;

    nonCSSHint = _nonCSSHint;

    CSSParser *old = currentParser;
    currentParser = this;
    cssyyparse( this );
    currentParser = old;

    delete rule;
    rule = 0;

    bool ok = false;
    if ( numParsedProperties ) {
	ok = true;
	for ( int i = 0; i < numParsedProperties; i++ ) {
	    declaration->removeProperty(parsedProperties[i]->m_id, false);
	    declaration->values()->append( parsedProperties[i] );
	}
	numParsedProperties = 0;
    }

    return ok;
}

void CSSParser::addProperty( int propId, CSSValueImpl *value, bool important )
{
    CSSProperty *prop = new CSSProperty;
    prop->m_id = propId;
    prop->setValue( value );
    prop->m_bImportant = important;
    prop->nonCSSHint = nonCSSHint;

    if ( numParsedProperties >= maxParsedProperties ) {
	maxParsedProperties += 32;
	parsedProperties = (CSSProperty **) realloc( parsedProperties,
						    maxParsedProperties*sizeof( CSSProperty * ) );
    }
    parsedProperties[numParsedProperties++] = prop;
}

CSSStyleDeclarationImpl *CSSParser::createStyleDeclaration( CSSStyleRuleImpl *rule )
{
    QPtrList<CSSProperty> *propList = new QPtrList<CSSProperty>;
    propList->setAutoDelete( true );
    for ( int i = 0; i < numParsedProperties; i++ )
	propList->append( parsedProperties[i] );

    numParsedProperties = 0;
    return new CSSStyleDeclarationImpl(rule, propList);
}

void CSSParser::clearProperties()
{
    for ( int i = 0; i < numParsedProperties; i++ )
	delete parsedProperties[i];
    numParsedProperties = 0;
}

DOM::DocumentImpl *CSSParser::document() const
{
    StyleBaseImpl *root = styleElement;
    DocumentImpl *doc = 0;
    while (root->parent())
	root = root->parent();
    if (root->isCSSStyleSheet())
	doc = static_cast<CSSStyleSheetImpl*>(root)->doc();
    return doc;
}


// defines units allowed for a certain property, used in parseUnit
enum Units
{
    FUnknown   = 0x0000,
    FInteger   = 0x0001,
    FNumber    = 0x0002,  // Real Numbers
    FPercent   = 0x0004,
    FLength    = 0x0008,
    FAngle     = 0x0010,
    FTime      = 0x0020,
    FFrequency = 0x0040,
    FRelative  = 0x0100,
    FNonNeg    = 0x0200
};

static bool validUnit( Value *value, int unitflags, bool strict )
{
    if ( unitflags & FNonNeg && value->fValue < 0 )
	return false;

    bool b = false;
    switch( value->unit ) {
    case CSSPrimitiveValue::CSS_NUMBER:
	b = (unitflags & FNumber);
	if ( !b && ( (unitflags & FLength) && (value->fValue == 0 || !strict ) ) ) {
	    value->unit = CSSPrimitiveValue::CSS_PX;
	    b = true;
	}
	if ( !b && ( unitflags & FInteger ) &&
	     (value->fValue - (int)value->fValue) < 0.001 )
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
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
	break;
    }
    return b;
}

bool CSSParser::parseValue( int propId, bool important )
{
    if ( !valueList ) return false;

    Value *value = valueList->current();

    if ( !value )
	return false;

    int id = 0;
    id = value->id;

    if ( id == CSS_VAL_INHERIT ) {
	addProperty( propId, new CSSInheritedValueImpl(), important );
	return true;
    }

    bool valid_primitive = false;
    CSSValueImpl *parsedValue = 0;

    switch(propId) {
	/* The comment to the left defines all valid value of this properties as defined
	 * in CSS 2, Appendix F. Property index
	 */

	/* All the CSS properties are not supported by the renderer at the moment.
	 * Note that all the CSS2 Aural properties are only checked, if CSS_AURAL is defined
	 * (see parseAuralValues). As we don't support them at all this seems reasonable.
	 */

    case CSS_PROP_SIZE:                 // <length>{1,2} | auto | portrait | landscape | inherit
    case CSS_PROP_QUOTES:               // [<string> <string>]+ | none | inherit
//     case CSS_PROP_PAGE:                 // <identifier> | auto // ### CHECK
	// ### To be done
	if (id)
	    valid_primitive = true;
	break;
    case CSS_PROP_UNICODE_BIDI:         // normal | embed | bidi-override | inherit
	if ( id == CSS_VAL_NORMAL ||
	     id == CSS_VAL_EMBED ||
	     id == CSS_VAL_BIDI_OVERRIDE )
	    valid_primitive = true;
	break;

    case CSS_PROP_POSITION:             // static | relative | absolute | fixed | inherit
	if ( id == CSS_VAL_STATIC ||
	     id == CSS_VAL_RELATIVE ||
	     id == CSS_VAL_ABSOLUTE ||
 	     id == CSS_VAL_FIXED )
	    valid_primitive = true;
	break;

    case CSS_PROP_PAGE_BREAK_AFTER:     // auto | always | avoid | left | right | inherit
    case CSS_PROP_PAGE_BREAK_BEFORE:    // auto | always | avoid | left | right | inherit
	if ( id == CSS_VAL_AUTO ||
	     id == CSS_VAL_ALWAYS ||
	     id == CSS_VAL_AVOID ||
 	     id == CSS_VAL_LEFT ||
 	     id == CSS_VAL_RIGHT )
	    valid_primitive = true;
	break;

    case CSS_PROP_PAGE_BREAK_INSIDE:    // avoid | auto | inherit
	if ( id == CSS_VAL_AUTO ||
	     id == CSS_VAL_AVOID )
	    valid_primitive = true;
	break;

    case CSS_PROP_EMPTY_CELLS:          // show | hide | inherit
	if ( id == CSS_VAL_SHOW ||
	     id == CSS_VAL_HIDE )
	    valid_primitive = true;
	break;

    case CSS_PROP_CONTENT:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
	// close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent( propId, important );
        break;

    case CSS_PROP_WHITE_SPACE:          // normal | pre | nowrap | inherit
	if ( id == CSS_VAL_NORMAL ||
	     id == CSS_VAL_PRE ||
	     id == CSS_VAL_NOWRAP )
	    valid_primitive = true;
	break;

    case CSS_PROP_CLIP:                 // <shape> | auto | inherit
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else if ( value->unit == Value::Function )
	    return parseShape( propId, important );
	break;

    /* Start of supported CSS properties with validation. This is needed for parseShortHand to work
     * correctly and allows optimization in khtml::applyRule(..)
     */
    case CSS_PROP_CAPTION_SIDE:         // top | bottom | left | right | inherit
	if (id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
	    id == CSS_VAL_TOP || id == CSS_VAL_BOTTOM)
	    valid_primitive = true;
	break;

    case CSS_PROP_BORDER_COLLAPSE:      // collapse | separate | inherit
	if ( id == CSS_VAL_COLLAPSE || id == CSS_VAL_SEPARATE )
	    valid_primitive = true;
	break;

    case CSS_PROP_VISIBILITY:           // visible | hidden | collapse | inherit
	if (id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_COLLAPSE)
	    valid_primitive = true;
	break;

    case CSS_PROP_OVERFLOW:             // visible | hidden | scroll | auto | inherit
	if ( id == CSS_VAL_VISIBLE || id == CSS_VAL_HIDDEN || id == CSS_VAL_SCROLL || id == CSS_VAL_AUTO )
	    valid_primitive = true;
	break;

    case CSS_PROP_LIST_STYLE_POSITION:  // inside | outside | inherit
	if ( id == CSS_VAL_INSIDE || id == CSS_VAL_OUTSIDE )
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
	if ((id >= CSS_VAL_INLINE && id <= CSS_VAL__KHTML_INLINE_BOX) || id == CSS_VAL_NONE)
	    valid_primitive = true;
	break;

    case CSS_PROP_DIRECTION:            // ltr | rtl | inherit
	if ( id == CSS_VAL_LTR || id == CSS_VAL_RTL )
	    valid_primitive = true;
	break;

    case CSS_PROP_TEXT_TRANSFORM:       // capitalize | uppercase | lowercase | none | inherit
	if ((id >= CSS_VAL_CAPITALIZE && id <= CSS_VAL_LOWERCASE) || id == CSS_VAL_NONE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FLOAT:                // left | right | none | inherit + center for buggy CSS
	if ( id == CSS_VAL_LEFT || id == CSS_VAL_RIGHT ||
	     id == CSS_VAL_NONE || id == CSS_VAL_CENTER)
	    valid_primitive = true;
	break;

    case CSS_PROP_CLEAR:                // none | left | right | both | inherit
	if ( id == CSS_VAL_NONE || id == CSS_VAL_LEFT ||
	     id == CSS_VAL_RIGHT|| id == CSS_VAL_BOTH)
	    valid_primitive = true;
	break;

    case CSS_PROP_TEXT_ALIGN:
    	// left | right | center | justify | khtml_left | khtml_right | khtml_center | <string> | inherit
	if ( ( id >= CSS_VAL__KHTML_AUTO && id <= CSS_VAL__KHTML_CENTER ) ||
	     value->unit == CSSPrimitiveValue::CSS_STRING )
	    valid_primitive = true;
	break;

    case CSS_PROP_OUTLINE_STYLE:        // <border-style> | inherit
    case CSS_PROP_BORDER_TOP_STYLE:     //// <border-style> | inherit
    case CSS_PROP_BORDER_RIGHT_STYLE:   //   Defined as:    none | hidden | dotted | dashed |
    case CSS_PROP_BORDER_BOTTOM_STYLE:  //   solid | double | groove | ridge | inset | outset
    case CSS_PROP_BORDER_LEFT_STYLE:    ////
	if (id >= CSS_VAL_NONE && id <= CSS_VAL_RIDGE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FONT_WEIGHT:  // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 |
	// 500 | 600 | 700 | 800 | 900 | inherit
	if (id >= CSS_VAL_NORMAL && id <= CSS_VAL_900) {
	    // Allready correct id
	    valid_primitive = true;
	} else if ( validUnit( value, FInteger|FNonNeg, false ) ) {
	    int weight = (int)value->fValue;
	    if ( (weight % 100) )
		break;
	    weight /= 100;
	    if ( weight >= 1 && weight <= 9 ) {
		id = CSS_VAL_100 + weight - 1;
		valid_primitive = true;
	    }
	}
	break;

    case CSS_PROP_BACKGROUND_REPEAT:    // repeat | repeat-x | repeat-y | no-repeat | inherit
	if ( id >= CSS_VAL_REPEAT && id <= CSS_VAL_NO_REPEAT )
	    valid_primitive = true;
	break;

    case CSS_PROP_BACKGROUND_ATTACHMENT: // scroll | fixed
	if ( id == CSS_VAL_SCROLL || id == CSS_VAL_FIXED )
	    valid_primitive = true;
	break;

    case CSS_PROP_BACKGROUND_POSITION:
	if ( id ) {
	    /* Problem: center is ambigous
	     * In case of 'center' center defines X and Y coords
	     * In case of 'center top', center defines the Y coord
	     * in case of 'center left', center defines the X coord
	     */
	    int pos[2];
	    pos[0] = -1;
	    pos[1] = -1;
	    bool invalid = false;
	    switch( id ) {
	    case CSS_VAL_TOP:
		pos[1] = 0;
		break;
	    case CSS_VAL_BOTTOM:
		pos[1] = 100;
		break;
	    case CSS_VAL_LEFT:
		pos[0] = 0;
		break;
	    case CSS_VAL_RIGHT:
		pos[0] = 100;
		break;
	    case  CSS_VAL_CENTER:
		break;
	    default:
		invalid = true;
	    }
	    if ( invalid )
		break;
	    value = valueList->next();
	    if ( value ) {
	        id = value->id;
	        switch( id ) {
	        case CSS_VAL_TOP:
	            if ( pos[1] != -1 )
		        invalid = true;
	            pos[1] = 0;
	            break;
	        case CSS_VAL_BOTTOM:
	            if ( pos[1] != -1 )
		        invalid = true;
	            pos[1] = 100;
	            break;
	        case CSS_VAL_LEFT:
	            if ( pos[0] != -1 )
		        invalid = true;
	            pos[0] = 0;
	            break;
	        case CSS_VAL_RIGHT:
	            if ( pos[0] != -1 )
		        invalid = true;
	            pos[0] = 100;
	            break;
	        case  CSS_VAL_CENTER:
	            break;
	        default:
	            invalid = true;
	        }
	        if ( !invalid )
	            value = valueList->next();
	    }
	    if ( pos[0] == -1 )
		pos[0] = 50;
	    if ( pos[1] == -1 )
		pos[1] = 50;
	    addProperty( CSS_PROP_BACKGROUND_POSITION_X,
			 new CSSPrimitiveValueImpl( pos[0], CSSPrimitiveValue::CSS_PERCENTAGE ),
			 important );
	    addProperty( CSS_PROP_BACKGROUND_POSITION_Y,
			 new CSSPrimitiveValueImpl( pos[1], CSSPrimitiveValue::CSS_PERCENTAGE ),
			 important );
	} else {
	    bool ok = parseValue( CSS_PROP_BACKGROUND_POSITION_X, important );
	    if ( !ok )
		break;
	    value = valueList->current();
	    if ( value )
		ok = parseValue( CSS_PROP_BACKGROUND_POSITION_Y, important );
	    if ( !ok )
		addProperty( CSS_PROP_BACKGROUND_POSITION_Y,
			     new CSSPrimitiveValueImpl( 50, CSSPrimitiveValue::CSS_PERCENTAGE ),
			     important );
	}
	return true;

    case CSS_PROP_BACKGROUND_POSITION_X:
    case CSS_PROP_BACKGROUND_POSITION_Y:
	valid_primitive = validUnit( value, FPercent|FLength, strict&(!nonCSSHint) );
	break;

    case CSS_PROP_BORDER_SPACING:
	// ### should be able to have two values
	valid_primitive = ( validUnit( value, FLength|FNonNeg, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_SCROLLBAR_FACE_COLOR:         // IE5.5
    case CSS_PROP_SCROLLBAR_SHADOW_COLOR:       // IE5.5
    case CSS_PROP_SCROLLBAR_HIGHLIGHT_COLOR:    // IE5.5
    case CSS_PROP_SCROLLBAR_3DLIGHT_COLOR:      // IE5.5
    case CSS_PROP_SCROLLBAR_DARKSHADOW_COLOR:   // IE5.5
    case CSS_PROP_SCROLLBAR_TRACK_COLOR:        // IE5.5
    case CSS_PROP_SCROLLBAR_ARROW_COLOR:        // IE5.5
	if ( strict )
	    break;
	/* nobreak */
    case CSS_PROP_OUTLINE_COLOR:        // <color> | invert | inherit
	// outline has "invert" as additional keyword.
	if ( propId == CSS_PROP_OUTLINE_COLOR && id == CSS_VAL_INVERT ) {
	    valid_primitive = true;
            break;
	}
	/* nobreak */
    case CSS_PROP_BACKGROUND_COLOR:     // <color> | transparent | inherit
	if ( propId == CSS_PROP_BACKGROUND_COLOR && id == CSS_VAL_TRANSPARENT ) {
            valid_primitive = true;
            break;
	}
	/* nobreak */
    case CSS_PROP_BORDER_TOP_COLOR:     // <color> | transparent | inherit
    case CSS_PROP_BORDER_RIGHT_COLOR:   // <color> | transparent | inherit
    case CSS_PROP_BORDER_BOTTOM_COLOR:  // <color> | transparent | inherit
    case CSS_PROP_BORDER_LEFT_COLOR:    // <color> | transparent | inherit
        if (id == CSS_VAL_TRANSPARENT) {
            valid_primitive = true;
            break;
        }
        /* fall through */
    case CSS_PROP_COLOR:                // <color> | inherit
    case CSS_PROP_TEXT_DECORATION_COLOR:
        if (id == CSS_VAL__KHTML_TEXT)
            valid_primitive = true; // Always allow this, even when strict parsing is on,
                                    // since we use this in our UA sheets.
	else if ( id >= CSS_VAL_AQUA && id <= CSS_VAL_WINDOWTEXT || id == CSS_VAL_MENU ||
	     (id >= CSS_VAL_GREY && id < CSS_VAL__KHTML_TEXT && (nonCSSHint|!strict)) ) {
	    valid_primitive = true;
	} else {
	    parsedValue = parseColor();
	    if ( parsedValue )
		valueList->next();
	}
	break;

    case CSS_PROP_CURSOR:
	//  [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
	// nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | text |
	// wait | help ] ] | inherit
    // MSIE 5 compatibility :/
	if ( !strict && id == CSS_VAL_HAND )
	    valid_primitive = true;
	else if ( id >= CSS_VAL_AUTO && id <= CSS_VAL_HELP )
	    valid_primitive = true;
	break;

    case CSS_PROP_BACKGROUND_IMAGE:     // <uri> | none | inherit
    case CSS_PROP_LIST_STYLE_IMAGE:     // <uri> | none | inherit

	if ( id == CSS_VAL_NONE ) {
	    parsedValue = new CSSImageValueImpl();
	    valueList->next();
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "empty image " << endl;
#endif
	} else if ( value->unit == CSSPrimitiveValue::CSS_URI ) {
	    // ### allow string in non strict mode?
	    DOMString uri = khtml::parseURL( domString( value->string ) );
	    if ( !uri.isEmpty() ) {
		parsedValue = new CSSImageValueImpl(
		    DOMString(KURL( styleElement->baseURL().string(), uri.string()).url()),
		    styleElement );
		valueList->next();
#ifdef CSS_DEBUG
		kdDebug( 6080 ) << "image, url=" << uri.string() << " base=" << styleElement->baseURL().string() << endl;
#endif
	    }
	}
	break;

    case CSS_PROP_OUTLINE_WIDTH:        // <border-width> | inherit
    case CSS_PROP_BORDER_TOP_WIDTH:     //// <border-width> | inherit
    case CSS_PROP_BORDER_RIGHT_WIDTH:   //   Which is defined as
    case CSS_PROP_BORDER_BOTTOM_WIDTH:  //   thin | medium | thick | <length>
    case CSS_PROP_BORDER_LEFT_WIDTH:    ////
	if (id == CSS_VAL_THIN || id == CSS_VAL_MEDIUM || id == CSS_VAL_THICK)
	    valid_primitive = true;
        else
            valid_primitive = ( validUnit( value, FLength, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_MARKER_OFFSET:        // <length> | auto | inherit
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else
            valid_primitive = validUnit( value, FLength, strict&(!nonCSSHint) );
	break;

    case CSS_PROP_LETTER_SPACING:       // normal | <length> | inherit
    case CSS_PROP_WORD_SPACING:         // normal | <length> | inherit
	if ( id == CSS_VAL_NORMAL )
	    valid_primitive = true;
	else
            valid_primitive = validUnit( value, FLength, strict&(!nonCSSHint) );
	break;

    case CSS_PROP_TEXT_INDENT:          // <length> | <percentage> | inherit
    case CSS_PROP_PADDING_TOP:          //// <padding-width> | inherit
    case CSS_PROP_PADDING_RIGHT:        //   Which is defined as
    case CSS_PROP_PADDING_BOTTOM:       //   <length> | <percentage>
    case CSS_PROP_PADDING_LEFT:         ////
	valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_MAX_HEIGHT:           // <length> | <percentage> | none | inherit
    case CSS_PROP_MAX_WIDTH:            // <length> | <percentage> | none | inherit
	if ( id == CSS_VAL_NONE ) {
	    valid_primitive = true;
	    break;
	}
	/* nobreak */
    case CSS_PROP_MIN_HEIGHT:           // <length> | <percentage> | inherit
    case CSS_PROP_MIN_WIDTH:            // <length> | <percentage> | inherit
    	valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_FONT_SIZE:
    	// <absolute-size> | <relative-size> | <length> | <percentage> | inherit
	if (id >= CSS_VAL_XX_SMALL && id <= CSS_VAL_LARGER)
	    valid_primitive = true;
	else
	    valid_primitive = ( validUnit( value, FLength|FPercent, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_FONT_STYLE:           // normal | italic | oblique | inherit
	if ( id == CSS_VAL_NORMAL || id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE)
	    valid_primitive = true;
	break;

    case CSS_PROP_FONT_VARIANT:         // normal | small-caps | inherit
	if ( id == CSS_VAL_NORMAL || id == CSS_VAL_SMALL_CAPS)
	    valid_primitive = true;
	break;

    case CSS_PROP_VERTICAL_ALIGN:
    	// baseline | sub | super | top | text-top | middle | bottom | text-bottom |
	// <percentage> | <length> | inherit

	if ( id >= CSS_VAL_BASELINE && id <= CSS_VAL__KHTML_BASELINE_MIDDLE )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_HEIGHT:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_WIDTH:                // <length> | <percentage> | auto | inherit
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else
	    // ### handle multilength case where we allow relative units
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent|FNonNeg, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_BOTTOM:               // <length> | <percentage> | auto | inherit
    case CSS_PROP_LEFT:                 // <length> | <percentage> | auto | inherit
    case CSS_PROP_RIGHT:                // <length> | <percentage> | auto | inherit
    case CSS_PROP_TOP:                  // <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_TOP:           //// <margin-width> | inherit
    case CSS_PROP_MARGIN_RIGHT:         //   Which is defined as
    case CSS_PROP_MARGIN_BOTTOM:        //   <length> | <percentage> | auto | inherit
    case CSS_PROP_MARGIN_LEFT:          ////
	if ( id == CSS_VAL_AUTO )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FLength|FPercent, strict&(!nonCSSHint) ) );
	break;

    case CSS_PROP_Z_INDEX:              // auto | <integer> | inherit
	// qDebug("parsing z-index: id=%d, fValue=%f", id, value->fValue );
	if ( id == CSS_VAL_AUTO ) {
            valid_primitive = true;
            break;
	}
	/* nobreak */
    case CSS_PROP_ORPHANS:              // <integer> | inherit
    case CSS_PROP_WIDOWS:               // <integer> | inherit
	// ### not supported later on
	valid_primitive = ( !id && validUnit( value, FInteger, false ) );
	break;

    case CSS_PROP_LINE_HEIGHT:          // normal | <number> | <length> | <percentage> | inherit
	if ( id == CSS_VAL_NORMAL )
	    valid_primitive = true;
	else
	    valid_primitive = ( !id && validUnit( value, FNumber|FLength|FPercent, strict&(!nonCSSHint) ) );
	break;
#if 0
	// removed from CSS 2.1
    case CSS_PROP_COUNTER_INCREMENT:    // [ <identifier> <integer>? ]+ | none | inherit
    case CSS_PROP_COUNTER_RESET:        // [ <identifier> <integer>? ]+ | none | inherit
	if ( id == CSS_VAL_NONE )
	    valid_primitive = true;
	else {
            CSSValueListImpl *list = new CSSValueListImpl;
            int pos=0, pos2;
            while( 1 )
	    {
                pos2 = value.find(',', pos);
                QString face = value.mid(pos, pos2-pos);
                face = face.stripWhiteSpace();
                if(face.length() == 0) break;
                // ### single quoted is missing...
                if(face[0] == '\"') face.remove(0, 1);
                if(face[face.length()-1] == '\"') face = face.left(face.length()-1);
                //kdDebug( 6080 ) << "found face '" << face << "'" << endl;
                list->append(new CSSPrimitiveValueImpl(DOMString(face), CSSPrimitiveValue::CSS_STRING));
                pos = pos2 + 1;
                if(pos2 == -1) break;
	    }
            //kdDebug( 6080 ) << "got " << list->length() << " faces" << endl;
            if(list->length()) {
		parsedValue = list;
		valueList->next();
            } else
		delete list;
            break;
	}
#endif
    case CSS_PROP_FONT_FAMILY:
    	// [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
	parsedValue = parseFontFamily();
	break;
    }

    case CSS_PROP_TEXT_DECORATION:
    	// none | [ underline || overline || line-through || blink ] | inherit
	if (id == CSS_VAL_NONE) {
	    valid_primitive = true;
	} else {
	    CSSValueListImpl *list = new CSSValueListImpl;
	    bool is_valid = true;
	    while( is_valid && value ) {
		switch ( value->id ) {
		case CSS_VAL_BLINK:
		    break;
		case CSS_VAL_UNDERLINE:
		case CSS_VAL_OVERLINE:
		case CSS_VAL_LINE_THROUGH:
		    list->append( new CSSPrimitiveValueImpl( value->id ) );
		    break;
		default:
		    is_valid = false;
		}
		value = valueList->next();
	    }
	    //kdDebug( 6080 ) << "got " << list->length() << "d decorations" << endl;
	    if(list->length() && is_valid) {
                parsedValue = list;
		valueList->next();
	    } else {
                delete list;
	    }
	}
	break;

    case CSS_PROP_TABLE_LAYOUT:         // auto | fixed | inherit
	if ( id == CSS_VAL_AUTO || id == CSS_VAL_FIXED )
	    valid_primitive = true;
	break;

    case CSS_PROP__KHTML_FLOW_MODE:
	if ( id == CSS_VAL__KHTML_NORMAL || id == CSS_VAL__KHTML_AROUND_FLOATS )
	    valid_primitive = true;
	break;

    /* CSS3 properties */
    case CSS_PROP_TEXT_SHADOW: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
        if (id == CSS_VAL_NONE)
            valid_primitive = true;
        else
            return parseShadow(propId, important);
        break;
    case CSS_PROP__KHTML_OPACITY:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__KHTML_BOX_ALIGN:
        if (id == CSS_VAL_STRETCH || id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_BASELINE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_DIRECTION:
        if (id == CSS_VAL_NORMAL || id == CSS_VAL_REVERSE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_LINES:
        if (id == CSS_VAL_SINGLE || id == CSS_VAL_MULTIPLE)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_ORIENT:
        if (id == CSS_VAL_HORIZONTAL || id == CSS_VAL_VERTICAL ||
            id == CSS_VAL_INLINE_AXIS || id == CSS_VAL_BLOCK_AXIS)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_PACK:
        if (id == CSS_VAL_START || id == CSS_VAL_END ||
            id == CSS_VAL_CENTER || id == CSS_VAL_JUSTIFY)
            valid_primitive = true;
        break;
    case CSS_PROP__KHTML_BOX_FLEX:
        valid_primitive = validUnit(value, FNumber, strict);
        break;
    case CSS_PROP__KHTML_BOX_FLEX_GROUP:
    case CSS_PROP__KHTML_BOX_ORDINAL_GROUP:
        valid_primitive = validUnit(value, FInteger|FNonNeg, true);
        break;
    
	/* shorthand properties */
    case CSS_PROP_BACKGROUND:
    	// ['background-color' || 'background-image' ||'background-repeat' ||
	// 'background-attachment' || 'background-position'] | inherit
    {
#ifdef CSS_DEBUG_BCKGR
	kdDebug(6080) << "CSS_PROP_BACKGROUND" << endl;
#endif
	const int properties[5] = { CSS_PROP_BACKGROUND_IMAGE, CSS_PROP_BACKGROUND_REPEAT,
				    CSS_PROP_BACKGROUND_ATTACHMENT, CSS_PROP_BACKGROUND_POSITION,
				    CSS_PROP_BACKGROUND_COLOR };
	return parseShortHand(properties, 5, important);
    }
    case CSS_PROP_BORDER:
 	// [ 'border-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_WIDTH, CSS_PROP_BORDER_STYLE,
				    CSS_PROP_BORDER_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_TOP:
    	// [ 'border-top-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_TOP_STYLE,
				    CSS_PROP_BORDER_TOP_COLOR};
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_RIGHT:
    	// [ 'border-right-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH, CSS_PROP_BORDER_RIGHT_STYLE,
				    CSS_PROP_BORDER_RIGHT_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_BOTTOM:
    	// [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_BOTTOM_STYLE,
				    CSS_PROP_BORDER_BOTTOM_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_LEFT:
    	// [ 'border-left-width' || 'border-style' || <color> ] | inherit
    {
	const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH, CSS_PROP_BORDER_LEFT_STYLE,
				    CSS_PROP_BORDER_LEFT_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_OUTLINE:
    	// [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
    {
	const int properties[3] = { CSS_PROP_OUTLINE_WIDTH, CSS_PROP_OUTLINE_STYLE,
				    CSS_PROP_OUTLINE_COLOR };
	return parseShortHand(properties, 3, important);
    }
    case CSS_PROP_BORDER_COLOR:
    	// <color>{1,4} | transparent | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR, CSS_PROP_BORDER_RIGHT_COLOR,
				    CSS_PROP_BORDER_BOTTOM_COLOR, CSS_PROP_BORDER_LEFT_COLOR };
	return parse4Values(properties, important);
    }
    case CSS_PROP_BORDER_WIDTH:
    	// <border-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH, CSS_PROP_BORDER_RIGHT_WIDTH,
				    CSS_PROP_BORDER_BOTTOM_WIDTH, CSS_PROP_BORDER_LEFT_WIDTH };
	return parse4Values(properties, important);
    }
    case CSS_PROP_BORDER_STYLE:
    	// <border-style>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE, CSS_PROP_BORDER_RIGHT_STYLE,
				    CSS_PROP_BORDER_BOTTOM_STYLE, CSS_PROP_BORDER_LEFT_STYLE };
	return parse4Values(properties, important);
    }
    case CSS_PROP_MARGIN:
    	// <margin-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_MARGIN_TOP, CSS_PROP_MARGIN_RIGHT,
				    CSS_PROP_MARGIN_BOTTOM, CSS_PROP_MARGIN_LEFT };
	return parse4Values(properties, important);
    }
    case CSS_PROP_PADDING:
    	// <padding-width>{1,4} | inherit
    {
	const int properties[4] = { CSS_PROP_PADDING_TOP, CSS_PROP_PADDING_RIGHT,
				    CSS_PROP_PADDING_BOTTOM, CSS_PROP_PADDING_LEFT };
	return parse4Values(properties, important);
    }
    case CSS_PROP_FONT:
    	// [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
	// 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
	if ( id >= CSS_VAL_CAPTION && id <= CSS_VAL_STATUS_BAR )
	    valid_primitive = true;
	else
	    return parseFont(important);

    case CSS_PROP_LIST_STYLE:
    {
	const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE, CSS_PROP_LIST_STYLE_POSITION,
				    CSS_PROP_LIST_STYLE_IMAGE };
	return parseShortHand(properties, 3, important);
    }
    default:
// #ifdef CSS_DEBUG
// 	kdDebug( 6080 ) << "illegal or CSS2 Aural property: " << val << endl;
// #endif
	break;
    }

    if ( valid_primitive ) {
	if ( id != 0 ) {
	    // qDebug(" new value: id=%d", id );
	    parsedValue = new CSSPrimitiveValueImpl( id );
	} else if ( value->unit == CSSPrimitiveValue::CSS_STRING )
	    parsedValue = new CSSPrimitiveValueImpl( domString( value->string ),
						     (CSSPrimitiveValue::UnitTypes) value->unit );
	else if ( value->unit >= CSSPrimitiveValue::CSS_NUMBER &&
		  value->unit <= CSSPrimitiveValue::CSS_KHZ ) {
	    // qDebug(" new value: value=%.2f, unit=%d", value->fValue, value->unit );
	    parsedValue = new CSSPrimitiveValueImpl( value->fValue,
						     (CSSPrimitiveValue::UnitTypes) value->unit );
	} else if ( value->unit >= Value::Q_EMS ) {
	    // qDebug(" new quirks value: value=%.2f, unit=%d", value->fValue, value->unit );
	    parsedValue = new CSSQuirkPrimitiveValueImpl( value->fValue, CSSPrimitiveValue::CSS_EMS );
	}
	valueList->next();
    }
    if ( parsedValue ) {
	addProperty( propId, parsedValue, important );
	return true;
    }
    return false;
}

bool CSSParser::parseShortHand( const int *properties, int numProperties, bool important )
{
    /* We try to match as many properties as possible
     * We setup an array of booleans to mark which property has been found,
     * and we try to search for properties until it makes no longer any sense
     */
    inParseShortHand = true;

    bool found = false;
    bool fnd[6]; //Trust me ;)
    for( int i = 0; i < numProperties; i++ )
    	fnd[i] = false;

#ifdef CSS_DEBUG
    kdDebug(6080) << "PSH: numProperties=" << numProperties << endl;
#endif

    while ( valueList->current() ) {
        found = false;
	// qDebug("outer loop" );
        for (int propIndex = 0; !found && propIndex < numProperties; ++propIndex) {
            if (!fnd[propIndex]) {
#ifdef CSS_DEBUG
		kdDebug(6080) << "LOOKING FOR: " << getPropertyName(properties[propIndex]).string() << endl;
#endif
		if ( parseValue( properties[propIndex], important ) ) {
		    fnd[propIndex] = found = true;
#ifdef CSS_DEBUG
		    kdDebug(6080) << "FOUND: " << getPropertyName(properties[propIndex]).string() << endl;
#endif
		}
	    }
	}
        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found) {
#ifdef CSS_DEBUG
	    qDebug("didn't find anything" );
#endif
	    inParseShortHand = false;
	    return false;
	}
    }
    inParseShortHand = false;
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "parsed shorthand" << endl;
#endif
    return true;
}

bool CSSParser::parse4Values( const int *properties,  bool important )
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    int num = inParseShortHand ? 1 : valueList->numValues;
    // qDebug("parse4Values: num=%d", num );

    // the order is top, right, bottom, left
    switch( num ) {
    case 1: {
        if( !parseValue( properties[0], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-1]->value();
	addProperty( properties[1], value, important );
	addProperty( properties[2], value, important );
	addProperty( properties[3], value, important );
        return true;
    }
    case 2: {

        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[2], value, important );
	value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[3], value, important );
        return true;
    }
    case 3: {
        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
        if( !parseValue( properties[2], important ) ) return false;
	CSSValueImpl *value = parsedProperties[numParsedProperties-2]->value();
	addProperty( properties[3], value, important );
        return true;
    }
    case 4: {
        if( !parseValue( properties[0], important ) ) return false;
        if( !parseValue( properties[1], important ) ) return false;
        if( !parseValue( properties[2], important ) ) return false;
        if( !parseValue( properties[3], important ) ) return false;
	return true;
    }
    default:
        return false;
    }
}

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSParser::parseContent( int propId, bool important )
{
    CSSValueListImpl* values = new CSSValueListImpl();

    Value *val;
    CSSValueImpl *parsedValue = 0;
    while ( (val = valueList->current()) ) {
        if ( val->unit == CSSPrimitiveValue::CSS_URI ) {
            // url
	    DOMString value = khtml::parseURL(domString(val->string));
            parsedValue = new CSSImageValueImpl(
		DOMString(KURL( styleElement->baseURL().string(), value.string()).url() ), styleElement );
#ifdef CSS_DEBUG
	    kdDebug( 6080 ) << "content, url=" << value.string() << " base=" << styleElement->baseURL().string() << endl;
#endif
        } else if ( val->unit == Value::Function ) {
	    // attr( X )
            ValueList *args = val->function->args;
            QString fname = qString( val->function->name ).lower();
            if ( fname != "attr(" || !args )
                return false;
            if ( args->numValues != 1)
                return false;
            Value *a = args->current();
            parsedValue = new CSSPrimitiveValueImpl(domString(a->string), CSSPrimitiveValue::CSS_ATTR);
        } else if ( val->unit == CSSPrimitiveValue::CSS_IDENT ) {
            // open-quote
            // close-quote
            // no-open-quote
            // no-close-quote
        } else if ( val->unit == CSSPrimitiveValue::CSS_STRING ) {
            parsedValue = new CSSPrimitiveValueImpl(domString(val->string), CSSPrimitiveValue::CSS_STRING);
        }
        if (parsedValue)
            values->append(parsedValue);
	else
	    break;
	valueList->next();
    }
    if ( values->length() ) {
	addProperty( propId, values, important );
	valueList->next();
	return true;
    }
    delete values;
    return false;
}

bool CSSParser::parseShape( int propId, bool important )
{
    Value *value = valueList->current();
    ValueList *args = value->function->args;
    QString fname = qString( value->function->name ).lower();
    if ( fname != "rect(" || !args )
	return false;

    // rect( t, r, b, l ) || rect( t r b l )
    if ( args->numValues != 4 && args->numValues != 7 )
	return false;
    RectImpl *rect = new RectImpl();
    bool valid = true;
    int i = 0;
    Value *a = args->current();
    while ( a ) {
	valid = validUnit( a, FLength, strict );
	if ( !valid )
	    break;
	CSSPrimitiveValueImpl *length =
	    new CSSPrimitiveValueImpl( a->fValue, (CSSPrimitiveValue::UnitTypes) a->unit );
	if ( i == 0 )
	    rect->setTop( length );
	else if ( i == 1 )
	    rect->setRight( length );
	else if ( i == 2 )
	    rect->setBottom( length );
	else
	    rect->setLeft( length );
	a = args->next();
	if ( a && args->numValues == 7 ) {
	    if ( a->unit == Value::Operator && a->iValue == ',' ) {
		a = args->next();
	    } else {
		valid = false;
		break;
	    }
	}
	i++;
    }
    if ( valid ) {
	addProperty( propId, new CSSPrimitiveValueImpl( rect ), important );
	valueList->next();
	return true;
    }
    delete rect;
    return false;
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSParser::parseFont( bool important )
{
//     kdDebug(6080) << "parsing font property current=" << valueList->currentValue << endl;
    bool valid = true;
    Value *value = valueList->current();
    FontValueImpl *font = new FontValueImpl;
    // optional font-style, font-variant and font-weight
    while ( value ) {
// 	kdDebug( 6080 ) << "got value " << value->id << " / " << (value->unit == CSSPrimitiveValue::CSS_STRING ||
	// 				   value->unit == CSSPrimitiveValue::CSS_IDENT ? qString( value->string ) : QString::null )
// 			<< endl;
	int id = value->id;
	if ( id ) {
	    if ( id == CSS_VAL_NORMAL ) {
		// do nothing, it's the inital value for all three
	    }
	    /*
	      else if ( id == CSS_VAL_INHERIT ) {
	      // set all non set ones to inherit
	      // This is not that simple as the inherit could also apply to the following font-size.
	      // very ahrd to tell without looking ahead.
	      inherit = true;
		} */
	    else if ( id == CSS_VAL_ITALIC || id == CSS_VAL_OBLIQUE ) {
		if ( font->style )
		    goto invalid;
		font->style = new CSSPrimitiveValueImpl( id );
	    } else if ( id == CSS_VAL_SMALL_CAPS ) {
		if ( font->variant )
		    goto invalid;
		font->variant = new CSSPrimitiveValueImpl( id );
	    } else if ( id >= CSS_VAL_BOLD && id <= CSS_VAL_LIGHTER ) {
		if ( font->weight )
		    goto invalid;
		font->weight = new CSSPrimitiveValueImpl( id );
	    } else {
		valid = false;
	    }
	} else if ( !font->weight && validUnit( value, FInteger|FNonNeg, true ) ) {
	    int weight = (int)value->fValue;
	    int val = 0;
	    if ( weight == 100 )
		val = CSS_VAL_100;
	    else if ( weight == 200 )
		val = CSS_VAL_200;
	    else if ( weight == 300 )
		val = CSS_VAL_300;
	    else if ( weight == 400 )
		val = CSS_VAL_400;
	    else if ( weight == 500 )
		val = CSS_VAL_500;
	    else if ( weight == 600 )
		val = CSS_VAL_600;
	    else if ( weight == 700 )
		val = CSS_VAL_700;
	    else if ( weight == 800 )
		val = CSS_VAL_800;
	    else if ( weight == 900 )
		val = CSS_VAL_900;

	    if ( val )
		font->weight = new CSSPrimitiveValueImpl( val );
	    else
		valid = false;
	} else {
	    valid = false;
	}
	if ( !valid )
	    break;
	value = valueList->next();
    }
    if ( !value )
	goto invalid;

    // set undefined values to default
    if ( !font->style )
	font->style = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !font->variant )
	font->variant = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    if ( !font->weight )
	font->weight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );

//     kdDebug( 6080 ) << "  got style, variant and weight current=" << valueList->currentValue << endl;

    // now a font size _must_ come
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if ( value->id >= CSS_VAL_XX_SMALL && value->id <= CSS_VAL_LARGER )
	font->size = new CSSPrimitiveValueImpl( value->id );
    else if ( validUnit( value, FLength|FPercent, strict ) ) {
	font->size = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
    }
    value = valueList->next();
    if ( !font->size || !value )
	goto invalid;

    // kdDebug( 6080 ) << "  got size" << endl;

    if ( value->unit == Value::Operator && value->iValue == '/' ) {
	// line-height
	value = valueList->next();
	if ( !value )
	    goto invalid;
	if ( value->id == CSS_VAL_NORMAL ) {
	    // default value, nothing to do
	} else if ( validUnit( value, FNumber|FLength|FPercent, strict ) ) {
	    font->lineHeight = new CSSPrimitiveValueImpl( value->fValue, (CSSPrimitiveValue::UnitTypes) value->unit );
	} else {
	    goto invalid;
	}
	value = valueList->next();
	if ( !value )
	    goto invalid;
    } else {
	font->lineHeight = new CSSPrimitiveValueImpl( CSS_VAL_NORMAL );
    }

//     kdDebug( 6080 ) << "  got line height current=" << valueList->currentValue << endl;
    // font family must come now
    font->family = parseFontFamily();

    if ( valueList->current() || !font->family )
	goto invalid;
//     kdDebug( 6080 ) << "  got family, parsing ok!" << endl;

    addProperty( CSS_PROP_FONT, font, important );
    return true;

 invalid:
//     kdDebug(6080) << "   -> invalid" << endl;
    delete font;
    return false;
}

CSSValueListImpl *CSSParser::parseFontFamily()
{
//     kdDebug( 6080 ) << "CSSParser::parseFontFamily current=" << valueList->currentValue << endl;
    CSSValueListImpl *list = new CSSValueListImpl;
    Value *value = valueList->current();
    FontFamilyValueImpl* currFamily = 0;
    while ( value ) {
// 	kdDebug( 6080 ) << "got value " << value->id << " / "
// 			<< (value->unit == CSSPrimitiveValue::CSS_STRING ||
// 			    value->unit == CSSPrimitiveValue::CSS_IDENT ? qString( value->string ) : QString::null )
// 			<< endl;
        Value* nextValue = valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == Value::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSS_VAL_SERIF && nextValue->id <= CSS_VAL__KHTML_BODY) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        if (value->id >= CSS_VAL_SERIF && value->id <= CSS_VAL__KHTML_BODY) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new CSSPrimitiveValueImpl(value->id));
            else
                list->append(currFamily = new FontFamilyValueImpl(qString( value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            currFamily = 0;
            list->append(new FontFamilyValueImpl(qString( value->string)));
        }
        else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (currFamily) {
                currFamily->parsedFontName += ' ';
                currFamily->parsedFontName += qString(value->string);
            }
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(new FontFamilyValueImpl(qString( value->string)));
            else
                list->append(currFamily = new FontFamilyValueImpl(qString(value->string)));
        }
	else {
// 	    kdDebug( 6080 ) << "invalid family part" << endl;
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
    if ( !list->length() ) {
	delete list;
	list = 0;
    }
    return list;
}


static bool parseColor(const QString &name, QRgb& rgb)
{
    int len = name.length();

    if ( !len )
        return false;


    bool ok;

    if ( len == 3 || len == 6 ) {
	int val = name.toInt(&ok, 16);
	if ( ok ) {
            if (len == 6) {
		rgb =  (0xff << 24) | val;
                return true;
            }
            else if ( len == 3 ) {
		// #abc converts to #aabbcc according to the specs
		rgb = (0xff << 24) |
		    (val&0xf00)<<12 | (val&0xf00)<<8 |
		    (val&0xf0)<<8 | (val&0xf0)<<4 |
		    (val&0xf)<<4 | (val&0xf);
                return true;
            }
	}
    }

    // try a little harder
    QColor tc;
    tc.setNamedColor(name.lower());
    if (tc.isValid()) {
        rgb = tc.rgb();
        return true;
    }

    return false;
}


CSSPrimitiveValueImpl *CSSParser::parseColor()
{
    return parseColorFromValue(valueList->current());
}

CSSPrimitiveValueImpl *CSSParser::parseColorFromValue(Value* value)
{
    QRgb c = khtml::transparentColor;
    if ( !strict && value->unit == CSSPrimitiveValue::CSS_NUMBER &&
        value->fValue >= 0. && value->fValue < 1000000. ) {
        QString str;
        str.sprintf( "%06d", (int)(value->fValue+.5) );
        if (!::parseColor( str, c ))
            return 0;
    } else if ( value->unit == CSSPrimitiveValue::CSS_RGBCOLOR ||
                value->unit == CSSPrimitiveValue::CSS_IDENT ||
                value->unit == CSSPrimitiveValue::CSS_DIMENSION ) {
	if (!::parseColor( qString( value->string ), c))
            return 0;
    }
    else if ( value->unit == Value::Function &&
		value->function->args != 0 &&
		value->function->args->numValues == 5 /* rgb + two commas */ &&
		qString( value->function->name ).lower() == "rgb(" ) {
	ValueList *args = value->function->args;
	Value *v = args->current();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int r = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	v = args->next();
	if ( v->unit != Value::Operator && v->iValue != ',' )
	    return 0;
	v = args->next();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int g = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	v = args->next();
	if ( v->unit != Value::Operator && v->iValue != ',' )
	    return 0;
	v = args->next();
	if ( !validUnit( v, FInteger|FPercent, true ) )
	    return 0;
	int b = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
	r = QMAX( 0, QMIN( 255, r ) );
	g = QMAX( 0, QMIN( 255, g ) );
	b = QMAX( 0, QMIN( 255, b ) );
	c = qRgb( r, g, b );
    }
    else if ( value->unit == Value::Function &&
              value->function->args != 0 &&
              value->function->args->numValues == 7 /* rgba + three commas */ &&
              qString( value->function->name ).lower() == "rgba(" ) {
        ValueList *args = value->function->args;
        Value *v = args->current();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int r = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int g = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FInteger|FPercent, true ) )
            return 0;
        int b = (int) ( v->fValue * (v->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 256./100. : 1.) );
        v = args->next();
        if ( v->unit != Value::Operator && v->iValue != ',' )
            return 0;
        v = args->next();
        if ( !validUnit( v, FNumber, true ) )
            return 0;
        r = QMAX( 0, QMIN( 255, r ) );
        g = QMAX( 0, QMIN( 255, g ) );
        b = QMAX( 0, QMIN( 255, b ) );
        int a = (int)(QMAX( 0, QMIN( 1.0f, v->fValue ) ) * 255);
        c = qRgba( r, g, b, a );
    }
    else
        return 0;

    return new CSSPrimitiveValueImpl(c);
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
struct ShadowParseContext {
    ShadowParseContext()
    :values(0), x(0), y(0), blur(0), color(0),
     allowX(true), allowY(false), allowBlur(false), allowColor(true),
     allowBreak(true)
    {}

    ~ShadowParseContext() {
        if (!allowBreak) {
            delete values;
            delete x;
            delete y;
            delete blur;
            delete color;
        }
    }

    bool allowLength() { return allowX || allowY || allowBlur; }

    bool failed() { return allowBreak = false; }
    
    void commitValue() {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || color) {
            if (!values)
                values = new CSSValueListImpl();
            
            // Construct the current shadow value and add it to the list.
            values->append(new ShadowValueImpl(x, y, blur, color));
        }
        
        // Now reset for the next shadow value.
        x = y = blur = color = 0;
        allowX = allowColor = allowBreak = true;
        allowY = allowBlur = false;  
    }

    void commitLength(Value* v) {
        CSSPrimitiveValueImpl* val = new CSSPrimitiveValueImpl(v->fValue,
                                                               (CSSPrimitiveValue::UnitTypes)v->unit);
        if (allowX) {
            x = val;
            allowX = false; allowY = true; allowColor = false; allowBreak = false;
        }
        else if (allowY) {
            y = val;
            allowY = false; allowBlur = true; allowColor = true; allowBreak = true;
        }
        else if (allowBlur) {
            blur = val;
            allowBlur = false;
        }
    }

    void commitColor(CSSPrimitiveValueImpl* val) {
        color = val;
        allowColor = false;
        if (allowX)
            allowBreak = false;
        else
            allowBlur = false;
    }
    
    CSSValueListImpl* values;
    bool succeeded;
    CSSPrimitiveValueImpl* x;
    CSSPrimitiveValueImpl* y;
    CSSPrimitiveValueImpl* blur;
    CSSPrimitiveValueImpl* color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowColor;
    bool allowBreak;
};

bool CSSParser::parseShadow( int propId, bool important )
{
    ShadowParseContext context;
    Value* val;
    while ((val = valueList->current())) {
        // Check for a comma break first.
        if (val->unit == Value::Operator) {
            if (val->iValue != ',' || !context.allowBreak)
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return context.failed();
            
            // The value is good.  Commit it.
            context.commitValue();
        }
        // Check to see if we're a length.
        else if (validUnit(val, FLength, true)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return context.failed();

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(val);
        }
        else {
            // The only other type of value that's ok is a color value.
            CSSPrimitiveValueImpl* parsedColor = 0;
            bool isColor = (val->id >= CSS_VAL_AQUA && val->id <= CSS_VAL_WINDOWTEXT || val->id == CSS_VAL_MENU ||
                            (val->id >= CSS_VAL_GREY && val->id <= CSS_VAL__KHTML_TEXT && (nonCSSHint|!strict)));
            if (isColor) {
                if (!context.allowColor)
                    return context.failed();
                parsedColor = new CSSPrimitiveValueImpl(val->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColorFromValue(val);

            if (!parsedColor || !context.allowColor)
                return context.failed(); // This value is not a color or length and is invalid or
                                         // it is a color, but a color isn't allowed at this point.
            
            context.commitColor(parsedColor);
        }
        
        valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values->length()) {
            addProperty(propId, context.values, important);
            valueList->next();
            return true;
        }
    }
    
    return context.failed();
}

static inline int yyerror( const char *str ) {
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "CSS parse error " << str << endl;
#else
    Q_UNUSED( str );
#endif
    return 1;
}

#define END 0

#include "parser.h"

int DOM::CSSParser::lex( void *_yylval ) {
    YYSTYPE *yylval = (YYSTYPE *)_yylval;
    int token = lex();
    int length;
    unsigned short *t = text( &length );

#ifdef TOKEN_DEBUG
    qDebug("CSSTokenizer: got token %d: '%s'", token, token == END ? "" : QString( (QChar *)t, length ).latin1() );
#endif
    switch( token ) {
    case S:
    case SGML_CD:
    case INCLUDES:
    case DASHMATCH:
	break;

    case URI:
    case STRING:
    case IDENT:
    case HASH:
    case DIMEN:
    case UNICODERANGE:
    case FUNCTION:
	yylval->string.string = t;
	yylval->string.length = length;
	break;

    case IMPORT_SYM:
    case PAGE_SYM:
    case MEDIA_SYM:
    case FONT_FACE_SYM:
    case CHARSET_SYM:

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
    case NUMBER:
	yylval->val = QString( (QChar *)t, length ).toDouble();
	//qDebug("value = %s, converted=%.2f", QString( (QChar *)t, length ).latin1(), yylval->val );
	break;

    default:
	break;
    }

    return token;
}

static inline int toHex( char c ) {
    if ( '0' <= c && c <= '9' )
	return c - '0';
    if ( 'a' <= c && c <= 'f' )
	return c - 'a' + 10;
    if ( 'A' <= c && c<= 'F' )
	return c - 'A' + 10;
    return 0;
}

unsigned short *DOM::CSSParser::text(int *length)
{
    unsigned short *start = yytext;
    int l = yyleng;
    switch( yyTok ) {
    case STRING:
	l--;
	/* nobreak */
    case HASH:
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
	while ( l &&
		(*start == ' ' || *start == '\t' || *start == '\r' ||
		 *start == '\n' || *start == '\f' ) ) {
	    start++; l--;
	}
	if ( *start == '"' || *start == '\'' ) {
	    start++; l--;
	}
	while ( l &&
		(start[l-1] == ' ' || start[l-1] == '\t' || start[l-1] == '\r' ||
		 start[l-1] == '\n' || start[l-1] == '\f' ) ) {
	    l--;
	}
	if ( l && (start[l-1] == '\"' || start[l-1] == '\'' ) )
	     l--;

    default:
	break;
    }

    // process escapes
    unsigned short *out = start;
    unsigned short *escape = 0;

    for ( int i = 0; i < l; i++ ) {
        unsigned short *current = start+i;
        if ( escape == current - 1 ) {
            if ( ( *current >= '0' && *current <= '9' ) ||
                 ( *current >= 'a' && *current <= 'f' ) ||
                 ( *current >= 'A' && *current <= 'F' ) )
                continue;
            if ( yyTok == STRING &&
                 ( *current == '\n' || *current == '\r' || *current == '\f' ) ) {
                // ### handle \r\n case
                if ( *current != '\r' )
                    escape = 0;
                continue;
            }
            // in all other cases copy the char to output
            // ###
            *out++ = *current;
            escape = 0;
            continue;
        }
        if ( escape == current - 2 && yyTok == STRING &&
             *(current-1) == '\r' && *current == '\n' ) {
            escape = 0;
            continue;
        }
        if ( escape > current - 7 &&
             ( ( *current >= '0' && *current <= '9' ) ||
               ( *current >= 'a' && *current <= 'f' ) ||
               ( *current >= 'A' && *current <= 'F' ) ) )
            continue;
        if ( escape ) {
            // add escaped char
            int uc = 0;
            escape++;
            while ( escape < current ) {
                // 		qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
                uc *= 16;
                uc += toHex( *escape );
                escape++;
            }
            // 	    qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).latin1(), uc );
            // can't handle chars outside ucs2
            if ( uc > 0xffff )
                uc = 0xfffd;
            *(out++) = (unsigned short)uc;
            escape = 0;
            if ( *current == ' ' ||
                 *current == '\t' ||
                 *current == '\r' ||
                 *current == '\n' ||
                 *current == '\f' )
                continue;
        }
        if ( !escape && *current == '\\' ) {
            escape = current;
            continue;
        }
        *(out++) = *current;
    }
    if ( escape ) {
        // add escaped char
        int uc = 0;
        escape++;
        while ( escape < start+l ) {
            // 		qDebug("toHex( %c = %x", (char)*escape, toHex( *escape ) );
            uc *= 16;
            uc += toHex( *escape );
            escape++;
        }
        // 	    qDebug(" converting escape: string='%s', value=0x%x", QString( (QChar *)e, current-e ).latin1(), uc );
        // can't handle chars outside ucs2
        if ( uc > 0xffff )
            uc = 0xfffd;
        *(out++) = (unsigned short)uc;
    }
    
    *length = out - start;
    return start;
}


#define YY_DECL int DOM::CSSParser::lex()
#define yyconst const
typedef int yy_state_type;
typedef unsigned int YY_CHAR;
// this line makes sure we treat all Unicode chars correctly.
#define YY_SC_TO_UI(c) (c > 0xff ? 0xff : c)
#define YY_DO_BEFORE_ACTION \
	yytext = yy_bp; \
	yyleng = (int) (yy_cp - yy_bp); \
	yy_hold_char = *yy_cp; \
	*yy_cp = 0; \
	yy_c_buf_p = yy_cp;
#define YY_BREAK break;
#define ECHO qDebug( "%s", QString( (QChar *)yytext, yyleng ).latin1() )
#define YY_RULE_SETUP
#define INITIAL 0
#define YY_STATE_EOF(state) (YY_END_OF_BUFFER + state + 1)
#define yyterminate() yyTok = END; return yyTok
#define YY_FATAL_ERROR(a) qFatal(a)
#define BEGIN yy_start = 1 + 2 *
#define COMMENT 1

#include "tokenizer.cpp"
