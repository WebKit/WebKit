/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorStyleSheet.h"

#include "CSSContainerRule.h"
#include "CSSGroupingRule.h"
#include "CSSImportRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSParserEnum.h"
#include "CSSParserObserver.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSSelectorParser.h"
#include "CSSStyleProperties.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsRule.h"
#include "CSSTokenizer.h"
#include "ContentSecurityPolicy.h"
#include "DocumentInlines.h"
#include "DocumentView.h"
#include "Element.h"
#include "ExtensionStyleSheets.h"
#include "EventTargetInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "InspectorCSSAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorResourceUtilities.h"
#include "MediaList.h"
#include "Node.h"
#include "SVGElementTypeHelpers.h"
#include "SVGStyleElement.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/NotFound.h>
#include <wtf/text/MakeString.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>

using JSON::ArrayOf;

namespace WebCore {

using namespace Inspector;

enum class RuleFlatteningStrategy {
    Ignore,
    CommitSelfThenChildren,
};

static RuleFlatteningStrategy flatteningStrategyForStyleRuleType(StyleRuleType styleRuleType)
{
    switch (styleRuleType) {
    case StyleRuleType::Style:
    case StyleRuleType::StyleWithNesting:
    case StyleRuleType::Media:
    case StyleRuleType::Supports:
    case StyleRuleType::LayerBlock:
    case StyleRuleType::Container:
    case StyleRuleType::Scope:
    case StyleRuleType::StartingStyle:
        // These rules MUST be handled by the following methods in order to provide functionality in
        // and avoid mismatched lists of source data and CSSOM wrappers:
        // - `isValidRuleHeaderText`
        // - `protocolGroupingTypeForStyleRuleType`
        // - `asCSSRuleList`
        // - `InspectorCSSOMWrappers::collect` .
        return RuleFlatteningStrategy::CommitSelfThenChildren;

    // FIXME (webkit.org/b/284176): support @position-try in Web Inspector.
    case StyleRuleType::PositionTry:

    case StyleRuleType::Charset:
    case StyleRuleType::Import:
    case StyleRuleType::FontFace:
    case StyleRuleType::Page:
    case StyleRuleType::Keyframes:
    case StyleRuleType::Keyframe:
    case StyleRuleType::Margin:
    case StyleRuleType::Namespace:
    case StyleRuleType::CounterStyle:
    case StyleRuleType::LayerStatement:
    case StyleRuleType::FontFeatureValues:
    case StyleRuleType::FontFeatureValuesBlock:
    case StyleRuleType::FontPaletteValues:
    case StyleRuleType::Property:
    case StyleRuleType::ViewTransition:
    case StyleRuleType::NestedDeclarations:
    case StyleRuleType::Function:
    case StyleRuleType::FunctionDeclarations:
        // These rule types do not contain rules that apply directly to an element (i.e. these rules should not appear
        // in the Styles details sidebar of the Elements tab in Web Inspector).
        return RuleFlatteningStrategy::Ignore;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static CSSParserContext parserContextForDocument(Document* document)
{
    return document ? CSSParserContext(*document) : strictCSSParserContext();
}

static ASCIILiteral atRuleIdentifierForType(StyleRuleType styleRuleType)
{
    switch (styleRuleType) {
    case StyleRuleType::Media:
        return "@media"_s;
    case StyleRuleType::Supports:
        return "@supports"_s;
    case StyleRuleType::LayerBlock:
        return "@layer"_s;
    case StyleRuleType::Container:
        return "@container"_s;
    case StyleRuleType::Scope:
        return "@scope"_s;
    case StyleRuleType::StartingStyle:
        return "@starting-style"_s;
    default:
        ASSERT_NOT_REACHED();
        return ""_s;
    }
}

static bool isValidRuleHeaderText(const String& headerText, StyleRuleType styleRuleType, Document* document, CSSParserEnum::NestedContext nestedContext = { })
{
    auto isValidAtRuleHeaderText = [&] (const String& atRuleIdentifier) {
        if (headerText.isEmpty())
            return false;

        // Make sure the engine can parse the provided `@` rule, even if it only uses unsupported features. As long as
        // the rule text is entirely consumed and it creates a rule of the expected type, we consider it valid because
        // we will be able to continue to edit the rule in the future.
        CSSParserContext context(parserContextForDocument(document)); // CSSParser holds a reference to this.
        CSSParser parser(context, makeString(atRuleIdentifier, ' ', headerText, " {}"_s));
        if (!parser.tokenizer())
            return false;

        auto tokenRange = parser.tokenizer()->tokenRange();
        auto rule = parser.consumeAtRule(tokenRange, CSSParser::AllowedRules::RegularRules);

        if (!rule)
            return false;

        if (rule->type() != styleRuleType)
            return false;

        // The new header text may cause a valid rule to be created without us parsing the entire range. For example new
        // header text of `screen {} @media print` would have caused us to parse `@media screen {} @media print {}` to
        // get a valid rule, but still have more text left over afterward the first `@media` rule was parsed.
        if (!tokenRange.atEnd())
            return false;

        return true;
    };

    switch (styleRuleType) {
    case StyleRuleType::Style:
        return !!CSSSelectorParser::parseSelectorList(headerText, parserContextForDocument(document), nullptr, nestedContext);
    case StyleRuleType::Media:
    case StyleRuleType::Supports:
    case StyleRuleType::LayerBlock:
    case StyleRuleType::Container:
    case StyleRuleType::Scope:
    case StyleRuleType::StartingStyle:
        return isValidAtRuleHeaderText(atRuleIdentifierForType(styleRuleType));
    default:
        return false;
    }
}

static std::optional<Inspector::Protocol::CSS::Grouping::Type> protocolGroupingTypeForStyleRuleType(StyleRuleType styleRuleType)
{
    switch (styleRuleType) {
    case StyleRuleType::Style:
        return Inspector::Protocol::CSS::Grouping::Type::StyleRule;
    case StyleRuleType::Media:
        return Inspector::Protocol::CSS::Grouping::Type::MediaRule;
    case StyleRuleType::Supports:
        return Inspector::Protocol::CSS::Grouping::Type::SupportsRule;
    case StyleRuleType::LayerBlock:
        return Inspector::Protocol::CSS::Grouping::Type::LayerRule;
    case StyleRuleType::Container:
        return Inspector::Protocol::CSS::Grouping::Type::ContainerRule;
    case StyleRuleType::Scope:
        return Inspector::Protocol::CSS::Grouping::Type::ScopeRule;
    case StyleRuleType::StartingStyle:
        return Inspector::Protocol::CSS::Grouping::Type::StartingStyleRule;
    default:
        return std::nullopt;
    }
}

class ParsedStyleSheet {
    WTF_MAKE_TZONE_ALLOCATED(ParsedStyleSheet);
public:
    ParsedStyleSheet();

    const String& text() const { ASSERT(m_hasText); return m_text; }
    void setText(const String& text);
    bool hasText() const { return m_hasText; }
    RuleSourceDataList* sourceData() const { return m_sourceData.get(); }
    void setSourceData(std::unique_ptr<RuleSourceDataList>);
    bool hasSourceData() const { return m_sourceData != nullptr; }
    WebCore::CSSRuleSourceData* ruleSourceDataAt(unsigned) const;

private:

    String m_text;
    bool m_hasText;
    std::unique_ptr<RuleSourceDataList> m_sourceData;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(ParsedStyleSheet);

ParsedStyleSheet::ParsedStyleSheet()
    : m_hasText(false)
{
}

void ParsedStyleSheet::setText(const String& text)
{
    m_hasText = true;
    m_text = text;
    setSourceData(nullptr);
}

static void flattenSourceData(RuleSourceDataList& dataList, RuleSourceDataList& target)
{
    for (auto& data : dataList) {
        switch (flatteningStrategyForStyleRuleType(data->type)) {
        case RuleFlatteningStrategy::CommitSelfThenChildren:
            target.append(data.copyRef());
            flattenSourceData(data->childRules, target);
            break;

        case RuleFlatteningStrategy::Ignore:
            break;
        }
    }
}

void ParsedStyleSheet::setSourceData(std::unique_ptr<RuleSourceDataList> sourceData)
{
    if (!sourceData) {
        m_sourceData.reset();
        return;
    }

    m_sourceData = makeUnique<RuleSourceDataList>();
    flattenSourceData(*sourceData, *m_sourceData);
}

WebCore::CSSRuleSourceData* ParsedStyleSheet::ruleSourceDataAt(unsigned index) const
{
    if (!hasSourceData() || index >= m_sourceData->size())
        return nullptr;

    return m_sourceData->at(index).ptr();
}

class StyleSheetHandler : public CSSParserObserver {
public:
    StyleSheetHandler(const String& parsedText, Document* document, RuleSourceDataList* result)
        : m_parsedText(parsedText)
        , m_document(document)
        , m_ruleSourceDataResult(result)
    {
        ASSERT(m_ruleSourceDataResult);
    }
    
private:
    void startRuleHeader(StyleRuleType, unsigned) override;
    void endRuleHeader(unsigned) override;
    void observeSelector(unsigned startOffset, unsigned endOffset) override;
    void startRuleBody(unsigned) override;
    void endRuleBody(unsigned) override;
    void markRuleBodyContainsImplicitlyNestedProperties() override;
    void observeProperty(unsigned startOffset, unsigned endOffset, bool isImportant, bool isParsed) override;
    void observeComment(unsigned startOffset, unsigned endOffset) override;
    
    Ref<CSSRuleSourceData> popRuleData();
    template <typename CharacterType> inline void setRuleHeaderEnd(std::span<const CharacterType>);
    void fixUnparsedPropertyRanges(CSSRuleSourceData*);
    
    const String& m_parsedText;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
    
    RuleSourceDataList m_currentRuleDataStack;
    RefPtr<CSSRuleSourceData> m_currentRuleData;
    RuleSourceDataList* m_ruleSourceDataResult { nullptr };
};

void StyleSheetHandler::startRuleHeader(StyleRuleType type, unsigned offset)
{
    // Pop off data for a previous invalid rule.
    if (m_currentRuleData)
        m_currentRuleDataStack.removeLast();

    auto data = CSSRuleSourceData::create(type);
    data->ruleHeaderRange.start = offset;
    m_currentRuleData = data.copyRef();
    m_currentRuleDataStack.append(WTF::move(data));
}

template <typename CharacterType> inline void StyleSheetHandler::setRuleHeaderEnd(std::span<const CharacterType> data)
{
    while (data.size() > m_currentRuleDataStack.last()->ruleHeaderRange.start) {
        if (isASCIIWhitespace<CharacterType>(data[data.size() - 1]))
            dropLast(data);
        else
            break;
    }

    m_currentRuleDataStack.last()->ruleHeaderRange.end = data.size();
    if (!m_currentRuleDataStack.last()->selectorRanges.isEmpty())
        m_currentRuleDataStack.last()->selectorRanges.last().end = data.size();
}

void StyleSheetHandler::endRuleHeader(unsigned offset)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    
    if (m_parsedText.is8Bit())
        setRuleHeaderEnd<Latin1Character>(m_parsedText.span8().first(offset));
    else
        setRuleHeaderEnd<char16_t>(m_parsedText.span16().first(offset));
}

void StyleSheetHandler::observeSelector(unsigned startOffset, unsigned endOffset)
{
    ASSERT(m_currentRuleDataStack.size());
    m_currentRuleDataStack.last()->selectorRanges.append(SourceRange(startOffset, endOffset));
}

void StyleSheetHandler::startRuleBody(unsigned offset)
{
    m_currentRuleData = nullptr;
    ASSERT(!m_currentRuleDataStack.isEmpty());
    
    // Skip the rule body opening brace.
    if (m_parsedText[offset] == '{')
        ++offset;

    m_currentRuleDataStack.last()->ruleBodyRange.start = offset;
}

void StyleSheetHandler::endRuleBody(unsigned offset)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleDataStack.last()->ruleBodyRange.end = offset;
    auto rule = popRuleData();
    fixUnparsedPropertyRanges(rule.ptr());

    if (rule->containsImplicitlyNestedProperties && rule->styleSourceData->propertyData.size()) {
        // Property declarations made outside of a Style rule will become part of a special inferred `& {}` rule after
        // the parser has already sent observers their notification of the property. In order to match the CSSOM
        // representation this must be handled here as well, otherwise the flat rule lists will differ in length.
        auto inferredStyleRule = CSSRuleSourceData::create(StyleRuleType::Style);
        inferredStyleRule->isImplicitlyNested = true;
        inferredStyleRule->ruleHeaderRange = rule->ruleHeaderRange;
        inferredStyleRule->ruleBodyRange = rule->ruleBodyRange;

        std::swap(rule->styleSourceData, inferredStyleRule->styleSourceData);

        // Inferred style rules are always logically placed at the start of their parent rule.
        rule->childRules.insert(0, WTF::move(inferredStyleRule));
    }

    if (m_currentRuleDataStack.isEmpty())
        m_ruleSourceDataResult->append(WTF::move(rule));
    else
        m_currentRuleDataStack.last()->childRules.append(WTF::move(rule));
}

void StyleSheetHandler::markRuleBodyContainsImplicitlyNestedProperties()
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleDataStack.last()->containsImplicitlyNestedProperties = true;
}

Ref<CSSRuleSourceData> StyleSheetHandler::popRuleData()
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleData = nullptr;
    auto data = WTF::move(m_currentRuleDataStack.last());
    m_currentRuleDataStack.removeLast();
    return data;
}

