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

#if ENABLE(INSPECTOR)

#include "CSSParser.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "Document.h"
#include "Element.h"
#include "HTMLHeadElement.h"
#include "InspectorCSSAgent.h"
#include "InspectorUtilities.h"
#include "InspectorValues.h"
#include "Node.h"
#include "StyleSheetList.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

class ParsedStyleSheet {
public:
    typedef Vector<RefPtr<WebCore::CSSRuleSourceData> > SourceData;
    ParsedStyleSheet();

    WebCore::CSSStyleSheet* cssStyleSheet() const { return m_parserOutput; }
    const String& text() const { return m_text; }
    void setText(const String& text);
    bool hasText() const { return m_hasText; }
    SourceData* sourceData() const { return m_sourceData.get(); }
    void setSourceData(PassOwnPtr<SourceData> sourceData);
    bool hasSourceData() const { return m_sourceData; }
    RefPtr<WebCore::CSSRuleSourceData> ruleSourceDataAt(unsigned index) const;

private:

    // StyleSheet constructed while parsing m_text.
    WebCore::CSSStyleSheet* m_parserOutput;
    String m_text;
    bool m_hasText;
    OwnPtr<SourceData> m_sourceData;
};

ParsedStyleSheet::ParsedStyleSheet()
    : m_parserOutput(0)
    , m_hasText(false)
{
}

void ParsedStyleSheet::setText(const String& text)
{
    m_hasText = true;
    m_text = text;
    setSourceData(0);
}

void ParsedStyleSheet::setSourceData(PassOwnPtr<SourceData> sourceData)
{
    m_sourceData = sourceData;
}

RefPtr<WebCore::CSSRuleSourceData> ParsedStyleSheet::ruleSourceDataAt(unsigned index) const
{
    if (!hasSourceData() || index >= m_sourceData->size())
        return 0;

    return m_sourceData->at(index);
}

namespace WebCore {

InspectorStyleSheet::InspectorStyleSheet(const String& id, CSSStyleSheet* pageStyleSheet, const String& origin, const String& documentURL)
    : m_id(id)
    , m_pageStyleSheet(pageStyleSheet)
    , m_origin(origin)
    , m_documentURL(documentURL)
    , m_isRevalidating(false)
{
    m_parsedStyleSheet = new ParsedStyleSheet();
}

InspectorStyleSheet::~InspectorStyleSheet()
{
    delete m_parsedStyleSheet;
}

bool InspectorStyleSheet::setText(const String& text)
{
    if (!m_parsedStyleSheet)
        return false;

    m_parsedStyleSheet->setText(text);
    for (unsigned i = 0, size = m_pageStyleSheet->length(); i < size; ++i)
        m_pageStyleSheet->remove(i);

    m_pageStyleSheet->parseString(text, m_pageStyleSheet->useStrictParsing());
    return true;
}

bool InspectorStyleSheet::setRuleSelector(const String& ruleId, const String& selector)
{
    CSSStyleRule* rule = ruleForId(ruleId);
    if (!rule)
        return false;
    CSSStyleSheet* styleSheet = InspectorCSSAgent::parentStyleSheet(rule);
    if (!styleSheet || !ensureParsedDataReady())
        return false;

    rule->setSelectorText(selector);
    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(rule->style());
    if (!sourceData)
        return false;

    const String& sheetText = m_parsedStyleSheet->text();
    String newStyleSheetText = sheetText.substring(0, sourceData->selectorListRange.start);
    newStyleSheetText += selector;
    newStyleSheetText += sheetText.right(sheetText.length() - sourceData->selectorListRange.end);
    m_parsedStyleSheet->setText(newStyleSheetText);
    return true;
}

CSSStyleRule* InspectorStyleSheet::addRule(const String& selector)
{
    String text;
    bool success = styleSheetText(&text);
    if (!success)
        return 0;

    ExceptionCode ec = 0;
    m_pageStyleSheet->addRule(selector, "", ec);
    if (ec)
        return 0;
    RefPtr<CSSRuleList> rules = m_pageStyleSheet->cssRules();
    ASSERT(rules->length());
    CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(rules->item(rules->length() - 1));
    ASSERT(rule);

    if (text.length())
        text += "\n";

    text += selector;
    text += " {}";
    m_parsedStyleSheet->setText(text);

    return rule;
}

CSSStyleRule* InspectorStyleSheet::ruleForId(const String& id) const
{
    if (!m_pageStyleSheet)
        return 0;

    bool ok;
    unsigned index = id.toUInt(&ok);
    if (!ok)
        return 0;

    unsigned currentIndex = 0;
    for (unsigned i = 0, size = m_pageStyleSheet->length(); i < size; ++i) {
        CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(m_pageStyleSheet->item(i));
        if (!rule)
            continue;
        if (index == currentIndex)
            return rule;

        ++currentIndex;
    }
    return 0;
}

PassRefPtr<InspectorObject> InspectorStyleSheet::buildObjectForStyleSheet()
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return 0;

