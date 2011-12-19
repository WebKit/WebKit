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
#include "InspectorCSSAgent.h"

#if ENABLE(INSPECTOR)

#include "CSSComputedStyleDeclaration.h"
#include "CSSImportRule.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "DOMWindow.h"
#include "HTMLHeadElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Node.h"
#include "NodeList.h"
#include "StyleSheetList.h"

#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

// Currently implemented model:
//
// cssProperty = {
//    name          : <string>,
//    value         : <string>,
//    priority      : <string>, // "" for non-parsedOk properties
//    implicit      : <boolean>,
//    parsedOk      : <boolean>, // whether property is understood by WebCore
//    status        : <string>, // "disabled" | "active" | "inactive" | "style"
//    shorthandName : <string>,
//    startOffset   : <number>, // Optional - property text start offset in enclosing style declaration. Absent for computed styles and such.
//    endOffset     : <number>, // Optional - property text end offset in enclosing style declaration. Absent for computed styles and such.
// }
//
// name + value + priority : present when the property is enabled
// text                    : present when the property is disabled
//
// For disabled properties, startOffset === endOffset === insertion point for the property.
//
// status:
// "disabled" == property disabled by user
// "active" == property participates in the computed style calculation
// "inactive" == property does no participate in the computed style calculation (i.e. overridden by a subsequent property with the same name)
// "style" == property is active and originates from the WebCore CSSStyleDeclaration rather than CSS source code (e.g. implicit longhand properties)
//
// cssStyle = {
//    styleId            : <string>, // Optional
//    cssProperties      : [
//                          #cssProperty,
//                          ...
//                          #cssProperty
//                         ],
//    shorthandEntries   : [
//                          #shorthandEntry,
//                          ...
//                          #shorthandEntry
//                         ],
//    cssText            : <string>, // Optional - declaration text
//    properties         : {
//                          width,
//                          height,
//                          startOffset, // Optional - for source-based styles only
//                          endOffset, // Optional - for source-based styles only
//                         }
// }
//
// shorthandEntry = {
//    name: <string>,
//    value: <string>
// }
//
// cssRule = {
//    ruleId       : <string>, // Optional
//    selectorText : <string>,
//    sourceURL    : <string>,
//    sourceLine   : <string>,
//    origin       : <string>, // "" || "user-agent" || "user" || "inspector"
//    style        : #cssStyle,
//    selectorRange: { start: <number>, end: <number> } // Optional - for source-based rules only
// }
//
// cssStyleSheetInfo = {
//    styleSheetId : <number>
//    sourceURL    : <string>
//    title        : <string>
//    disabled     : <boolean>
// }
//
// cssStyleSheet = {
//    styleSheetId : <number>
//    rules        : [
//                       #cssRule,
//                       ...
//                       #cssRule
//                   ]
//    text         : <string> // Optional - whenever the text is available for a text-based stylesheet
// }

namespace CSSAgentState {
static const char cssAgentEnabled[] = "cssAgentEnabled";
static const char isSelectorProfiling[] = "isSelectorProfiling";
}

namespace WebCore {

enum ForcePseudoClassFlags {
    PseudoNone = 0,
    PseudoHover = 1 << 0,
    PseudoFocus = 1 << 1,
    PseudoActive = 1 << 2,
    PseudoVisited = 1 << 3
};

struct RuleMatchingStats {
    RuleMatchingStats()
        : totalTime(0.0), hits(0), matches(0)
    {
    }
    RuleMatchingStats(double totalTime, unsigned hits, unsigned matches)
        : totalTime(totalTime), hits(hits), matches(matches)
    {
    }
    double totalTime;
    unsigned hits;
    unsigned matches;
};

class SelectorProfile {
public:
    // FIXME: handle different rules with the same selector differently?
    SelectorProfile()
        : m_totalMatchingTimeMs(0.0)
    {
    }
    virtual ~SelectorProfile()
    {
    }

