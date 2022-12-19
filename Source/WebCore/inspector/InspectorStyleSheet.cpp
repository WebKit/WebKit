/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 * Copyright (C) 2021-2022, Apple Inc. All rights reserved.
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
#include "CSSImportRule.h"
#include "CSSKeyframesRule.h"
#include "CSSLayerBlockRule.h"
#include "CSSLayerStatementRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSParserImpl.h"
#include "CSSParserObserver.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsRule.h"
#include "CSSTokenizer.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "Element.h"
#include "ExtensionStyleSheets.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLStyleElement.h"
#include "InspectorCSSAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorPageAgent.h"
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
#include <wtf/text/StringBuilder.h>

using JSON::ArrayOf;

namespace WebCore {

using namespace Inspector;

enum class RuleFlatteningStrategy {
    Ignore,
    Commit,
    CommitSelfThenChildren,
};

static RuleFlatteningStrategy flatteningStrategyForStyleRuleType(StyleRuleType styleRuleType)
{
    switch (styleRuleType) {
    case StyleRuleType::Style:
        return RuleFlatteningStrategy::Commit;

    case StyleRuleType::Media:
    case StyleRuleType::Supports:
    case StyleRuleType::LayerBlock:
    case StyleRuleType::Container:
        // These rules MUST be handled by the static `isValidRuleHeaderText`, `protocolGroupingTypeForStyleRuleType`,
        // and `asCSSRuleList` in order to provide functionality in Web Inspector. Additionally, they MUST have a CSSOM
        // representation created in `StyleRuleBase::createCSSOMWrapper`, otherwise we will end up with a mismatched
        // lists of source data and CSSOM wrappers.
        return RuleFlatteningStrategy::CommitSelfThenChildren;

    case StyleRuleType::Unknown:
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

static bool isValidRuleHeaderText(const String& headerText, StyleRuleType styleRuleType, Document* document)
{
    auto isValidAtRuleHeaderText = [&] (const String& atRuleIdentifier) {
        if (headerText.isEmpty())
            return false;

        // Make sure the engine can parse the provided `@` rule, even if it only uses unsupported features. As long as
        // the rule text is entirely consumed and it creates a rule of the expected type, we consider it valid because
        // we will be able to continue to edit the rule in the future.
        CSSParserContext context(parserContextForDocument(document)); // CSSParserImpl holds a reference to this.
        CSSParserImpl parser(context, atRuleIdentifier + ' ' + headerText + " {}");
        if (!parser.tokenizer())
            return false;

        auto tokenRange = parser.tokenizer()->tokenRange();
        auto rule = parser.consumeAtRule(tokenRange, CSSParserImpl::AllowedRulesType::RegularRules);

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
    case StyleRuleType::Style: {
        CSSParser parser(parserContextForDocument(document));
        return !!parser.parseSelector(headerText);
    }
    case StyleRuleType::Media:
        return isValidAtRuleHeaderText("@media"_s);
    case StyleRuleType::Supports:
        return isValidAtRuleHeaderText("@supports"_s);
    case StyleRuleType::LayerBlock:
        return isValidAtRuleHeaderText("@layer"_s);
    case StyleRuleType::Container:
        return isValidAtRuleHeaderText("@container"_s);
    default:
        return false;
    }
}

static std::optional<Protocol::CSS::Grouping::Type> protocolGroupingTypeForStyleRuleType(StyleRuleType styleRuleType)
{
    switch (styleRuleType) {
    case StyleRuleType::Media:
        return Protocol::CSS::Grouping::Type::MediaRule;
    case StyleRuleType::Supports:
        return Protocol::CSS::Grouping::Type::SupportsRule;
    case StyleRuleType::LayerBlock:
        return Protocol::CSS::Grouping::Type::LayerRule;
    case StyleRuleType::Container:
        return Protocol::CSS::Grouping::Type::ContainerRule;
    default:
        return std::nullopt;
    }
}

class ParsedStyleSheet {
    WTF_MAKE_FAST_ALLOCATED;
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
        case RuleFlatteningStrategy::Commit:
            target.append(data.copyRef());
            break;

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
    void observeProperty(unsigned startOffset, unsigned endOffset, bool isImportant, bool isParsed) override;
    void observeComment(unsigned startOffset, unsigned endOffset) override;
    
    Ref<CSSRuleSourceData> popRuleData();
    template <typename CharacterType> inline void setRuleHeaderEnd(const CharacterType*, unsigned);
    void fixUnparsedPropertyRanges(CSSRuleSourceData*);
    
    const String& m_parsedText;
    Document* m_document;
    
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
    m_currentRuleDataStack.append(WTFMove(data));
}

template <typename CharacterType> inline void StyleSheetHandler::setRuleHeaderEnd(const CharacterType* dataStart, unsigned listEndOffset)
{
    while (listEndOffset > m_currentRuleDataStack.last()->ruleHeaderRange.start) {
        if (isHTMLSpace<CharacterType>(*(dataStart + listEndOffset - 1)))
            --listEndOffset;
        else
            break;
    }

    m_currentRuleDataStack.last()->ruleHeaderRange.end = listEndOffset;
    if (!m_currentRuleDataStack.last()->selectorRanges.isEmpty())
        m_currentRuleDataStack.last()->selectorRanges.last().end = listEndOffset;
}

void StyleSheetHandler::endRuleHeader(unsigned offset)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    
    if (m_parsedText.is8Bit())
        setRuleHeaderEnd<LChar>(m_parsedText.characters8(), offset);
    else
        setRuleHeaderEnd<UChar>(m_parsedText.characters16(), offset);
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
    if (m_currentRuleDataStack.isEmpty())
        m_ruleSourceDataResult->append(WTFMove(rule));
    else
        m_currentRuleDataStack.last()->childRules.append(WTFMove(rule));
}

Ref<CSSRuleSourceData> StyleSheetHandler::popRuleData()
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleData = nullptr;
    auto data = WTFMove(m_currentRuleDataStack.last());
    m_currentRuleDataStack.removeLast();
    return data;
}

template <typename CharacterType>
static inline void fixUnparsedProperties(const CharacterType* characters, CSSRuleSourceData* ruleData)
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
        
