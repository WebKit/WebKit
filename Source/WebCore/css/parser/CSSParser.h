/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2010, 2015 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSCalculationValue.h"
#include "CSSGradientValue.h"
#include "CSSParserMode.h"
#include "CSSParserValues.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "MediaQuery.h"
#include "SourceSizeList.h"
#include "StyleRuleImport.h"
#include "WebKitCSSFilterValue.h"
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/TextPosition.h>

#if ENABLE(CSS_GRID_LAYOUT)
#include "GridArea.h"
#endif

namespace WebCore {

class AnimationParseContext;
class CSSBorderImageSliceValue;
class CSSContentDistributionValue;
class CSSPrimitiveValue;
class CSSSelectorList;
class CSSValue;
class CSSValueList;
class CSSBasicShape;
class CSSBasicShapeCircle;
class CSSBasicShapeEllipse;
class CSSBasicShapeInset;
class CSSBasicShapePath;
class CSSBasicShapePolygon;
class CSSGridLineNamesValue;
class CSSImageSetValue;
class CSSVariableDependentValue;
class Document;
class Element;
class ImmutableStyleProperties;
class MediaQueryExpression;
class MediaQuerySet;
class MutableStyleProperties;
class SVGColor;
class SVGPaint;
class StyleKeyframe;
class StylePropertyShorthand;
class StyleRule;
class StyleRuleBase;
class StyleRuleFontFace;
class StyleRuleKeyframes;
class StyleRuleMedia;
class StyleRulePage;
class StyleRuleRegion;
class StyleRuleSupports;
class StyleRuleViewport;
class StyleKeyframe;
class StyleSheetContents;
class StyledElement;
class WebKitCSSTransformValue;

class CSSParser {
    friend inline int cssyylex(void*, CSSParser*);

public:
    struct Location;
    enum SyntaxErrorType {
        PropertyDeclarationError,
        GeneralSyntaxError
    };

    enum class ParseResult {
        Changed,
        Unchanged,
        Error
    };

    class ValueWithCalculation {
    public:
        explicit ValueWithCalculation(CSSParserValue& value)
            : m_value(value)
        { }

        CSSParserValue& value() const { return m_value; }
        operator CSSParserValue&() { return m_value; }

        RefPtr<CSSCalcValue> calculation() const { return m_calculation; }
        void setCalculation(RefPtr<CSSCalcValue>&& calculation)
        {
            ASSERT(isCalculation(m_value));
            m_calculation = calculation;
        }

    private:
        CSSParserValue& m_value;
        RefPtr<CSSCalcValue> m_calculation;
    };

    WEBCORE_EXPORT CSSParser(const CSSParserContext&);
    WEBCORE_EXPORT ~CSSParser();

    void parseSheet(StyleSheetContents*, const String&, const TextPosition&, RuleSourceDataList*, bool logErrors);
    RefPtr<StyleRuleBase> parseRule(StyleSheetContents*, const String&);
    RefPtr<StyleKeyframe> parseKeyframeRule(StyleSheetContents*, const String&);
    bool parseSupportsCondition(const String&);

    static ParseResult parseValue(MutableStyleProperties&, CSSPropertyID, const String&, bool important, CSSParserMode, StyleSheetContents*);
    static ParseResult parseCustomPropertyValue(MutableStyleProperties&, const AtomicString& propertyName, const String&, bool important, CSSParserMode, StyleSheetContents* contextStyleSheet);

    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool isValidSystemColorValue(CSSValueID);
    static bool parseSystemColor(RGBA32& color, const String&, Document*);
    static RefPtr<CSSValueList> parseFontFaceValue(const AtomicString&);
    RefPtr<CSSPrimitiveValue> parseValidPrimitive(CSSValueID ident, ValueWithCalculation&);

    WEBCORE_EXPORT bool parseDeclaration(MutableStyleProperties&, const String&, RefPtr<CSSRuleSourceData>&&, StyleSheetContents* contextStyleSheet);
    static Ref<ImmutableStyleProperties> parseInlineStyleDeclaration(const String&, Element*);
    std::unique_ptr<MediaQuery> parseMediaQuery(const String&);

