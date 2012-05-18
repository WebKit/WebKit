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

#include "CSSCalculationValue.h"
#include "CSSGradientValue.h"
#include "CSSParserMode.h"
#include "CSSParserValues.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
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
class CSSPrimitiveValue;
class CSSProperty;
class CSSSelectorList;
class CSSValue;
class CSSValueList;
class CSSWrapShape;
class Document;
class MediaQueryExp;
class MediaQuerySet;
class StyleKeyframe;
class StylePropertySet;
class StylePropertyShorthand;
class StyleRuleBase;
class StyleRuleKeyframes;
class StyleKeyframe;
class StyleSheetInternal;
class StyledElement;

class CSSParser {
public:
    CSSParser(const CSSParserContext&);

    ~CSSParser();

    void parseSheet(StyleSheetInternal*, const String&, int startLineNumber = 0, StyleRuleRangeMap* = 0);
    PassRefPtr<StyleRuleBase> parseRule(StyleSheetInternal*, const String&);
    PassRefPtr<StyleKeyframe> parseKeyframeRule(StyleSheetInternal*, const String&);
    static bool parseValue(StylePropertySet*, CSSPropertyID, const String&, bool important, CSSParserMode, StyleSheetInternal*);
    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool parseSystemColor(RGBA32& color, const String&, Document*);
    static PassRefPtr<CSSValueList> parseFontFaceValue(const AtomicString&);
    PassRefPtr<CSSPrimitiveValue> parseValidPrimitive(int ident, CSSParserValue*);
    bool parseDeclaration(StylePropertySet*, const String&, RefPtr<CSSStyleSourceData>*, StyleSheetInternal* contextStyleSheet);
    PassOwnPtr<MediaQuery> parseMediaQuery(const String&);

    void addProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void rollbackLastProperties(int num);
    bool hasProperties() const { return !m_parsedProperties.isEmpty(); }