template <typename CharacterType>
static inline void fixUnparsedProperties(std::span<const CharacterType> characters, CSSRuleSourceData* ruleData)
{
    Vector<CSSPropertySourceData>& propertyData = ruleData->styleSourceData->propertyData;
    unsigned size = propertyData.size();
    if (!size)
        return;
    
    unsigned styleStart = ruleData->ruleBodyRange.start;
    
    CSSPropertySourceData* nextData = &(propertyData.at(0));
    for (unsigned i = 0; i < size; ++i) {
        CSSPropertySourceData* currentData = nextData;
        nextData = i < size - 1 ? &(propertyData.at(i + 1)) : nullptr;
        
        if (currentData->parsedOk)
            continue;
        if (currentData->range.end > 0 && characters[styleStart + currentData->range.end - 1] == ';')
            continue;
        
        unsigned propertyEnd;
        if (!nextData)
            propertyEnd = ruleData->ruleBodyRange.end - 1;
        else
            propertyEnd = styleStart + nextData->range.start - 1;
        
        while (isASCIIWhitespace<CharacterType>(characters[propertyEnd]))
            --propertyEnd;
        
        // propertyEnd points at the last property text character.
        unsigned newRangeEnd = (propertyEnd - styleStart) + 1;
        if (currentData->range.end != newRangeEnd) {
            currentData->range.end = newRangeEnd;
            unsigned valueStart = styleStart + currentData->range.start + currentData->name.length();
            while (valueStart < propertyEnd && characters[valueStart] != ':')
                ++valueStart;
            
            // Shift past the ':'.
            if (valueStart < propertyEnd)
                ++valueStart;

            while (valueStart < propertyEnd && isASCIIWhitespace<CharacterType>(characters[valueStart]))
                ++valueStart;
            
            // Need to exclude the trailing ';' from the property value.
            currentData->value = String(characters.subspan(valueStart, propertyEnd - valueStart + (characters[propertyEnd] == ';' ? 0 : 1)));
        }
    }
}

void StyleSheetHandler::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData)
{
    if (!ruleData->styleSourceData)
        return;
    
    if (m_parsedText.is8Bit()) {
        fixUnparsedProperties<Latin1Character>(m_parsedText.span8(), ruleData);
        return;
    }
    
    fixUnparsedProperties<char16_t>(m_parsedText.span16(), ruleData);
}

void StyleSheetHandler::observeProperty(unsigned startOffset, unsigned endOffset, bool isImportant, bool isParsed)
{
    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->styleSourceData)
        return;
    
    ASSERT(endOffset <= m_parsedText.length());
    
    // Include semicolon in the property text.
    if (endOffset < m_parsedText.length() && m_parsedText[endOffset] == ';')
        ++endOffset;
    
    ASSERT(startOffset < endOffset);
    auto propertyString = StringView(m_parsedText).substring(startOffset, endOffset - startOffset).trim(deprecatedIsSpaceOrNewline);
    if (propertyString.endsWith(';'))
        propertyString = propertyString.left(propertyString.length() - 1);
    size_t colonIndex = propertyString.find(':');
    ASSERT(colonIndex != notFound);

    auto name = propertyString.left(colonIndex).trim(deprecatedIsSpaceOrNewline).toString();
    auto value = propertyString.substring(colonIndex + 1, propertyString.length()).trim(deprecatedIsSpaceOrNewline).toString();
    
    // FIXME-NEWPARSER: The property range is relative to the declaration start offset, but no
    // good reason for it, and it complicates fixUnparsedProperties.
    SourceRange& topRuleBodyRange = m_currentRuleDataStack.last()->ruleBodyRange;
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(CSSPropertySourceData(name, value, isImportant, false, isParsed, SourceRange(startOffset - topRuleBodyRange.start, endOffset - topRuleBodyRange.start)));
}