        while (isHTMLSpace<CharacterType>(characters[propertyEnd]))
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

            while (valueStart < propertyEnd && isHTMLSpace<CharacterType>(characters[valueStart]))
                ++valueStart;
            
            // Need to exclude the trailing ';' from the property value.
            currentData->value = String(characters + valueStart, propertyEnd - valueStart + (characters[propertyEnd] == ';' ? 0 : 1));
        }
    }
}

void StyleSheetHandler::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData)
{
    if (!ruleData->styleSourceData)
        return;
    
    if (m_parsedText.is8Bit()) {
        fixUnparsedProperties<LChar>(m_parsedText.characters8(), ruleData);
        return;
    }
    
    fixUnparsedProperties<UChar>(m_parsedText.characters16(), ruleData);
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
    StringView propertyString = StringView(m_parsedText).substring(startOffset, endOffset - startOffset).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline);
    if (propertyString.endsWith(';'))
        propertyString = propertyString.left(propertyString.length() - 1);
    size_t colonIndex = propertyString.find(':');
    ASSERT(colonIndex != notFound);

    String name = propertyString.left(colonIndex).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline).toString();
    String value = propertyString.substring(colonIndex + 1, propertyString.length()).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline).toString();
    
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
    commentTextView = commentTextView.left(commentTextView.length() - 2).stripLeadingAndTrailingMatchedCharacters(isSpaceOrNewline);
    if (commentTextView.isEmpty())
        return;

    auto commentText = commentTextView.toString();
    
    // FIXME: Use the actual rule type rather than STYLE_RULE?
    RuleSourceDataList sourceData;
    
    StyleSheetHandler handler(commentText, m_document, &sourceData);
    CSSParser::parseDeclarationForInspector(parserContextForDocument(m_document), commentText, handler);
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

static RefPtr<Protocol::CSS::SourceRange> buildSourceRangeObject(const SourceRange& range, const Vector<size_t>& lineEndings, int* endingLine = nullptr)
{
    if (lineEndings.isEmpty())
        return nullptr;

    TextPosition start = ContentSearchUtilities::textPositionFromOffset(range.start, lineEndings);
    TextPosition end = ContentSearchUtilities::textPositionFromOffset(range.end, lineEndings);

    if (endingLine)
        *endingLine = end.m_line.zeroBasedInt();

    return Protocol::CSS::SourceRange::create()
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

    if (is<CSSMediaRule>(*rule))
        return &downcast<CSSMediaRule>(*rule).cssRules();

    if (is<CSSKeyframesRule>(*rule))
        return &downcast<CSSKeyframesRule>(*rule).cssRules();

    if (is<CSSSupportsRule>(*rule))
        return &downcast<CSSSupportsRule>(*rule).cssRules();

    if (is<CSSLayerBlockRule>(*rule))
        return &downcast<CSSLayerBlockRule>(*rule).cssRules();

    if (auto* containerRule = dynamicDowncast<CSSContainerRule>(rule))
        return &containerRule->cssRules();

    return nullptr;
}