    RefPtr<InspectorObject> result = InspectorObject::create();
    result->setString("id", id());
    result->setBoolean("disabled", styleSheet->disabled());
    result->setString("href", styleSheet->href());
    result->setString("title", styleSheet->title());
    RefPtr<CSSRuleList> cssRuleList = CSSRuleList::create(styleSheet, true);
    RefPtr<InspectorArray> cssRules = buildArrayForRuleList(cssRuleList.get());
    result->setArray("cssRules", cssRules.release());

    String styleSheetText;
    bool success = text(&styleSheetText);
    if (success)
        result->setString("text", styleSheetText);

    return result.release();
}

PassRefPtr<InspectorObject> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return 0;

    RefPtr<InspectorObject> result = InspectorObject::create();
    result->setString("selectorText", rule->selectorText());
    result->setString("cssText", rule->cssText());
    result->setNumber("sourceLine", rule->sourceLine());
    result->setString("documentURL", m_documentURL);

    RefPtr<InspectorObject> parentStyleSheetValue = InspectorObject::create();
    parentStyleSheetValue->setString("id", id());
    parentStyleSheetValue->setString("href", styleSheet->href());
    result->setObject("parentStyleSheet", parentStyleSheetValue.release());
    result->setString("origin", m_origin);

    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(rule->style());
    if (sourceData) {
        result->setNumber("selectorStartOffset", sourceData->selectorListRange.start);
        result->setNumber("selectorEndOffset", sourceData->selectorListRange.end);
    }

    result->setObject("style", buildObjectForStyle(rule->style()));
    if (canBind())
        result->setString("id", fullRuleId(rule));

    return result.release();
}

PassRefPtr<InspectorObject> InspectorStyleSheet::buildObjectForStyle(CSSStyleDeclaration* style)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(style);

    RefPtr<InspectorObject> result = InspectorCSSAgent::buildObjectForStyle(style, fullStyleId(style), sourceData ? sourceData->styleSourceData.get() : 0);
    result->setString("parentStyleSheetId", id());
    return result.release();
}

CSSStyleDeclaration* InspectorStyleSheet::styleForId(const String& id) const
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule)
        return 0;

    return rule->style();
}

bool InspectorStyleSheet::setStyleText(const String& styleId, const String& newText)
{
    CSSStyleDeclaration* style = styleForId(styleId);
    if (!style)
        return false;

    return innerSetStyleText(style, newText);
}

Document* InspectorStyleSheet::ownerDocument() const
{
    return m_pageStyleSheet->document();
}

RefPtr<CSSRuleSourceData> InspectorStyleSheet::ruleSourceDataFor(CSSStyleDeclaration* style) const
{
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByStyle(style));
}

unsigned InspectorStyleSheet::ruleIndexByStyle(CSSStyleDeclaration* pageStyle) const
{
    unsigned index = 0;
    for (unsigned i = 0, size = m_pageStyleSheet->length(); i < size; ++i) {
        CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(m_pageStyleSheet->item(i));
        if (!rule)
            continue;
        if (rule->style() == pageStyle)
            return index;

        ++index;
    }
    return UINT_MAX;
}