void StyleSheetHandler::observeComment(unsigned startOffset, unsigned endOffset)
{
    ASSERT(endOffset <= m_parsedText.length());

    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->ruleHeaderRange.end || !m_currentRuleDataStack.last()->styleSourceData)
        return;
    
    // The lexer is not inside a property AND it is scanning a declaration-aware
    // rule body.
    auto commentTextView = StringView(m_parsedText).substring(startOffset, endOffset - startOffset);
    
    ASSERT(commentTextView.startsWith("/*"_s));
    commentTextView = commentTextView.substring(2);
    
    // Require well-formed comments.
    if (!commentTextView.endsWith("*/"_s))
        return;
    commentTextView = commentTextView.left(commentTextView.length() - 2).trim(deprecatedIsSpaceOrNewline);
    if (commentTextView.isEmpty())
        return;

    auto commentText = commentTextView.toString();
    
    // FIXME: Use the actual rule type rather than STYLE_RULE?
    RuleSourceDataList sourceData;
    
    StyleSheetHandler handler(commentText, m_document.get(), &sourceData);
    CSSParser::parseDeclarationListForInspector(commentText, parserContextForDocument(m_document.get()), handler);
    Vector<CSSPropertySourceData>& commentPropertyData = sourceData.first()->styleSourceData->propertyData;
    if (commentPropertyData.size() != 1)
        return;
    CSSPropertySourceData& propertyData = commentPropertyData.at(0);
    bool parsedOk = propertyData.parsedOk || propertyData.name.startsWith("-moz-"_s) || propertyData.name.startsWith("-o-"_s) || propertyData.name.startsWith("-webkit-"_s) || propertyData.name.startsWith("-ms-"_s);
    if (!parsedOk || propertyData.range.length() != commentText.length())
        return;
    
    // FIXME-NEWPARSER: The property range is relative to the declaration start offset, but no
    // good reason for it, and it complicates fixUnparsedProperties.
    SourceRange& topRuleBodyRange = m_currentRuleDataStack.last()->ruleBodyRange;
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(CSSPropertySourceData(propertyData.name, propertyData.value, false, true, true, SourceRange(startOffset - topRuleBodyRange.start, endOffset - topRuleBodyRange.start)));
}

static RefPtr<Inspector::Protocol::CSS::SourceRange> buildSourceRangeObject(const SourceRange& range, const Vector<size_t>& lineEndings, int* endingLine = nullptr)
{
    if (lineEndings.isEmpty())
        return nullptr;

    TextPosition start = ContentSearchUtilities::textPositionFromOffset(range.start, lineEndings);
    TextPosition end = ContentSearchUtilities::textPositionFromOffset(range.end, lineEndings);

    if (endingLine)
        *endingLine = end.m_line.zeroBasedInt();

    return Inspector::Protocol::CSS::SourceRange::create()
        .setStartLine(start.m_line.zeroBasedInt())
        .setStartColumn(start.m_column.zeroBasedInt())
        .setEndLine(end.m_line.zeroBasedInt())
        .setEndColumn(end.m_column.zeroBasedInt())
        .release();
}

static RefPtr<CSSRuleList> asCSSRuleList(CSSStyleSheet* styleSheet)
{
    if (!styleSheet)
        return nullptr;

    auto list = StaticCSSRuleList::create();
    Vector<RefPtr<CSSRule>>& listRules = list->rules();
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i)
        listRules.append(styleSheet->item(i));
    return list;
}

static RefPtr<CSSRuleList> asCSSRuleList(CSSRule* rule)
{
    if (!rule)
        return nullptr;

    if (auto* styleRule = dynamicDowncast<CSSStyleRule>(rule))
        return &styleRule->cssRules();

    if (auto* keyframesRule = dynamicDowncast<CSSKeyframesRule>(*rule))
        return &keyframesRule->cssRules();

    if (auto* groupingRule = dynamicDowncast<CSSGroupingRule>(*rule))
        return &groupingRule->cssRules();

    return nullptr;
}

static String sourceURLForCSSRule(CSSRule& rule)
{
    if (RefPtr parentStyleSheet = rule.parentStyleSheet()) {
        if (auto sourceURL = parentStyleSheet->contents().baseURL().string(); !sourceURL.isEmpty())
            return sourceURL;

        if (RefPtr ownerDocument = parentStyleSheet->ownerDocument())
            return InspectorDOMAgent::documentURLString(ownerDocument.get());
    }

    return nullString();
}

RefPtr<Inspector::Protocol::CSS::Grouping> InspectorStyleSheet::buildObjectForGrouping(CSSRule* groupingRule)
{
    if (!groupingRule)
        return nullptr;

    auto groupingRuleProtocolType = protocolGroupingTypeForStyleRuleType(groupingRule->styleRuleType());
    if (!groupingRuleProtocolType)
        return nullptr;

    auto payload = Inspector::Protocol::CSS::Grouping::create()
        .setType(*groupingRuleProtocolType)
        .release();

    if (auto ruleId = this->ruleOrStyleId(groupingRule).asProtocolValue<Inspector::Protocol::CSS::CSSRuleId>())
        payload->setRuleId(ruleId.releaseNonNull());

    if (RefPtr<CSSRuleSourceData> sourceData = ensureParsedDataReady() ? ruleSourceDataFor(groupingRule) : nullptr) {
        if (auto text = m_parsedStyleSheet->text().substring(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length()); !text.isEmpty())
            payload->setText(text);

        if (auto range = buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings()))
            payload->setRange(range.releaseNonNull());
    }

    if (auto sourceURL = sourceURLForCSSRule(*groupingRule); !sourceURL.isEmpty())
        payload->setSourceURL(sourceURL);

    return payload;
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::Grouping>> InspectorStyleSheet::buildArrayForGroupings(CSSRule& rule)
{
    auto groupingsPayload = JSON::ArrayOf<Inspector::Protocol::CSS::Grouping>::create();

    RefPtr parentRule = &rule;

    while (parentRule) {
        // The rule for which we are building groupings should not be included in the array of groupings, otherwise
        // every CSSStyleRule will have itself as its first grouping.
        if (parentRule.get() != &rule) {
            if (auto groupingRule = buildObjectForGrouping(parentRule.get()))
                groupingsPayload->addItem(groupingRule.releaseNonNull());
            else if (RefPtr importRule = dynamicDowncast<CSSImportRule>(parentRule.get())) {
                // FIXME: <webkit.org/b/246958> Show import rule as a single rule, instead of two rules for media and layer.
                auto sourceURL = sourceURLForCSSRule(*importRule);
                if (auto layerName = importRule->layerName(); !layerName.isNull()) {
                    auto layerRulePayload = Inspector::Protocol::CSS::Grouping::create()
                        .setType(Inspector::Protocol::CSS::Grouping::Type::LayerImportRule)
                        .release();
                    layerRulePayload->setText(layerName);
                    if (!sourceURL.isEmpty())
                        layerRulePayload->setSourceURL(sourceURL);
                    groupingsPayload->addItem(WTF::move(layerRulePayload));
                }

                if (Ref media = importRule->media(); media->length() && media->mediaText() != "all"_s) {
                    auto mediaRulePayload = Inspector::Protocol::CSS::Grouping::create()
                        .setType(Inspector::Protocol::CSS::Grouping::Type::MediaImportRule)
                        .release();
                    mediaRulePayload->setText(media->mediaText());
                    if (!sourceURL.isEmpty())
                        mediaRulePayload->setSourceURL(sourceURL);
                    groupingsPayload->addItem(WTF::move(mediaRulePayload));
                }
            }
        }

        if (parentRule->parentRule()) {
            parentRule = parentRule->parentRule();
            continue;
        }

        RefPtr styleSheet = parentRule->parentStyleSheet();
        while (styleSheet) {
            RefPtr media = styleSheet->media();
            // FIXME: <webkit.org/b/246959> Support editing `style` and `link` node media queries.
            if (media && media->length() && media->mediaText() != "all"_s) {
                auto sheetGroupingPayload = Inspector::Protocol::CSS::Grouping::create()
                    .setType(is<HTMLStyleElement>(styleSheet->ownerNode()) ? Inspector::Protocol::CSS::Grouping::Type::MediaStyleNode: Inspector::Protocol::CSS::Grouping::Type::MediaLinkNode)
                    .release();
                sheetGroupingPayload->setText(media->mediaText());

                String sourceURL;
                if (RefPtr ownerDocument = styleSheet->ownerDocument())
                    sourceURL = ownerDocument->url().string();
                else if (!styleSheet->contents().baseURL().isEmpty())
                    sourceURL = styleSheet->contents().baseURL().string();
                if (!sourceURL.isEmpty())
                    sheetGroupingPayload->setSourceURL(sourceURL);

                groupingsPayload->addItem(WTF::move(sheetGroupingPayload));
            }

            parentRule = styleSheet->ownerRule();
            if (parentRule)
                break;

            styleSheet = styleSheet->parentStyleSheet();
        }
    }

    return groupingsPayload;
}

Ref<InspectorStyle> InspectorStyle::create(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&& style, InspectorStyleSheet* parentStyleSheet)
{
    return adoptRef(*new InspectorStyle(styleId, WTF::move(style), parentStyleSheet));
}

