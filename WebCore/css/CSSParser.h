/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSParser_h
#define CSSParser_h

#include "AtomicString.h"
#include "Color.h"
#include "CSSParserValues.h"
#include "CSSSelectorList.h"
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
    class CSSVariablesDeclaration;
    class Document;
    class MediaList;
    class MediaQueryExp;
    class StyleBase;
    class StyleList;
    class WebKitCSSKeyframeRule;
    class WebKitCSSKeyframesRule;

    class CSSParser {
    public:
        CSSParser(bool strictParsing = true);
        ~CSSParser();

        void parseSheet(CSSStyleSheet*, const String&);
        PassRefPtr<CSSRule> parseRule(CSSStyleSheet*, const String&);
        PassRefPtr<CSSRule> parseKeyframeRule(CSSStyleSheet*, const String&);
        bool parseValue(CSSMutableStyleDeclaration*, int propId, const String&, bool important);
        static bool parseColor(RGBA32& color, const String&, bool strict = false);
        bool parseColor(CSSMutableStyleDeclaration*, const String&);
        bool parseDeclaration(CSSMutableStyleDeclaration*, const String&);
        bool parseMediaQuery(MediaList*, const String&);

        Document* document() const;

        void addProperty(int propId, PassRefPtr<CSSValue>, bool important);
        void rollbackLastProperties(int num);
        bool hasProperties() const { return m_numParsedProperties > 0; }

        bool parseValue(int propId, bool important);
        bool parseShorthand(int propId, const int* properties, int numProperties, bool important);
        bool parse4Values(int propId, const int* properties, bool important);
        bool parseContent(int propId, bool important);

        PassRefPtr<CSSValue> parseAttr(CSSParserValueList* args);

        PassRefPtr<CSSValue> parseBackgroundColor();

        bool parseFillImage(RefPtr<CSSValue>&);
        PassRefPtr<CSSValue> parseFillPositionXY(bool& xFound, bool& yFound);
        void parseFillPosition(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        PassRefPtr<CSSValue> parseFillSize();
        
        bool parseFillProperty(int propId, int& propId1, int& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        bool parseFillShorthand(int propId, const int* properties, int numProperties, bool important);

        void addFillValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

        void addAnimationValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

        PassRefPtr<CSSValue> parseAnimationDelay();
        PassRefPtr<CSSValue> parseAnimationDirection();
        PassRefPtr<CSSValue> parseAnimationDuration();
        PassRefPtr<CSSValue> parseAnimationIterationCount();
        PassRefPtr<CSSValue> parseAnimationName();
        PassRefPtr<CSSValue> parseAnimationProperty();
        PassRefPtr<CSSValue> parseAnimationTimingFunction();

        void parseTransformOriginShorthand(RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        bool parseTimingFunctionValue(CSSParserValueList*& args, double& result);
        bool parseAnimationProperty(int propId, RefPtr<CSSValue>&);
        bool parseTransitionShorthand(bool important);
        bool parseAnimationShorthand(bool important);
        
        bool parseDashboardRegions(int propId, bool important);

        bool parseShape(int propId, bool important);

        bool parseFont(bool important);
        PassRefPtr<CSSValueList> parseFontFamily();

        bool parseCounter(int propId, int defaultValue, bool important);
        PassRefPtr<CSSValue> parseCounterContent(CSSParserValueList* args, bool counters);

        bool parseColorParameters(CSSParserValue*, int* colorValues, bool parseAlpha);
        bool parseHSLParameters(CSSParserValue*, double* colorValues, bool parseAlpha);
        PassRefPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = 0);
        bool parseColorFromValue(CSSParserValue*, RGBA32&, bool = false);
        void parseSelector(const String&, Document* doc, CSSSelectorList&);

        static bool parseColor(const String&, RGBA32& rgb, bool strict);

        bool parseFontStyle(bool important);
        bool parseFontVariant(bool important);
        bool parseFontWeight(bool important);
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
        bool parseBorderImage(int propId, bool important, RefPtr<CSSValue>&);
        
        bool parseReflect(int propId, bool important);

        // Image generators
        bool parseCanvas(RefPtr<CSSValue>&);
        bool parseGradient(RefPtr<CSSValue>&);

        PassRefPtr<CSSValueList> parseTransform();
        bool parseTransformOrigin(int propId, int& propId1, int& propId2, int& propId3, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        bool parsePerspectiveOrigin(int propId, int& propId1, int& propId2,  RefPtr<CSSValue>&, RefPtr<CSSValue>&);
        bool parseVariable(CSSVariablesDeclaration*, const String& variableName, const String& variableValue);
        void parsePropertyWithResolvedVariables(int propId, bool important, CSSMutableStyleDeclaration*, CSSParserValueList*);

        int yyparse();

        CSSSelector* createFloatingSelector();
        CSSSelector* sinkFloatingSelector(CSSSelector*);

        CSSParserValueList* createFloatingValueList();
        CSSParserValueList* sinkFloatingValueList(CSSParserValueList*);

        CSSParserFunction* createFloatingFunction();
        CSSParserFunction* sinkFloatingFunction(CSSParserFunction*);

        CSSParserValue& sinkFloatingValue(CSSParserValue&);

        MediaList* createMediaList();
        CSSRule* createCharsetRule(const CSSParserString&);
        CSSRule* createImportRule(const CSSParserString&, MediaList*);
        WebKitCSSKeyframeRule* createKeyframeRule(CSSParserValueList*);
        WebKitCSSKeyframesRule* createKeyframesRule();
        CSSRule* createMediaRule(MediaList*, CSSRuleList*);
        CSSRuleList* createRuleList();
        CSSRule* createStyleRule(Vector<CSSSelector*>* selectors);
        CSSRule* createFontFaceRule();
        CSSRule* createVariablesRule(MediaList*, bool variablesKeyword);

        MediaQueryExp* createFloatingMediaQueryExp(const AtomicString&, CSSParserValueList*);
        MediaQueryExp* sinkFloatingMediaQueryExp(MediaQueryExp*);
        Vector<MediaQueryExp*>* createFloatingMediaQueryExpList();
        Vector<MediaQueryExp*>* sinkFloatingMediaQueryExpList(Vector<MediaQueryExp*>*);
        MediaQuery* createFloatingMediaQuery(MediaQuery::Restrictor, const String&, Vector<MediaQueryExp*>*);
        MediaQuery* createFloatingMediaQuery(Vector<MediaQueryExp*>*);
        MediaQuery* sinkFloatingMediaQuery(MediaQuery*);

        bool addVariable(const CSSParserString&, CSSParserValueList*);
        bool addVariableDeclarationBlock(const CSSParserString&);
        bool checkForVariables(CSSParserValueList*);
        void addUnresolvedProperty(int propId, bool important);
        
        Vector<CSSSelector*>* reusableSelectorVector() { return &m_reusableSelectorVector; }
        
    public:
        bool m_strict;
        bool m_important;
        int m_id;
        CSSStyleSheet* m_styleSheet;
        RefPtr<CSSRule> m_rule;
        RefPtr<CSSRule> m_keyframe;
        MediaQuery* m_mediaQuery;
        CSSParserValueList* m_valueList;
        CSSProperty** m_parsedProperties;
        CSSSelectorList* m_selectorListForParseSelector;
        unsigned m_numParsedProperties;
        unsigned m_maxParsedProperties;

        int m_inParseShorthand;
        int m_currentShorthand;
        bool m_implicitShorthand;

        bool m_hasFontFaceOnlyValues;

        Vector<String> m_variableNames;
        Vector<RefPtr<CSSValue> > m_variableValues;

        AtomicString m_defaultNamespace;

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
        
        void clearVariables();

        void deleteFontFaceOnlyValues();

        UChar* m_data;
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
        HashSet<CSSParserValueList*> m_floatingValueLists;
        HashSet<CSSParserFunction*> m_floatingFunctions;

        MediaQuery* m_floatingMediaQuery;
        MediaQueryExp* m_floatingMediaQueryExp;
        Vector<MediaQueryExp*>* m_floatingMediaQueryExpList;
        
        Vector<CSSSelector*> m_reusableSelectorVector;

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

        static bool validUnit(CSSParserValue*, Units, bool strict);
        
        friend class TransformOperationInfo;
    };

    int cssPropertyID(const CSSParserString&);
    int cssPropertyID(const String&);
    int cssValueKeywordID(const CSSParserString&);

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

} // namespace WebCore

#endif // CSSParser_h
