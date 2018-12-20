/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "CSSImportRule.h"
#include "CSSKeyframesRule.h"
#include "CSSMediaRule.h"
#include "CSSParser.h"
#include "CSSParserObserver.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSSupportsRule.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "Element.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLStyleElement.h"
#include "InspectorCSSAgent.h"
#include "InspectorPageAgent.h"
#include "MediaList.h"
#include "Node.h"
#include "SVGElement.h"
#include "SVGStyleElement.h"
#include "StyleProperties.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleRuleImport.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/text/StringBuilder.h>

using JSON::ArrayOf;
using WebCore::RuleSourceDataList;
using WebCore::CSSRuleSourceData;

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
        if (data->type == WebCore::StyleRule::Style)
            target.append(data.copyRef());
        else if (data->type == WebCore::StyleRule::Media)
            flattenSourceData(data->childRules, target);
        else if (data->type == WebCore::StyleRule::Supports)
            flattenSourceData(data->childRules, target);
    }
}

void ParsedStyleSheet::setSourceData(std::unique_ptr<RuleSourceDataList> sourceData)
{
    if (!sourceData) {
        m_sourceData.reset();
        return;
    }

    m_sourceData = std::make_unique<RuleSourceDataList>();

    // FIXME: This is a temporary solution to retain the original flat sourceData structure
    // containing only style rules, even though CSSParser now provides the full rule source data tree.
    // Normally, we should just assign m_sourceData = sourceData;
    flattenSourceData(*sourceData, *m_sourceData);
}

WebCore::CSSRuleSourceData* ParsedStyleSheet::ruleSourceDataAt(unsigned index) const
{
    if (!hasSourceData() || index >= m_sourceData->size())
        return nullptr;

    return m_sourceData->at(index).ptr();
}