InspectorStyle::InspectorStyle(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&& style, InspectorStyleSheet* parentStyleSheet)
    : m_styleId(styleId)
    , m_style(WTF::move(style))
    , m_parentStyleSheet(parentStyleSheet)
{
}

InspectorStyle::~InspectorStyle() = default;

Ref<Inspector::Protocol::CSS::CSSStyle> InspectorStyle::buildObjectForStyle()
{
    auto result = styleWithProperties();
    if (auto styleId = m_styleId.asProtocolValue<Inspector::Protocol::CSS::CSSStyleId>())
        result->setStyleId(styleId.releaseNonNull());

    result->setWidth(m_style->getPropertyValue("width"_s));
    result->setHeight(m_style->getPropertyValue("height"_s));

    if (auto sourceData = extractSourceData()) {
        if (auto range = buildSourceRangeObject(sourceData->ruleBodyRange, m_parentStyleSheet->lineEndings()))
            result->setRange(range.releaseNonNull());
    }

    return result;
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>> InspectorStyle::buildArrayForComputedStyle()
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>::create();
    for (auto& property : collectProperties(true)) {
        const CSSPropertySourceData& propertyEntry = property.sourceData;
        auto entry = Inspector::Protocol::CSS::CSSComputedStyleProperty::create()
            .setName(propertyEntry.name)
            .setValue(propertyEntry.value)
            .release();
        result->addItem(WTF::move(entry));
    }
    return result;
}

ExceptionOr<String> InspectorStyle::text()
{
    // Precondition: m_parentStyleSheet->ensureParsedDataReady() has been called successfully.
    auto sourceData = extractSourceData();
    if (!sourceData)
        return Exception { ExceptionCode::NotFoundError };

    auto result = m_parentStyleSheet->text();
    if (result.hasException())
        return result.releaseException();

    auto& bodyRange = sourceData->ruleBodyRange;
    return result.releaseReturnValue().substring(bodyRange.start, bodyRange.end - bodyRange.start);
}

static String lowercasePropertyName(const String& name)
{
    // Custom properties are case-sensitive.
    if (name.startsWith("--"_s))
        return name;
    return name.convertToASCIILowercase();
}

Vector<InspectorStyleProperty> InspectorStyle::collectProperties(bool includeAll)
{
    Vector<InspectorStyleProperty> result;
    HashSet<String> sourcePropertyNames;

    auto sourceData = extractSourceData();
    auto* sourcePropertyData = sourceData ? &sourceData->styleSourceData->propertyData : nullptr;
    if (sourcePropertyData) {
        auto styleDeclarationOrException = text();
        ASSERT(!styleDeclarationOrException.hasException());
        String styleDeclaration = styleDeclarationOrException.hasException() ? emptyString() : styleDeclarationOrException.releaseReturnValue();
        for (auto& sourceData : *sourcePropertyData) {
            InspectorStyleProperty p(sourceData, true, sourceData.disabled);
            p.setRawTextFromStyleDeclaration(styleDeclaration);
            result.append(p);
            sourcePropertyNames.add(lowercasePropertyName(sourceData.name));
        }
    }

    for (int i = 0, size = m_style->length(); i < size; ++i) {
        String name = m_style->item(i);
        if (sourcePropertyNames.add(lowercasePropertyName(name)))
            result.append(InspectorStyleProperty(CSSPropertySourceData(name, m_style->getPropertyValue(name), !m_style->getPropertyPriority(name).isEmpty(), false, true, SourceRange()), false, false));
    }

    if (includeAll) {
        for (auto id : allCSSProperties()) {
            if (!isExposed(id, m_style->settings()))
                continue;

            auto name = nameString(id);
            if (!sourcePropertyNames.add(lowercasePropertyName(name)))
                continue;

            auto value = m_style->getPropertyValue(name);
            if (value.isEmpty())
                continue;

            result.append(InspectorStyleProperty(CSSPropertySourceData(name, value, !m_style->getPropertyPriority(name).isEmpty(), false, true, SourceRange()), false, false));
        }
    }

    return result;
}

Ref<Inspector::Protocol::CSS::CSSStyle> InspectorStyle::styleWithProperties()
{
    auto properties = collectProperties(false);

    auto propertiesObject = JSON::ArrayOf<Inspector::Protocol::CSS::CSSProperty>::create();
    auto shorthandEntries = ArrayOf<Inspector::Protocol::CSS::ShorthandEntry>::create();
    HashMap<String, Ref<Inspector::Protocol::CSS::CSSProperty>> propertyNameToPreviousActiveProperty;
    HashSet<String> foundShorthands;
    String previousPriority;
    String previousStatus;
    Vector<size_t> lineEndings = m_parentStyleSheet ? m_parentStyleSheet->lineEndings() : Vector<size_t> { };
    auto sourceData = extractSourceData();
    unsigned ruleBodyRangeStart = sourceData ? sourceData->ruleBodyRange.start : 0;

    for (auto& styleProperty : properties) {
        const CSSPropertySourceData& propertyEntry = styleProperty.sourceData;
        const String& name = propertyEntry.name;

        auto status = styleProperty.disabled ? Inspector::Protocol::CSS::CSSPropertyStatus::Disabled : Inspector::Protocol::CSS::CSSPropertyStatus::Active;

        auto property = Inspector::Protocol::CSS::CSSProperty::create()
            .setName(lowercasePropertyName(name))
            .setValue(propertyEntry.value)
            .release();

        propertiesObject->addItem(property.copyRef());

        CSSPropertyID propertyId = cssPropertyID(name);

        if (isCustomPropertyName(name))
            propertyId = CSSPropertyID::CSSPropertyCustom;

        // Default "parsedOk" == true.
        if (!propertyEntry.parsedOk || !isExposed(propertyId, m_style->settings()))
            property->setParsedOk(false);
        if (styleProperty.hasRawText())
            property->setText(styleProperty.rawText);

        // Default "priority" == "".
        if (propertyEntry.important)
            property->setPriority("important"_s);

        if (styleProperty.hasSource) {
            // The property range is relative to the style body start.
            // Should be converted into an absolute range (relative to the stylesheet start)
            // for the proper conversion into line:column.
            SourceRange absolutePropertyRange = propertyEntry.range;
            absolutePropertyRange.start += ruleBodyRangeStart;
            absolutePropertyRange.end += ruleBodyRangeStart;
            if (auto range = buildSourceRangeObject(absolutePropertyRange, lineEndings))
                property->setRange(range.releaseNonNull());
        }

        if (!styleProperty.disabled) {
            if (styleProperty.hasSource) {
                ASSERT(sourceData);
                property->setImplicit(false);

                // Parsed property overrides any property with the same name. Non-parsed property overrides
                // previous non-parsed property with the same name (if any).
                bool shouldInactivate = false;

                // Canonicalize property names to treat non-prefixed and vendor-prefixed property names the same (opacity vs. -webkit-opacity).
                String canonicalPropertyName = propertyId != CSSPropertyID::CSSPropertyInvalid && propertyId != CSSPropertyID::CSSPropertyCustom ? nameString(propertyId) : name;
                auto activeIt = propertyNameToPreviousActiveProperty.find(canonicalPropertyName);
                if (activeIt != propertyNameToPreviousActiveProperty.end()) {
                    if (propertyEntry.parsedOk) {
                        auto newPriority = activeIt->value->getString("priority"_s);
                        if (!!newPriority)
                            previousPriority = newPriority;

                        auto newStatus = activeIt->value->getString("status"_s);
                        if (!!newStatus) {
                            previousStatus = newStatus;
                            if (previousStatus != Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::CSS::CSSPropertyStatus::Inactive)) {
                                if (propertyEntry.important || !newPriority) // Priority not set == "not important".
                                    shouldInactivate = true;
                                else if (status == Inspector::Protocol::CSS::CSSPropertyStatus::Active) {
                                    // Inactivate a non-important property following the same-named important property.
                                    status = Inspector::Protocol::CSS::CSSPropertyStatus::Inactive;
                                }
                            }
                        }
                    } else {
                        auto previousParsedOk = activeIt->value->getBoolean("parsedOk"_s);
                        if (previousParsedOk && !previousParsedOk)
                            shouldInactivate = true;
                    }
                } else
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property.copyRef());

                if (shouldInactivate) {
                    activeIt->value->setStatus(Inspector::Protocol::CSS::CSSPropertyStatus::Inactive);
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property.copyRef());
                }
            } else {
                bool implicit = m_style->isPropertyImplicit(name);
                // Default "implicit" == false.
                if (implicit)
                    property->setImplicit(true);
                status = Inspector::Protocol::CSS::CSSPropertyStatus::Style;

                String shorthand = m_style->getPropertyShorthand(name);
                if (!shorthand.isEmpty()) {
                    if (!foundShorthands.contains(shorthand)) {
                        foundShorthands.add(shorthand);
                        auto entry = Inspector::Protocol::CSS::ShorthandEntry::create()
                            .setName(shorthand)
                            .setValue(shorthandValue(shorthand))
                            .release();
                        shorthandEntries->addItem(WTF::move(entry));
                    }
                }
            }
        }

        // Default "status" == "style".
        if (status != Inspector::Protocol::CSS::CSSPropertyStatus::Style)
            property->setStatus(status);
    }

    return Inspector::Protocol::CSS::CSSStyle::create()
        .setCssProperties(WTF::move(propertiesObject))
        .setShorthandEntries(WTF::move(shorthandEntries))
        .release();
}

