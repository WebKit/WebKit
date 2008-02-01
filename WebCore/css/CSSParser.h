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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSParser_h
#define CSSParser_h

#include "AtomicString.h"
#include "Color.h"
#include "MediaQuery.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

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
    class MediaList;
    class MediaQueryExp;
    class StyleBase;
    class StyleList;
    struct Function;

    struct ParseString {
        UChar* characters;
        int length;

        void lower();
    };

    struct Value {
        int id;
        bool isInt;
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

    DeprecatedString deprecatedString(const ParseString&);

    static inline String domString(const ParseString& ps)
    {
        return String(ps.characters, ps.length);
    }

    static inline AtomicString atomicString(const ParseString& ps)
    {
        return AtomicString(ps.characters, ps.length);
    }

    class ValueList {
    public:
        ValueList() : m_current(0) { }
        ~ValueList();

        void addValue(const Value& v) { m_values.append(v); }
        unsigned size() const { return m_values.size(); }
        Value* current() { return m_current < m_values.size() ? &m_values[m_current] : 0; }
        Value* next() { ++m_current; return current(); }

        Value* valueAt(unsigned i) { return i < m_values.size() ? &m_values[i] : 0; }
        void deleteValueAt(unsigned i) { m_values.remove(i); }

    private:
        Vector<Value, 16> m_values;
        unsigned m_current;
    };

    struct Function {
        ParseString name;
        ValueList* args;

        ~Function() { delete args; }
    };

    class CSSParser {
    public:
        CSSParser(bool strictParsing = true);
        ~CSSParser();

        void parseSheet(CSSStyleSheet*, const String&);
        PassRefPtr<CSSRule> parseRule(CSSStyleSheet*, const String&);
        bool parseValue(CSSMutableStyleDeclaration*, int propId, const String&, bool important);
        static bool parseColor(RGBA32& color, const String&, bool strict = false);
        bool parseColor(CSSMutableStyleDeclaration*, const String&);
        bool parseDeclaration(CSSMutableStyleDeclaration*, const String&);
        bool parseMediaQuery(MediaList*, const String&);

        static CSSParser* current() { return currentParser; }

        Document* document() const;

        void addProperty(int propId, PassRefPtr<CSSValue>, bool important);
        void rollbackLastProperties(int num);
        bool hasProperties() const { return numParsedProperties > 0; }

        bool parseValue(int propId, bool important);
        bool parseShorthand(int propId, const int* properties, int numProperties, bool important);
        bool parse4Values(int propId, const int* properties, bool important);
        bool parseContent(int propId, bool important);

        PassRefPtr<CSSValue> parseBackgroundColor();
        bool parseBackgroundImage(RefPtr<CSSValue>&);
        PassRefPtr<CSSValue> parseBackgroundPositionXY(bool& xFound, bool& yFound);
        void parseBackgroundPosition(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        PassRefPtr<CSSValue> parseBackgroundSize();
        
        bool parseBackgroundProperty(int propId, int& propId1, int& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        bool parseBackgroundShorthand(bool important);

        void addBackgroundValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

        void addTransitionValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);
        PassRefPtr<CSSValue> parseTransitionDuration();
        PassRefPtr<CSSValue> parseTransitionRepeatCount();
        PassRefPtr<CSSValue> parseTransitionTimingFunction();
        bool parseTimingFunctionValue(ValueList*& args, double& result);
        PassRefPtr<CSSValue> parseTransitionProperty();
        bool parseTransitionProperty(int propId, RefPtr<CSSValue>&);
        bool parseTransitionShorthand(bool important);
        
        bool parseDashboardRegions(int propId, bool important);

        bool parseShape(int propId, bool important);

        bool parseFont(bool important);
        PassRefPtr<CSSValueList> parseFontFamily();

        bool parseCounter(int propId, int defaultValue, bool important);
        PassRefPtr<CSSValue> parseCounterContent(ValueList* args, bool counters);

        bool parseColorParameters(Value*, int* colorValues, bool parseAlpha);
        bool parseHSLParameters(Value*, double* colorValues, bool parseAlpha);
        PassRefPtr<CSSPrimitiveValue> parseColor(Value* = 0);
        bool parseColorFromValue(Value*, RGBA32&, bool = false);

        static bool parseColor(const String&, RGBA32& rgb, bool strict);

        bool parseFontFaceSrc();
        bool parseFontFaceUnicodeRange();

#if ENABLE(SVG)
        bool parseSVGValue(int propId, bool important);
        PassRefPtr<CSSValue> parseSVGPaint();
        PassRefPtr<CSSValue> parseSVGColor();
        PassRefPtr<CSSValue> parseSVGStrokeDasharray();
#endif

        // CSS3 Parsing Routines (for properties specific to CSS3)
        bool parseShadow(int propId, bool important);
        bool parseBorderImage(int propId, bool important);
        
        PassRefPtr<CSSValue> parseTransform();
        bool parseTransformOrigin(int propId, int& propId1, int& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        
        int yyparse();

        CSSSelector* createFloatingSelector();
        CSSSelector* sinkFloatingSelector(CSSSelector*);

        ValueList* createFloatingValueList();
        ValueList* sinkFloatingValueList(ValueList*);

        Function* createFloatingFunction();
        Function* sinkFloatingFunction(Function*);

        Value& sinkFloatingValue(Value&);

        MediaList* createMediaList();
        CSSRule* createCharsetRule(const ParseString&);
        CSSRule* createImportRule(const ParseString&, MediaList*);
        CSSRule* createMediaRule(MediaList*, CSSRuleList*);
        CSSRuleList* createRuleList();
        CSSRule* createStyleRule(CSSSelector*);
        CSSRule* createFontFaceRule();

        MediaQueryExp* createFloatingMediaQueryExp(const AtomicString&, ValueList*);
        MediaQueryExp* sinkFloatingMediaQueryExp(MediaQueryExp*);
        Vector<MediaQueryExp*>* createFloatingMediaQueryExpList();
        Vector<MediaQueryExp*>* sinkFloatingMediaQueryExpList(Vector<MediaQueryExp*>*);
        MediaQuery* createFloatingMediaQuery(MediaQuery::Restrictor, const String&, Vector<MediaQueryExp*>*);
        MediaQuery* sinkFloatingMediaQuery(MediaQuery*);

    public:
        bool strict;
        bool important;
        int id;
        StyleList* styleElement;
        RefPtr<CSSRule> rule;
        MediaQuery* mediaQuery;
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
        UChar* text(int* length);
        int lex();
        
    private:
        void clearProperties();

        void setupParser(const char* prefix, const String&, const char* suffix);

        bool inShorthand() const { return m_inParseShorthand; }

        void checkForOrphanedUnits();
        
        UChar* data;
        UChar* yytext;
        UChar* yy_c_buf_p;
        UChar yy_hold_char;
        int yy_last_accepting_state;
        UChar* yy_last_accepting_cpos;
        int yyleng;
        int yyTok;
        int yy_start;

        Vector<RefPtr<StyleBase> > m_parsedStyleObjects;
        Vector<RefPtr<CSSRuleList> > m_parsedRuleLists;
        HashSet<CSSSelector*> m_floatingSelectors;
        HashSet<ValueList*> m_floatingValueLists;
        HashSet<Function*> m_floatingFunctions;

        MediaQuery* m_floatingMediaQuery;
        MediaQueryExp* m_floatingMediaQueryExp;
        Vector<MediaQueryExp*>* m_floatingMediaQueryExpList;

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
        {
            return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
        }

        static bool validUnit(Value*, Units, bool strict);
        
        friend class TransformOperationInfo;
    };

} // namespace WebCore

#endif // CSSParser_h
