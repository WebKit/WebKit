/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef CSS_cssparser_h_
#define CSS_cssparser_h_

#include "AtomicString.h"
#include "Color.h"
#include "QString.h"

namespace KXMLCore {
    template <typename T> class PassRefPtr;
}
using KXMLCore::PassRefPtr;

namespace WebCore {

    class StyleListImpl;
    class CSSStyleSheetImpl;
    class CSSRuleImpl;
    class CSSStyleRuleImpl;
    class DocumentImpl;
    class NodeImpl;
    class CSSValueImpl;
    class CSSValueListImpl;
    class CSSPrimitiveValueImpl;
    class CSSMutableStyleDeclarationImpl;
    class CSSProperty;
    class CSSRuleListImpl;

    struct ParseString {
	unsigned short* string;
	int length;
        
        void lower();
    };

    struct Value;
    class ValueList;
    struct Function;
    
    struct Value {
	int id;
	union {
	    double fValue;
	    int iValue;
	    ParseString string;
	    Function* function;
	};
	enum {
	    Operator = 0x100000,
	    Function = 0x100001,
	    Q_EMS    = 0x100002
	};
	int unit;
    };

    static inline QString qString(const ParseString& ps) {
	return QString((QChar *)ps.string, ps.length);
    }
    static inline String domString(const ParseString& ps) {
	return String((QChar *)ps.string, ps.length);
    }
    static inline AtomicString atomicString(const ParseString& ps) {
	return AtomicString(ps.string, ps.length);
    }

    class ValueList {
    public:
	ValueList();
	~ValueList();
	void addValue(const Value&);
	Value* current() { return currentValue < numValues ? values + currentValue : 0; }
	Value* next() { ++currentValue; return current(); }
	Value* values;
	int numValues;
	int maxValues;
	int currentValue;
    };

    struct Function {
        ParseString name;
        ValueList* args;

        ~Function() { delete args; }
    };
    
    class CSSParser
    {
    public:
	CSSParser(bool strictParsing = true);
	~CSSParser();

	void parseSheet(CSSStyleSheetImpl*, const String&);
	CSSRuleImpl* parseRule(CSSStyleSheetImpl*, const String&);
	bool parseValue(CSSMutableStyleDeclarationImpl*, int id, const String&, bool important);
        static RGBA32 parseColor(const String&);
	bool parseColor(CSSMutableStyleDeclarationImpl*, const String&);
	bool parseDeclaration(CSSMutableStyleDeclarationImpl*, const String&);

	static CSSParser* current() { return currentParser; }

	DocumentImpl* document() const;

	void addProperty(int propId, CSSValueImpl*, bool important);
	bool hasProperties() const { return numParsedProperties > 0; }
	PassRefPtr<CSSMutableStyleDeclarationImpl> createStyleDeclaration(CSSStyleRuleImpl*);
	void clearProperties();

	bool parseValue(int propId, bool important);
	bool parseShorthand(int propId, const int* properties, int numProperties, bool important);
	bool parse4Values(int propId, const int* properties, bool important);
	bool parseContent(int propId, bool important);

        CSSValueImpl* parseBackgroundColor();
        CSSValueImpl* parseBackgroundImage();
        CSSValueImpl* parseBackgroundPositionXY(bool& xFound, bool& yFound);
        void parseBackgroundPosition(CSSValueImpl*& value1, CSSValueImpl*& value2);
        
        bool parseBackgroundProperty(int propId, int& propId1, int& propId2, CSSValueImpl*& retValue1, CSSValueImpl*& retValue2);
        bool parseBackgroundShorthand(bool important);

        void addBackgroundValue(CSSValueImpl*& lval, CSSValueImpl* rval);
      
#if __APPLE__
	bool parseDashboardRegions(int propId, bool important);
#endif

	bool parseShape(int propId, bool important);
	bool parseFont(bool important);
	CSSValueListImpl* parseFontFamily();
        CSSPrimitiveValueImpl* parseColor();
	CSSPrimitiveValueImpl* parseColorFromValue(Value*);
        
#if SVG_SUPPORT
        bool parseSVGValue(int propId, bool important);
        CSSValueImpl* parseSVGPaint();
        CSSValueImpl* parseSVGColor();
        CSSValueImpl* parseSVGStrokeDasharray();
#endif

        static bool parseColor(const QString&, RGBA32& rgb);

        // CSS3 Parsing Routines (for properties specific to CSS3)
        bool parseShadow(int propId, bool important);
        bool parseBorderImage(int propId, bool important);

	int yyparse(void);

    public:
	bool strict;
	bool important;
	int id;
	StyleListImpl* styleElement;
        CSSRuleImpl* rule;
	ValueList* valueList;
	CSSProperty** parsedProperties;
	int numParsedProperties;
	int maxParsedProperties;
	
        int m_inParseShorthand;
        int m_currentShorthand;
        bool m_implicitShorthand;

        AtomicString defaultNamespace;
        
	static CSSParser* currentParser;

	// tokenizer methods and data
    public:
	int lex(void* yylval);
	int token() { return yyTok; }
	unsigned short* text(int* length);
	int lex();
        
    private:
        void setupParser(const char* prefix, const String&, const char* suffix);
        void enterShorthand(int propId)
        {
            if (!(m_inParseShorthand++))
                m_currentShorthand = propId;
        }
        void exitShorthand()
        {
            if (!(--m_inParseShorthand))
                m_currentShorthand = 0;
        }
        bool inShorthand() const { return m_inParseShorthand; }

	unsigned short* data;
	unsigned short* yytext;
	unsigned short* yy_c_buf_p;
	unsigned short yy_hold_char;
	int yy_last_accepting_state;
	unsigned short* yy_last_accepting_cpos;
	int yyleng;
	int yyTok;
	int yy_start;
    };

} // namespace

#endif