    double totalMatchingTimeMs() const { return m_totalMatchingTimeMs; }

    void startSelector(const String&);
    void commitSelector(bool);
    void commitSelectorTime();
    PassRefPtr<InspectorObject> toInspectorObject() const;

private:
    typedef HashMap<String, RuleMatchingStats> RuleMatchingStatsMap;
    struct RuleMatchData {
        String selector;
        double startTime;
    };

    double m_totalMatchingTimeMs;
    RuleMatchingStatsMap m_ruleMatchingStats;
    RuleMatchData m_currentMatchData;
};


static unsigned computePseudoClassMask(InspectorArray* pseudoClassArray)
{
    DEFINE_STATIC_LOCAL(String, active, ("active"));
    DEFINE_STATIC_LOCAL(String, hover, ("hover"));
    DEFINE_STATIC_LOCAL(String, focus, ("focus"));
    DEFINE_STATIC_LOCAL(String, visited, ("visited"));
    if (!pseudoClassArray || !pseudoClassArray->length())
        return PseudoNone;

    unsigned result = PseudoNone;
    for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
        RefPtr<InspectorValue> pseudoClassValue = pseudoClassArray->get(i);
        String pseudoClass;
        bool success = pseudoClassValue->asString(&pseudoClass);
        if (!success)
            continue;
        if (pseudoClass == active)
            result |= PseudoActive;
        else if (pseudoClass == hover)
            result |= PseudoHover;
        else if (pseudoClass == focus)
            result |= PseudoFocus;
        else if (pseudoClass == visited)
            result |= PseudoVisited;
    }

    return result;
}

inline void SelectorProfile::startSelector(const String& selectorText)
{
    m_currentMatchData.selector = selectorText;
    m_currentMatchData.startTime = WTF::currentTimeMS();
}

inline void SelectorProfile::commitSelector(bool matched)
{
    double matchTimeMs = WTF::currentTimeMS() - m_currentMatchData.startTime;
    m_totalMatchingTimeMs += matchTimeMs;
    pair<RuleMatchingStatsMap::iterator, bool> result = m_ruleMatchingStats.add(m_currentMatchData.selector, RuleMatchingStats(matchTimeMs, 1, matched ? 1 : 0));
    if (!result.second) {
        result.first->second.totalTime += matchTimeMs;
        result.first->second.hits += 1;
        if (matched)
            result.first->second.matches += 1;
    }
}

inline void SelectorProfile::commitSelectorTime()
{
    double processingTimeMs = WTF::currentTimeMS() - m_currentMatchData.startTime;
    m_totalMatchingTimeMs += processingTimeMs;
    RuleMatchingStatsMap::iterator it = m_ruleMatchingStats.find(m_currentMatchData.selector);
    if (it == m_ruleMatchingStats.end())
        return;

    it->second.totalTime += WTF::currentTimeMS() - m_currentMatchData.startTime;
}

PassRefPtr<InspectorObject> SelectorProfile::toInspectorObject() const
{
    RefPtr<InspectorArray> data = InspectorArray::create();
    for (RuleMatchingStatsMap::const_iterator it = m_ruleMatchingStats.begin(); it != m_ruleMatchingStats.end(); ++it) {
        RefPtr<TypeBuilder::CSS::SelectorProfileEntry> stat = TypeBuilder::CSS::SelectorProfileEntry::create()
            .setSelector(it->first)
            .setTime(it->second.totalTime)
            .setHitCount(it->second.hits)
            .setMatchCount(it->second.matches);
        data->pushObject(stat.release());
    }

    RefPtr<TypeBuilder::CSS::SelectorProfile> result = TypeBuilder::CSS::SelectorProfile::create()
        .setTotalTime(totalMatchingTimeMs())
        .setData(data);
    return result.release();
}