RefPtr<CSSRuleSourceData> InspectorStyle::extractSourceData() const
{
    if (!m_parentStyleSheet || !m_parentStyleSheet->ensureParsedDataReady())
        return nullptr;
    return m_parentStyleSheet->ruleSourceDataFor(m_style.ptr());
}

String InspectorStyle::shorthandValue(const String& shorthandProperty) const
{
    String value = m_style->getPropertyValue(shorthandProperty);
    if (!value.isEmpty())
        return value;
    StringBuilder builder;
    for (unsigned i = 0; i < m_style->length(); ++i) {
        String individualProperty = m_style->item(i);
        if (m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
            continue;
        if (m_style->isPropertyImplicit(individualProperty))
            continue;
        String individualValue = m_style->getPropertyValue(individualProperty);
        if (individualValue == "initial"_s)
            continue;
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(individualValue);
    }
    return builder.toString();
}

String InspectorStyle::shorthandPriority(const String& shorthandProperty) const
{
    String priority = m_style->getPropertyPriority(shorthandProperty);
    if (priority.isEmpty()) {
        for (unsigned i = 0; i < m_style->length(); ++i) {
            String individualProperty = m_style->item(i);
            if (m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            priority = m_style->getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}

Vector<String> InspectorStyle::longhandProperties(const String& shorthandProperty) const
{
    Vector<String> properties;
    HashSet<String> foundProperties;
    for (unsigned i = 0; i < m_style->length(); ++i) {
        String individualProperty = m_style->item(i);
        if (foundProperties.contains(individualProperty) || m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
            continue;

        foundProperties.add(individualProperty);
        properties.append(individualProperty);
    }
    return properties;
}

Ref<InspectorStyleSheet> InspectorStyleSheet::create(InspectorPageAgent* pageAgent, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin origin, const String& documentURL, Listener* listener)
{
    return adoptRef(*new InspectorStyleSheet(pageAgent, id, WTF::move(pageStyleSheet), origin, documentURL, listener));
}

String InspectorStyleSheet::styleSheetURL(CSSStyleSheet* pageStyleSheet)
{
    if (pageStyleSheet && !pageStyleSheet->contents().baseURL().isEmpty())
        return pageStyleSheet->contents().baseURL().string();
    return emptyString();
}

InspectorStyleSheet::InspectorStyleSheet(InspectorPageAgent* pageAgent, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin origin, const String& documentURL, Listener* listener)
    : m_pageAgent(pageAgent)
    , m_id(id)
    , m_pageStyleSheet(WTF::move(pageStyleSheet))
    , m_origin(origin)
    , m_documentURL(documentURL)
    , m_listener(listener)
{
    m_parsedStyleSheet = new ParsedStyleSheet();
}

InspectorStyleSheet::~InspectorStyleSheet()
{
    delete m_parsedStyleSheet;
}

String InspectorStyleSheet::finalURL() const
{
    String url = styleSheetURL(m_pageStyleSheet.get());
    return url.isEmpty() ? m_documentURL : url;
}

void InspectorStyleSheet::reparseStyleSheet(const String& text)
{
    {
        // Have a separate scope for clearRules() (bug 95324).
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents().clearRules();
    }
    {
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents().parseString(text);
        m_pageStyleSheet->clearChildRuleCSSOMWrappers();
        fireStyleSheetChanged();
    }
}

ExceptionOr<void> InspectorStyleSheet::setText(const String& text)
{
    if (!m_pageStyleSheet)
        return Exception { ExceptionCode::NotSupportedError };

    m_parsedStyleSheet->setText(text);
    m_flatRules.clear();

    return { };
}

ExceptionOr<String> InspectorStyleSheet::ruleHeaderText(const InspectorCSSId& id)
{
    RefPtr rule = ruleForId(id);
    if (!rule)
        return Exception { ExceptionCode::NotFoundError };

    if (RefPtr cssStyleRule = dynamicDowncast<CSSStyleRule>(rule.get()))
        return cssStyleRule->selectorText();

    auto sourceData = ruleSourceDataFor(rule.get());
    if (!sourceData)
        return Exception { ExceptionCode::NotFoundError };

    String sheetText = m_parsedStyleSheet->text();
    return sheetText.substring(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length());
}

ExceptionOr<void> InspectorStyleSheet::setRuleHeaderText(const InspectorCSSId& id, const String& newHeaderText)
{
    if (!m_pageStyleSheet)
        return Exception { ExceptionCode::NotSupportedError };

    RefPtr rule = ruleForId(id);
    if (!rule)
        return Exception { ExceptionCode::NotFoundError };

    if (!isValidRuleHeaderText(newHeaderText, rule->styleRuleType(), m_pageStyleSheet->ownerDocument(), rule->nestedContext()))
        return Exception { ExceptionCode::SyntaxError };

    RefPtr styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { ExceptionCode::NotFoundError };

    auto correctedHeaderText = newHeaderText;

    // Fast-path the editing of `CSSStyleRules` by using its built-in CSSOM support for editing instead of reparsing the entire style sheet.
    RefPtr cssStyleRule = dynamicDowncast<CSSStyleRule>(rule.get());
    if (cssStyleRule)
        cssStyleRule->setSelectorText(correctedHeaderText);

    auto sourceData = ruleSourceDataFor(rule.get());
    if (!sourceData)
        return Exception { ExceptionCode::NotFoundError };

    String sheetText = m_parsedStyleSheet->text();

    if (!cssStyleRule
        && sourceData->ruleHeaderRange.start
        && sheetText.characterAt(sourceData->ruleHeaderRange.start - 1) != ' '
        && !correctedHeaderText.startsWith('(')) {
        // @ rules do not include the `@whatever` part of the declaration in their header text range, nor do they
        // include the space between the `@whatever` and the query/name/etc.. However, not all rules must contain a
        // space between those, for example `@media(...)`. We need to add the space if the new header text does not
        // start with an opening parenthesis, otherwise we will create an invalid declaration (e.g. `@mediascreen`).
        correctedHeaderText = makeString(' ', correctedHeaderText);
    }

    sheetText = makeStringByReplacing(sheetText, sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), correctedHeaderText);

    if (cssStyleRule) {
        // Set the style sheet text directly so we don't rebuild our flat rule set. The CSSStyleRule has been directly
        // updated already.
        m_parsedStyleSheet->setText(sheetText);
        fireStyleSheetChanged();
    } else {
        setText(sheetText);
        reparseStyleSheet(sheetText);
    }

    return { };
}

ExceptionOr<CSSStyleRule*> InspectorStyleSheet::addRule(const String& selector)
{
    if (!m_pageStyleSheet)
        return Exception { ExceptionCode::NotSupportedError };

    if (!isValidRuleHeaderText(selector, StyleRuleType::Style, m_pageStyleSheet->ownerDocument()))
        return Exception { ExceptionCode::SyntaxError };

    auto text = this->text();
    if (text.hasException())
        return text.releaseException();

    auto addRuleResult = m_pageStyleSheet->addRule(selector, emptyString(), std::nullopt);
    if (addRuleResult.hasException())
        return addRuleResult.releaseException();

    StringBuilder styleSheetText;
    styleSheetText.append(text.releaseReturnValue());

    if (!styleSheetText.isEmpty())
        styleSheetText.append('\n');

    styleSheetText.append(selector, " {}"_s);

    // Using setText() as this operation changes the stylesheet rule set.
    setText(styleSheetText.toString());

    // Inspector Style Sheets are always treated as though their parsed data is ready.
    if (m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::Inspector)
        fireStyleSheetChanged();
    else
        reparseStyleSheet(styleSheetText.toString());

    ASSERT(m_pageStyleSheet->length());
    unsigned lastRuleIndex = m_pageStyleSheet->length() - 1;
    RefPtr rule = m_pageStyleSheet->item(lastRuleIndex);
    ASSERT(rule);

    RefPtr styleRule = dynamicDowncast<CSSStyleRule>(rule.get());
    if (!styleRule) {
        // What we just added has to be a CSSStyleRule - we cannot handle other types of rules yet.
        // If it is not a style rule, pretend we never touched the stylesheet.
        m_pageStyleSheet->deleteRule(lastRuleIndex);
        return Exception { ExceptionCode::SyntaxError };
    }

    return styleRule.get();
}

ExceptionOr<void> InspectorStyleSheet::deleteRule(const InspectorCSSId& id)
{
    if (!m_pageStyleSheet)
        return Exception { ExceptionCode::NotSupportedError };

    RefPtr<CSSStyleRule> rule = dynamicDowncast<CSSStyleRule>(ruleForId(id));
    if (!rule)
        return Exception { ExceptionCode::NotFoundError };
    RefPtr styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { ExceptionCode::NotFoundError };

    auto sourceData = ruleSourceDataFor(rule.get());
    if (!sourceData)
        return Exception { ExceptionCode::NotFoundError };

    auto deleteRuleResult = styleSheet->deleteRule(id.ordinal());
    if (deleteRuleResult.hasException())
        return deleteRuleResult.releaseException();

    // |rule| MAY NOT be addressed after this!

    auto sheetText = makeStringByRemoving(m_parsedStyleSheet->text(), sourceData->ruleHeaderRange.start, sourceData->ruleBodyRange.end - sourceData->ruleHeaderRange.start + 1);
    setText(sheetText);
    fireStyleSheetChanged();
    return { };
}

CSSRule* InspectorStyleSheet::ruleForId(const InspectorCSSId& id) const
{
    if (!m_pageStyleSheet)
        return nullptr;

    ASSERT(!id.isEmpty());
    ensureFlatRules();
    return id.ordinal() >= m_flatRules.size() ? nullptr : m_flatRules.at(id.ordinal()).get();
}

RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody> InspectorStyleSheet::buildObjectForStyleSheet()
{
    RefPtr styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    RefPtr<CSSRuleList> cssRuleList = asCSSRuleList(styleSheet.get());

    auto result = Inspector::Protocol::CSS::CSSStyleSheetBody::create()
        .setStyleSheetId(id())
        .setRules(buildArrayForRuleList(cssRuleList.get()))
        .release();

    auto styleSheetText = text();
    if (!styleSheetText.hasException())
        result->setText(styleSheetText.releaseReturnValue());

    return result;
}

RefPtr<Inspector::Protocol::CSS::CSSStyleSheetHeader> InspectorStyleSheet::buildObjectForStyleSheetInfo()
{
    RefPtr styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    RefPtr document = styleSheet->ownerDocument();
    RefPtr frame = document ? document->frame() : nullptr;
    return Inspector::Protocol::CSS::CSSStyleSheetHeader::create()
        .setStyleSheetId(id())
        .setOrigin(m_origin)
        .setDisabled(styleSheet->disabled())
        .setSourceURL(finalURL())
        .setTitle(styleSheet->title())
        .setFrameId(m_pageAgent->frameId(frame.get()))
        .setIsInline(styleSheet->isInline() && styleSheet->startPosition() != TextPosition())
        .setStartLine(styleSheet->startPosition().m_line.zeroBasedInt())
        .setStartColumn(styleSheet->startPosition().m_column.zeroBasedInt())
        .release();
}

static Ref<Inspector::Protocol::CSS::CSSSelector> buildObjectForSelectorHelper(const String& selectorText, const CSSSelector& selector)
{
    auto inspectorSelector = Inspector::Protocol::CSS::CSSSelector::create()
        .setText(selectorText)
        .release();

    // FIXME: <webkit.org/b/250141> Add support for resolving the specificity of a nested selector.
    if (selector.hasExplicitNestingParent())
        return inspectorSelector;

    auto specificity = selector.computeSpecificityTuple();

    auto tuple = JSON::ArrayOf<int>::create();
    tuple->addItem(specificity[0]);
    tuple->addItem(specificity[1]);
    tuple->addItem(specificity[2]);
    inspectorSelector->setSpecificity(WTF::move(tuple));

    return inspectorSelector;
}

static Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>> selectorsFromSource(const CSSRuleSourceData* sourceData, const String& sheetText, const Vector<const CSSSelector*> selectors)
{
    static NeverDestroyed<JSC::Yarr::RegularExpression> comment("/\\*[^]*?\\*/"_s, OptionSet<JSC::Yarr::Flags> { JSC::Yarr::Flags::Multiline });

    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>::create();
    unsigned selectorIndex = 0;
    for (auto& range : sourceData->selectorRanges) {
        // If we don't have a selector, that means the SourceData for this CSSStyleSheet
        // no longer matches up with the actual rules in the CSSStyleSheet.
        ASSERT(selectorIndex < selectors.size());
        if (selectorIndex >= selectors.size())
            break;

        String selectorText = sheetText.substring(range.start, range.length());

        // We don't want to see any comments in the selector components, only the meaningful parts.
        replace(selectorText, comment, String());
        result->addItem(buildObjectForSelectorHelper(selectorText.trim(deprecatedIsSpaceOrNewline), *selectors.at(selectorIndex)));

        ++selectorIndex;
    }
    return result;
}

Vector<Ref<CSSStyleRule>> InspectorStyleSheet::cssStyleRulesSplitFromSameRule(CSSStyleRule& rule)
{
    if (!rule.styleRule().isSplitRule())
        return { rule };

    Vector<Ref<CSSStyleRule>> rules;

    ensureFlatRules();
    auto firstIndexOfSplitRule = m_flatRules.find(&rule);
    if (firstIndexOfSplitRule == notFound)
        return { rule };

    for (; firstIndexOfSplitRule > 0; --firstIndexOfSplitRule) {
        auto* ruleAtPreviousIndex = dynamicDowncast<CSSStyleRule>(m_flatRules.at(firstIndexOfSplitRule - 1).get());
        if (!ruleAtPreviousIndex)
            break;

        if (!ruleAtPreviousIndex->styleRule().isSplitRule() || ruleAtPreviousIndex->styleRule().isLastRuleInSplitRule())
            break;
    }

    for (auto i = firstIndexOfSplitRule; i < m_flatRules.size(); ++i) {
        RefPtr rule = dynamicDowncast<CSSStyleRule>(m_flatRules.at(i).get());
        if (!rule)
            break;

        if (!rule->styleRule().isSplitRule())
            break;

        rules.append(*rule);

        if (rule->styleRule().isLastRuleInSplitRule())
            break;
    }

    return rules;
}

Vector<const CSSSelector*> InspectorStyleSheet::selectorsForCSSStyleRule(CSSStyleRule& rule)
{
    auto rules = cssStyleRulesSplitFromSameRule(rule);

    Vector<const CSSSelector*> selectors;
    for (auto& rule : cssStyleRulesSplitFromSameRule(rule)) {
        for (auto& selector : rule->styleRule().selectorList())
            selectors.append(&selector);
    }
    return selectors;
}

Ref<Inspector::Protocol::CSS::CSSSelector> InspectorStyleSheet::buildObjectForSelector(const CSSSelector* selector)
{
    return buildObjectForSelectorHelper(selector->selectorText(), *selector);
}

Ref<Inspector::Protocol::CSS::SelectorList> InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule, int& endingLine)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(rule);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>> selectors;

    // This intentionally does not rely on the source data to avoid catching the trailing comments (before the declaration starting '{').
    String selectorText = rule->selectorText();

    if (sourceData)
        selectors = selectorsFromSource(sourceData.get(), m_parsedStyleSheet->text(), selectorsForCSSStyleRule(*rule));
    else {
        selectors = JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>::create();
        for (const CSSSelector* selector : selectorsForCSSStyleRule(*rule))
            selectors->addItem(buildObjectForSelector(selector));
    }
    auto result = Inspector::Protocol::CSS::SelectorList::create()
        .setSelectors(selectors.releaseNonNull())
        .setText(selectorText)
        .release();
    if (sourceData) {
        if (auto range = buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings(), &endingLine))
            result->setRange(range.releaseNonNull());
    }
    return result;
}

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule)
{
    RefPtr styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    int endingLine = 0;
    auto result = Inspector::Protocol::CSS::CSSRule::create()
        .setSelectorList(buildObjectForSelectorList(rule, endingLine))
        .setSourceLine(endingLine)
        .setOrigin(m_origin)
        .setStyle(buildObjectForStyle(&rule->style()))
        .release();

    if (m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::Author || m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::User)
        result->setSourceURL(finalURL());

    if (canBind()) {
        if (auto ruleId = this->ruleOrStyleId(rule).asProtocolValue<Inspector::Protocol::CSS::CSSRuleId>())
            result->setRuleId(ruleId.releaseNonNull());
    }

    auto groupingsPayload = buildArrayForGroupings(*rule);
    if (groupingsPayload->length())
        result->setGroupings(WTF::move(groupingsPayload));

    if (auto sourceData = ruleSourceDataFor(rule))
        result->setIsImplicitlyNested(sourceData->isImplicitlyNested);

    return result;
}

