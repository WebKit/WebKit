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
#include "HTMLHeadElement.h"
#include "InspectorController.h"
#include "InspectorResource.h"
#include "PlatformString.h"
#include "StyleSheetList.h"

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

HashMap<long, SourceRange> InspectorCSSStore::getRuleRanges(CSSStyleSheet* styleSheet)
{
    if (!styleSheet)
        return HashMap<long, SourceRange>();
    RefPtr<CSSRuleList> originalRuleList = CSSRuleList::create(styleSheet, false);
    StyleSheetToOffsetsMap::iterator it = m_styleSheetToOffsets.find(styleSheet);
    Vector<SourceRange>* offsetVector = 0;
    if (it == m_styleSheetToOffsets.end()) {
        InspectorResource* resource = m_inspectorController->resourceForURL(styleSheet->finalURL().string());
        if (resource) {
            offsetVector = new Vector<SourceRange>;
            RefPtr<CSSStyleSheet> newStyleSheet = CSSStyleSheet::create(styleSheet->ownerNode());
            CSSParser p;
            CSSParser::StyleRuleRanges ruleRangeMap;
            p.parseSheet(newStyleSheet.get(), resource->sourceString(), 0, &ruleRangeMap);
            for (unsigned i = 0, length = newStyleSheet->length(); i < length; ++i) {
                CSSStyleRule* rule = asCSSStyleRule(newStyleSheet->item(i));
                if (!rule)
                    continue;
                HashMap<CSSStyleRule*, std::pair<unsigned, unsigned> >::iterator it = ruleRangeMap.find(rule);
                if (it != ruleRangeMap.end())
                    offsetVector->append(it->second);
            }
            m_styleSheetToOffsets.set(styleSheet, offsetVector);
        }
    } else
        offsetVector = it->second;
    if (!offsetVector)
        return HashMap<long, SourceRange>();

    unsigned ruleIndex = 0;
    HashMap<long, SourceRange> result;
    for (unsigned i = 0, length = styleSheet->length(); i < length; ++i) {
        ASSERT(ruleIndex < offsetVector->size());
        CSSStyleRule* rule = asCSSStyleRule(styleSheet->item(i));
        if (!rule)
            continue;
        // This maps the style id to the rule body range.
        result.set(bindStyle(rule->style()), offsetVector->at(ruleIndex));
        ruleIndex++;
    }
    return result;
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

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