    bool parseValue(CSSPropertyID, bool important);
    bool parseShorthand(CSSPropertyID, const StylePropertyShorthand&, bool important);
    bool parse4Values(CSSPropertyID, const CSSPropertyID* properties, bool important);
    bool parseContent(CSSPropertyID, bool important);
    bool parseQuotes(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseAttr(CSSParserValueList* args);

    PassRefPtr<CSSValue> parseBackgroundColor();

    bool parseFillImage(CSSParserValueList*, RefPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    PassRefPtr<CSSValue> parseFillPositionComponent(CSSParserValueList*, unsigned& cumulativeFlags, FillPositionFlag& individualFlag);
    PassRefPtr<CSSValue> parseFillPositionX(CSSParserValueList*);
    PassRefPtr<CSSValue> parseFillPositionY(CSSParserValueList*);
    void parseFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    void parseFillRepeat(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    PassRefPtr<CSSValue> parseFillSize(CSSPropertyID, bool &allowComma);

    bool parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseFillShorthand(CSSPropertyID, const CSSPropertyID* properties, int numProperties, bool important);

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
    bool parseAnimationProperty(CSSPropertyID, RefPtr<CSSValue>&);
    bool parseTransitionShorthand(bool important);
    bool parseAnimationShorthand(bool important);

    bool cssGridLayoutEnabled() const;
    bool parseGridTrackList(CSSPropertyID, bool important);

    bool parseDashboardRegions(CSSPropertyID, bool important);

    bool parseClipShape(CSSPropertyID, bool important);

    bool parseExclusionShape(bool shapeInside, bool important);
    PassRefPtr<CSSWrapShape> parseExclusionShapeRectangle(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseExclusionShapeCircle(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseExclusionShapeEllipse(CSSParserValueList* args);
    PassRefPtr<CSSWrapShape> parseExclusionShapePolygon(CSSParserValueList* args);

    bool parseFont(bool important);
    PassRefPtr<CSSValueList> parseFontFamily();

    bool parseCounter(CSSPropertyID, int defaultValue, bool important);
    PassRefPtr<CSSValue> parseCounterContent(CSSParserValueList* args, bool counters);

    bool parseColorParameters(CSSParserValue*, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(CSSParserValue*, double* colorValues, bool parseAlpha);
    PassRefPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = 0);
    bool parseColorFromValue(CSSParserValue*, RGBA32&);
    void parseSelector(const String&, CSSSelectorList&);

    static bool fastParseColor(RGBA32&, const String&, bool strict);

    bool parseLineHeight(bool important);
    bool parseFontSize(bool important);
    bool parseFontVariant(bool important);
    bool parseFontWeight(bool important);
    bool parseFontFaceSrc();
    bool parseFontFaceUnicodeRange();

#if ENABLE(SVG)
    bool parseSVGValue(CSSPropertyID propId, bool important);
    PassRefPtr<CSSValue> parseSVGPaint();
    PassRefPtr<CSSValue> parseSVGColor();
    PassRefPtr<CSSValue> parseSVGStrokeDasharray();
#endif

    // CSS3 Parsing Routines (for properties specific to CSS3)
    PassRefPtr<CSSValueList> parseShadow(CSSParserValueList*, CSSPropertyID);
    bool parseBorderImage(CSSPropertyID, RefPtr<CSSValue>&, bool important = false);
    bool parseBorderImageRepeat(RefPtr<CSSValue>&);
    bool parseBorderImageSlice(CSSPropertyID, RefPtr<CSSBorderImageSliceValue>&);
    bool parseBorderImageWidth(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderImageOutset(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderRadius(CSSPropertyID, bool important);

    bool parseAspectRatio(bool important);

    bool parseReflect(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseFlex(CSSParserValueList* args);

    // Image generators
    bool parseCanvas(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseDeprecatedGradient(CSSParserValueList*, RefPtr<CSSValue>&);
    bool parseLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList*, CSSGradientValue*, bool expectComma);

    bool parseCrossfade(CSSParserValueList*, RefPtr<CSSValue>&);

#if ENABLE(CSS_IMAGE_SET)
    PassRefPtr<CSSValue> parseImageSet(CSSParserValueList*);
#endif

#if ENABLE(CSS_FILTERS)
    PassRefPtr<CSSValueList> parseFilter();
    PassRefPtr<WebKitCSSFilterValue> parseBuiltinFilterArguments(CSSParserValueList*, WebKitCSSFilterValue::FilterOperationType);
#if ENABLE(CSS_SHADERS)
    PassRefPtr<WebKitCSSFilterValue> parseCustomFilter(CSSParserValue*);
#endif
#endif

    PassRefPtr<CSSValueList> parseTransform();
    bool parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,  RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    bool parseTextEmphasisStyle(bool important);

    bool parseLineBoxContain(bool important);
    bool parseCalculation(CSSParserValue*, CalculationPermittedValueRange);

    bool parseFontFeatureTag(CSSValueList*);
    bool parseFontFeatureSettings(bool important);

    bool cssRegionsEnabled() const;
    bool parseFlowThread(const String& flowName);
    bool parseFlowThread(CSSPropertyID, bool important);
    bool parseRegionThread(CSSPropertyID, bool important);

    bool parseFontVariantLigatures(bool important);

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

    MediaQuerySet* createMediaQuerySet();
    StyleRuleBase* createImportRule(const CSSParserString&, MediaQuerySet*);
    StyleKeyframe* createKeyframe(CSSParserValueList*);
    StyleRuleKeyframes* createKeyframesRule();

    typedef Vector<RefPtr<StyleRuleBase> > RuleList;
    StyleRuleBase* createMediaRule(MediaQuerySet*, RuleList*);
    RuleList* createRuleList();
    StyleRuleBase* createStyleRule(Vector<OwnPtr<CSSParserSelector> >* selectors);
    StyleRuleBase* createFontFaceRule();
    StyleRuleBase* createPageRule(PassOwnPtr<CSSParserSelector> pageSelector);
    StyleRuleBase* createRegionRule(Vector<OwnPtr<CSSParserSelector> >* regionSelector, RuleList* rules);
    StyleRuleBase* createMarginAtRule(CSSSelector::MarginBoxType);
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
    void updateLastMediaLine(MediaQuerySet*);

    void clearProperties();

    CSSParserContext m_context;

    bool m_important;
    CSSPropertyID m_id;
    StyleSheetInternal* m_styleSheet;
    RefPtr<StyleRuleBase> m_rule;
    RefPtr<StyleKeyframe> m_keyframe;
    OwnPtr<MediaQuery> m_mediaQuery;
    OwnPtr<CSSParserValueList> m_valueList;
    Vector<CSSProperty, 256> m_parsedProperties;
    CSSSelectorList* m_selectorListForParseSelector;

    unsigned m_numParsedPropertiesBeforeMarginBox;

    int m_inParseShorthand;
    CSSPropertyID m_currentShorthand;
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
    int token() { return m_token; }

    PassRefPtr<CSSPrimitiveValue> createPrimitiveNumericValue(CSSParserValue*);
    PassRefPtr<CSSPrimitiveValue> createPrimitiveStringValue(CSSParserValue*);

    static KURL completeURL(const CSSParserContext&, const String& url);

private:
    inline bool isIdentifierStart();

    static inline UChar* checkAndSkipString(UChar*, UChar);

    void parseEscape(UChar*&);
    inline void parseIdentifier(UChar*&, bool&);
    inline void parseString(UChar*&, UChar);
    inline void parseURI(UChar*&, UChar*&);
    inline bool parseUnicodeRange();
    bool parseNthChild();
    bool parseNthChildExtra();
    inline void detectFunctionTypeToken(int);
    inline void detectMediaQueryToken(int);
    inline void detectNumberToken(UChar*, int);
    inline void detectDashToken(int);
    inline void detectAtToken(int, bool);

    void setStyleSheet(StyleSheetInternal*);

    inline bool inStrictMode() const { return m_context.mode == CSSStrictMode || m_context.mode == SVGAttributeMode; }
    inline bool inQuirksMode() const { return m_context.mode == CSSQuirksMode; }
    
    KURL completeURL(const String& url) const;

    void recheckAtKeyword(const UChar* str, int len);

    void setupParser(const char* prefix, const String&, const char* suffix);

    bool inShorthand() const { return m_inParseShorthand; }

    void checkForOrphanedUnits();

    void deleteFontFaceOnlyValues();

    bool isGeneratedImageValue(CSSParserValue*) const;
    bool parseGeneratedImage(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseValue(StylePropertySet*, CSSPropertyID, const String&, bool important, StyleSheetInternal* contextStyleSheet);

    enum SizeParameterType {
        None,
        Auto,
        Length,
        PageSize,
        Orientation,
    };

    bool parsePage(CSSPropertyID propId, bool important);
    bool parseSize(CSSPropertyID propId, bool important);
    SizeParameterType parseSizeParameter(CSSValueList* parsedValues, CSSParserValue* value, SizeParameterType prevParamType);

    bool parseFontFaceSrcURI(CSSValueList*);
    bool parseFontFaceSrcLocal(CSSValueList*);

    bool parseColor(const String&);

    enum ParsingMode {
        NormalMode,
        MediaQueryMode,
        NthChildMode
    };

    ParsingMode m_parsingMode;
    OwnArrayPtr<UChar> m_dataStart;
    UChar* m_currentCharacter;
    UChar* m_tokenStart;
    int m_token;
    int m_lineNumber;
    int m_lastSelectorLineNumber;

    bool m_allowImportRules;
    bool m_allowNamespaceDeclarations;

    Vector<RefPtr<StyleRuleBase> > m_parsedRules;
    Vector<RefPtr<StyleKeyframe> > m_parsedKeyframes;
    Vector<RefPtr<MediaQuerySet> > m_parsedMediaQuerySets;
    Vector<OwnPtr<RuleList> > m_parsedRuleLists;
    HashSet<CSSParserSelector*> m_floatingSelectors;
    HashSet<Vector<OwnPtr<CSSParserSelector> >*> m_floatingSelectorVectors;
    HashSet<CSSParserValueList*> m_floatingValueLists;
    HashSet<CSSParserFunction*> m_floatingFunctions;

    OwnPtr<MediaQuery> m_floatingMediaQuery;
    OwnPtr<MediaQueryExp> m_floatingMediaQueryExp;
    OwnPtr<Vector<OwnPtr<MediaQueryExp> > > m_floatingMediaQueryExpList;

    Vector<OwnPtr<CSSParserSelector> > m_reusableSelectorVector;
    Vector<OwnPtr<CSSParserSelector> > m_reusableRegionSelectorVector;

    RefPtr<CSSCalcValue> m_parsedCalculation;

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

    bool validCalculationUnit(CSSParserValue*, Units);

    bool shouldAcceptUnitLessValues(CSSParserValue*, Units, CSSParserMode);

    inline bool validUnit(CSSParserValue* value, Units unitflags) { return validUnit(value, unitflags, m_context.mode); }
    bool validUnit(CSSParserValue*, Units, CSSParserMode);

    bool parseBorderImageQuad(Units, RefPtr<CSSPrimitiveValue>&);
    int colorIntFromValue(CSSParserValue*);

    enum ReleaseParsedCalcValueCondition {
        ReleaseParsedCalcValue,
        DoNotReleaseParsedCalcValue
    };    
    double parsedDouble(CSSParserValue*, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);
    bool isCalculation(CSSParserValue*);
    
    friend class TransformOperationInfo;
#if ENABLE(CSS_FILTERS)
    friend class FilterOperationInfo;
#endif
};

CSSPropertyID cssPropertyID(const CSSParserString&);
CSSPropertyID cssPropertyID(const String&);
int cssValueKeywordID(const CSSParserString&);
#if PLATFORM(IOS)
void cssPropertyNameIOSAliasing(const char* propertyName, const char*& propertyNameAlias, unsigned& newLength);
#endif

class ShorthandScope {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShorthandScope(CSSParser* parser, CSSPropertyID propId) : m_parser(parser)
    {
        if (!(m_parser->m_inParseShorthand++))
            m_parser->m_currentShorthand = propId;
    }
    ~ShorthandScope()
    {
        if (!(--m_parser->m_inParseShorthand))
            m_parser->m_currentShorthand = CSSPropertyInvalid;
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