    void addPropertyWithPrefixingVariant(CSSPropertyID, RefPtr<CSSValue>&&, bool important, bool implicit = false);
    void addProperty(CSSPropertyID, RefPtr<CSSValue>&&, bool important, bool implicit = false);
    void rollbackLastProperties(int num);
    bool hasProperties() const { return !m_parsedProperties.isEmpty(); }
    void addExpandedPropertyForValue(CSSPropertyID propId, Ref<CSSValue>&&, bool);

    bool parseValue(CSSPropertyID, bool important);
    bool parseShorthand(CSSPropertyID, const StylePropertyShorthand&, bool important);
    bool parse4Values(CSSPropertyID, const CSSPropertyID* properties, bool important);
    bool parseContent(CSSPropertyID, bool important);
    bool parseQuotes(CSSPropertyID, bool important);
    bool parseAlt(CSSPropertyID, bool important);
    
    bool parseCustomPropertyDeclaration(bool important, CSSValueID);
    
    RefPtr<CSSPrimitiveValue> parseAttr(CSSParserValueList& args);

    RefPtr<CSSPrimitiveValue> parseBackgroundColor();

    struct SourceSize {
        MediaQueryExpression expression;
        Ref<CSSValue> length;

        SourceSize(SourceSize&&);
        SourceSize(MediaQueryExpression&&, Ref<CSSValue>&&);
    };
    Vector<SourceSize> parseSizesAttribute(StringView);
    Optional<SourceSize> sourceSize(MediaQueryExpression&&, CSSParserValue&);