bool InspectorStyleSheet::ensureParsedDataReady()
{
    return ensureText() && ensureSourceData(pageStyleSheet()->ownerNode());
}

bool InspectorStyleSheet::text(String* result) const
{
    if (!ensureText())
        return false;
    *result = m_parsedStyleSheet->text();
    return true;
}

bool InspectorStyleSheet::ensureText() const
{
    if (!m_parsedStyleSheet)
        return false;
    if (m_parsedStyleSheet->hasText())
        return true;

    String text;
    bool success = styleSheetText(&text);
    if (success)
        m_parsedStyleSheet->setText(text);

    return success;
}

bool InspectorStyleSheet::ensureSourceData(Node* ownerNode)
{
    if (m_parsedStyleSheet->hasSourceData())
        return true;

    if (!m_parsedStyleSheet->hasText())
        return false;

    RefPtr<CSSStyleSheet> newStyleSheet = CSSStyleSheet::create(ownerNode);
    CSSParser p;
    StyleRuleRangeMap ruleRangeMap;
    p.parseSheet(newStyleSheet.get(), m_parsedStyleSheet->text(), 0, &ruleRangeMap);
    OwnPtr<ParsedStyleSheet::SourceData> rangesVector(new ParsedStyleSheet::SourceData());

    for (unsigned i = 0, length = newStyleSheet->length(); i < length; ++i) {
        CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(newStyleSheet->item(i));
        if (!rule)
            continue;
        StyleRuleRangeMap::iterator it = ruleRangeMap.find(rule);
        if (it != ruleRangeMap.end())
            rangesVector->append(it->second);
    }

    m_parsedStyleSheet->setSourceData(rangesVector.release());
    return m_parsedStyleSheet->hasSourceData();
}

void InspectorStyleSheet::innerSetStyleSheetText(const String& newText)
{
    m_parsedStyleSheet->setText(newText);
}

bool InspectorStyleSheet::innerSetStyleText(CSSStyleDeclaration* style, const String& text)
{
    if (!pageStyleSheet())
        return false;
    if (!ensureParsedDataReady())
        return false;

    String patchedStyleSheetText;
    bool success = styleSheetTextWithChangedStyle(style, text, &patchedStyleSheetText);
    if (!success)
        return false;

    String id = ruleOrStyleId(style);
    if (id.isEmpty())
        return false;

    ExceptionCode ec = 0;
    style->setCssText(text, ec);
    if (!ec)
        innerSetStyleSheetText(patchedStyleSheetText);

    return !ec;
}

bool InspectorStyleSheet::styleSheetTextWithChangedStyle(CSSStyleDeclaration* style, const String& newStyleText, String* result)
{
    if (!style)
        return false;

    if (!ensureParsedDataReady())
        return false;

    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(style);
    unsigned bodyStart = sourceData->styleSourceData->styleBodyRange.start;
    unsigned bodyEnd = sourceData->styleSourceData->styleBodyRange.end;
    ASSERT(bodyStart <= bodyEnd);

    String text = m_parsedStyleSheet->text();
    ASSERT(bodyEnd <= text.length()); // bodyEnd is exclusive

    String patchedText = text.substring(0, bodyStart);
    patchedText += newStyleText;
    patchedText += text.substring(bodyEnd, text.length());
    *result = patchedText;
    return true;
}

CSSStyleRule* InspectorStyleSheet::findPageRuleWithStyle(CSSStyleDeclaration* style)
{
    for (unsigned i = 0, size = m_pageStyleSheet->length(); i < size; ++i) {
        CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(m_pageStyleSheet->item(i));
        if (!rule)
            continue;
        if (rule->style() == style)
            return rule;
    }
    return 0;
}

String InspectorStyleSheet::fullRuleId(CSSStyleRule* rule) const
{
    return fullRuleOrStyleId(rule->style());
}

