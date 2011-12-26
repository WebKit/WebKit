/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "CSSGradientValue.h"
#include "CSSParserValues.h"
#include "CSSPropertySourceData.h"
#include "CSSSelector.h"
#include "Color.h"
#include "MediaQuery.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

#if ENABLE(CSS_FILTERS)
#include "WebKitCSSFilterValue.h"
#endif

namespace WebCore {

class CSSBorderImageSliceValue;
class CSSMappedAttributeDeclaration;
class CSSMutableStyleDeclaration;
class CSSPrimitiveValue;
class CSSValuePool;
class CSSProperty;
class CSSRule;
class CSSRuleList;
class CSSSelectorList;
class CSSStyleSheet;
class CSSValue;
class CSSValueList;
class CSSWrapShape;
class Document;
class MediaList;
class MediaQueryExp;
class StyledElement;
class WebKitCSSKeyframeRule;
class WebKitCSSKeyframesRule;

class CSSParser {
public:
    CSSParser(bool strictParsing = true);
    ~CSSParser();

    void parseSheet(CSSStyleSheet*, const String&, int startLineNumber = 0, StyleRuleRangeMap* ruleRangeMap = 0);
    PassRefPtr<CSSRule> parseRule(CSSStyleSheet*, const String&);
    PassRefPtr<WebKitCSSKeyframeRule> parseKeyframeRule(CSSStyleSheet*, const String&);
    static bool parseValue(CSSMutableStyleDeclaration*, int propId, const String&, bool important, bool strict);
    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool parseSystemColor(RGBA32& color, const String&, Document*);
    PassRefPtr<CSSPrimitiveValue> parseValidPrimitive(int propId, CSSParserValue*);
    bool parseDeclaration(CSSMutableStyleDeclaration*, const String&, RefPtr<CSSStyleSourceData>* = 0, CSSStyleSheet* contextStyleSheet = 0);
    bool parseMediaQuery(MediaList*, const String&);

    static bool parseMappedAttributeValue(CSSMappedAttributeDeclaration*, StyledElement*, int propertyId, const String&);

    Document* findDocument() const;

    CSSValuePool* cssValuePool() const { return m_cssValuePool.get(); }