static String sourceURLForCSSRule(CSSRule& rule)
{
    if (auto* parentStyleSheet = rule.parentStyleSheet()) {
        if (auto sourceURL = parentStyleSheet->contents().baseURL().string(); !sourceURL.isEmpty())
            return sourceURL;

        if (auto* ownerDocument = parentStyleSheet->ownerDocument())
            return InspectorDOMAgent::documentURLString(ownerDocument);
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

    auto payload = Protocol::CSS::Grouping::create()
        .setType(*groupingRuleProtocolType)
        .release();

    if (auto ruleId = this->ruleOrStyleId(groupingRule).asProtocolValue<Protocol::CSS::CSSRuleId>())
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

    auto* parentRule = &rule;
    while (parentRule) {
        if (auto groupingRule = buildObjectForGrouping(parentRule))
            groupingsPayload->addItem(groupingRule.releaseNonNull());
        else if (auto* importRule = dynamicDowncast<CSSImportRule>(parentRule)) {
            // FIXME: <webkit.org/b/246958> Show import rule as a single rule, instead of two rules for media and layer.
            auto sourceURL = sourceURLForCSSRule(*importRule);
            if (auto layerName = importRule->layerName(); !layerName.isNull()) {
                auto layerRulePayload = Protocol::CSS::Grouping::create()
                    .setType(Protocol::CSS::Grouping::Type::LayerImportRule)
                    .release();
                layerRulePayload->setText(layerName);
                if (!sourceURL.isEmpty())
                    layerRulePayload->setSourceURL(sourceURL);
                groupingsPayload->addItem(WTFMove(layerRulePayload));
            }

            if (auto& media = importRule->media(); media.length() && media.mediaText() != "all"_s) {
                auto mediaRulePayload = Protocol::CSS::Grouping::create()
                    .setType(Protocol::CSS::Grouping::Type::MediaImportRule)
                    .release();
                mediaRulePayload->setText(media.mediaText());
                if (!sourceURL.isEmpty())
                    mediaRulePayload->setSourceURL(sourceURL);
                groupingsPayload->addItem(WTFMove(mediaRulePayload));
            }
        }

        if (parentRule->parentRule()) {
            parentRule = parentRule->parentRule();
            continue;
        }

        auto* styleSheet = parentRule->parentStyleSheet();
        while (styleSheet) {
            auto* media = styleSheet->media();
            // FIXME: <webkit.org/b/246959> Support editing `style` and `link` node media queries.
            if (media && media->length() && media->mediaText() != "all"_s) {
                auto sheetGroupingPayload = Protocol::CSS::Grouping::create()
                    .setType(is<HTMLStyleElement>(styleSheet->ownerNode()) ? Protocol::CSS::Grouping::Type::MediaStyleNode: Protocol::CSS::Grouping::Type::MediaLinkNode)
                    .release();
                sheetGroupingPayload->setText(media->mediaText());

                String sourceURL;
                if (auto* ownerDocument = styleSheet->ownerDocument())
                    sourceURL = ownerDocument->url().string();
                else if (!styleSheet->contents().baseURL().isEmpty())
                    sourceURL = styleSheet->contents().baseURL().string();
                if (!sourceURL.isEmpty())
                    sheetGroupingPayload->setSourceURL(sourceURL);

                groupingsPayload->addItem(WTFMove(sheetGroupingPayload));
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
    return adoptRef(*new InspectorStyle(styleId, WTFMove(style), parentStyleSheet));
}

InspectorStyle::InspectorStyle(const InspectorCSSId& styleId, Ref<CSSStyleDeclaration>&& style, InspectorStyleSheet* parentStyleSheet)
    : m_styleId(styleId)
    , m_style(WTFMove(style))
    , m_parentStyleSheet(parentStyleSheet)
{
}

InspectorStyle::~InspectorStyle() = default;

Ref<Protocol::CSS::CSSStyle> InspectorStyle::buildObjectForStyle() const
{
    auto result = styleWithProperties();
    if (auto styleId = m_styleId.asProtocolValue<Protocol::CSS::CSSStyleId>())
        result->setStyleId(styleId.releaseNonNull());

    result->setWidth(m_style->getPropertyValue("width"_s));
    result->setHeight(m_style->getPropertyValue("height"_s));

    if (auto sourceData = extractSourceData()) {
        if (auto range = buildSourceRangeObject(sourceData->ruleBodyRange, m_parentStyleSheet->lineEndings()))
            result->setRange(range.releaseNonNull());
    }

    return result;
}

Ref<JSON::ArrayOf<Protocol::CSS::CSSComputedStyleProperty>> InspectorStyle::buildArrayForComputedStyle() const
{
    auto result = JSON::ArrayOf<Protocol::CSS::CSSComputedStyleProperty>::create();
    for (auto& property : collectProperties(true)) {
        const CSSPropertySourceData& propertyEntry = property.sourceData;
        auto entry = Protocol::CSS::CSSComputedStyleProperty::create()
            .setName(propertyEntry.name)
            .setValue(propertyEntry.value)
            .release();
        result->addItem(WTFMove(entry));
    }
    return result;
}

ExceptionOr<String> InspectorStyle::text() const
{
    // Precondition: m_parentStyleSheet->ensureParsedDataReady() has been called successfully.
    auto sourceData = extractSourceData();
    if (!sourceData)
        return Exception { NotFoundError };

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

Vector<InspectorStyleProperty> InspectorStyle::collectProperties(bool includeAll) const
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

Ref<Protocol::CSS::CSSStyle> InspectorStyle::styleWithProperties() const
{
    auto properties = collectProperties(false);

    auto propertiesObject = JSON::ArrayOf<Protocol::CSS::CSSProperty>::create();
    auto shorthandEntries = ArrayOf<Protocol::CSS::ShorthandEntry>::create();
    HashMap<String, RefPtr<Protocol::CSS::CSSProperty>> propertyNameToPreviousActiveProperty;
    HashSet<String> foundShorthands;
    String previousPriority;
    String previousStatus;
    Vector<size_t> lineEndings = m_parentStyleSheet ? m_parentStyleSheet->lineEndings() : Vector<size_t> { };
    auto sourceData = extractSourceData();
    unsigned ruleBodyRangeStart = sourceData ? sourceData->ruleBodyRange.start : 0;

    for (Vector<InspectorStyleProperty>::iterator it = properties.begin(), itEnd = properties.end(); it != itEnd; ++it) {
        const CSSPropertySourceData& propertyEntry = it->sourceData;
        const String& name = propertyEntry.name;

        auto status = it->disabled ? Protocol::CSS::CSSPropertyStatus::Disabled : Protocol::CSS::CSSPropertyStatus::Active;

        auto property = Protocol::CSS::CSSProperty::create()
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
        if (it->hasRawText())
            property->setText(it->rawText);

        // Default "priority" == "".
        if (propertyEntry.important)
            property->setPriority("important"_s);

        if (it->hasSource) {
            // The property range is relative to the style body start.
            // Should be converted into an absolute range (relative to the stylesheet start)
            // for the proper conversion into line:column.
            SourceRange absolutePropertyRange = propertyEntry.range;
            absolutePropertyRange.start += ruleBodyRangeStart;
            absolutePropertyRange.end += ruleBodyRangeStart;
            if (auto range = buildSourceRangeObject(absolutePropertyRange, lineEndings))
                property->setRange(range.releaseNonNull());
        }

        if (!it->disabled) {
            if (it->hasSource) {
                ASSERT(sourceData);
                property->setImplicit(false);

                // Parsed property overrides any property with the same name. Non-parsed property overrides
                // previous non-parsed property with the same name (if any).
                bool shouldInactivate = false;

                // Canonicalize property names to treat non-prefixed and vendor-prefixed property names the same (opacity vs. -webkit-opacity).
                String canonicalPropertyName = propertyId != CSSPropertyID::CSSPropertyInvalid && propertyId != CSSPropertyID::CSSPropertyCustom ? nameString(propertyId) : name;
                HashMap<String, RefPtr<Protocol::CSS::CSSProperty>>::iterator activeIt = propertyNameToPreviousActiveProperty.find(canonicalPropertyName);
                if (activeIt != propertyNameToPreviousActiveProperty.end()) {
                    if (propertyEntry.parsedOk) {
                        auto newPriority = activeIt->value->getString(Protocol::CSS::CSSProperty::priorityKey);
                        if (!!newPriority)
                            previousPriority = newPriority;

                        auto newStatus = activeIt->value->getString(Protocol::CSS::CSSProperty::statusKey);
                        if (!!newStatus) {
                            previousStatus = newStatus;
                            if (previousStatus != Protocol::Helpers::getEnumConstantValue(Protocol::CSS::CSSPropertyStatus::Inactive)) {
                                if (propertyEntry.important || !newPriority) // Priority not set == "not important".
                                    shouldInactivate = true;
                                else if (status == Protocol::CSS::CSSPropertyStatus::Active) {
                                    // Inactivate a non-important property following the same-named important property.
                                    status = Protocol::CSS::CSSPropertyStatus::Inactive;
                                }
                            }
                        }
                    } else {
                        auto previousParsedOk = activeIt->value->getBoolean(Protocol::CSS::CSSProperty::parsedOkKey);
                        if (previousParsedOk && !previousParsedOk)
                            shouldInactivate = true;
                    }
                } else
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property.copyRef());

                if (shouldInactivate) {
                    activeIt->value->setStatus(Protocol::CSS::CSSPropertyStatus::Inactive);
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property.copyRef());
                }
            } else {
                bool implicit = m_style->isPropertyImplicit(name);
                // Default "implicit" == false.
                if (implicit)
                    property->setImplicit(true);
                status = Protocol::CSS::CSSPropertyStatus::Style;

                String shorthand = m_style->getPropertyShorthand(name);
                if (!shorthand.isEmpty()) {
                    if (!foundShorthands.contains(shorthand)) {
                        foundShorthands.add(shorthand);
                        auto entry = Protocol::CSS::ShorthandEntry::create()
                            .setName(shorthand)
                            .setValue(shorthandValue(shorthand))
                            .release();
                        shorthandEntries->addItem(WTFMove(entry));
                    }
                }
            }
        }

        // Default "status" == "style".
        if (status != Protocol::CSS::CSSPropertyStatus::Style)
            property->setStatus(status);
    }

    return Protocol::CSS::CSSStyle::create()
        .setCssProperties(WTFMove(propertiesObject))
        .setShorthandEntries(WTFMove(shorthandEntries))
        .release();
}

RefPtr<CSSRuleSourceData> InspectorStyle::extractSourceData() const
{
    if (!m_parentStyleSheet || !m_parentStyleSheet->ensureParsedDataReady())
        return nullptr;
    return m_parentStyleSheet->ruleSourceDataFor(m_style.ptr());
}

ExceptionOr<void> InspectorStyle::setText(const String& text)
{
    return m_parentStyleSheet->setStyleText(m_style.ptr(), text);
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

Ref<InspectorStyleSheet> InspectorStyleSheet::create(InspectorPageAgent* pageAgent, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Protocol::CSS::StyleSheetOrigin origin, const String& documentURL, Listener* listener)
{
    return adoptRef(*new InspectorStyleSheet(pageAgent, id, WTFMove(pageStyleSheet), origin, documentURL, listener));
}

String InspectorStyleSheet::styleSheetURL(CSSStyleSheet* pageStyleSheet)
{
    if (pageStyleSheet && !pageStyleSheet->contents().baseURL().isEmpty())
        return pageStyleSheet->contents().baseURL().string();
    return emptyString();
}

InspectorStyleSheet::InspectorStyleSheet(InspectorPageAgent* pageAgent, const String& id, RefPtr<CSSStyleSheet>&& pageStyleSheet, Protocol::CSS::StyleSheetOrigin origin, const String& documentURL, Listener* listener)
    : m_pageAgent(pageAgent)
    , m_id(id)
    , m_pageStyleSheet(WTFMove(pageStyleSheet))
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

    // We just wiped the entire contents of the stylesheet. Clear the mutation flag.
    m_pageStyleSheet->clearHadRulesMutation();
}

ExceptionOr<void> InspectorStyleSheet::setText(const String& text)
{
    if (!m_pageStyleSheet)
        return Exception { NotSupportedError };

    m_parsedStyleSheet->setText(text);
    m_flatRules.clear();

    return { };
}

ExceptionOr<String> InspectorStyleSheet::ruleHeaderText(const InspectorCSSId& id)
{
    auto* rule = ruleForId(id);
    if (!rule)
        return Exception { NotFoundError };

    if (auto* cssStyleRule = dynamicDowncast<CSSStyleRule>(rule))
        return cssStyleRule->selectorText();

    auto sourceData = ruleSourceDataFor(rule);
    if (!sourceData)
        return Exception { NotFoundError };

    String sheetText = m_parsedStyleSheet->text();
    return sheetText.substring(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length());
}

ExceptionOr<void> InspectorStyleSheet::setRuleHeaderText(const InspectorCSSId& id, const String& newHeaderText)
{
    if (!m_pageStyleSheet)
        return Exception { NotSupportedError };

    auto* rule = ruleForId(id);
    if (!rule)
        return Exception { NotFoundError };

    if (!isValidRuleHeaderText(newHeaderText, rule->styleRuleType(), m_pageStyleSheet->ownerDocument()))
        return Exception { SyntaxError };

    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { NotFoundError };

    auto correctedHeaderText = newHeaderText;

    // Fast-path the editing of `CSSStyleRules` by using its built-in CSSOM support for editing instead of reparsing the entire style sheet.
    auto* cssStyleRule = dynamicDowncast<CSSStyleRule>(rule);
    if (cssStyleRule)
        cssStyleRule->setSelectorText(correctedHeaderText);

    auto sourceData = ruleSourceDataFor(rule);
    if (!sourceData)
        return Exception { NotFoundError };

    String sheetText = m_parsedStyleSheet->text();

    if (!cssStyleRule
        && sourceData->ruleHeaderRange.start
        && sheetText.characterAt(sourceData->ruleHeaderRange.start - 1) != ' '
        && !correctedHeaderText.startsWith('(')) {
        // @ rules do not include the `@whatever` part of the declaration in their header text range, nor do they
        // include the space between the `@whatever` and the query/name/etc.. However, not all rules must contain a
        // space between those, for example `@media(...)`. We need to add the space if the new header text does not
        // start with an opening parenthesis, otherwise we will create an invalid declaration (e.g. `@mediascreen`).
        correctedHeaderText = " "_s + correctedHeaderText;
    }

    sheetText = makeStringByReplacing(sheetText, sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), correctedHeaderText);

    if (cssStyleRule) {
        // Set the style sheet text directly so we don't rebuild our flat rule set. The CSSStyleRule has been directly
        // updated already.
        m_parsedStyleSheet->setText(sheetText);
        m_pageStyleSheet->clearHadRulesMutation();
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
        return Exception { NotSupportedError };

    if (!isValidRuleHeaderText(selector, StyleRuleType::Style, m_pageStyleSheet->ownerDocument()))
        return Exception { SyntaxError };

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

    styleSheetText.append(selector, " {}");

    // Using setText() as this operation changes the stylesheet rule set.
    setText(styleSheetText.toString());

    // Inspector Style Sheets are always treated as though their parsed data is ready.
    if (m_origin == Protocol::CSS::StyleSheetOrigin::Inspector)
        fireStyleSheetChanged();
    else
        reparseStyleSheet(styleSheetText.toString());

    ASSERT(m_pageStyleSheet->length());
    unsigned lastRuleIndex = m_pageStyleSheet->length() - 1;
    CSSRule* rule = m_pageStyleSheet->item(lastRuleIndex);
    ASSERT(rule);

    CSSStyleRule* styleRule = dynamicDowncast<CSSStyleRule>(rule);
    if (!styleRule) {
        // What we just added has to be a CSSStyleRule - we cannot handle other types of rules yet.
        // If it is not a style rule, pretend we never touched the stylesheet.
        m_pageStyleSheet->deleteRule(lastRuleIndex);
        return Exception { SyntaxError };
    }

    return styleRule;
}

ExceptionOr<void> InspectorStyleSheet::deleteRule(const InspectorCSSId& id)
{
    if (!m_pageStyleSheet)
        return Exception { NotSupportedError };

    RefPtr<CSSStyleRule> rule = dynamicDowncast<CSSStyleRule>(ruleForId(id));
    if (!rule)
        return Exception { NotFoundError };
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { NotFoundError };

    auto sourceData = ruleSourceDataFor(rule.get());
    if (!sourceData)
        return Exception { NotFoundError };

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

RefPtr<Protocol::CSS::CSSStyleSheetBody> InspectorStyleSheet::buildObjectForStyleSheet()
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    RefPtr<CSSRuleList> cssRuleList = asCSSRuleList(styleSheet);

    auto result = Protocol::CSS::CSSStyleSheetBody::create()
        .setStyleSheetId(id())
        .setRules(buildArrayForRuleList(cssRuleList.get()))
        .release();

    auto styleSheetText = text();
    if (!styleSheetText.hasException())
        result->setText(styleSheetText.releaseReturnValue());

    return result;
}

RefPtr<Protocol::CSS::CSSStyleSheetHeader> InspectorStyleSheet::buildObjectForStyleSheetInfo()
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    Document* document = styleSheet->ownerDocument();
    Frame* frame = document ? document->frame() : nullptr;
    return Protocol::CSS::CSSStyleSheetHeader::create()
        .setStyleSheetId(id())
        .setOrigin(m_origin)
        .setDisabled(styleSheet->disabled())
        .setSourceURL(finalURL())
        .setTitle(styleSheet->title())
        .setFrameId(m_pageAgent->frameId(frame))
        .setIsInline(styleSheet->isInline() && styleSheet->startPosition() != TextPosition())
        .setStartLine(styleSheet->startPosition().m_line.zeroBasedInt())
        .setStartColumn(styleSheet->startPosition().m_column.zeroBasedInt())
        .release();
}

static Ref<Protocol::CSS::CSSSelector> buildObjectForSelectorHelper(const String& selectorText, const CSSSelector& selector)
{
    auto inspectorSelector = Protocol::CSS::CSSSelector::create()
        .setText(selectorText)
        .release();

    auto specificity = selector.computeSpecificityTuple();

    auto tuple = JSON::ArrayOf<int>::create();
    tuple->addItem(specificity[0]);
    tuple->addItem(specificity[1]);
    tuple->addItem(specificity[2]);
    inspectorSelector->setSpecificity(WTFMove(tuple));

    return inspectorSelector;
}

static Ref<JSON::ArrayOf<Protocol::CSS::CSSSelector>> selectorsFromSource(const CSSRuleSourceData* sourceData, const String& sheetText, const Vector<const CSSSelector*> selectors)
{
    static NeverDestroyed<JSC::Yarr::RegularExpression> comment("/\\*[^]*?\\*/"_s, JSC::Yarr::TextCaseSensitive, JSC::Yarr::MultilineEnabled);

    auto result = JSON::ArrayOf<Protocol::CSS::CSSSelector>::create();
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
        result->addItem(buildObjectForSelectorHelper(selectorText.stripWhiteSpace(), *selectors.at(selectorIndex)));

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
        auto* rule = dynamicDowncast<CSSStyleRule>(m_flatRules.at(i).get());
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
        for (const CSSSelector* selector = rule->styleRule().selectorList().first(); selector; selector = CSSSelectorList::next(selector))
            selectors.append(selector);
    }
    return selectors;
}

Ref<Protocol::CSS::CSSSelector> InspectorStyleSheet::buildObjectForSelector(const CSSSelector* selector)
{
    return buildObjectForSelectorHelper(selector->selectorText(), *selector);
}

Ref<Protocol::CSS::SelectorList> InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule, int& endingLine)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(rule);
    RefPtr<JSON::ArrayOf<Protocol::CSS::CSSSelector>> selectors;

    // This intentionally does not rely on the source data to avoid catching the trailing comments (before the declaration starting '{').
    String selectorText = rule->selectorText();

    if (sourceData)
        selectors = selectorsFromSource(sourceData.get(), m_parsedStyleSheet->text(), selectorsForCSSStyleRule(*rule));
    else {
        selectors = JSON::ArrayOf<Protocol::CSS::CSSSelector>::create();
        for (const CSSSelector* selector : selectorsForCSSStyleRule(*rule))
            selectors->addItem(buildObjectForSelector(selector));
    }
    auto result = Protocol::CSS::SelectorList::create()
        .setSelectors(selectors.releaseNonNull())
        .setText(selectorText)
        .release();
    if (sourceData) {
        if (auto range = buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings(), &endingLine))
            result->setRange(range.releaseNonNull());
    }
    return result;
}