// static
CSSStyleSheet* InspectorCSSAgent::parentStyleSheet(CSSRule* rule)
{
    if (!rule)
        return 0;

    return rule->parentStyleSheet();
}

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule)
{
    if (!rule->isStyleRule())
        return 0;
    return static_cast<CSSStyleRule*>(rule);
}

InspectorCSSAgent::InspectorCSSAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, InspectorDOMAgent* domAgent)
    : InspectorBaseAgent<InspectorCSSAgent>("CSS", instrumentingAgents, state)
    , m_frontend(0)
    , m_domAgent(domAgent)
    , m_lastPseudoState(0)
    , m_lastStyleSheetId(1)
    , m_lastRuleId(1)
    , m_lastStyleId(1)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    ASSERT(!m_domAgent);
    m_instrumentingAgents->setInspectorCSSAgent(0);
    reset();
}

void InspectorCSSAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->css();
}

void InspectorCSSAgent::clearFrontend()
{
    ASSERT(m_frontend);
    m_frontend = 0;
    clearPseudoState(true);
    String errorString;
    stopSelectorProfiler(&errorString);
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(0);
    m_domAgent = 0;
}

void InspectorCSSAgent::restore()
{
    if (m_state->getBoolean(CSSAgentState::cssAgentEnabled)) {
        ErrorString error;
        enable(&error);
    }
    if (m_state->getBoolean(CSSAgentState::isSelectorProfiling)) {
        String errorString;
        startSelectorProfiler(&errorString);
    }
}

void InspectorCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
}

void InspectorCSSAgent::enable(ErrorString*)
{
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, true);
    m_instrumentingAgents->setInspectorCSSAgent(this);
}

void InspectorCSSAgent::disable(ErrorString*)
{
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, false);
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    if (m_frontend)
        m_frontend->mediaQueryResultChanged();
}

bool InspectorCSSAgent::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoType)
{
    if (m_lastElementWithPseudoState != element)
        return false;

    switch (pseudoType) {
    case CSSSelector::PseudoActive:
        return m_lastPseudoState & PseudoActive;
    case CSSSelector::PseudoFocus:
        return m_lastPseudoState & PseudoFocus;
    case CSSSelector::PseudoHover:
        return m_lastPseudoState & PseudoHover;
    case CSSSelector::PseudoVisited:
        return m_lastPseudoState & PseudoVisited;
    default:
        return false;
    }
}

