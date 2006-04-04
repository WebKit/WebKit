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
#include "DeprecatedString.h"
#include <kxmlcore/HashSet.h>
#include <kxmlcore/Vector.h>

namespace KXMLCore {
    template <typename T> class PassRefPtr;
}
using KXMLCore::PassRefPtr;

namespace WebCore {

    class CSSMutableStyleDeclaration;
    class CSSPrimitiveValue;
    class CSSProperty;
    class CSSRule;
    class CSSRuleList;
    class CSSSelector;
    class CSSStyleSheet;
    class CSSValue;
    class CSSValueList;
    class Document;
    class MediaList;
    class StyleBase;
    class StyleList;

    struct ParseString {
        unsigned short* string;
        int length;
        
        void lower();
    };

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

    static inline DeprecatedString deprecatedString(const ParseString& ps) {
        return DeprecatedString((QChar *)ps.string, ps.length);
    }
    static inline String domString(const ParseString& ps) {
        return String((QChar *)ps.string, ps.length);
    }
    static inline AtomicString atomicString(const ParseString& ps) {
        return AtomicString(ps.string, ps.length);
    }

    class ValueList {
    public:
        ValueList() : m_current(0) { }
        ~ValueList();
        void addValue(const Value& v) { m_values.append(v); }
        unsigned size() const { return m_values.size(); }
        Value* current() { return m_current < m_values.size() ? &m_values[m_current] : 0; }
        Value* next() { ++m_current; return current(); }
    private:
        Vector<Value, 16> m_values;
        unsigned m_current;
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

        void parseSheet(CSSStyleSheet*, const String&);
        PassRefPtr<CSSRule> parseRule(CSSStyleSheet*, const String&);
        bool parseValue(CSSMutableStyleDeclaration*, int id, const String&, bool important);
        static RGBA32 parseColor(const String&);
        bool parseColor(CSSMutableStyleDeclaration*, const String&);
        bool parseDeclaration(CSSMutableStyleDeclaration*, const String&);

        static CSSParser* current() { return currentParser; }

        Document* document() const;

        void addProperty(int propId, CSSValue*, bool important);
        void rollbackLastProperties(int num);
        bool hasProperties() const { return numParsedProperties > 0; }

        bool parseValue(int propId, bool important);
        bool parseShorthand(int propId, const int* properties, int numProperties, bool important);
        bool parse4Values(int propId, const int* properties, bool important);
        bool parseContent(int propId, bool important);

        CSSValue* parseBackgroundColor();
        CSSValue* parseBackgroundImage();
        CSSValue* parseBackgroundPositionXY(bool& xFound, bool& yFound);
        void parseBackgroundPosition(CSSValue*& value1, CSSValue*& value2);
        
        bool parseBackgroundProperty(int propId, int& propId1, int& propId2, CSSValue*& retValue1, CSSValue*& retValue2);
        bool parseBackgroundShorthand(bool important);

        void addBackgroundValue(CSSValue*& lval, CSSValue* rval);
      
#if __APPLE__
        bool parseDashboardRegions(int propId, bool important);
#endif

        bool parseShape(int propId, bool important);
        bool parseFont(bool important);
        CSSValueList* parseFontFamily();
        CSSPrimitiveValue* parseColor();
        CSSPrimitiveValue* parseColorFromValue(Value*);
        
#if SVG_SUPPORT
        bool parseSVGValue(int propId, bool important);
        CSSValue* parseSVGPaint();
        CSSValue* parseSVGColor();
        CSSValue* parseSVGStrokeDasharray();
#endif

        static bool parseColor(const DeprecatedString&, RGBA32& rgb);

        // CSS3 Parsing Routines (for properties specific to CSS3)
        bool parseShadow(int propId, bool important);
        bool parseBorderImage(int propId, bool important);

        int yyparse();

        CSSSelector* createFloatingSelector();
        CSSSelector* sinkFloatingSelector(CSSSelector*);

        ValueList* createFloatingValueList();
        ValueList* sinkFloatingValueList(ValueList*);

        Function* createFloatingFunction();
        Function* sinkFloatingFunction(Function*);

        Value& sinkFloatingValue(Value&);

        MediaList* createMediaList();
        CSSRule* createImportRule(const ParseString&, MediaList*);
        CSSRule* createMediaRule(MediaList*, CSSRuleList*);
        CSSRuleList* createRuleList();
        CSSRule* createStyleRule(CSSSelector*);

    public:
        bool strict;
        bool important;
        int id;
        StyleList* styleElement;
        RefPtr<CSSRule> rule;
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
        void clearProperties();

        void setupParser(const char* prefix, const String&, const char* suffix);

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

        Vector<RefPtr<StyleBase> > m_parsedStyleObjects;
        Vector<RefPtr<CSSRuleList> > m_parsedRuleLists;
        HashSet<CSSSelector*> m_floatingSelectors;
        HashSet<ValueList*> m_floatingValueLists;
        HashSet<Function*> m_floatingFunctions;

        // defines units allowed for a certain property, used in parseUnit
        enum Units {
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

        friend inline Units operator|(Units a, Units b)
            { return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }

        static bool validUnit(Value*, Units, bool strict);
    };

} // namespace

#endif