namespace WebCore {

using namespace Inspector;

static CSSParserContext parserContextForDocument(Document* document)
{
    return document ? CSSParserContext(*document) : strictCSSParserContext();
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
    void startRuleHeader(StyleRule::Type, unsigned) override;
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

void StyleSheetHandler::startRuleHeader(StyleRule::Type type, unsigned offset)
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
    while (listEndOffset > 1) {
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
    String propertyString = m_parsedText.substring(startOffset, endOffset - startOffset).stripWhiteSpace();
    if (propertyString.endsWith(';'))
        propertyString = propertyString.left(propertyString.length() - 1);
    size_t colonIndex = propertyString.find(':');
    ASSERT(colonIndex != notFound);

    String name = propertyString.left(colonIndex).stripWhiteSpace();
    String value = propertyString.substring(colonIndex + 1, propertyString.length()).stripWhiteSpace();
    
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
    String commentText = m_parsedText.substring(startOffset, endOffset - startOffset);
    
    ASSERT(commentText.startsWith("/*"));
    commentText = commentText.substring(2);
    
    // Require well-formed comments.
    if (!commentText.endsWith("*/"))
        return;
    commentText = commentText.substring(0, commentText.length() - 2).stripWhiteSpace();
    if (commentText.isEmpty())
        return;
    
    // FIXME: Use the actual rule type rather than STYLE_RULE?
    RuleSourceDataList sourceData;
    
    StyleSheetHandler handler(commentText, m_document, &sourceData);
    CSSParser::parseDeclarationForInspector(parserContextForDocument(m_document), commentText, handler);
    Vector<CSSPropertySourceData>& commentPropertyData = sourceData.first()->styleSourceData->propertyData;
    if (commentPropertyData.size() != 1)
        return;
    CSSPropertySourceData& propertyData = commentPropertyData.at(0);
    bool parsedOk = propertyData.parsedOk || propertyData.name.startsWith("-moz-") || propertyData.name.startsWith("-o-") || propertyData.name.startsWith("-webkit-") || propertyData.name.startsWith("-ms-");
    if (!parsedOk || propertyData.range.length() != commentText.length())
        return;
    
    // FIXME-NEWPARSER: The property range is relative to the declaration start offset, but no
    // good reason for it, and it complicates fixUnparsedProperties.
    SourceRange& topRuleBodyRange = m_currentRuleDataStack.last()->ruleBodyRange;
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(CSSPropertySourceData(propertyData.name, propertyData.value, false, true, true, SourceRange(startOffset - topRuleBodyRange.start, endOffset - topRuleBodyRange.start)));
}

enum MediaListSource {
    MediaListSourceLinkedSheet,
    MediaListSourceInlineSheet,
    MediaListSourceMediaRule,
    MediaListSourceImportRule
};

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

static Ref<Inspector::Protocol::CSS::CSSMedia> buildMediaObject(const MediaList* media, MediaListSource mediaListSource, const String& sourceURL)
{
    // Make certain compilers happy by initializing |source| up-front.
    Inspector::Protocol::CSS::CSSMedia::Source source = Inspector::Protocol::CSS::CSSMedia::Source::InlineSheet;
    switch (mediaListSource) {
    case MediaListSourceMediaRule:
        source = Inspector::Protocol::CSS::CSSMedia::Source::MediaRule;
        break;
    case MediaListSourceImportRule:
        source = Inspector::Protocol::CSS::CSSMedia::Source::ImportRule;
        break;
    case MediaListSourceLinkedSheet:
        source = Inspector::Protocol::CSS::CSSMedia::Source::LinkedSheet;
        break;
    case MediaListSourceInlineSheet:
        source = Inspector::Protocol::CSS::CSSMedia::Source::InlineSheet;
        break;
    }

    auto mediaObject = Inspector::Protocol::CSS::CSSMedia::create()
        .setText(media->mediaText())
        .setSource(source)
        .release();

    if (!sourceURL.isEmpty()) {
        mediaObject->setSourceURL(sourceURL);
        mediaObject->setSourceLine(media->queries()->lastLine());
    }
    return mediaObject;
}

static RefPtr<CSSRuleList> asCSSRuleList(CSSStyleSheet* styleSheet)
{
    if (!styleSheet)
        return nullptr;

    RefPtr<StaticCSSRuleList> list = StaticCSSRuleList::create();
    Vector<RefPtr<CSSRule>>& listRules = list->rules();
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i)
        listRules.append(styleSheet->item(i));
    return WTFMove(list);
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

    return nullptr;
}

static void fillMediaListChain(CSSRule* rule, JSON::ArrayOf<Inspector::Protocol::CSS::CSSMedia>& mediaArray)
{
    MediaList* mediaList;
    CSSRule* parentRule = rule;
    String sourceURL;
    while (parentRule) {
        CSSStyleSheet* parentStyleSheet = nullptr;
        bool isMediaRule = true;
        if (is<CSSMediaRule>(*parentRule)) {
            CSSMediaRule& mediaRule = downcast<CSSMediaRule>(*parentRule);
            mediaList = mediaRule.media();
            parentStyleSheet = mediaRule.parentStyleSheet();
        } else if (is<CSSImportRule>(*parentRule)) {
            CSSImportRule& importRule = downcast<CSSImportRule>(*parentRule);
            mediaList = &importRule.media();
            parentStyleSheet = importRule.parentStyleSheet();
            isMediaRule = false;
        } else
            mediaList = nullptr;

        if (parentStyleSheet) {
            sourceURL = parentStyleSheet->contents().baseURL();
            if (sourceURL.isEmpty())
                sourceURL = InspectorDOMAgent::documentURLString(parentStyleSheet->ownerDocument());
        } else
            sourceURL = emptyString();

        if (mediaList && mediaList->length())
            mediaArray.addItem(buildMediaObject(mediaList, isMediaRule ? MediaListSourceMediaRule : MediaListSourceImportRule, sourceURL));

        if (parentRule->parentRule())
            parentRule = parentRule->parentRule();
        else {
            CSSStyleSheet* styleSheet = parentRule->parentStyleSheet();
            while (styleSheet) {
                mediaList = styleSheet->media();
                if (mediaList && mediaList->length()) {
                    Document* doc = styleSheet->ownerDocument();
                    if (doc)
                        sourceURL = doc->url();
                    else if (!styleSheet->contents().baseURL().isEmpty())
                        sourceURL = styleSheet->contents().baseURL();
                    else
                        sourceURL = emptyString();
                    mediaArray.addItem(buildMediaObject(mediaList, styleSheet->ownerNode() ? MediaListSourceLinkedSheet : MediaListSourceInlineSheet, sourceURL));
                }
                parentRule = styleSheet->ownerRule();
                if (parentRule)
                    break;
                styleSheet = styleSheet->parentStyleSheet();
            }
        }
    }
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

RefPtr<Inspector::Protocol::CSS::CSSStyle> InspectorStyle::buildObjectForStyle() const
{
    Ref<Inspector::Protocol::CSS::CSSStyle> result = styleWithProperties();
    if (!m_styleId.isEmpty())
        result->setStyleId(m_styleId.asProtocolValue<Inspector::Protocol::CSS::CSSStyleId>());

    result->setWidth(m_style->getPropertyValue("width"));
    result->setHeight(m_style->getPropertyValue("height"));

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (sourceData)
        result->setRange(buildSourceRangeObject(sourceData->ruleBodyRange, m_parentStyleSheet->lineEndings()));

    return WTFMove(result);
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>> InspectorStyle::buildArrayForComputedStyle() const
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>::create();
    Vector<InspectorStyleProperty> properties;
    populateAllProperties(&properties);

    for (auto& property : properties) {
        const CSSPropertySourceData& propertyEntry = property.sourceData;
        auto entry = Inspector::Protocol::CSS::CSSComputedStyleProperty::create()
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
    if (name.startsWith("--"))
        return name;
    return name.convertToASCIILowercase();
}

void InspectorStyle::populateAllProperties(Vector<InspectorStyleProperty>* result) const
{
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
            result->append(p);
            sourcePropertyNames.add(lowercasePropertyName(sourceData.name));
        }
    }

    for (int i = 0, size = m_style->length(); i < size; ++i) {
        String name = m_style->item(i);
        if (sourcePropertyNames.add(lowercasePropertyName(name)))
            result->append(InspectorStyleProperty(CSSPropertySourceData(name, m_style->getPropertyValue(name), !m_style->getPropertyPriority(name).isEmpty(), false, true, SourceRange()), false, false));
    }
}

Ref<Inspector::Protocol::CSS::CSSStyle> InspectorStyle::styleWithProperties() const
{
    Vector<InspectorStyleProperty> properties;
    populateAllProperties(&properties);

    auto propertiesObject = JSON::ArrayOf<Inspector::Protocol::CSS::CSSProperty>::create();
    auto shorthandEntries = ArrayOf<Inspector::Protocol::CSS::ShorthandEntry>::create();
    HashMap<String, RefPtr<Inspector::Protocol::CSS::CSSProperty>> propertyNameToPreviousActiveProperty;
    HashSet<String> foundShorthands;
    String previousPriority;
    String previousStatus;
    Vector<size_t> lineEndings = m_parentStyleSheet ? m_parentStyleSheet->lineEndings() : Vector<size_t> { };
    auto sourceData = extractSourceData();
    unsigned ruleBodyRangeStart = sourceData ? sourceData->ruleBodyRange.start : 0;

    for (Vector<InspectorStyleProperty>::iterator it = properties.begin(), itEnd = properties.end(); it != itEnd; ++it) {
        const CSSPropertySourceData& propertyEntry = it->sourceData;
        const String& name = propertyEntry.name;

        auto status = it->disabled ? Inspector::Protocol::CSS::CSSPropertyStatus::Disabled : Inspector::Protocol::CSS::CSSPropertyStatus::Active;

        RefPtr<Inspector::Protocol::CSS::CSSProperty> property = Inspector::Protocol::CSS::CSSProperty::create()
            .setName(name.convertToASCIILowercase())
            .setValue(propertyEntry.value)
            .release();

        propertiesObject->addItem(property.copyRef());

        CSSPropertyID propertyId = cssPropertyID(name);

        // Default "parsedOk" == true.
        if (!propertyEntry.parsedOk || isInternalCSSProperty(propertyId))
            property->setParsedOk(false);
        if (it->hasRawText())
            property->setText(it->rawText);

        // Default "priority" == "".
        if (propertyEntry.important)
            property->setPriority("important");

        if (it->hasSource) {
            // The property range is relative to the style body start.
            // Should be converted into an absolute range (relative to the stylesheet start)
            // for the proper conversion into line:column.
            SourceRange absolutePropertyRange = propertyEntry.range;
            absolutePropertyRange.start += ruleBodyRangeStart;
            absolutePropertyRange.end += ruleBodyRangeStart;
            property->setRange(buildSourceRangeObject(absolutePropertyRange, lineEndings));
        }

        if (!it->disabled) {
            if (it->hasSource) {
                ASSERT(sourceData);
                property->setImplicit(false);

                // Parsed property overrides any property with the same name. Non-parsed property overrides
                // previous non-parsed property with the same name (if any).
                bool shouldInactivate = false;

                // Canonicalize property names to treat non-prefixed and vendor-prefixed property names the same (opacity vs. -webkit-opacity).
                String canonicalPropertyName = propertyId ? getPropertyNameString(propertyId) : name;
                HashMap<String, RefPtr<Inspector::Protocol::CSS::CSSProperty>>::iterator activeIt = propertyNameToPreviousActiveProperty.find(canonicalPropertyName);
                if (activeIt != propertyNameToPreviousActiveProperty.end()) {
                    if (propertyEntry.parsedOk) {
                        bool successPriority = activeIt->value->getString(Inspector::Protocol::CSS::CSSProperty::Priority, previousPriority);
                        bool successStatus = activeIt->value->getString(Inspector::Protocol::CSS::CSSProperty::Status, previousStatus);
                        if (successStatus && previousStatus != "inactive") {
                            if (propertyEntry.important || !successPriority) // Priority not set == "not important".
                                shouldInactivate = true;
                            else if (status == Inspector::Protocol::CSS::CSSPropertyStatus::Active) {
                                // Inactivate a non-important property following the same-named important property.
                                status = Inspector::Protocol::CSS::CSSPropertyStatus::Inactive;
                            }
                        }
                    } else {
                        bool previousParsedOk;
                        bool success = activeIt->value->getBoolean(Inspector::Protocol::CSS::CSSProperty::ParsedOk, previousParsedOk);
                        if (success && !previousParsedOk)
                            shouldInactivate = true;
                    }
                } else
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property);

                if (shouldInactivate) {
                    activeIt->value->setStatus(Inspector::Protocol::CSS::CSSPropertyStatus::Inactive);
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property);
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
                        shorthandEntries->addItem(WTFMove(entry));
                    }
                }
            }
        }

        // Default "status" == "style".
        if (status != Inspector::Protocol::CSS::CSSPropertyStatus::Style)
            property->setStatus(status);
    }

    return Inspector::Protocol::CSS::CSSStyle::create()
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
        if (individualValue == "initial")
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
    return adoptRef(*new InspectorStyleSheet(pageAgent, id, WTFMove(pageStyleSheet), origin, documentURL, listener));
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