Ref<Inspector::Protocol::CSS::CSSStyle> InspectorStyleSheet::buildObjectForStyle(CSSStyleDeclaration* style)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(style);

    InspectorCSSId id = ruleOrStyleId(style);
    if (id.isEmpty()) {
        return Inspector::Protocol::CSS::CSSStyle::create()
            .setCssProperties(ArrayOf<Inspector::Protocol::CSS::CSSProperty>::create())
            .setShorthandEntries(ArrayOf<Inspector::Protocol::CSS::ShorthandEntry>::create())
            .release();
    }

    RefPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);

    auto result = inspectorStyle->buildObjectForStyle();

    // Style text cannot be retrieved without stylesheet, so set cssText here.
    if (sourceData) {
        auto sheetText = text();
        if (!sheetText.hasException()) {
            auto& bodyRange = sourceData->ruleBodyRange;
            result->setCssText(sheetText.releaseReturnValue().substring(bodyRange.start, bodyRange.end - bodyRange.start));
        }
    }

    return result;
}

static inline bool isNotSpaceOrTab(char16_t character)
{
    return character != ' ' && character != '\t';
}

// A CSS rule's body can contain a mix of property declarations and nested child rules.
// This function formats a rule's body text by putting the `ruleStyleDeclarationText`
// at the start, followed by the nested child rules scraped from the full `styleSheetText`,
// and returns the patched rule body text.
//
// Canonicalizing is useful for generating the new style sheet text after some style edit;
// it'd be hard to compute the replacement text if property declarations were scattered.
static String computeCanonicalRuleText(const String& styleSheetText, const String& ruleStyleDeclarationText, const CSSRuleSourceData& logicalContainingRuleSourceData)
{
    auto indentation = emptyString();
    auto startOfSecondLine = ruleStyleDeclarationText.find('\n');
    if (startOfSecondLine != notFound) {
        ++startOfSecondLine;
        auto endOfSecondLineWhitespace = ruleStyleDeclarationText.find(isNotSpaceOrTab, startOfSecondLine);
        if (endOfSecondLineWhitespace != notFound)
            indentation = ruleStyleDeclarationText.substring(startOfSecondLine, endOfSecondLineWhitespace - startOfSecondLine);
    }

    StringBuilder canonicalRuleText;
    canonicalRuleText.append(ruleStyleDeclarationText);

    for (auto& childRuleSourceData : logicalContainingRuleSourceData.childRules) {
        if (childRuleSourceData->isImplicitlyNested)
            continue;

        unsigned childStart = childRuleSourceData->ruleHeaderRange.start;
        unsigned childEnd = childRuleSourceData->ruleBodyRange.end;
        ASSERT(childStart <= childEnd);
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(childEnd <= styleSheetText.length());

        canonicalRuleText.append('\n', indentation);

        // Non-style rules don't include the `@rule` prelude in their header range.
        if (childRuleSourceData->type != StyleRuleType::Style)
            canonicalRuleText.append(atRuleIdentifierForType(childRuleSourceData->type));

        canonicalRuleText.appendSubstring(styleSheetText, childStart, childEnd - childStart);
        canonicalRuleText.append("}\n"_s);
    }

    auto closingIndentationLineStart = ruleStyleDeclarationText.reverseFind('\n');
    if (closingIndentationLineStart != notFound)
        canonicalRuleText.appendSubstring(ruleStyleDeclarationText, closingIndentationLineStart);

    return canonicalRuleText.toString();
}