void InspectorCSSAgent::recalcStyleForPseudoStateIfNeeded(Element* element, InspectorArray* forcedPseudoClasses)
{
    unsigned forcePseudoState = computePseudoClassMask(forcedPseudoClasses);
    bool needStyleRecalc = element != m_lastElementWithPseudoState || forcePseudoState != m_lastPseudoState;
    m_lastPseudoState = forcePseudoState;
    m_lastElementWithPseudoState = element;
    if (needStyleRecalc)
        element->ownerDocument()->styleSelectorChanged(RecalcStyleImmediately);
}

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString* errorString, int nodeId, const RefPtr<InspectorArray>* forcedPseudoClasses, bool* needPseudo, bool* needInherited, RefPtr<InspectorArray>* matchedCSSRules, RefPtr<InspectorArray>* pseudoIdRules, RefPtr<InspectorArray>* inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    recalcStyleForPseudoStateIfNeeded(element, forcedPseudoClasses ? forcedPseudoClasses->get() : 0);

    // Matched rules.
    CSSStyleSelector* selector = element->ownerDocument()->styleSelector();
    RefPtr<CSSRuleList> matchedRules = selector->styleRulesForElement(element, CSSStyleSelector::AllCSSRules);
    *matchedCSSRules = buildArrayForRuleList(matchedRules.get());

    // Pseudo elements.
    if (!needPseudo || *needPseudo) {
        RefPtr<InspectorArray> pseudoElements = InspectorArray::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            RefPtr<CSSRuleList> matchedRules = selector->pseudoStyleRulesForElement(element, pseudoId, CSSStyleSelector::AllCSSRules);
            if (matchedRules && matchedRules->length()) {
                RefPtr<InspectorObject> pseudoStyles = InspectorObject::create();
                pseudoStyles->setNumber("pseudoId", static_cast<int>(pseudoId));
                pseudoStyles->setArray("rules", buildArrayForRuleList(matchedRules.get()));
                pseudoElements->pushObject(pseudoStyles.release());
            }
        }

        *pseudoIdRules = pseudoElements.release();
    }

    // Inherited styles.
    if (!needInherited || *needInherited) {
        RefPtr<InspectorArray> inheritedStyles = InspectorArray::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            RefPtr<InspectorObject> parentStyle = InspectorObject::create();
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    parentStyle->setObject("inlineStyle", styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            CSSStyleSelector* parentSelector = parentElement->ownerDocument()->styleSelector();
            RefPtr<CSSRuleList> parentMatchedRules = parentSelector->styleRulesForElement(parentElement, CSSStyleSelector::AllCSSRules);
            parentStyle->setArray("matchedCSSRules", buildArrayForRuleList(parentMatchedRules.get()));
            inheritedStyles->pushObject(parentStyle.release());
            parentElement = parentElement->parentElement();
        }

        *inheritedEntries = inheritedStyles.release();
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString* errorString, int nodeId, RefPtr<InspectorObject>* inlineStyle, RefPtr<InspectorArray>* attributes)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    *inlineStyle = styleSheet->buildObjectForStyle(element->style());
    *attributes = buildArrayForAttributeStyles(element);
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString* errorString, int nodeId, const RefPtr<InspectorArray>* forcedPseudoClasses, RefPtr<InspectorArray>* style)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    recalcStyleForPseudoStateIfNeeded(element, forcedPseudoClasses ? forcedPseudoClasses->get() : 0);

    RefPtr<CSSComputedStyleDeclaration> computedStyleInfo = computedStyle(element, true);
    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, 0);
    *style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::getAllStyleSheets(ErrorString*, RefPtr<InspectorArray>* styleInfos)
{
    Vector<Document*> documents = m_domAgent->documents();
    for (Vector<Document*>::iterator it = documents.begin(); it != documents.end(); ++it) {
        StyleSheetList* list = (*it)->styleSheets();
        for (unsigned i = 0; i < list->length(); ++i) {
            StyleSheet* styleSheet = list->item(i);
            if (styleSheet->isCSSStyleSheet())
                collectStyleSheets(static_cast<CSSStyleSheet*>(styleSheet), styleInfos->get());
        }
    }
}

void InspectorCSSAgent::getStyleSheet(ErrorString* errorString, const String& styleSheetId, RefPtr<InspectorObject>* styleSheetObject)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    *styleSheetObject = inspectorStyleSheet->buildObjectForStyleSheet();
}

