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

#include "CSSMutableStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSRuleList.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "HTMLHeadElement.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "InspectorResource.h"
#include "PlatformString.h"
#include "ScriptObject.h"
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

CSSStyleSheet* InspectorCSSStore::inspectorStyleSheet(Document* ownerDocument, bool createIfAbsent, long callId)
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
    if (ec) {
        m_inspectorController->inspectorFrontend()->didAddRule(callId, ScriptValue::undefined(), false);
        return 0;
    }
    StyleSheetList* styleSheets = ownerDocument->styleSheets();
    StyleSheet* styleSheet = styleSheets->item(styleSheets->length() - 1);
    if (!styleSheet->isCSSStyleSheet()) {
        m_inspectorController->inspectorFrontend()->didAddRule(callId, ScriptValue::undefined(), false);
        return 0;
    }
    CSSStyleSheet* inspectorStyleSheet = static_cast<CSSStyleSheet*>(styleSheet);
    m_documentNodeToInspectorStyleSheetMap.set(ownerDocument, inspectorStyleSheet);
    return inspectorStyleSheet;
}

SourceRange InspectorCSSStore::getStartEndOffsets(CSSStyleRule* rule)
{
    CSSStyleSheet* parentStyleSheet = rule->parentStyleSheet();
    if (!parentStyleSheet)
        return std::pair<unsigned, unsigned>(0, 0);
    RefPtr<CSSRuleList> originalRuleList = CSSRuleList::create(parentStyleSheet, false);
    unsigned ruleIndex = getIndexInStyleRules(rule, originalRuleList.get());
    if (ruleIndex == UINT_MAX)
        return std::pair<unsigned, unsigned>(0, 0);
    StyleSheetToOffsetsMap::iterator it = m_styleSheetToOffsets.find(parentStyleSheet);
    Vector<SourceRange>* offsetVector = 0;
    if (it == m_styleSheetToOffsets.end()) {
        InspectorResource* resource = m_inspectorController->resourceForURL(parentStyleSheet->finalURL().string());
        if (resource) {
            offsetVector = new Vector<SourceRange>;
            RefPtr<CSSStyleSheet> newStyleSheet = CSSStyleSheet::create(parentStyleSheet->ownerNode());
            CSSParser p;
            p.parseSheet(newStyleSheet.get(), resource->sourceString(), offsetVector);
            m_styleSheetToOffsets.set(parentStyleSheet, offsetVector);
        }
    } else
        offsetVector = it->second;
    if (!offsetVector)
        return std::pair<unsigned, unsigned>(0, 0);
    ASSERT(offsetVector->size() > ruleIndex);
    return offsetVector->at(ruleIndex);
}

unsigned InspectorCSSStore::getIndexInStyleRules(CSSStyleRule* rule, CSSRuleList* ruleList)
{
    unsigned i = 0;
    unsigned ruleIndex = 0;
    unsigned originalRuleListLength = ruleList->length();
    while (i < originalRuleListLength) {
        if (ruleList->item(i) == rule)
            break;
        if (ruleList->item(i)->type() == CSSRule::STYLE_RULE)
            ++ruleIndex;
        ++i;
    }
    return (i == originalRuleListLength) ? UINT_MAX : ruleIndex;
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