    bool parseFillImage(CSSParserValueList&, RefPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    enum FillPositionParsingMode { ResolveValuesAsPercent = 0, ResolveValuesAsKeyword = 1 };
    RefPtr<CSSPrimitiveValue> parseFillPositionComponent(CSSParserValueList&, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode = ResolveValuesAsPercent);
    RefPtr<CSSPrimitiveValue> parsePositionX(CSSParserValueList&);
    RefPtr<CSSPrimitiveValue> parsePositionY(CSSParserValueList&);
    void parse2ValuesFillPosition(CSSParserValueList&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&);
    bool isPotentialPositionValue(CSSParserValue&);
    void parseFillPosition(CSSParserValueList&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&);
    void parse3ValuesFillPosition(CSSParserValueList&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&, Ref<CSSPrimitiveValue>&&, Ref<CSSPrimitiveValue>&&);
    void parse4ValuesFillPosition(CSSParserValueList&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&, Ref<CSSPrimitiveValue>&&, Ref<CSSPrimitiveValue>&&);

    void parseFillRepeat(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    RefPtr<CSSPrimitiveValue> parseFillSize(CSSPropertyID, bool &allowComma);

    bool parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseFillShorthand(CSSPropertyID, const CSSPropertyID* properties, int numProperties, bool important);

    void addFillValue(RefPtr<CSSValue>& lval, Ref<CSSValue>&& rval);
    void addAnimationValue(RefPtr<CSSValue>& lval, Ref<CSSValue>&& rval);

    RefPtr<CSSPrimitiveValue> parseAnimationDelay();
    RefPtr<CSSPrimitiveValue> parseAnimationDirection();
    RefPtr<CSSPrimitiveValue> parseAnimationDuration();
    RefPtr<CSSPrimitiveValue> parseAnimationFillMode();
    RefPtr<CSSPrimitiveValue> parseAnimationIterationCount();
    RefPtr<CSSPrimitiveValue> parseAnimationName();
    RefPtr<CSSPrimitiveValue> parseAnimationPlayState();
    RefPtr<CSSPrimitiveValue> parseAnimationProperty(AnimationParseContext&);
    RefPtr<CSSValue> parseAnimationTimingFunction();
#if ENABLE(CSS_ANIMATIONS_LEVEL_2)
    RefPtr<CSSValue> parseAnimationTrigger();
#endif
    static Vector<double> parseKeyframeSelector(const String&);

    bool parseTransformOriginShorthand(RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSValue>&);
    Optional<double> parseCubicBezierTimingFunctionValue(CSSParserValueList&);
    Optional<double> parseSpringTimingFunctionValue(CSSParserValueList&);
    bool parseAnimationProperty(CSSPropertyID, RefPtr<CSSValue>&, AnimationParseContext&);
    bool parseTransitionShorthand(CSSPropertyID, bool important);
    bool parseAnimationShorthand(CSSPropertyID, bool important);

    bool isSpringTimingFunctionEnabled() const;

    RefPtr<CSSPrimitiveValue> parseColumnWidth();
    RefPtr<CSSPrimitiveValue> parseColumnCount();
    bool parseColumnsShorthand(bool important);

#if ENABLE(IOS_TEXT_AUTOSIZING)
    bool isTextAutosizingEnabled() const;
#endif

#if ENABLE(CSS_GRID_LAYOUT)
    bool isCSSGridLayoutEnabled() const;
    RefPtr<CSSValue> parseGridPosition();
    bool parseGridItemPositionShorthand(CSSPropertyID, bool important);
    enum TrackListType { GridTemplate, GridTemplateNoRepeat, GridAuto };
    RefPtr<CSSValue> parseGridTemplateColumns(TrackListType = GridTemplate);
    bool parseGridTemplateRowsAndAreasAndColumns(bool important);
    bool parseGridTemplateShorthand(bool important);
    bool parseGridShorthand(bool important);
    bool parseGridAreaShorthand(bool important);
    bool parseGridGapShorthand(bool important);
    bool parseSingleGridAreaLonghand(RefPtr<CSSValue>&);
    RefPtr<CSSValue> parseGridTrackList(TrackListType);
    bool parseGridTrackRepeatFunction(CSSValueList&, bool& isAutoRepeat, bool& allTracksAreFixedSized);
    RefPtr<CSSValue> parseGridTrackSize(CSSParserValueList& inputList);
    RefPtr<CSSPrimitiveValue> parseGridBreadth(CSSParserValue&);
    bool parseGridTemplateAreasRow(NamedGridAreaMap&, const unsigned, unsigned&);
    RefPtr<CSSValue> parseGridTemplateAreas();
    bool parseGridLineNames(CSSParserValueList&, CSSValueList&, CSSGridLineNamesValue* = nullptr);
    RefPtr<CSSValue> parseGridAutoFlow(CSSParserValueList&);
#endif

    bool parseDashboardRegions(CSSPropertyID, bool important);

    bool parseClipShape(CSSPropertyID, bool important);

    bool parseLegacyPosition(CSSPropertyID, bool important);
    bool parseItemPositionOverflowPosition(CSSPropertyID, bool important);
    RefPtr<CSSContentDistributionValue> parseContentDistributionOverflowPosition();

#if ENABLE(CSS_SHAPES)
    RefPtr<CSSValue> parseShapeProperty(CSSPropertyID);
#endif

    RefPtr<CSSValueList> parseBasicShapeAndOrBox(CSSPropertyID propId);
    RefPtr<CSSPrimitiveValue> parseBasicShape();
    RefPtr<CSSPrimitiveValue> parseShapeRadius(CSSParserValue&);
    RefPtr<CSSBasicShapeCircle> parseBasicShapeCircle(CSSParserValueList&);
    RefPtr<CSSBasicShapeEllipse> parseBasicShapeEllipse(CSSParserValueList&);
    RefPtr<CSSBasicShapePolygon> parseBasicShapePolygon(CSSParserValueList&);
    RefPtr<CSSBasicShapePath> parseBasicShapePath(CSSParserValueList&);
    RefPtr<CSSBasicShapeInset> parseBasicShapeInset(CSSParserValueList&);

    bool parseFont(bool important);
    void parseSystemFont(bool important);
    RefPtr<CSSValueList> parseFontFamily();

    bool parseCounter(CSSPropertyID, int defaultValue, bool important);
    RefPtr<CSSPrimitiveValue> parseCounterContent(CSSParserValueList& args, bool counters);

    bool parseColorParameters(CSSParserValue&, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(CSSParserValue&, double* colorValues, bool parseAlpha);
    RefPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = nullptr);
    bool parseColorFromValue(CSSParserValue&, RGBA32&);
    void parseSelector(const String&, CSSSelectorList&);

    template<typename StringType>
    static bool fastParseColor(RGBA32&, const StringType&, bool strict);

    bool parseLineHeight(bool important);
    bool parseFontSize(bool important);
    bool parseFontWeight(bool important);
    bool parseFontSynthesis(bool important);
    bool parseFontFaceSrc();
    bool parseFontFaceUnicodeRange();

    bool parseSVGValue(CSSPropertyID propId, bool important);
    RefPtr<SVGPaint> parseSVGPaint();
    RefPtr<SVGColor> parseSVGColor();
    RefPtr<CSSValueList> parseSVGStrokeDasharray();
    RefPtr<CSSValueList> parsePaintOrder();

    // CSS3 Parsing Routines (for properties specific to CSS3)
    RefPtr<CSSValueList> parseShadow(CSSParserValueList&, CSSPropertyID);
    bool parseBorderImage(CSSPropertyID, RefPtr<CSSValue>&, bool important = false);
    bool parseBorderImageRepeat(RefPtr<CSSValue>&);
    bool parseBorderImageSlice(CSSPropertyID, RefPtr<CSSBorderImageSliceValue>&);
    bool parseBorderImageWidth(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderImageOutset(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderRadius(CSSPropertyID, bool important);

    bool parseAspectRatio(bool important);

    bool parseReflect(CSSPropertyID, bool important);

    bool parseFlex(CSSParserValueList& args, bool important);

    // Image generators
    bool parseCanvas(CSSParserValueList&, RefPtr<CSSValue>&);
    bool parseNamedImage(CSSParserValueList&, RefPtr<CSSValue>&);

    bool parseDeprecatedGradient(CSSParserValueList&, RefPtr<CSSValue>&);
    bool parseDeprecatedLinearGradient(CSSParserValueList&, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseDeprecatedRadialGradient(CSSParserValueList&, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseLinearGradient(CSSParserValueList&, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList&, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList&, CSSGradientValue&, bool expectComma);

    bool parseCrossfade(CSSParserValueList&, RefPtr<CSSValue>&, bool prefixed);

#if ENABLE(CSS_IMAGE_RESOLUTION)
    RefPtr<CSSValueList> parseImageResolution();
#endif

    RefPtr<CSSImageSetValue> parseImageSet();

    bool parseFilterImage(CSSParserValueList&, RefPtr<CSSValue>&);

    bool parseFilter(CSSParserValueList&, RefPtr<CSSValueList>&);
    RefPtr<WebKitCSSFilterValue> parseBuiltinFilterArguments(CSSParserValueList&, WebKitCSSFilterValue::FilterOperationType);

    RefPtr<CSSValue> parseClipPath();

    static bool isBlendMode(CSSValueID);
    static bool isCompositeOperator(CSSValueID);

    RefPtr<CSSValueList> parseTransform();
    RefPtr<WebKitCSSTransformValue> parseTransformValue(CSSParserValue&);
    bool parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&, RefPtr<CSSValue>&);
    bool parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,  RefPtr<CSSPrimitiveValue>&, RefPtr<CSSPrimitiveValue>&);

    bool parseTextEmphasisStyle(bool important);
    bool parseTextEmphasisPosition(bool important);

    void addTextDecorationProperty(CSSPropertyID, RefPtr<CSSValue>&&, bool important);
    bool parseTextDecoration(CSSPropertyID propId, bool important);
    bool parseTextDecorationSkip(bool important);
    bool parseTextUnderlinePosition(bool important);

    RefPtr<CSSValueList> parseTextIndent();
    
    bool parseHangingPunctuation(bool important);

    bool parseLineBoxContain(bool important);
    RefPtr<CSSCalcValue> parseCalculation(CSSParserValue&, CalculationPermittedValueRange);

    bool parseFontFeatureTag(CSSValueList&);
    bool parseFontFeatureSettings(bool important);

    bool parseFlowThread(CSSPropertyID, bool important);
    bool parseRegionThread(CSSPropertyID, bool important);

    bool parseFontVariantLigatures(bool important, bool unknownIsFailure, bool implicit);
    bool parseFontVariantNumeric(bool important, bool unknownIsFailure, bool implicit);
    bool parseFontVariantEastAsian(bool important, bool unknownIsFailure, bool implicit);
    bool parseFontVariant(bool important);

    bool parseWillChange(bool important);

    // Faster than doing a new/delete each time since it keeps one vector.
    std::unique_ptr<Vector<std::unique_ptr<CSSParserSelector>>> createSelectorVector();
    void recycleSelectorVector(std::unique_ptr<Vector<std::unique_ptr<CSSParserSelector>>>);

    RefPtr<StyleRuleImport> createImportRule(const CSSParserString&, RefPtr<MediaQuerySet>&&);
    RefPtr<StyleKeyframe> createKeyframe(CSSParserValueList&);
    Ref<StyleRuleKeyframes> createKeyframesRule(const String&, std::unique_ptr<Vector<RefPtr<StyleKeyframe>>>);

    typedef Vector<RefPtr<StyleRuleBase>> RuleList;
    Ref<StyleRuleMedia> createMediaRule(RefPtr<MediaQuerySet>&&, RuleList*);
    Ref<StyleRuleMedia> createEmptyMediaRule(RuleList*);
    RefPtr<StyleRule> createStyleRule(Vector<std::unique_ptr<CSSParserSelector>>* selectors);
    RefPtr<StyleRuleFontFace> createFontFaceRule();
    RefPtr<StyleRulePage> createPageRule(std::unique_ptr<CSSParserSelector> pageSelector);
    RefPtr<StyleRuleRegion> createRegionRule(Vector<std::unique_ptr<CSSParserSelector>>* regionSelector, RuleList* rules);
    void createMarginAtRule(CSSSelector::MarginBoxType);
    Ref<StyleRuleSupports> createSupportsRule(bool conditionIsSupported, RuleList*);
    void markSupportsRuleHeaderStart();
    void markSupportsRuleHeaderEnd();
    Ref<CSSRuleSourceData> popSupportsRuleData();

    void startDeclarationsForMarginBox();
    void endDeclarationsForMarginBox();

    void addNamespace(const AtomicString& prefix, const AtomicString& uri);
    QualifiedName determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName);

    void rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector&);
    void rewriteSpecifiersWithElementName(const QualifiedName& tagName, CSSParserSelector&, bool isNamespacePlaceholder = false);
    void rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector&);
    std::unique_ptr<CSSParserSelector> rewriteSpecifiers(std::unique_ptr<CSSParserSelector>, std::unique_ptr<CSSParserSelector>);

    void invalidBlockHit();

    void updateLastSelectorLineAndPosition();
    void updateLastMediaLine(MediaQuerySet&);

    void clearProperties();

    Ref<ImmutableStyleProperties> createStyleProperties();

    static const unsigned invalidParsedPropertiesCount;

    CSSParserContext m_context;

    bool m_important { false };
    CSSPropertyID m_id { CSSPropertyInvalid };
    AtomicString m_customPropertyName;
    StyleSheetContents* m_styleSheet { nullptr };
    RefPtr<StyleRuleBase> m_rule;
    RefPtr<StyleKeyframe> m_keyframe;
    std::unique_ptr<MediaQuery> m_mediaQuery;
    std::unique_ptr<Vector<SourceSize>> m_sourceSizeList;
    std::unique_ptr<CSSParserValueList> m_valueList;
    bool m_supportsCondition { false };

    ParsedPropertyVector m_parsedProperties;
    CSSSelectorList* m_selectorListForParseSelector { nullptr };

    unsigned m_numParsedPropertiesBeforeMarginBox { invalidParsedPropertiesCount };

    int m_inParseShorthand { 0 };
    CSSPropertyID m_currentShorthand { CSSPropertyInvalid };
    bool m_implicitShorthand { false };

    bool m_hadSyntacticallyValidCSSRule { false };
    bool m_logErrors { false };
    bool m_ignoreErrorsInDeclaration { false };

    AtomicString m_defaultNamespace { starAtom };

    // tokenizer methods and data
    size_t m_parsedTextPrefixLength { 0 };
    unsigned m_nestedSelectorLevel { 0 };
    SourceRange m_selectorRange;
    SourceRange m_propertyRange { std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max() };
    std::unique_ptr<RuleSourceDataList> m_currentRuleDataStack;
    RefPtr<CSSRuleSourceData> m_currentRuleData;
    RuleSourceDataList* m_ruleSourceDataResult { nullptr };

    void fixUnparsedPropertyRanges(CSSRuleSourceData&);
    void markRuleHeaderStart(CSSRuleSourceData::Type);
    void markRuleHeaderEnd();

    void startNestedSelectorList() { ++m_nestedSelectorLevel; }
    void endNestedSelectorList() { --m_nestedSelectorLevel; }
    void markSelectorStart();
    void markSelectorEnd();

    void markRuleBodyStart();
    void markRuleBodyEnd();
    void markPropertyStart();
    void markPropertyEnd(bool isImportantFound, bool isPropertyParsed);
    void processAndAddNewRuleToSourceTreeIfNeeded();
    void addNewRuleToSourceTree(Ref<CSSRuleSourceData>&&);
    RefPtr<CSSRuleSourceData> popRuleData();
    void resetPropertyRange() { m_propertyRange.start = m_propertyRange.end = std::numeric_limits<unsigned>::max(); }
    bool isExtractingSourceData() const { return !!m_currentRuleDataStack; }
    void syntaxError(const Location&, SyntaxErrorType = GeneralSyntaxError);

    inline int lex(void* yylval) { return (this->*m_lexFunc)(yylval); }

    int token() { return m_token; }

#if ENABLE(CSS_DEVICE_ADAPTATION)
    void markViewportRuleBodyStart() { m_inViewport = true; }
    void markViewportRuleBodyEnd() { m_inViewport = false; }
    Ref<StyleRuleViewport> createViewportRule();
#endif

    Ref<CSSPrimitiveValue> createPrimitiveNumericValue(ValueWithCalculation&);
    Ref<CSSPrimitiveValue> createPrimitiveStringValue(CSSParserValue&);

    static URL completeURL(const CSSParserContext&, const String& url);

    Location currentLocation();
    static bool isCalculation(CSSParserValue&);

    void setCustomPropertyName(const AtomicString& propertyName) { m_customPropertyName = propertyName; }

    RefPtr<CSSValue> parseVariableDependentValue(CSSPropertyID, const CSSVariableDependentValue&, const CustomPropertyValueMap& customProperties, TextDirection, WritingMode);

private:
    bool is8BitSource() { return m_is8BitSource; }

    template <typename SourceCharacterType>
    int realLex(void* yylval);

    UChar*& currentCharacter16();

    template <typename CharacterType>
    inline CharacterType*& currentCharacter();

    template <typename CharacterType>
    inline CharacterType* tokenStart();

    template <typename CharacterType>
    inline void setTokenStart(CharacterType*);

    inline unsigned tokenStartOffset();
    inline UChar tokenStartChar();

    inline unsigned currentCharacterOffset();

    template <typename CharacterType>
    inline bool isIdentifierStart();

    template <typename CharacterType>
    unsigned parseEscape(CharacterType*&);
    template <typename DestCharacterType>
    inline void UnicodeToChars(DestCharacterType*&, unsigned);
    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseIdentifierInternal(SrcCharacterType*&, DestCharacterType*&, bool&);

    template <typename CharacterType>
    inline void parseIdentifier(CharacterType*&, CSSParserString&, bool&);

    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseStringInternal(SrcCharacterType*&, DestCharacterType*&, UChar);

    template <typename CharacterType>
    inline void parseString(CharacterType*&, CSSParserString& resultString, UChar);

    template <typename CharacterType>
    inline bool findURI(CharacterType*& start, CharacterType*& end, UChar& quote);

    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseURIInternal(SrcCharacterType*&, DestCharacterType*&, UChar quote);

    template <typename CharacterType>
    inline void parseURI(CSSParserString&);
    template <typename CharacterType>
    inline bool parseUnicodeRange();
    template <typename CharacterType>
    bool parseNthChild();
    template <typename CharacterType>
    bool parseNthChildExtra();
    template <typename CharacterType>
    inline bool detectFunctionTypeToken(int);
    template <typename CharacterType>
    inline void detectMediaQueryToken(int);
    template <typename CharacterType>
    inline void detectNumberToken(CharacterType*, int);
    template <typename CharacterType>
    inline void detectDashToken(int);
    template <typename CharacterType>
    inline void detectAtToken(int, bool);
    template <typename CharacterType>
    inline void detectSupportsToken(int);

    template <typename CharacterType>
    inline void setRuleHeaderEnd(const CharacterType*);

    void setStyleSheet(StyleSheetContents* styleSheet) { m_styleSheet = styleSheet; }

    inline bool inStrictMode() const { return m_context.mode == CSSStrictMode || m_context.mode == SVGAttributeMode; }
    inline bool inQuirksMode() const { return m_context.mode == CSSQuirksMode; }
    
    URL completeURL(const String& url) const;

    void recheckAtKeyword(const UChar* str, int len);

    template<unsigned prefixLength, unsigned suffixLength>
    void setupParser(const char (&prefix)[prefixLength], StringView string, const char (&suffix)[suffixLength])
    {
        setupParser(prefix, prefixLength - 1, string, suffix, suffixLength - 1);
    }
    void setupParser(const char* prefix, unsigned prefixLength, StringView, const char* suffix, unsigned suffixLength);
    bool inShorthand() const { return m_inParseShorthand; }

    bool isValidSize(ValueWithCalculation&);

    bool isGeneratedImageValue(CSSParserValue&) const;
    bool parseGeneratedImage(CSSParserValueList&, RefPtr<CSSValue>&);

    ParseResult parseValue(MutableStyleProperties&, CSSPropertyID, const String&, bool important, StyleSheetContents* contextStyleSheet);
    Ref<ImmutableStyleProperties> parseDeclaration(const String&, StyleSheetContents* contextStyleSheet);

    RefPtr<CSSBasicShapeInset> parseInsetRoundedCorners(Ref<CSSBasicShapeInset>&&, CSSParserValueList&);

    enum SizeParameterType {
        None,
        Auto,
        Length,
        PageSize,
        Orientation,
    };

    bool parsePage(CSSPropertyID propId, bool important);
    bool parseSize(CSSPropertyID propId, bool important);
    SizeParameterType parseSizeParameter(CSSValueList& parsedValues, CSSParserValue&, SizeParameterType prevParamType);

#if ENABLE(CSS_SCROLL_SNAP)
    bool parseNonElementSnapPoints(CSSPropertyID propId, bool important);
    bool parseScrollSnapDestination(CSSPropertyID propId, bool important);
    bool parseScrollSnapCoordinate(CSSPropertyID propId, bool important);
    bool parseScrollSnapPositions(RefPtr<CSSValue>& cssValueX, RefPtr<CSSValue>& cssValueY);
#endif

    bool parseFontFaceSrcURI(CSSValueList&);
    bool parseFontFaceSrcLocal(CSSValueList&);

    bool parseColor(const String&);

#if ENABLE(CSS_GRID_LAYOUT)
    bool parseIntegerOrCustomIdentFromGridPosition(RefPtr<CSSPrimitiveValue>& numericValue, RefPtr<CSSPrimitiveValue>& gridLineName);
#endif

    enum ParsingMode {
        NormalMode,
        MediaQueryMode,
        SupportsMode,
        NthChildMode
    };

    ParsingMode m_parsingMode { NormalMode };
    bool m_is8BitSource { false };
    std::unique_ptr<LChar[]> m_dataStart8;
    std::unique_ptr<UChar[]> m_dataStart16;
    LChar* m_currentCharacter8 { nullptr };
    UChar* m_currentCharacter16 { nullptr };
    union {
        LChar* ptr8;
        UChar* ptr16;
    } m_tokenStart { nullptr };
    unsigned m_length { 0 };
    int m_token { 0 };
    int m_lineNumber { 0 };
    int m_tokenStartLineNumber { 0 };
    int m_tokenStartColumnNumber { 0 };
    int m_lastSelectorLineNumber { 0 };
    int m_columnOffsetForLine { 0 };

    int m_sheetStartLineNumber { 0 };
    int m_sheetStartColumnNumber { 0 };

    bool m_allowImportRules { true };
    bool m_allowNamespaceDeclarations { true };

#if ENABLE(CSS_DEVICE_ADAPTATION)
    bool parseViewportProperty(CSSPropertyID propId, bool important);
    bool parseViewportShorthand(CSSPropertyID propId, CSSPropertyID first, CSSPropertyID second, bool important);

    bool inViewport() const { return m_inViewport; }
    bool m_inViewport { false };
#endif

    bool useLegacyBackgroundSizeShorthandBehavior() const;

    int (CSSParser::*m_lexFunc)(void*);

    std::unique_ptr<Vector<std::unique_ptr<CSSParserSelector>>> m_recycledSelectorVector;

    std::unique_ptr<RuleSourceDataList> m_supportsRuleDataStack;

    // defines units allowed for a certain property, used in parseUnit
    enum Units {
        FUnknown = 0x0000,
        FInteger = 0x0001,
        FNumber = 0x0002, // Real Numbers
        FPercent = 0x0004,
        FLength = 0x0008,
        FAngle = 0x0010,
        FTime = 0x0020,
        FFrequency = 0x0040,
        FPositiveInteger = 0x0080,
        FRelative = 0x0100,
#if ENABLE(CSS_IMAGE_RESOLUTION) || ENABLE(RESOLUTION_MEDIA_QUERY)
        FResolution = 0x0200,
#endif
        FNonNeg = 0x0400
    };

    friend inline Units operator|(Units a, Units b)
    {
        return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    enum ReleaseParsedCalcValueCondition {
        ReleaseParsedCalcValue,
        DoNotReleaseParsedCalcValue
    };

    bool isLoggingErrors();
    void logError(const String& message, int lineNumber, int columnNumber);

    bool validateCalculationUnit(ValueWithCalculation&, Units);

    bool shouldAcceptUnitLessValues(CSSParserValue&, Units, CSSParserMode);

    inline bool validateUnit(ValueWithCalculation& value, Units unitFlags) { return validateUnit(value, unitFlags, m_context.mode); }
    bool validateUnit(ValueWithCalculation&, Units, CSSParserMode);

    bool parseBorderImageQuad(Units, RefPtr<CSSPrimitiveValue>&);
    int colorIntFromValue(ValueWithCalculation&);
    double parsedDouble(ValueWithCalculation&);
    
    friend class TransformOperationInfo;
    friend class FilterOperationInfo;
};

CSSPropertyID cssPropertyID(const CSSParserString&);
CSSPropertyID cssPropertyID(const String&);
CSSValueID cssValueKeywordID(const CSSParserString&);
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

struct CSSParser::Location {
    int lineNumber;
    int columnNumber;
    CSSParserString token;
};

String quoteCSSString(const String&);
String quoteCSSStringIfNeeded(const String&);
String quoteCSSURLIfNeeded(const String&);

bool isValidNthToken(const CSSParserString&);

template <>
inline void CSSParser::setTokenStart<LChar>(LChar* tokenStart)
{
    m_tokenStart.ptr8 = tokenStart;
}

template <>
inline void CSSParser::setTokenStart<UChar>(UChar* tokenStart)
{
    m_tokenStart.ptr16 = tokenStart;
}

inline unsigned CSSParser::tokenStartOffset()
{
    if (is8BitSource())
        return m_tokenStart.ptr8 - m_dataStart8.get();
    return m_tokenStart.ptr16 - m_dataStart16.get();
}

unsigned CSSParser::currentCharacterOffset()
{
    if (is8BitSource())
        return m_currentCharacter8 - m_dataStart8.get();
    return m_currentCharacter16 - m_dataStart16.get();
}

inline UChar CSSParser::tokenStartChar()
{
    if (is8BitSource())
        return *m_tokenStart.ptr8;
    return *m_tokenStart.ptr16;
}

inline bool isCustomPropertyName(const String& propertyName)
{
    return propertyName.length() > 2 && propertyName.characterAt(0) == '-' && propertyName.characterAt(1) == '-';
}

inline int cssyylex(void* yylval, CSSParser* parser)
{
    return parser->lex(yylval);
}

} // namespace WebCore