ExceptionOr<String> InspectorStyleSheet::ruleSelector(const InspectorCSSId& id)
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule)
        return Exception { NotFoundError };
    return rule->selectorText();
}

static bool isValidSelectorListString(const String& selector, Document* document)
{
    CSSSelectorList selectorList;
    CSSParser parser(parserContextForDocument(document));
    parser.parseSelector(selector, selectorList);
    return selectorList.isValid();
}

ExceptionOr<void> InspectorStyleSheet::setRuleSelector(const InspectorCSSId& id, const String& selector)
{
    if (!m_pageStyleSheet)
        return Exception { NotSupportedError };

    // If the selector is invalid, do not proceed any further.
    if (!isValidSelectorListString(selector, m_pageStyleSheet->ownerDocument()))
        return Exception { SyntaxError };

    CSSStyleRule* rule = ruleForId(id);
    if (!rule)
        return Exception { NotFoundError };

    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { NotFoundError };

    // If the stylesheet is already mutated at this point, that must mean that our data has been modified
    // elsewhere. This should never happen as ensureParsedDataReady would return false in that case.
    ASSERT(!styleSheetMutated());

    rule->setSelectorText(selector);
    auto sourceData = ruleSourceDataFor(&rule->style());
    if (!sourceData)
        return Exception { NotFoundError };

    String sheetText = m_parsedStyleSheet->text();
    sheetText.replace(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), selector);
    m_parsedStyleSheet->setText(sheetText);
    m_pageStyleSheet->clearHadRulesMutation();
    fireStyleSheetChanged();
    return { };
}