RefPtr<Protocol::CSS::CSSRule> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    int endingLine = 0;
    auto result = Protocol::CSS::CSSRule::create()
        .setSelectorList(buildObjectForSelectorList(rule, endingLine))
        .setSourceLine(endingLine)
        .setOrigin(m_origin)
        .setStyle(buildObjectForStyle(&rule->style()))
        .release();

    if (m_origin == Protocol::CSS::StyleSheetOrigin::Author || m_origin == Protocol::CSS::StyleSheetOrigin::User)
        result->setSourceURL(finalURL());

    if (canBind()) {
        if (auto ruleId = this->ruleOrStyleId(rule).asProtocolValue<Protocol::CSS::CSSRuleId>())
            result->setRuleId(ruleId.releaseNonNull());
    }

    auto groupingsPayload = buildArrayForGroupings(*rule);
    if (groupingsPayload->length())
        result->setGroupings(WTFMove(groupingsPayload));

    return result;
}

Ref<Protocol::CSS::CSSStyle> InspectorStyleSheet::buildObjectForStyle(CSSStyleDeclaration* style)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(style);

    InspectorCSSId id = ruleOrStyleId(style);
    if (id.isEmpty()) {
        return Protocol::CSS::CSSStyle::create()
            .setCssProperties(ArrayOf<Protocol::CSS::CSSProperty>::create())
            .setShorthandEntries(ArrayOf<Protocol::CSS::ShorthandEntry>::create())
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

ExceptionOr<void> InspectorStyleSheet::setStyleText(const InspectorCSSId& id, const String& text, String* oldText)
{
    auto inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle)
        return Exception { NotFoundError };

    if (oldText) {
        auto result = inspectorStyle->text();
        if (result.hasException())
            return result.releaseException();
        *oldText = result.releaseReturnValue();
    }

    auto result = inspectorStyle->setText(text);
    if (!result.hasException())
        fireStyleSheetChanged();
    return result;
}