void InspectorCSSAgent::getStyleSheetText(ErrorString* errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->text(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString* errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    if (inspectorStyleSheet->setText(text))
        inspectorStyleSheet->reparseStyleSheet(text);
    else
        *errorString = "Internal error setting style sheet text";
}

void InspectorCSSAgent::setPropertyText(ErrorString* errorString, const RefPtr<InspectorObject>& fullStyleId, int propertyIndex, const String& text, bool overwrite, RefPtr<InspectorObject>* result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    bool success = inspectorStyleSheet->setPropertyText(errorString, compoundId, propertyIndex, text, overwrite);
    if (success)
        *result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
}

void InspectorCSSAgent::toggleProperty(ErrorString* errorString, const RefPtr<InspectorObject>& fullStyleId, int propertyIndex, bool disable, RefPtr<InspectorObject>* result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    bool success = inspectorStyleSheet->toggleProperty(errorString, compoundId, propertyIndex, disable);
    if (success)
        *result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
}

void InspectorCSSAgent::setRuleSelector(ErrorString* errorString, const RefPtr<InspectorObject>& fullRuleId, const String& selector, RefPtr<InspectorObject>* result)
{
    InspectorCSSId compoundId(fullRuleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    bool success = inspectorStyleSheet->setRuleSelector(compoundId, selector);
    if (!success)
        return;

    *result = inspectorStyleSheet->buildObjectForRule(inspectorStyleSheet->ruleForId(compoundId));
}

void InspectorCSSAgent::addRule(ErrorString*, const int contextNodeId, const String& selector, RefPtr<InspectorObject>* result)
{
    Node* node = m_domAgent->nodeForId(contextNodeId);
    if (!node)
        return;

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(node->document(), true);
    if (!inspectorStyleSheet)
        return;
    CSSStyleRule* newRule = inspectorStyleSheet->addRule(selector);
    if (!newRule)
        return;

    *result = inspectorStyleSheet->buildObjectForRule(newRule);
}

void InspectorCSSAgent::getSupportedCSSProperties(ErrorString*, RefPtr<InspectorArray>* cssProperties)
{
    RefPtr<InspectorArray> properties = InspectorArray::create();
    for (int i = 0; i < numCSSProperties; ++i)
        properties->pushString(propertyNameStrings[i]);

    *cssProperties = properties.release();
}

void InspectorCSSAgent::startSelectorProfiler(ErrorString*)
{
    m_currentSelectorProfile = adoptPtr(new SelectorProfile());
    m_instrumentingAgents->setInspectorCSSAgent(this);
    m_state->setBoolean(CSSAgentState::isSelectorProfiling, true);
}

void InspectorCSSAgent::stopSelectorProfiler(ErrorString*, RefPtr<InspectorObject>* result)
{
    if (!m_state->getBoolean(CSSAgentState::isSelectorProfiling))
        return;
    m_state->setBoolean(CSSAgentState::isSelectorProfiling, false);
    m_instrumentingAgents->setInspectorCSSAgent(0);
    if (m_frontend && result)
        *result = m_currentSelectorProfile->toInspectorObject();
    m_currentSelectorProfile.clear();
}

void InspectorCSSAgent::willMatchRule(const CSSStyleRule* rule)
{
    m_currentSelectorProfile->startSelector(rule->selectorText());
}

void InspectorCSSAgent::didMatchRule(bool matched)
{
    m_currentSelectorProfile->commitSelector(matched);
}

void InspectorCSSAgent::willProcessRule(const CSSStyleRule* rule)
{
    m_currentSelectorProfile->startSelector(rule->selectorText());
}

void InspectorCSSAgent::didProcessRule()
{
    m_currentSelectorProfile->commitSelectorTime();
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end()) {
        CSSStyleDeclaration* style = element->isStyledElement() ? element->style() : 0;
        if (!style)
            return 0;

        String newStyleSheetId = String::number(m_lastStyleSheetId++);
        RefPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(newStyleSheetId, element, "");
        m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet);
        m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
        return inspectorStyleSheet.get();
    }

    return it->second.get();
}

Element* InspectorCSSAgent::elementForId(ErrorString* errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return 0;
    }
    if (node->nodeType() != Node::ELEMENT_NODE) {
        *errorString = "Not an element node";
        return 0;
    }
    return static_cast<Element*>(node);
}