ExceptionOr<CSSStyleRule*> InspectorStyleSheet::addRule(const String& selector)
{
    if (!m_pageStyleSheet)
        return Exception { NotSupportedError };

    if (!isValidSelectorListString(selector, m_pageStyleSheet->ownerDocument()))
        return Exception { SyntaxError };

    auto text = this->text();
    if (text.hasException())
        return text.releaseException();

    auto addRuleResult = m_pageStyleSheet->addRule(selector, emptyString(), WTF::nullopt);
    if (addRuleResult.hasException())
        return addRuleResult.releaseException();

    StringBuilder styleSheetText;
    styleSheetText.append(text.releaseReturnValue());

    if (!styleSheetText.isEmpty())
        styleSheetText.append('\n');

    styleSheetText.append(selector);
    styleSheetText.appendLiteral(" {}");

    // Using setText() as this operation changes the stylesheet rule set.
    setText(styleSheetText.toString());

    // Inspector Style Sheets are always treated as though their parsed data is ready.
    if (m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::Inspector)
        fireStyleSheetChanged();
    else
        reparseStyleSheet(styleSheetText.toString());

    ASSERT(m_pageStyleSheet->length());
    unsigned lastRuleIndex = m_pageStyleSheet->length() - 1;
    CSSRule* rule = m_pageStyleSheet->item(lastRuleIndex);
    ASSERT(rule);

    CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(*rule);
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

    RefPtr<CSSStyleRule> rule = ruleForId(id);
    if (!rule)
        return Exception { NotFoundError };
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady())
        return Exception { NotFoundError };

    auto sourceData = ruleSourceDataFor(&rule->style());
    if (!sourceData)
        return Exception { NotFoundError };

    auto deleteRuleResult = styleSheet->deleteRule(id.ordinal());
    if (deleteRuleResult.hasException())
        return deleteRuleResult.releaseException();

    // |rule| MAY NOT be addressed after this!

    String sheetText = m_parsedStyleSheet->text();
    sheetText.remove(sourceData->ruleHeaderRange.start, sourceData->ruleBodyRange.end - sourceData->ruleHeaderRange.start + 1);
    setText(sheetText);
    fireStyleSheetChanged();
    return { };
}