ExceptionOr<String> InspectorStyleSheet::text() const
{
    if (!ensureText())
        return Exception { NotFoundError };
    return String { m_parsedStyleSheet->text() };
}

CSSStyleDeclaration* InspectorStyleSheet::styleForId(const InspectorCSSId& id) const
{
    auto* rule = dynamicDowncast<CSSStyleRule>(ruleForId(id));
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
    CSSStyleDeclaration* style = styleForId(id);
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
        auto* cssStyleRule = dynamicDowncast<CSSStyleRule>(rule.get());

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

bool InspectorStyleSheet::styleSheetMutated() const
{
    return m_pageStyleSheet && m_pageStyleSheet->hadRulesMutation();
}

bool InspectorStyleSheet::ensureParsedDataReady()
{
    bool allowParsedData = m_origin == Protocol::CSS::StyleSheetOrigin::Inspector || !styleSheetMutated();
    return allowParsedData && ensureText() && ensureSourceData();
}

bool InspectorStyleSheet::ensureText() const
{
    if (!m_parsedStyleSheet)
        return false;
    if (m_parsedStyleSheet->hasText())
        return true;

    String text;
    bool success = originalStyleSheetText(&text);
    if (success)
        m_parsedStyleSheet->setText(text);
    // No need to clear m_flatRules here - it's empty.

    return success;
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
    if (auto* ownerNode = m_pageStyleSheet->ownerNode(); ownerNode && ownerNode->isInUserAgentShadowTree())
        context.mode = UASheetMode;
    
    StyleSheetHandler handler(m_parsedStyleSheet->text(), m_pageStyleSheet->ownerDocument(), ruleSourceDataResult.get());
    CSSParser::parseSheetForInspector(context, newStyleSheet.ptr(), m_parsedStyleSheet->text(), handler);
    m_parsedStyleSheet->setSourceData(WTFMove(ruleSourceDataResult));
    return m_parsedStyleSheet->hasSourceData();
}

void InspectorStyleSheet::ensureFlatRules() const
{
    // We are fine with redoing this for empty stylesheets as this will run fast.
    if (m_flatRules.isEmpty())
        collectFlatRules(asCSSRuleList(pageStyleSheet()), &m_flatRules);
}

ExceptionOr<void> InspectorStyleSheet::setStyleText(CSSStyleDeclaration* style, const String& text)
{
    if (!m_pageStyleSheet)
        return Exception { NotFoundError };
    if (!ensureParsedDataReady())
        return Exception { NotFoundError };

    String patchedStyleSheetText;
    bool success = styleSheetTextWithChangedStyle(style, text, &patchedStyleSheetText);
    if (!success)
        return Exception { NotFoundError };

    InspectorCSSId id = ruleOrStyleId(style);
    if (id.isEmpty())
        return Exception { NotFoundError };

    auto setCssTextResult = style->setCssText(text);
    if (setCssTextResult.hasException())
        return setCssTextResult.releaseException();

    m_parsedStyleSheet->setText(patchedStyleSheetText);
    return { };
}

bool InspectorStyleSheet::styleSheetTextWithChangedStyle(CSSStyleDeclaration* style, const String& newStyleText, String* result)
{
    if (!style)
        return false;

    if (!ensureParsedDataReady())
        return false;

    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(style);
    unsigned bodyStart = sourceData->ruleBodyRange.start;
    unsigned bodyEnd = sourceData->ruleBodyRange.end;
    ASSERT(bodyStart <= bodyEnd);

    String text = m_parsedStyleSheet->text();
    ASSERT_WITH_SECURITY_IMPLICATION(bodyEnd <= text.length()); // bodyEnd is exclusive

    *result = makeStringByReplacing(text, bodyStart, bodyEnd - bodyStart, newStyleText);
    return true;
}

bool InspectorStyleSheet::originalStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet || m_origin == Protocol::CSS::StyleSheetOrigin::UserAgent)
        return false;
    return inlineStyleSheetText(result) || resourceStyleSheetText(result) || extensionStyleSheetText(result);
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (!ownerDocument() || !ownerDocument()->frame())
        return false;

    String error;
    bool base64Encoded;
    InspectorPageAgent::resourceContent(error, ownerDocument()->frame(), URL({ }, m_pageStyleSheet->href()), result, &base64Encoded);
    return error.isEmpty() && !base64Encoded;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    auto* ownerNode = m_pageStyleSheet->ownerNode();
    if (!is<Element>(ownerNode))
        return false;

    auto& ownerElement = downcast<Element>(*ownerNode);
    if (!is<HTMLStyleElement>(ownerElement) && !is<SVGStyleElement>(ownerElement))
        return false;

    *result = ownerElement.textContent();
    return true;
}