void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, InspectorArray* result)
{
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(static_cast<CSSStyleSheet*>(styleSheet));
    result->pushObject(inspectorStyleSheet->buildObjectForStyleSheetInfo());
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (rule->isImportRule()) {
            CSSStyleSheet* importedStyleSheet = static_cast<CSSImportRule*>(rule)->styleSheet();
            if (importedStyleSheet)
                collectStyleSheets(importedStyleSheet, result);
        }
    }
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
    if (!inspectorStyleSheet) {
        String id = String::number(m_lastStyleSheetId++);
        Document* document = styleSheet->findDocument();
        inspectorStyleSheet = InspectorStyleSheet::create(id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document));
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return 0;
    }

    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_documentToInspectorStyleSheet.get(document);
    if (inspectorStyleSheet || !createIfAbsent)
        return inspectorStyleSheet.get();

    ExceptionCode ec = 0;
    RefPtr<Element> styleElement = document->createElement("style", ec);
    if (!ec)
        styleElement->setAttribute("type", "text/css", ec);
    if (!ec) {
        ContainerNode* targetNode;
        // HEAD is absent in ImageDocuments, for example.
        if (document->head())
            targetNode = document->head();
        else if (document->body())
            targetNode = document->body();
        else
            return 0;
        targetNode->appendChild(styleElement, ec);
    }
    if (ec)
        return 0;
    StyleSheetList* styleSheets = document->styleSheets();
    StyleSheet* styleSheet = styleSheets->item(styleSheets->length() - 1);
    if (!styleSheet->isCSSStyleSheet())
        return 0;
    CSSStyleSheet* cssStyleSheet = static_cast<CSSStyleSheet*>(styleSheet);
    String id = String::number(m_lastStyleSheetId++);
    inspectorStyleSheet = InspectorStyleSheet::create(id, cssStyleSheet, "inspector", InspectorDOMAgent::documentURLString(document));
    m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
    m_cssStyleSheetToInspectorStyleSheet.set(cssStyleSheet, inspectorStyleSheet);
    m_documentToInspectorStyleSheet.set(document, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::assertStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        *errorString = "No style sheet with given id found";
        return 0;
    }
    return it->second.get();
}

String InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    DEFINE_STATIC_LOCAL(String, userAgent, ("user-agent"));
    DEFINE_STATIC_LOCAL(String, user, ("user"));
    DEFINE_STATIC_LOCAL(String, inspector, ("inspector"));

    String origin("");
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = userAgent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->nodeName() == "#document")
        origin = user;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = inspector;
    }
    return origin;
}

PassRefPtr<InspectorArray> InspectorCSSAgent::buildArrayForRuleList(CSSRuleList* ruleList)
{
    RefPtr<InspectorArray> result = InspectorArray::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        if (!rule)
            continue;

        InspectorStyleSheet* styleSheet = bindStyleSheet(parentStyleSheet(rule));
        if (styleSheet)
            result->pushObject(styleSheet->buildObjectForRule(rule));
    }
    return result.release();
}

PassRefPtr<InspectorArray> InspectorCSSAgent::buildArrayForAttributeStyles(Element* element)
{
    RefPtr<InspectorArray> attrStyles = InspectorArray::create();
    NamedNodeMap* attributes = element->attributes();
    for (unsigned i = 0; attributes && i < attributes->length(); ++i) {
        Attribute* attribute = attributes->attributeItem(i);
        if (attribute->style()) {
            RefPtr<InspectorObject> attrStyleObject = InspectorObject::create();
            String attributeName = attribute->localName();
            RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), attribute->style(), 0);
            attrStyleObject->setString("name", attributeName.utf8().data());
            attrStyleObject->setObject("style", inspectorStyle->buildObjectForStyle());
            attrStyles->pushObject(attrStyleObject.release());
        }
    }

    return attrStyles.release();
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    if (document)
        m_documentToInspectorStyleSheet.remove(document);
    clearPseudoState(false);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    if (!node)
        return;

    if (m_lastElementWithPseudoState.get() == node) {
        clearPseudoState(false);
        return;
    }

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheet.remove(it->second->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element)
{
    if (!element)
        return;

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    it->second->didModifyElementAttribute();
}

void InspectorCSSAgent::clearPseudoState(bool recalcStyles)
{
    RefPtr<Element> element = m_lastElementWithPseudoState;
    m_lastElementWithPseudoState = 0;
    m_lastPseudoState = 0;
    if (recalcStyles && element) {
        Document* document = element->ownerDocument();
        if (document)
            document->styleSelectorChanged(RecalcStyleImmediately);
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