    void addProperty(int propId, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void rollbackLastProperties(int num);
    bool hasProperties() const { return m_numParsedProperties > 0; }

    bool parseValue(int propId, bool important);
    bool parseShorthand(int propId, const int* properties, int numProperties, bool important);
    bool parse4Values(int propId, const int* properties, bool important);
    bool parseContent(int propId, bool important);
    bool parseQuotes(int propId, bool important);

    PassRefPtr<CSSValue> parseAttr(CSSParserValueList* args);

    PassRefPtr<CSSValue> parseBackgroundColor();

    bool parseFillImage(CSSParserValueList*, RefPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    PassRefPtr<CSSValue> parseFillPositionComponent(CSSParserValueList*, unsigned& cumulativeFlags, FillPositionFlag& individualFlag);
    PassRefPtr<CSSValue> parseFillPositionX(CSSParserValueList*);
    PassRefPtr<CSSValue> parseFillPositionY(CSSParserValueList*);
    void parseFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    void parseFillRepeat(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    PassRefPtr<CSSValue> parseFillSize(int propId, bool &allowComma);

    bool parseFillProperty(int propId, int& propId1, int& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseFillShorthand(int propId, const int* properties, int numProperties, bool important);

    void addFillValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    void addAnimationValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    PassRefPtr<CSSValue> parseAnimationDelay();
    PassRefPtr<CSSValue> parseAnimationDirection();
    PassRefPtr<CSSValue> parseAnimationDuration();
    PassRefPtr<CSSValue> parseAnimationFillMode();
    PassRefPtr<CSSValue> parseAnimationIterationCount();
    PassRefPtr<CSSValue> parseAnimationName();
    PassRefPtr<CSSValue> parseAnimationPlayState();
    PassRefPtr<CSSValue> parseAnimationProperty();
    PassRefPtr<CSSValue> parseAnimationTimingFunction();

    bool parseTransformOriginShorthand(RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseCubicBezierTimingFunctionValue(CSSParserValueList*& args, double& result);
    bool parseAnimationProperty(int propId, RefPtr<CSSValue>&);
    bool parseTransitionShorthand(bool important);
    bool parseAnimationShorthand(bool important);

#if ENABLE(CSS_GRID_LAYOUT)
    bool parseGridTrackList(int propId, bool important);
#endif

    bool parseDashboardRegions(int propId, bool important);

    bool parseShape(int propId, bool important);

    bool parseWrapShape(bool shapeInside, bool important);
    PassRefPtr<CSSWrapShape> parseWrapShapeRect(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseWrapShapeCircle(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseWrapShapeEllipse(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseWrapShapePolygon(CSSParserValueList* args);

    bool parseFont(bool important);
    PassRefPtr<CSSValueList> parseFontFamily();

    bool parseCounter(int propId, int defaultValue, bool important);
    PassRefPtr<CSSValue> parseCounterContent(CSSParserValueList* args, bool counters);

    bool parseColorParameters(CSSParserValue*, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(CSSParserValue*, double* colorValues, bool parseAlpha);
    PassRefPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = 0);
    bool parseColorFromValue(CSSParserValue*, RGBA32&);
    void parseSelector(const String&, Document* doc, CSSSelectorList&);

    static bool fastParseColor(RGBA32&, const String&, bool strict);

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
    PassRefPtr<CSSValueList> parseShadow(CSSParserValueList*, int propId);
    bool parseBorderImage(int propId, RefPtr<CSSValue>&);
    bool parseBorderImageRepeat(RefPtr<CSSValue>&);
    bool parseBorderImageSlice(int propId, RefPtr<CSSBorderImageSliceValue>&);
    bool parseBorderImageWidth(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderImageOutset(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderRadius(int propId, bool important);

    bool parseAspectRatio(bool important);

    bool parseReflect(int propId, bool important);

    bool parseFlex(int propId, bool important);

    // Image generators
    bool parseCanvas(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseDeprecatedGradient(CSSParserValueList*, RefPtr<CSSValue>&);
    bool parseLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList*, CSSGradientValue*, bool expectComma);

    bool parseCrossfade(CSSParserValueList*, RefPtr<CSSValue>&);

#if ENABLE(CSS_FILTERS)
    PassRefPtr<CSSValueList> parseFilter();
    PassRefPtr<WebKitCSSFilterValue> parseBuiltinFilterArguments(CSSParserValueList*, WebKitCSSFilterValue::FilterOperationType);
#if ENABLE(CSS_SHADERS)
    PassRefPtr<WebKitCSSFilterValue> parseCustomFilter(CSSParserValue*);
#endif
#endif

    PassRefPtr<CSSValueList> parseTransform();
    bool parseTransformOrigin(int propId, int& propId1, int& propId2, int& propId3, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parsePerspectiveOrigin(int propId, int& propId1, int& propId2,  RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    bool parseTextEmphasisStyle(bool important);

    bool parseLineBoxContain(bool important);

    bool parseFontFeatureTag(CSSValueList*);
    bool parseFontFeatureSettings(bool important);

    bool parseFlowThread(int propId, bool important);
    bool parseRegionThread(int propId, bool important);

    int yyparse();

    CSSParserSelector* createFloatingSelector();
    PassOwnPtr<CSSParserSelector> sinkFloatingSelector(CSSParserSelector*);

    Vector<OwnPtr<CSSParserSelector> >* createFloatingSelectorVector();
    PassOwnPtr<Vector<OwnPtr<CSSParserSelector> > > sinkFloatingSelectorVector(Vector<OwnPtr<CSSParserSelector> >*);

    CSSParserValueList* createFloatingValueList();
    PassOwnPtr<CSSParserValueList> sinkFloatingValueList(CSSParserValueList*);

    CSSParserFunction* createFloatingFunction();
    PassOwnPtr<CSSParserFunction> sinkFloatingFunction(CSSParserFunction*);

    CSSParserValue& sinkFloatingValue(CSSParserValue&);

    MediaList* createMediaList();
    CSSRule* createCharsetRule(const CSSParserString&);
    CSSRule* createImportRule(const CSSParserString&, MediaList*);
    WebKitCSSKeyframeRule* createKeyframeRule(CSSParserValueList*);
    WebKitCSSKeyframesRule* createKeyframesRule();
    CSSRule* createMediaRule(MediaList*, CSSRuleList*);
    CSSRuleList* createRuleList();
    CSSRule* createStyleRule(Vector<OwnPtr<CSSParserSelector> >* selectors);
    CSSRule* createFontFaceRule();
    CSSRule* createPageRule(PassOwnPtr<CSSParserSelector> pageSelector);
    CSSRule* createRegionRule(Vector<OwnPtr<CSSParserSelector> >* regionSelector, CSSRuleList* rules);
    CSSRule* createMarginAtRule(CSSSelector::MarginBoxType marginBox);
    void startDeclarationsForMarginBox();
    void endDeclarationsForMarginBox();

    MediaQueryExp* createFloatingMediaQueryExp(const AtomicString&, CSSParserValueList*);
    PassOwnPtr<MediaQueryExp> sinkFloatingMediaQueryExp(MediaQueryExp*);
    Vector<OwnPtr<MediaQueryExp> >* createFloatingMediaQueryExpList();
    PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > > sinkFloatingMediaQueryExpList(Vector<OwnPtr<MediaQueryExp> >*);
    MediaQuery* createFloatingMediaQuery(MediaQuery::Restrictor, const String&, PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > >);
    MediaQuery* createFloatingMediaQuery(PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > >);
    PassOwnPtr<MediaQuery> sinkFloatingMediaQuery(MediaQuery*);

    void addNamespace(const AtomicString& prefix, const AtomicString& uri);
    void updateSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*);
    CSSParserSelector* updateSpecifiers(CSSParserSelector*, CSSParserSelector*);

    void invalidBlockHit();

    Vector<OwnPtr<CSSParserSelector> >* reusableSelectorVector() { return &m_reusableSelectorVector; }

    void setReusableRegionSelectorVector(Vector<OwnPtr<CSSParserSelector> >* selectors);
    Vector<OwnPtr<CSSParserSelector> >* reusableRegionSelectorVector() { return &m_reusableRegionSelectorVector; }

    void updateLastSelectorLineAndPosition();
    void updateLastMediaLine(MediaList*);

    void clearProperties();

    bool m_strict;
    bool m_important;
    int m_id;
    CSSStyleSheet* m_styleSheet;
    RefPtr<CSSRule> m_rule;
    RefPtr<WebKitCSSKeyframeRule> m_keyframe;
    OwnPtr<MediaQuery> m_mediaQuery;
    OwnPtr<CSSParserValueList> m_valueList;
    CSSProperty** m_parsedProperties;
    CSSSelectorList* m_selectorListForParseSelector;

    RefPtr<CSSValuePool> m_cssValuePool;
    unsigned m_numParsedProperties;
    unsigned m_maxParsedProperties;
    unsigned m_numParsedPropertiesBeforeMarginBox;

    int m_inParseShorthand;
    int m_currentShorthand;
    bool m_implicitShorthand;

    bool m_hasFontFaceOnlyValues;
    bool m_hadSyntacticallyValidCSSRule;

    AtomicString m_defaultNamespace;

    // tokenizer methods and data
    bool m_inStyleRuleOrDeclaration;
    SourceRange m_selectorListRange;
    SourceRange m_ruleBodyRange;
    SourceRange m_propertyRange;
    StyleRuleRangeMap* m_ruleRangeMap;
    RefPtr<CSSRuleSourceData> m_currentRuleData;
    void markSelectorListStart();
    void markSelectorListEnd();
    void markRuleBodyStart();
    void markRuleBodyEnd();
    void markPropertyStart();
    void markPropertyEnd(bool isImportantFound, bool isPropertyParsed);
    void resetSelectorListMarks() { m_selectorListRange.start = m_selectorListRange.end = 0; }
    void resetRuleBodyMarks() { m_ruleBodyRange.start = m_ruleBodyRange.end = 0; }
    void resetPropertyMarks() { m_propertyRange.start = m_propertyRange.end = UINT_MAX; }
    int lex(void* yylval);
    int token() { return yyTok; }
    UChar* text(int* length);
    void countLines();
    int lex();

private:
    void setStyleSheet(CSSStyleSheet*);
    void ensureCSSValuePool();

    void recheckAtKeyword(const UChar* str, int len);

    void setupParser(const char* prefix, const String&, const char* suffix);

    bool inShorthand() const { return m_inParseShorthand; }

    void checkForOrphanedUnits();

    void deleteFontFaceOnlyValues();

    bool isGeneratedImageValue(CSSParserValue*) const;
    bool parseGeneratedImage(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseValue(CSSMutableStyleDeclaration*, int propId, const String&, bool important, CSSStyleSheet* contextStyleSheet = 0);

    enum SizeParameterType {
        None,
        Auto,
        Length,
        PageSize,
        Orientation,
    };

    bool parsePage(int propId, bool important);
    bool parseSize(int propId, bool important);
    SizeParameterType parseSizeParameter(CSSValueList* parsedValues, CSSParserValue* value, SizeParameterType prevParamType);

    bool parseFontFaceSrcURI(CSSValueList*);
    bool parseFontFaceSrcLocal(CSSValueList*);

    bool parseColor(const String&);

    OwnArrayPtr<UChar> m_data;
    UChar* yytext;
    UChar* yy_c_buf_p;
    UChar yy_hold_char;
    int yy_last_accepting_state;
    UChar* yy_last_accepting_cpos;
    int yyleng;
    int yyTok;
    int yy_start;
    int m_lineNumber;
    int m_lastSelectorLineNumber;

    bool m_allowImportRules;
    bool m_allowNamespaceDeclarations;

    Vector<RefPtr<CSSRule> > m_parsedRules;
    Vector<RefPtr<MediaList> > m_parsedMediaLists;
    Vector<RefPtr<CSSRuleList> > m_parsedRuleLists;
    HashSet<CSSParserSelector*> m_floatingSelectors;
    HashSet<Vector<OwnPtr<CSSParserSelector> >*> m_floatingSelectorVectors;
    HashSet<CSSParserValueList*> m_floatingValueLists;
    HashSet<CSSParserFunction*> m_floatingFunctions;

    OwnPtr<MediaQuery> m_floatingMediaQuery;
    OwnPtr<MediaQueryExp> m_floatingMediaQueryExp;
    OwnPtr<Vector<OwnPtr<MediaQueryExp> > > m_floatingMediaQueryExpList;

    Vector<OwnPtr<CSSParserSelector> > m_reusableSelectorVector;
    Vector<OwnPtr<CSSParserSelector> > m_reusableRegionSelectorVector;

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

    bool parseBorderImageQuad(Units, RefPtr<CSSPrimitiveValue>&);

    PassRefPtr<CSSPrimitiveValue> createPrimitiveNumericValue(CSSParserValue*);
    PassRefPtr<CSSPrimitiveValue> createPrimitiveStringValue(CSSParserValue*);

    friend class TransformOperationInfo;
#if ENABLE(CSS_FILTERS)
    friend class FilterOperationInfo;
#endif
};

int cssPropertyID(const CSSParserString&);
int cssPropertyID(const String&);
int cssValueKeywordID(const CSSParserString&);

class ShorthandScope {
    WTF_MAKE_FAST_ALLOCATED;
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

String quoteCSSString(const String&);
String quoteCSSStringIfNeeded(const String&);
String quoteCSSURLIfNeeded(const String&);

bool isValidNthToken(const CSSParserString&);
} // namespace WebCore

#endif // CSSParser_h