// This function updates the style declaration text of the rule given by `id`.
// The original style declaration text and rule text will be stored in the `out` arguments, if given.
// If `newRuleText` is null, the canonicalized rule text will be computed and used to patch the
// full style sheet text.
ExceptionOr<void> InspectorStyleSheet::setRuleStyleText(const InspectorCSSId& id, const String& newStyleDeclarationText, String* outOldStyleDeclarationText, const String* newRuleText, String* outOldRuleText)
{
    RefPtr cssStyleDeclaration = styleForId(id);
    if (!cssStyleDeclaration)
        return Exception { ExceptionCode::NotFoundError };

    RefPtr cssRule = ruleForId(id);
    if (!cssRule)
        return Exception { ExceptionCode::NotFoundError };

    if (!ensureParsedDataReady())
        return Exception { ExceptionCode::NotFoundError };

    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(cssRule.get());
    if (!sourceData)
        return Exception { ExceptionCode::NotFoundError };

    RefPtr<CSSRuleSourceData> logicalContainingRuleSourceData = sourceData->isImplicitlyNested ? ruleSourceDataFor(cssRule->parentRule()) : sourceData;
    if (!logicalContainingRuleSourceData)
        return Exception { ExceptionCode::NotFoundError };

    unsigned bodyStart = logicalContainingRuleSourceData->ruleBodyRange.start;
    unsigned bodyEnd = logicalContainingRuleSourceData->ruleBodyRange.end;
    ASSERT(bodyStart <= bodyEnd);

    const String& styleSheetText = m_parsedStyleSheet->text();
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(bodyEnd <= styleSheetText.length());

    if (outOldStyleDeclarationText)
        *outOldStyleDeclarationText = cssStyleDeclaration->cssText();

    if (outOldRuleText)
        *outOldRuleText = styleSheetText.substring(bodyStart, bodyEnd - bodyStart);

    cssStyleDeclaration->setCssText(newStyleDeclarationText);

    // Don't canonicalize the rule text if a `newRuleText` is provided, to allow for faithful undoing.
    String replacementBodyText = newRuleText ? *newRuleText : computeCanonicalRuleText(styleSheetText, newStyleDeclarationText, *logicalContainingRuleSourceData);

    m_parsedStyleSheet->setText(makeStringByReplacing(styleSheetText, bodyStart, bodyEnd - bodyStart, replacementBodyText));
    fireStyleSheetChanged();

    return { };
}

ExceptionOr<String> InspectorStyleSheet::text()
{
    if (!ensureText())
        return Exception { ExceptionCode::NotFoundError };
    return String { m_parsedStyleSheet->text() };
}

CSSStyleDeclaration* InspectorStyleSheet::styleForId(const InspectorCSSId& id) const
{
    RefPtr rule = dynamicDowncast<CSSStyleRule>(ruleForId(id));
    if (!rule)
        return nullptr;

    return &rule->style();
}

void InspectorStyleSheet::fireStyleSheetChanged()
{
    if (m_listener)
        m_listener->styleSheetChanged(this);
}

RefPtr<InspectorStyle> InspectorStyleSheet::inspectorStyleForId(const InspectorCSSId& id)
{
    RefPtr style = styleForId(id);
    if (!style)
        return nullptr;

    return InspectorStyle::create(id, *style, this);
}

InspectorCSSId InspectorStyleSheet::ruleOrStyleId(StyleDeclarationOrCSSRule ruleOrDeclaration) const
{
    unsigned index = ruleIndexByStyle(ruleOrDeclaration);
    if (index != UINT_MAX)
        return InspectorCSSId(id(), index);
    return InspectorCSSId();
}

Document* InspectorStyleSheet::ownerDocument() const
{
    return m_pageStyleSheet->ownerDocument();
}

RefPtr<CSSRuleSourceData> InspectorStyleSheet::ruleSourceDataFor(CSSStyleDeclaration* style) const
{
    static constexpr auto combineSplitRules = true;
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByStyle(style, combineSplitRules));
}

RefPtr<CSSRuleSourceData> InspectorStyleSheet::ruleSourceDataFor(CSSRule* rule) const
{
    static constexpr auto combineSplitRules = true;
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByStyle(rule, combineSplitRules));
}

Vector<size_t> InspectorStyleSheet::lineEndings() const
{
    if (!m_parsedStyleSheet->hasText())
        return { };
    return ContentSearchUtilities::lineEndings(m_parsedStyleSheet->text());
}


unsigned InspectorStyleSheet::ruleIndexByStyle(StyleDeclarationOrCSSRule ruleOrDeclaration, bool combineSplitRules) const
{
    ensureFlatRules();
    unsigned index = 0;
    for (auto& rule : m_flatRules) {
        RefPtr cssStyleRule = dynamicDowncast<CSSStyleRule>(rule.get());

        auto matches = WTF::switchOn(ruleOrDeclaration,
            [&] (const CSSStyleDeclaration* styleDeclaration) { return cssStyleRule && &cssStyleRule->style() == styleDeclaration; },
            [&] (const CSSRule* cssRule) { return rule.get() == cssRule; }
        );
        if (matches)
            return index;

        if (cssStyleRule && combineSplitRules) {
            if (!cssStyleRule->styleRule().isSplitRule() || cssStyleRule->styleRule().isLastRuleInSplitRule())
                ++index;

            continue;
        }

        ++index;
    }
    return UINT_MAX;
}

bool InspectorStyleSheet::ensureParsedDataReady()
{
    return ensureText() && ensureSourceData();
}