CSSStyleRule* InspectorStyleSheet::ruleForId(const InspectorCSSId& id) const
{
    if (!m_pageStyleSheet)
        return nullptr;

    ASSERT(!id.isEmpty());
    ensureFlatRules();
    return id.ordinal() >= m_flatRules.size() ? nullptr : m_flatRules.at(id.ordinal()).get();
}

RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody> InspectorStyleSheet::buildObjectForStyleSheet()
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    RefPtr<CSSRuleList> cssRuleList = asCSSRuleList(styleSheet);

    auto result = Inspector::Protocol::CSS::CSSStyleSheetBody::create()
        .setStyleSheetId(id())
        .setRules(buildArrayForRuleList(cssRuleList.get()))
        .release();

    auto styleSheetText = text();
    if (!styleSheetText.hasException())
        result->setText(styleSheetText.releaseReturnValue());

    return WTFMove(result);
}

RefPtr<Inspector::Protocol::CSS::CSSStyleSheetHeader> InspectorStyleSheet::buildObjectForStyleSheetInfo()
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    Document* document = styleSheet->ownerDocument();
    Frame* frame = document ? document->frame() : nullptr;
    return Inspector::Protocol::CSS::CSSStyleSheetHeader::create()
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

static bool hasDynamicSpecificity(const CSSSelector& simpleSelector)
{
    // It is possible that these can have a static specificity if each selector in the list has
    // equal specificity, but lets always report that they can be dynamic.
    for (const CSSSelector* selector = &simpleSelector; selector; selector = selector->tagHistory()) {
        if (selector->match() == CSSSelector::PseudoClass) {
            CSSSelector::PseudoClassType pseudoClassType = selector->pseudoClassType();
            if (pseudoClassType == CSSSelector::PseudoClassMatches)
                return true;
            if (pseudoClassType == CSSSelector::PseudoClassNthChild || pseudoClassType == CSSSelector::PseudoClassNthLastChild) {
                if (selector->selectorList())
                    return true;
                return false;
            }
        }
    }

    return false;
}

static Ref<Inspector::Protocol::CSS::CSSSelector> buildObjectForSelectorHelper(const String& selectorText, const CSSSelector& selector, Element* element)
{
    auto inspectorSelector = Inspector::Protocol::CSS::CSSSelector::create()
        .setText(selectorText)
        .release();

    if (element) {
        bool dynamic = hasDynamicSpecificity(selector);
        if (dynamic)
            inspectorSelector->setDynamic(true);

        SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRules);
        SelectorChecker selectorChecker(element->document());

        unsigned specificity;
        bool okay = selectorChecker.match(selector, *element, context, specificity);
        if (!okay)
            specificity = selector.staticSpecificity(okay);

        if (okay) {
            auto tuple = JSON::ArrayOf<int>::create();
            tuple->addItem(static_cast<int>((specificity & CSSSelector::idMask) >> 16));
            tuple->addItem(static_cast<int>((specificity & CSSSelector::classMask) >> 8));
            tuple->addItem(static_cast<int>(specificity & CSSSelector::elementMask));
            inspectorSelector->setSpecificity(WTFMove(tuple));
        }
    }

    return inspectorSelector;
}

static Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>> selectorsFromSource(const CSSRuleSourceData* sourceData, const String& sheetText, const CSSSelectorList& selectorList, Element* element)
{
    static NeverDestroyed<JSC::Yarr::RegularExpression> comment("/\\*[^]*?\\*/", JSC::Yarr::TextCaseSensitive, JSC::Yarr::MultilineEnabled);

    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>::create();
    const CSSSelector* selector = selectorList.first();
    for (auto& range : sourceData->selectorRanges) {
        // If we don't have a selector, that means the SourceData for this CSSStyleSheet
        // no longer matches up with the actual rules in the CSSStyleSheet.
        ASSERT(selector);
        if (!selector)
            break;

        String selectorText = sheetText.substring(range.start, range.length());

        // We don't want to see any comments in the selector components, only the meaningful parts.
        replace(selectorText, comment, String());
        result->addItem(buildObjectForSelectorHelper(selectorText.stripWhiteSpace(), *selector, element));

        selector = CSSSelectorList::next(selector);
    }
    return result;
}

Ref<Inspector::Protocol::CSS::CSSSelector> InspectorStyleSheet::buildObjectForSelector(const CSSSelector* selector, Element* element)
{
    return buildObjectForSelectorHelper(selector->selectorText(), *selector, element);
}

Ref<Inspector::Protocol::CSS::SelectorList> InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule, Element* element, int& endingLine)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(&rule->style());
    RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>> selectors;

    // This intentionally does not rely on the source data to avoid catching the trailing comments (before the declaration starting '{').
    String selectorText = rule->selectorText();

    if (sourceData)
        selectors = selectorsFromSource(sourceData.get(), m_parsedStyleSheet->text(), rule->styleRule().selectorList(), element);
    else {
        selectors = JSON::ArrayOf<Inspector::Protocol::CSS::CSSSelector>::create();
        const CSSSelectorList& selectorList = rule->styleRule().selectorList();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
            selectors->addItem(buildObjectForSelector(selector, element));
    }
    auto result = Inspector::Protocol::CSS::SelectorList::create()
        .setSelectors(WTFMove(selectors))
        .setText(selectorText)
        .release();
    if (sourceData)
        result->setRange(buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings(), &endingLine));
    return result;
}

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule, Element* element)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return nullptr;

    int endingLine = 0;
    auto result = Inspector::Protocol::CSS::CSSRule::create()
        .setSelectorList(buildObjectForSelectorList(rule, element, endingLine))
        .setSourceLine(endingLine)
        .setOrigin(m_origin)
        .setStyle(buildObjectForStyle(&rule->style()))
        .release();

    // "sourceURL" is present only for regular rules, otherwise "origin" should be used in the frontend.
    if (m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::Regular)
        result->setSourceURL(finalURL());

    if (canBind()) {
        InspectorCSSId id(ruleId(rule));
        if (!id.isEmpty())
            result->setRuleId(id.asProtocolValue<Inspector::Protocol::CSS::CSSRuleId>());
    }

    auto mediaArray = ArrayOf<Inspector::Protocol::CSS::CSSMedia>::create();

    fillMediaListChain(rule, mediaArray.get());
    if (mediaArray->length())
        result->setMedia(WTFMove(mediaArray));

    return WTFMove(result);
}

RefPtr<Inspector::Protocol::CSS::CSSStyle> InspectorStyleSheet::buildObjectForStyle(CSSStyleDeclaration* style)
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
    RefPtr<Inspector::Protocol::CSS::CSSStyle> result = inspectorStyle->buildObjectForStyle();

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
    CSSStyleRule* rule = ruleForId(id);
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

InspectorCSSId InspectorStyleSheet::ruleOrStyleId(CSSStyleDeclaration* style) const
{
    unsigned index = ruleIndexByStyle(style);
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
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByStyle(style));
}

Vector<size_t> InspectorStyleSheet::lineEndings() const
{
    if (!m_parsedStyleSheet->hasText())
        return { };
    return ContentSearchUtilities::lineEndings(m_parsedStyleSheet->text());
}

