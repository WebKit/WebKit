/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorCSSStore.h"

#if ENABLE(INSPECTOR)

#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CachedResource.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLHeadElement.h"
#include "InspectorController.h"
#include "InspectorResourceAgent.h"
#include "Node.h"
#include "PlatformString.h"
#include "SharedBuffer.h"
#include "StyleSheetList.h"
#include "TextEncoding.h"

namespace WebCore {

InspectorCSSStore::InspectorCSSStore(InspectorController* inspectorController)
    : m_inspectorController(inspectorController)
    , m_lastStyleId(1)
    , m_lastStyleSheetId(1)
    , m_lastRuleId(1)
{
}

InspectorCSSStore::~InspectorCSSStore()
{
}

void InspectorCSSStore::reset()
{
    m_styleToId.clear();
    m_idToStyle.clear();
    m_ruleToId.clear();
    m_idToRule.clear();
    deleteAllValues(m_styleSheetToOffsets);
    m_styleSheetToOffsets.clear();
    m_styleSheetToId.clear();
    m_idToStyleSheet.clear();
    m_idToStyleSheetText.clear();
    m_idToDisabledStyle.clear();
    m_documentNodeToInspectorStyleSheetMap.clear();

    m_lastStyleId = 1;
    m_lastStyleSheetId = 1;
    m_lastRuleId = 1;
}

void InspectorCSSStore::removeDocument(Document* doc)
{
    m_documentNodeToInspectorStyleSheetMap.remove(doc);
}

CSSStyleSheet* InspectorCSSStore::inspectorStyleSheet(Document* ownerDocument, bool createIfAbsent)
{
    DocumentToStyleSheetMap::iterator it = m_documentNodeToInspectorStyleSheetMap.find(ownerDocument);
    if (it != m_documentNodeToInspectorStyleSheetMap.end())
        return it->second.get();
    if (!createIfAbsent)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Element> styleElement = ownerDocument->createElement("style", ec);
    if (!ec)
        styleElement->setAttribute("type", "text/css", ec);
    if (!ec)
        ownerDocument->head()->appendChild(styleElement, ec);
    if (ec)
        return 0;
    StyleSheetList* styleSheets = ownerDocument->styleSheets();
    StyleSheet* styleSheet = styleSheets->item(styleSheets->length() - 1);
    if (!styleSheet->isCSSStyleSheet())
        return 0;
    CSSStyleSheet* inspectorStyleSheet = static_cast<CSSStyleSheet*>(styleSheet);
    m_documentNodeToInspectorStyleSheetMap.set(ownerDocument, inspectorStyleSheet);
    return inspectorStyleSheet;
}

String InspectorCSSStore::styleSheetText(long styleSheetId)
{
    IdToStyleSheetTextMap::iterator it = m_idToStyleSheetText.find(styleSheetId);
    if (it != m_idToStyleSheetText.end())
        return it->second;

    CSSStyleSheet* styleSheet = styleSheetForId(styleSheetId);
    if (!styleSheet)
        return String();

    String result;
    bool success = false;
    Node* ownerNode = styleSheet->ownerNode();
    if (ownerNode && ownerNode->nodeType() == Node::ELEMENT_NODE) {
        Element* ownerElement = static_cast<Element*>(ownerNode);
        if (ownerElement->tagName().lower() == "style") {
            result = inlineStyleSheetText(styleSheet);
            success = true;
        }
    }
    if (!success)
        success = resourceStyleSheetText(styleSheet, &result);

    if (success)
        m_idToStyleSheetText.set(styleSheetId, result);
    return result;
}

bool InspectorCSSStore::resourceStyleSheetText(CSSStyleSheet* styleSheet, String* result)
{
    return InspectorResourceAgent::resourceContent(styleSheet->document()->frame(), styleSheet->finalURL(), result);
}

String InspectorCSSStore::inlineStyleSheetText(CSSStyleSheet* styleSheet)
{
    Node* ownerNode = styleSheet->ownerNode();
    if (!ownerNode || ownerNode->nodeType() != Node::ELEMENT_NODE || m_styleSheetToId.find(styleSheet) == m_styleSheetToId.end())
        return String();
    Element* ownerElement = static_cast<Element*>(ownerNode);
    if (ownerElement->tagName().lower() != "style")
        return String();
    return ownerElement->innerText();
}


// All ranges are: [start; end) (start - inclusive, end - exclusive).
bool InspectorCSSStore::getRuleSourceData(CSSStyleDeclaration* style, RefPtr<CSSRuleSourceData>* result)
{
    if (!style)
        return false;

    Element* element = inlineStyleElement(style);
    if (element) {
        // Inline: style="...".
        RefPtr<CSSRuleSourceData> ruleSourceData = CSSRuleSourceData::create();
        RefPtr<CSSStyleSourceData> styleSourceData = CSSStyleSourceData::create();
        bool success = getStyleAttributeRanges(element, &styleSourceData);
        if (!success)
            return false;
        ruleSourceData->styleSourceData = styleSourceData.release();
        *result = ruleSourceData;
        return true;
    }

    CSSStyleSheet* styleSheet = getParentStyleSheet(style);
    if (!styleSheet)
        return false;

    Vector<RefPtr<CSSRuleSourceData> >* rangesVector = 0;
    StyleSheetToOffsetsMap::iterator it = m_styleSheetToOffsets.find(styleSheet);
    if (it == m_styleSheetToOffsets.end()) {
        String text = styleSheetText(bindStyleSheet(styleSheet));
        if (!text.isEmpty()) {
            RefPtr<CSSStyleSheet> newStyleSheet = CSSStyleSheet::create(styleSheet->ownerNode());
            CSSParser p;
            StyleRuleRangeMap ruleRangeMap;
            p.parseSheet(newStyleSheet.get(), text, 0, &ruleRangeMap);
            rangesVector = new Vector<RefPtr<CSSRuleSourceData> >;
            extractRanges(newStyleSheet.get(), ruleRangeMap, rangesVector);
            m_styleSheetToOffsets.set(styleSheet, rangesVector);
        }
    } else
        rangesVector = it->second;
    if (!rangesVector)
        return false;

    unsigned ruleIndex = 0;
    for (unsigned i = 0, length = styleSheet->length(); i < length; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(styleSheet->item(i));
        if (!rule)
            continue;
        if (rule->style() == style) {
            ASSERT(ruleIndex < rangesVector->size());
            *result = rangesVector->at(ruleIndex);
            return true;
        }
        ruleIndex++;
    }
    return false;
}

void InspectorCSSStore::extractRanges(CSSStyleSheet* styleSheet, const StyleRuleRangeMap& ruleRangeMap, Vector<RefPtr<CSSRuleSourceData> >* rangesVector)
{
    for (unsigned i = 0, length = styleSheet->length(); i < length; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(styleSheet->item(i));
        if (!rule)
            continue;
        StyleRuleRangeMap::const_iterator it = ruleRangeMap.find(rule);
        if (it != ruleRangeMap.end())
            rangesVector->append(it->second);
    }
}

bool InspectorCSSStore::getStyleAttributeRanges(Node* node, RefPtr<CSSStyleSourceData>* result)
{
    if (!node || !node->isStyledElement())
        return false;

    String styleText = static_cast<StyledElement*>(node)->getAttribute("style");
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

CSSStyleSheet* InspectorCSSStore::getParentStyleSheet(CSSStyleDeclaration* style)
{
    if (!style)
        return 0;

    StyleSheet* styleSheet = style->stylesheet();
    if (styleSheet && styleSheet->isCSSStyleSheet())
        return static_cast<CSSStyleSheet*>(styleSheet);

    return 0;
}

CSSStyleRule* InspectorCSSStore::asCSSStyleRule(StyleBase* styleBase)
{
    if (!styleBase->isStyleRule())
        return 0;
    CSSRule* rule = static_cast<CSSRule*>(styleBase);
    if (rule->type() != CSSRule::STYLE_RULE)
        return 0;
    return static_cast<CSSStyleRule*>(rule);
}

DisabledStyleDeclaration* InspectorCSSStore::disabledStyleForId(long styleId, bool createIfAbsent)
{
    IdToDisabledStyleMap::iterator it = m_idToDisabledStyle.find(styleId);
    if (it == m_idToDisabledStyle.end() && createIfAbsent)
        it = m_idToDisabledStyle.set(styleId, DisabledStyleDeclaration()).first;
    return it == m_idToDisabledStyle.end() ? 0 : &(it->second);
}

CSSStyleDeclaration* InspectorCSSStore::styleForId(long styleId)
{
    IdToStyleMap::iterator it = m_idToStyle.find(styleId);
    return it == m_idToStyle.end() ? 0 : it->second.get();
}

CSSStyleSheet* InspectorCSSStore::styleSheetForId(long styleSheetId)
{
    IdToStyleSheetMap::iterator it = m_idToStyleSheet.find(styleSheetId);
    return it == m_idToStyleSheet.end() ? 0 : it->second.get();
}

CSSStyleRule* InspectorCSSStore::ruleForId(long ruleId)
{
    IdToRuleMap::iterator it = m_idToRule.find(ruleId);
    return it == m_idToRule.end() ? 0 : it->second.get();
}

long InspectorCSSStore::bindStyle(CSSStyleDeclaration* style)
{
    long id = m_styleToId.get(style);
    if (!id) {
        id = m_lastStyleId++;
        m_idToStyle.set(id, style);
        m_styleToId.set(style, id);
    }
    return id;
}

long InspectorCSSStore::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    long id = m_styleSheetToId.get(styleSheet);
    if (!id) {
        id = m_lastStyleSheetId++;
        m_idToStyleSheet.set(id, styleSheet);
        m_styleSheetToId.set(styleSheet, id);
    }
    return id;
}

long InspectorCSSStore::bindRule(CSSStyleRule* rule)
{
    long id = m_ruleToId.get(rule);
    if (!id) {
        id = m_lastRuleId++;
        m_idToRule.set(id, rule);
        m_ruleToId.set(rule, id);
    }
    return id;
}

// static
Element* InspectorCSSStore::inlineStyleElement(CSSStyleDeclaration* style)
{
    if (!style || !style->isMutableStyleDeclaration())
        return 0;
    Node* node = static_cast<CSSMutableStyleDeclaration*>(style)->node();
    if (!node || !node->isStyledElement() || static_cast<StyledElement*>(node)->getInlineStyleDecl() != style)
        return 0;
    return static_cast<Element*>(node);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