void InspectorStyleSheet::revalidateStyle(CSSStyleDeclaration* pageStyle)
{
    if (m_isRevalidating)
        return;

    m_isRevalidating = true;
    CSSStyleSheet* parsedSheet = m_parsedStyleSheet->cssStyleSheet();
    for (unsigned i = 0, size = parsedSheet->length(); i < size; ++i) {
        StyleBase* styleBase = parsedSheet->item(i);
        CSSStyleRule* parsedRule = InspectorCSSAgent::asCSSStyleRule(styleBase);
        if (!parsedRule)
            continue;
        if (parsedRule->style() == pageStyle) {
            if (parsedRule->style()->cssText() != pageStyle->cssText())
                innerSetStyleText(pageStyle, pageStyle->cssText());
            break;
        }
    }
    m_isRevalidating = false;
}

bool InspectorStyleSheet::styleSheetText(String* result) const
{
    String text;
    bool success = inlineStyleSheetText(&text);
    if (!success)
        success = resourceStyleSheetText(&text);
    if (success)
        *result = text;
    return success;
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet)
        return false;

    return InspectorUtilities::resourceContentForURL(m_pageStyleSheet->finalURL(), ownerDocument(), result);
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet)
        return false;

    Node* ownerNode = m_pageStyleSheet->ownerNode();
    if (!ownerNode || ownerNode->nodeType() != Node::ELEMENT_NODE)
        return false;
    Element* ownerElement = static_cast<Element*>(ownerNode);
    if (ownerElement->tagName().lower() != "style")
        return false;
    *result = ownerElement->innerText();
    return true;
}

PassRefPtr<InspectorArray> InspectorStyleSheet::buildArrayForRuleList(CSSRuleList* ruleList)
{
    RefPtr<InspectorArray> result = InspectorArray::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = InspectorCSSAgent::asCSSStyleRule(ruleList->item(i));
        if (!rule)
            continue;

        result->pushObject(buildObjectForRule(rule));
    }
    return result.release();
}


InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(const String& id, Element* element, const String& origin)
    : InspectorStyleSheet(id, 0, origin, "")
    , m_element(element)
    , m_ruleSourceData(0)
{
    ASSERT(element);
}

bool InspectorStyleSheetForInlineStyle::setStyleText(const String& styleId, const String& text)
{
    ASSERT(styleId == "0");
    ExceptionCode ec = 0;
    m_element->setAttribute("style", text, ec);
    m_ruleSourceData.clear();
    return !ec;
}

Document* InspectorStyleSheetForInlineStyle::ownerDocument() const
{
    return m_element->document();
}

bool InspectorStyleSheetForInlineStyle::ensureParsedDataReady()
{
    if (m_ruleSourceData)
        return true;

    m_ruleSourceData = CSSRuleSourceData::create();
    RefPtr<CSSStyleSourceData> sourceData = CSSStyleSourceData::create();
    bool success = getStyleAttributeRanges(&sourceData);
    if (!success)
        return false;

    m_ruleSourceData->styleSourceData = sourceData.release();
    return true;
}

CSSStyleDeclaration* InspectorStyleSheetForInlineStyle::inlineStyle() const
{
    return m_element->style();
}

bool InspectorStyleSheetForInlineStyle::getStyleAttributeRanges(RefPtr<CSSStyleSourceData>* result)
{
    DEFINE_STATIC_LOCAL(String, styleAttributeName, ("style"));

    if (!m_element->isStyledElement())
        return false;

    String styleText = static_cast<StyledElement*>(m_element)->getAttribute(styleAttributeName);
    if (styleText.isEmpty()) {
        (*result)->styleBodyRange.start = 0;
        (*result)->styleBodyRange.end = 0;
        return true;
    }

    RefPtr<CSSMutableStyleDeclaration> tempDeclaration = CSSMutableStyleDeclaration::create();
    CSSParser p;
    p.parseDeclaration(tempDeclaration.get(), styleText, result);
    return true;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