unsigned InspectorStyleSheet::ruleIndexByStyle(CSSStyleDeclaration* pageStyle) const
{
    ensureFlatRules();
    unsigned index = 0;
    for (auto& rule : m_flatRules) {
        if (&rule->style() == pageStyle)
            return index;

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
    bool allowParsedData = m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::Inspector || !styleSheetMutated();
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

    RefPtr<StyleSheetContents> newStyleSheet = StyleSheetContents::create();
    auto ruleSourceDataResult = std::make_unique<RuleSourceDataList>();
    
    CSSParserContext context(parserContextForDocument(m_pageStyleSheet->ownerDocument()));
    StyleSheetHandler handler(m_parsedStyleSheet->text(), m_pageStyleSheet->ownerDocument(), ruleSourceDataResult.get());
    CSSParser::parseSheetForInspector(context, newStyleSheet.get(), m_parsedStyleSheet->text(), handler);
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

    text.replace(bodyStart, bodyEnd - bodyStart, newStyleText);
    *result = text;
    return true;
}

InspectorCSSId InspectorStyleSheet::ruleId(CSSStyleRule* rule) const
{
    return ruleOrStyleId(&rule->style());
}

bool InspectorStyleSheet::originalStyleSheetText(String* result) const
{
    bool success = inlineStyleSheetText(result);
    if (!success)
        success = resourceStyleSheetText(result);
    return success;
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::User || m_origin == Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent)
        return false;

    if (!m_pageStyleSheet || !ownerDocument() || !ownerDocument()->frame())
        return false;

    String error;
    bool base64Encoded;
    InspectorPageAgent::resourceContent(error, ownerDocument()->frame(), URL({ }, m_pageStyleSheet->href()), result, &base64Encoded);
    return error.isEmpty() && !base64Encoded;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet)
        return false;

    Node* ownerNode = m_pageStyleSheet->ownerNode();
    if (!is<Element>(ownerNode))
        return false;
    Element& ownerElement = downcast<Element>(*ownerNode);

    if (!is<HTMLStyleElement>(ownerElement) && !is<SVGStyleElement>(ownerElement))
        return false;
    *result = ownerElement.textContent();
    return true;
}

Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>> InspectorStyleSheet::buildArrayForRuleList(CSSRuleList* ruleList)
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::CSSRule>::create();
    if (!ruleList)
        return result;

    RefPtr<CSSRuleList> refRuleList = ruleList;
    CSSStyleRuleVector rules;
    collectFlatRules(WTFMove(refRuleList), &rules);

    for (auto& rule : rules)
        result->addItem(buildObjectForRule(rule.get(), nullptr));

    return result;
}

void InspectorStyleSheet::collectFlatRules(RefPtr<CSSRuleList>&& ruleList, CSSStyleRuleVector* result)
{
    if (!ruleList)
        return;

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSRule* rule = ruleList->item(i);
        CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(*rule);
        if (styleRule)
            result->append(styleRule);
        else {
            RefPtr<CSSRuleList> childRuleList = asCSSRuleList(rule);
            if (childRuleList)
                collectFlatRules(WTFMove(childRuleList), result);
        }
    }
}

Ref<InspectorStyleSheetForInlineStyle> InspectorStyleSheetForInlineStyle::create(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Inspector::Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
{
    return adoptRef(*new InspectorStyleSheetForInlineStyle(pageAgent, id, WTFMove(element), origin, listener));
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(InspectorPageAgent* pageAgent, const String& id, Ref<StyledElement>&& element, Inspector::Protocol::CSS::StyleSheetOrigin origin, Listener* listener)
    : InspectorStyleSheet(pageAgent, id, nullptr, origin, String(), listener)
    , m_element(WTFMove(element))
    , m_ruleSourceData(nullptr)
    , m_isStyleTextValid(false)
{
    m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id, 0), inlineStyle(), this);
    m_styleText = m_element->getAttribute("style").string();
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
        m_element->setAttribute(HTMLNames::styleAttr, text);
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
    return m_element->getAttribute("style").string();
}

Ref<CSSRuleSourceData> InspectorStyleSheetForInlineStyle::ruleSourceData() const
{
    if (m_styleText.isEmpty()) {
        auto result = CSSRuleSourceData::create(StyleRule::Style);
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