bool InspectorStyleSheet::ensureText()
{
    if (!m_parsedStyleSheet)
        return false;
    if (m_parsedStyleSheet->hasText())
        return true;

    String text;
    if (m_pageStyleSheet->wasMutated()) {
        // This style sheet in memory no longer matches its original text from static source.
        // Reconstruct its current text by serializing the contained CSSRules.
        if (!styleSheetTextFromCSSRuleSerialization(&text))
            return false;

        m_parsedStyleSheet->setText(text);
        fireStyleSheetChanged();
        return true;
    }

    if (!originalStyleSheetText(&text))
        return false;

    m_parsedStyleSheet->setText(text);
    // No need to clear m_flatRules here - it's empty.
    return true;
}

bool InspectorStyleSheet::ensureSourceData()
{
    if (m_parsedStyleSheet->hasSourceData())
        return true;

    if (!m_parsedStyleSheet->hasText())
        return false;

    auto newStyleSheet = StyleSheetContents::create();
    auto ruleSourceDataResult = makeUnique<RuleSourceDataList>();
    
    CSSParserContext context(parserContextForDocument(m_pageStyleSheet->ownerDocument()));

    // FIXME: <webkit.org/b/161747> Media control CSS uses out-of-spec selectors in inline user agent shadow root style
    // element. See corresponding workaround in `CSSSelectorParser::extractCompoundFlags`.
    if (RefPtr ownerNode = m_pageStyleSheet->ownerNode(); ownerNode && ownerNode->isInUserAgentShadowTree())
        context.setUASheetMode();

    StyleSheetHandler handler(m_parsedStyleSheet->text(), m_pageStyleSheet->ownerDocument(), ruleSourceDataResult.get());
    CSSParser::parseStyleSheetForInspector(m_parsedStyleSheet->text(), context, newStyleSheet, handler);
    m_parsedStyleSheet->setSourceData(WTF::move(ruleSourceDataResult));
    return m_parsedStyleSheet->hasSourceData();
}

void InspectorStyleSheet::ensureFlatRules() const
{
    // We are fine with redoing this for empty stylesheets as this will run fast.
    if (m_flatRules.isEmpty())
        collectFlatRules(asCSSRuleList(pageStyleSheet()), &m_flatRules);
}

bool InspectorStyleSheet::originalStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet || m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent)
        return false;
    return inlineStyleSheetText(result) || resourceStyleSheetText(result) || extensionStyleSheetText(result);
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (!ownerDocument() || !ownerDocument()->frame())
        return false;

    String error;
    bool base64Encoded;
    ResourceUtilities::resourceContent(error, ownerDocument()->frame(), URL({ }, m_pageStyleSheet->href()), result, &base64Encoded);
    return error.isEmpty() && !base64Encoded;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    RefPtr ownerNode = m_pageStyleSheet->ownerNode();
    if (!is<Element>(ownerNode.get()))
        return false;

    Ref ownerElement = downcast<Element>(*ownerNode);
    if (!is<HTMLStyleElement>(ownerElement.ptr()) && !is<SVGStyleElement>(ownerElement.ptr()))
        return false;

    *result = ownerElement->textContent();
    return true;
}

bool InspectorStyleSheet::extensionStyleSheetText(String* result) const
{
    if (!ownerDocument())
        return false;

    auto content = ownerDocument()->extensionStyleSheets().contentForInjectedStyleSheet(*m_pageStyleSheet);
    if (content.isEmpty())
        return false;

    *result = content;
    return true;
}

bool InspectorStyleSheet::styleSheetTextFromCSSRuleSerialization(String* result) const
{
    if (!m_pageStyleSheet || m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent)
        return false;

    StringBuilder text;
    for (unsigned i = 0, length = m_pageStyleSheet->length(); i < length; ++i) {
        text.append(m_pageStyleSheet->item(i)->cssText());
        text.append('\n');
    }
    *result = text.toString();
    return true;
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>> InspectorStyleSheet::buildArrayForRuleList(CSSRuleList* ruleList)
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>::create();
    if (!ruleList)
        return result;

    RefPtr<CSSRuleList> refRuleList = ruleList;
    Vector<RefPtr<CSSRule>> rules;
    collectFlatRules(WTF::move(refRuleList), &rules);

    for (auto& rule : rules) {
        if (RefPtr styleRule = dynamicDowncast<CSSStyleRule>(rule.get())) {
            if (auto ruleObject = buildObjectForRule(styleRule.get()))
                result->addItem(ruleObject.releaseNonNull());
        }
    }

    return result;
}

void InspectorStyleSheet::collectFlatRules(RefPtr<CSSRuleList>&& ruleList, Vector<RefPtr<CSSRule>>* result)
{
    if (!ruleList)
        return;

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        RefPtr rule = ruleList->item(i);
        if (!rule)
            continue;

        switch (flatteningStrategyForStyleRuleType(rule->styleRuleType())) {
        case RuleFlatteningStrategy::CommitSelfThenChildren: {
            result->append(rule);

            auto childRuleList = asCSSRuleList(rule.get());
            ASSERT(childRuleList);
            collectFlatRules(WTF::move(childRuleList), result);
            break;
        }

        case RuleFlatteningStrategy::Ignore:
            break;
        }
    }
}

Ref<InspectorStyleSheetForInlineStyle> InspectorStyleSheetForInlineStyle::create(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Inspector::Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
{
    return adoptRef(*new InspectorStyleSheetForInlineStyle(pageAgent, id, WTF::move(element), origin, listener));
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Inspector::Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
    : InspectorStyleSheet(pageAgent, id, nullptr, origin, String(), listener)
    , m_element(WTF::move(element))
    , m_ruleSourceData(nullptr)
    , m_isStyleTextValid(false)
{
    m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id, 0), inlineStyle(), this);
    m_styleText = m_element->getAttribute(HTMLNames::styleAttr).string();
}

void InspectorStyleSheetForInlineStyle::didModifyElementAttribute()
{
    m_isStyleTextValid = false;
    if (&m_element->cssomStyle() != &m_inspectorStyle->cssStyle())
        m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id(), 0), inlineStyle(), this);
    m_ruleSourceData = nullptr;
}

ExceptionOr<String> InspectorStyleSheetForInlineStyle::text()
{
    if (!m_isStyleTextValid) {
        m_styleText = elementStyleText();
        m_isStyleTextValid = true;
    }
    return String { m_styleText };
}

ExceptionOr<void> InspectorStyleSheetForInlineStyle::setRuleStyleText(const InspectorCSSId& id, const String& newStyleDeclarationText, String* outOldStyleDeclarationText, const String* newRuleText, String* outOldRuleText)
{
    UNUSED_PARAM(id);
    UNUSED_PARAM(newRuleText);
    UNUSED_PARAM(outOldRuleText);

    if (outOldStyleDeclarationText)
        *outOldStyleDeclarationText = m_styleText;

    {
        InspectorCSSAgent::InlineStyleOverrideScope overrideScope(m_element->document());
        m_element->setAttribute(HTMLNames::styleAttr, AtomString { newStyleDeclarationText });
    }

    m_styleText = newStyleDeclarationText;
    m_isStyleTextValid = true;
    m_ruleSourceData = nullptr;

    return { };
}

Vector<size_t> InspectorStyleSheetForInlineStyle::lineEndings() const
{
    return ContentSearchUtilities::lineEndings(elementStyleText());
}

Document* InspectorStyleSheetForInlineStyle::ownerDocument() const
{
    return &m_element->document();
}

bool InspectorStyleSheetForInlineStyle::ensureParsedDataReady()
{
    // The "style" property value can get changed indirectly, e.g. via element.style.borderWidth = "2px".
    const String& currentStyleText = elementStyleText();
    if (m_styleText != currentStyleText) {
        m_ruleSourceData = nullptr;
        m_styleText = currentStyleText;
        m_isStyleTextValid = true;
    }

    if (m_ruleSourceData)
        return true;

    m_ruleSourceData = ruleSourceData();
    return true;
}

RefPtr<InspectorStyle> InspectorStyleSheetForInlineStyle::inspectorStyleForId(const InspectorCSSId& id)
{
    ASSERT_UNUSED(id, !id.ordinal());
    return m_inspectorStyle.copyRef();
}

CSSStyleDeclaration& InspectorStyleSheetForInlineStyle::inlineStyle() const
{
    return m_element->cssomStyle();
}

const String& InspectorStyleSheetForInlineStyle::elementStyleText() const
{
    return m_element->getAttribute(HTMLNames::styleAttr).string();
}

Ref<CSSRuleSourceData> InspectorStyleSheetForInlineStyle::ruleSourceData() const
{
    if (m_styleText.isEmpty()) {
        auto result = CSSRuleSourceData::create(StyleRuleType::Style);
        result->ruleBodyRange.start = 0;
        result->ruleBodyRange.end = 0;
        return result;
    }

    CSSParserContext context(parserContextForDocument(&m_element->document()));
    RuleSourceDataList ruleSourceDataResult;
    StyleSheetHandler handler(m_styleText, &m_element->document(), &ruleSourceDataResult);
    CSSParser::parseDeclarationListForInspector(m_styleText, context, handler);
    return WTF::move(ruleSourceDataResult.first());
}

} // namespace WebCore