bool InspectorStyleSheet::extensionStyleSheetText(String* result) const
{
    if (!ownerDocument())
        return false;

    auto content = ownerDocument()->extensionStyleSheets().contentForInjectedStyleSheet(m_pageStyleSheet);
    if (content.isEmpty())
        return false;

    *result = content;
    return true;
}

Ref<JSON::ArrayOf<Protocol::CSS::CSSRule>> InspectorStyleSheet::buildArrayForRuleList(CSSRuleList* ruleList)
{
    auto result = JSON::ArrayOf<Protocol::CSS::CSSRule>::create();
    if (!ruleList)
        return result;

    RefPtr<CSSRuleList> refRuleList = ruleList;
    Vector<RefPtr<CSSRule>> rules;
    collectFlatRules(WTFMove(refRuleList), &rules);

    for (auto& rule : rules) {
        if (auto* styleRule = dynamicDowncast<CSSStyleRule>(rule.get())) {
            if (auto ruleObject = buildObjectForRule(styleRule))
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
        CSSRule* rule = ruleList->item(i);
        if (!rule)
            continue;

        switch (flatteningStrategyForStyleRuleType(rule->styleRuleType())) {
        case RuleFlatteningStrategy::Commit:
            result->append(rule);
            break;

        case RuleFlatteningStrategy::CommitSelfThenChildren: {
            result->append(rule);

            auto childRuleList = asCSSRuleList(rule);
            ASSERT(childRuleList);
            collectFlatRules(WTFMove(childRuleList), result);
            break;
        }

        case RuleFlatteningStrategy::Ignore:
            break;
        }
    }
}

Ref<InspectorStyleSheetForInlineStyle> InspectorStyleSheetForInlineStyle::create(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
{
    return adoptRef(*new InspectorStyleSheetForInlineStyle(pageAgent, id, WTFMove(element), origin, listener));
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
    : InspectorStyleSheet(pageAgent, id, nullptr, origin, String(), listener)
    , m_element(WTFMove(element))
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

ExceptionOr<String> InspectorStyleSheetForInlineStyle::text() const
{
    if (!m_isStyleTextValid) {
        m_styleText = elementStyleText();
        m_isStyleTextValid = true;
    }
    return String { m_styleText };
}

ExceptionOr<void> InspectorStyleSheetForInlineStyle::setStyleText(CSSStyleDeclaration* style, const String& text)
{
    ASSERT_UNUSED(style, style == &inlineStyle());

    {
        InspectorCSSAgent::InlineStyleOverrideScope overrideScope(m_element->document());
        m_element->setAttribute(HTMLNames::styleAttr, AtomString { text });
    }

    m_styleText = text;
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
    CSSParser::parseDeclarationForInspector(context, m_styleText, handler);
    return WTFMove(ruleSourceDataResult.first());
}

} // namespace WebCore
