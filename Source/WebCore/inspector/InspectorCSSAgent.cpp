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

#if ENABLE(INSPECTOR)

#include "InspectorCSSAgent.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSImportRule.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "ExceptionCodePlaceholder.h"
#include "HTMLHeadElement.h"
#include "HTMLStyleElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorHistory.h"
#include "InspectorWebProtocolTypes.h"
#include "InstrumentingAgents.h"
#include "NamedFlowCollection.h"
#include "Node.h"
#include "NodeList.h"
#include "RenderNamedFlowFragment.h"
#include "SVGStyleElement.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleSheetList.h"
#include "WebKitNamedFlow.h"
#include <inspector/InspectorValues.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

using namespace Inspector;

namespace WebCore {

enum ForcePseudoClassFlags {
    PseudoClassNone = 0,
    PseudoClassHover = 1 << 0,
    PseudoClassFocus = 1 << 1,
    PseudoClassActive = 1 << 2,
    PseudoClassVisited = 1 << 3
};

static unsigned computePseudoClassMask(InspectorArray* pseudoClassArray)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, active, (ASCIILiteral("active")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, hover, (ASCIILiteral("hover")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, focus, (ASCIILiteral("focus")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, visited, (ASCIILiteral("visited")));
    if (!pseudoClassArray || !pseudoClassArray->length())
        return PseudoClassNone;

    unsigned result = PseudoClassNone;
    for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
        RefPtr<InspectorValue> pseudoClassValue = pseudoClassArray->get(i);
        String pseudoClass;
        bool success = pseudoClassValue->asString(pseudoClass);
        if (!success)
            continue;
        if (pseudoClass == active)
            result |= PseudoClassActive;
        else if (pseudoClass == hover)
            result |= PseudoClassHover;
        else if (pseudoClass == focus)
            result |= PseudoClassFocus;
        else if (pseudoClass == visited)
            result |= PseudoClassVisited;
    }

    return result;
}

class ChangeRegionOversetTask {
public:
    ChangeRegionOversetTask(InspectorCSSAgent*);
    void scheduleFor(WebKitNamedFlow*, int documentNodeId);
    void unschedule(WebKitNamedFlow*);
    void reset();
    void timerFired(Timer<ChangeRegionOversetTask>&);

private:
    InspectorCSSAgent* m_cssAgent;
    Timer<ChangeRegionOversetTask> m_timer;
    HashMap<WebKitNamedFlow*, int> m_namedFlows;
};

ChangeRegionOversetTask::ChangeRegionOversetTask(InspectorCSSAgent* cssAgent)
    : m_cssAgent(cssAgent)
    , m_timer(this, &ChangeRegionOversetTask::timerFired)
{
}

void ChangeRegionOversetTask::scheduleFor(WebKitNamedFlow* namedFlow, int documentNodeId)
{
    m_namedFlows.add(namedFlow, documentNodeId);

    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void ChangeRegionOversetTask::unschedule(WebKitNamedFlow* namedFlow)
{
    m_namedFlows.remove(namedFlow);
}

void ChangeRegionOversetTask::reset()
{
    m_timer.stop();
    m_namedFlows.clear();
}

void ChangeRegionOversetTask::timerFired(Timer<ChangeRegionOversetTask>&)
{
    // The timer is stopped on m_cssAgent destruction, so this method will never be called after m_cssAgent has been destroyed.
    for (HashMap<WebKitNamedFlow*, int>::iterator it = m_namedFlows.begin(), end = m_namedFlows.end(); it != end; ++it)
        m_cssAgent->regionOversetChanged(it->key, it->value);

    m_namedFlows.clear();
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    StyleSheetAction(const String& name, InspectorStyleSheet* styleSheet)
        : InspectorHistory::Action(name)
        , m_styleSheet(styleSheet)
    {
    }

protected:
    RefPtr<InspectorStyleSheet> m_styleSheet;
};

class InspectorCSSAgent::SetStyleSheetTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);
public:
    SetStyleSheetTextAction(InspectorStyleSheet* styleSheet, const String& text)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("SetStyleSheetText"), styleSheet)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        if (!m_styleSheet->getText(&m_oldText))
            return false;
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        if (m_styleSheet->setText(m_oldText, ec)) {
            m_styleSheet->reparseStyleSheet(m_oldText);
            return true;
        }
        return false;
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        if (m_styleSheet->setText(m_text, ec)) {
            m_styleSheet->reparseStyleSheet(m_text);
            return true;
        }
        return false;
    }

    virtual String mergeId() override
    {
        return String::format("SetStyleSheetText %s", m_styleSheet->id().utf8().data());
    }

    virtual void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleSheetTextAction* other = static_cast<SetStyleSheetTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetStyleTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleTextAction);
public:
    SetStyleTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& text)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("SetStyleText"), styleSheet)
        , m_cssId(cssId)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        return m_styleSheet->setStyleText(m_cssId, m_oldText, nullptr, ec);
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        return m_styleSheet->setStyleText(m_cssId, m_text, &m_oldText, ec);
    }

    virtual String mergeId() override
    {
        ASSERT(m_styleSheet->id() == m_cssId.styleSheetId());
        return String::format("SetStyleText %s:%u", m_styleSheet->id().utf8().data(), m_cssId.ordinal());
    }

    virtual void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleTextAction* other = static_cast<SetStyleTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    InspectorCSSId m_cssId;
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetPropertyTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetPropertyTextAction);
public:
    SetPropertyTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, const String& text, bool overwrite)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("SetPropertyText"), styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_text(text)
        , m_overwrite(overwrite)
    {
    }

    virtual String toString() override
    {
        return mergeId() + ": " + m_oldText + " -> " + m_text;
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        String placeholder;
        return m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_overwrite ? m_oldText : "", true, &placeholder, ec);
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        String oldText;
        bool result = m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_text, m_overwrite, &oldText, ec);
        m_oldText = oldText.stripWhiteSpace();
        // FIXME: remove this once the model handles this case.
        if (!m_oldText.endsWith(';'))
            m_oldText.append(';');

        return result;
    }

    virtual String mergeId() override
    {
        return String::format("SetPropertyText %s:%u:%s", m_styleSheet->id().utf8().data(), m_propertyIndex, m_overwrite ? "true" : "false");
    }

    virtual void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());

        SetPropertyTextAction* other = static_cast<SetPropertyTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    String m_text;
    String m_oldText;
    bool m_overwrite;
};

class InspectorCSSAgent::TogglePropertyAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(TogglePropertyAction);
public:
    TogglePropertyAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, bool disable)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("ToggleProperty"), styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_disable(disable)
    {
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        return m_styleSheet->toggleProperty(m_cssId, m_propertyIndex, !m_disable, ec);
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        return m_styleSheet->toggleProperty(m_cssId, m_propertyIndex, m_disable, ec);
    }

private:
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    bool m_disable;
};

class InspectorCSSAgent::SetRuleSelectorAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleSelectorAction);
public:
    SetRuleSelectorAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& selector)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("SetRuleSelector"), styleSheet)
        , m_cssId(cssId)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        m_oldSelector = m_styleSheet->ruleSelector(m_cssId, ec);
        if (ec)
            return false;
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_oldSelector, ec);
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_selector, ec);
    }

private:
    InspectorCSSId m_cssId;
    String m_selector;
    String m_oldSelector;
};

class InspectorCSSAgent::AddRuleAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& selector)
        : InspectorCSSAgent::StyleSheetAction(ASCIILiteral("AddRule"), styleSheet)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionCode& ec) override
    {
        return redo(ec);
    }

    virtual bool undo(ExceptionCode& ec) override
    {
        return m_styleSheet->deleteRule(m_newId, ec);
    }

    virtual bool redo(ExceptionCode& ec) override
    {
        CSSStyleRule* cssStyleRule = m_styleSheet->addRule(m_selector, ec);
        if (ec)
            return false;
        m_newId = m_styleSheet->ruleId(cssStyleRule);
        return true;
    }

    InspectorCSSId newRuleId() { return m_newId; }

private:
    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule)
{
    if (rule->type() != CSSRule::STYLE_RULE)
        return nullptr;
    return toCSSStyleRule(rule);
}

InspectorCSSAgent::InspectorCSSAgent(InstrumentingAgents* instrumentingAgents, InspectorDOMAgent* domAgent)
    : InspectorAgentBase(ASCIILiteral("CSS"), instrumentingAgents)
    , m_domAgent(domAgent)
    , m_lastStyleSheetId(1)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    ASSERT(!m_domAgent);
    reset();
}

void InspectorCSSAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorCSSFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorCSSBackendDispatcher::create(backendDispatcher, this);
}

void InspectorCSSAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    resetNonPersistentData();
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(nullptr);
    m_domAgent = nullptr;
}

void InspectorCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
    resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData()
{
    m_namedFlowCollectionsRequested.clear();
    if (m_changeRegionOversetTask)
        m_changeRegionOversetTask->reset();
    resetPseudoStates();
}

void InspectorCSSAgent::enable(ErrorString*)
{
    m_instrumentingAgents->setInspectorCSSAgent(this);
}

void InspectorCSSAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorCSSAgent(nullptr);
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->mediaQueryResultChanged();
}

void InspectorCSSAgent::didCreateNamedFlow(Document* document, WebKitNamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    ErrorString errorString;
    m_frontendDispatcher->namedFlowCreated(buildObjectForNamedFlow(&errorString, namedFlow, documentNodeId));
}

void InspectorCSSAgent::willRemoveNamedFlow(Document* document, WebKitNamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    if (m_changeRegionOversetTask)
        m_changeRegionOversetTask->unschedule(namedFlow);

    m_frontendDispatcher->namedFlowRemoved(documentNodeId, namedFlow->name().string());
}

void InspectorCSSAgent::didChangeRegionOverset(Document* document, WebKitNamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    if (!m_changeRegionOversetTask)
        m_changeRegionOversetTask = std::make_unique<ChangeRegionOversetTask>(this);
    m_changeRegionOversetTask->scheduleFor(namedFlow, documentNodeId);
}

void InspectorCSSAgent::regionOversetChanged(WebKitNamedFlow* namedFlow, int documentNodeId)
{
    if (namedFlow->flowState() == WebKitNamedFlow::FlowStateNull)
        return;

    ErrorString errorString;
    Ref<WebKitNamedFlow> protect(*namedFlow);

    m_frontendDispatcher->regionOversetChanged(buildObjectForNamedFlow(&errorString, namedFlow, documentNodeId));
}

void InspectorCSSAgent::didRegisterNamedFlowContentElement(Document* document, WebKitNamedFlow* namedFlow, Node* contentElement, Node* nextContentElement)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    ErrorString errorString;
    int contentElementNodeId = m_domAgent->pushNodeToFrontend(&errorString, documentNodeId, contentElement);
    int nextContentElementNodeId = nextContentElement ? m_domAgent->pushNodeToFrontend(&errorString, documentNodeId, nextContentElement) : 0;
    m_frontendDispatcher->registeredNamedFlowContentElement(documentNodeId, namedFlow->name().string(), contentElementNodeId, nextContentElementNodeId);
}

void InspectorCSSAgent::didUnregisterNamedFlowContentElement(Document* document, WebKitNamedFlow* namedFlow, Node* contentElement)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    ErrorString errorString;
    int contentElementNodeId = m_domAgent->pushNodeToFrontend(&errorString, documentNodeId, contentElement);
    if (!contentElementNodeId) {
        // We've already notified that the DOM node was removed from the DOM, so there's no need to send another event.
        return;
    }
    m_frontendDispatcher->unregisteredNamedFlowContentElement(documentNodeId, namedFlow->name().string(), contentElementNodeId);
}

bool InspectorCSSAgent::forcePseudoState(Element* element, CSSSelector::PseudoClassType pseudoClassType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(element);
    if (!nodeId)
        return false;

    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    if (it == m_nodeIdToForcedPseudoState.end())
        return false;

    unsigned forcedPseudoState = it->value;
    switch (pseudoClassType) {
    case CSSSelector::PseudoClassActive:
        return forcedPseudoState & PseudoClassActive;
    case CSSSelector::PseudoClassFocus:
        return forcedPseudoState & PseudoClassFocus;
    case CSSSelector::PseudoClassHover:
        return forcedPseudoState & PseudoClassHover;
    case CSSSelector::PseudoClassVisited:
        return forcedPseudoState & PseudoClassVisited;
    default:
        return false;
    }
}

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString* errorString, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>>& matchedCSSRules, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>>& pseudoIdMatches, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>>& inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    // Matched rules.
    StyleResolver& styleResolver = element->document().ensureStyleResolver();
    auto matchedRules = styleResolver.styleRulesForElement(element, StyleResolver::AllCSSRules);
    matchedCSSRules = buildArrayForMatchedRuleList(matchedRules, styleResolver, element);

    // Pseudo elements.
    if (!includePseudo || *includePseudo) {
        RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>> pseudoElements = Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
            if (!matchedRules.isEmpty()) {
                RefPtr<Inspector::Protocol::CSS::PseudoIdMatches> matches = Inspector::Protocol::CSS::PseudoIdMatches::create()
                    .setPseudoId(static_cast<int>(pseudoId))
                    .setMatches(buildArrayForMatchedRuleList(matchedRules, styleResolver, element));
                pseudoElements->addItem(matches.release());
            }
        }

        pseudoIdMatches = pseudoElements.release();
    }

    // Inherited styles.
    if (!includeInherited || *includeInherited) {
        RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>> entries = Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            StyleResolver& parentStyleResolver = parentElement->document().ensureStyleResolver();
            auto parentMatchedRules = parentStyleResolver.styleRulesForElement(parentElement, StyleResolver::AllCSSRules);
            RefPtr<Inspector::Protocol::CSS::InheritedStyleEntry> entry = Inspector::Protocol::CSS::InheritedStyleEntry::create()
                .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules, styleResolver, parentElement));
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    entry->setInlineStyle(styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            entries->addItem(entry.release());
            parentElement = parentElement->parentElement();
        }

        inheritedEntries = entries.release();
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString* errorString, int nodeId, RefPtr<Inspector::Protocol::CSS::CSSStyle>& inlineStyle, RefPtr<Inspector::Protocol::CSS::CSSStyle>& attributesStyle)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    inlineStyle = styleSheet->buildObjectForStyle(element->style());
    RefPtr<Inspector::Protocol::CSS::CSSStyle> attributes = buildObjectForAttributesStyle(element);
    attributesStyle = attributes ? attributes.release() : nullptr;
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString* errorString, int nodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSComputedStyleProperty>>& style)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(element, true);
    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, nullptr);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::getAllStyleSheets(ErrorString*, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>>& styleInfos)
{
    styleInfos = Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>::create();
    Vector<Document*> documents = m_domAgent->documents();
    for (Vector<Document*>::iterator it = documents.begin(); it != documents.end(); ++it) {
        StyleSheetList* list = (*it)->styleSheets();
        for (unsigned i = 0; i < list->length(); ++i) {
            StyleSheet& styleSheet = *list->item(i);
            if (styleSheet.isCSSStyleSheet())
                collectStyleSheets(&toCSSStyleSheet(styleSheet), styleInfos.get());
        }
    }
}

void InspectorCSSAgent::getStyleSheet(ErrorString* errorString, const String& styleSheetId, RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody>& styleSheetObject)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    styleSheetObject = inspectorStyleSheet->buildObjectForStyleSheet();
}

void InspectorCSSAgent::getStyleSheetText(ErrorString* errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->getText(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString* errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    m_domAgent->history()->perform(std::make_unique<SetStyleSheetTextAction>(inspectorStyleSheet, text), ec);
    *errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::setStyleText(ErrorString* errorString, const RefPtr<InspectorObject>& fullStyleId, const String& text, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    bool success = m_domAgent->history()->perform(std::make_unique<SetStyleTextAction>(inspectorStyleSheet, compoundId, text), ec);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::setPropertyText(ErrorString* errorString, const RefPtr<InspectorObject>& fullStyleId, int propertyIndex, const String& text, bool overwrite, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    bool success = m_domAgent->history()->perform(std::make_unique<SetPropertyTextAction>(inspectorStyleSheet, compoundId, propertyIndex, text, overwrite), ec);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::toggleProperty(ErrorString* errorString, const RefPtr<InspectorObject>& fullStyleId, int propertyIndex, bool disable, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    bool success = m_domAgent->history()->perform(std::make_unique<TogglePropertyAction>(inspectorStyleSheet, compoundId, propertyIndex, disable), ec);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::setRuleSelector(ErrorString* errorString, const RefPtr<InspectorObject>& fullRuleId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    InspectorCSSId compoundId(fullRuleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    bool success = m_domAgent->history()->perform(std::make_unique<SetRuleSelectorAction>(inspectorStyleSheet, compoundId, selector), ec);

    if (success)
        result = inspectorStyleSheet->buildObjectForRule(inspectorStyleSheet->ruleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::addRule(ErrorString* errorString, const int contextNodeId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    Node* node = m_domAgent->assertNode(errorString, contextNodeId);
    if (!node)
        return;

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(&node->document(), true);
    if (!inspectorStyleSheet) {
        *errorString = "No target stylesheet found";
        return;
    }

    ExceptionCode ec = 0;
    auto action = std::make_unique<AddRuleAction>(inspectorStyleSheet, selector);
    AddRuleAction* rawAction = action.get();
    bool success = m_domAgent->history()->perform(WTF::move(action), ec);
    if (!success) {
        *errorString = InspectorDOMAgent::toErrorString(ec);
        return;
    }

    InspectorCSSId ruleId = rawAction->newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule);
}

void InspectorCSSAgent::getSupportedCSSProperties(ErrorString*, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>>& cssProperties)
{
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>> properties = Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>::create();
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
        CSSPropertyID id = convertToCSSPropertyID(i);
        RefPtr<Inspector::Protocol::CSS::CSSPropertyInfo> property = Inspector::Protocol::CSS::CSSPropertyInfo::create()
            .setName(getPropertyNameString(id));

        const StylePropertyShorthand& shorthand = shorthandForProperty(id);
        if (!shorthand.length()) {
            properties->addItem(property.release());
            continue;
        }
        RefPtr<Inspector::Protocol::Array<String>> longhands = Inspector::Protocol::Array<String>::create();
        for (unsigned j = 0; j < shorthand.length(); ++j) {
            CSSPropertyID longhandID = shorthand.properties()[j];
            longhands->addItem(getPropertyNameString(longhandID));
        }
        property->setLonghands(longhands);
        properties->addItem(property.release());
    }
    cssProperties = properties.release();
}

void InspectorCSSAgent::forcePseudoState(ErrorString* errorString, int nodeId, const RefPtr<InspectorArray>& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses.get());
    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    unsigned currentForcedPseudoState = it == m_nodeIdToForcedPseudoState.end() ? 0 : it->value;
    bool needStyleRecalc = forcedPseudoState != currentForcedPseudoState;
    if (!needStyleRecalc)
        return;

    if (forcedPseudoState)
        m_nodeIdToForcedPseudoState.set(nodeId, forcedPseudoState);
    else
        m_nodeIdToForcedPseudoState.remove(nodeId);
    element->document().styleResolverChanged(RecalcStyleImmediately);
}

void InspectorCSSAgent::getNamedFlowCollection(ErrorString* errorString, int documentNodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>>& result)
{
    Document* document = m_domAgent->assertDocument(errorString, documentNodeId);
    if (!document)
        return;

    m_namedFlowCollectionsRequested.add(documentNodeId);

    Vector<RefPtr<WebKitNamedFlow>> namedFlowsVector = document->namedFlows()->namedFlows();
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>> namedFlows = Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>::create();

    for (Vector<RefPtr<WebKitNamedFlow>>::iterator it = namedFlowsVector.begin(); it != namedFlowsVector.end(); ++it)
        namedFlows->addItem(buildObjectForNamedFlow(errorString, it->get(), documentNodeId));

    result = namedFlows.release();
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end()) {
        CSSStyleDeclaration* style = element->isStyledElement() ? element->style() : nullptr;
        if (!style)
            return nullptr;

        String newStyleSheetId = String::number(m_lastStyleSheetId++);
        RefPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(m_domAgent->pageAgent(), newStyleSheetId, element, Inspector::Protocol::CSS::StyleSheetOrigin::Regular, this);
        m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet);
        m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
        return inspectorStyleSheet.get();
    }

    return it->value.get();
}

Element* InspectorCSSAgent::elementForId(ErrorString* errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return nullptr;
    }
    if (!node->isElementNode()) {
        *errorString = "Not an element node";
        return nullptr;
    }
    return toElement(node);
}

int InspectorCSSAgent::documentNodeWithRequestedFlowsId(Document* document)
{
    int documentNodeId = m_domAgent->boundNodeId(document);
    if (!documentNodeId || !m_namedFlowCollectionsRequested.contains(documentNodeId))
        return 0;

    return documentNodeId;
}

void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>* result)
{
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(styleSheet);
    result->addItem(inspectorStyleSheet->buildObjectForStyleSheetInfo());
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (rule->type() == CSSRule::IMPORT_RULE) {
            CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
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
        Document* document = styleSheet->ownerDocument();
        inspectorStyleSheet = InspectorStyleSheet::create(m_domAgent->pageAgent(), id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document), this);
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return nullptr;
    }

    if (!document->isHTMLDocument() && !document->isSVGDocument())
        return nullptr;

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
            return nullptr;

        InlineStyleOverrideScope overrideScope(document);
        targetNode->appendChild(styleElement, ec);
    }
    if (ec)
        return nullptr;

    CSSStyleSheet* cssStyleSheet = nullptr;
    if (styleElement->isHTMLElement())
        cssStyleSheet = toHTMLStyleElement(styleElement.get())->sheet();
    else if (styleElement->isSVGElement())
        cssStyleSheet = toSVGStyleElement(styleElement.get())->sheet();

    if (!cssStyleSheet)
        return nullptr;

    String id = String::number(m_lastStyleSheetId++);
    inspectorStyleSheet = InspectorStyleSheet::create(m_domAgent->pageAgent(), id, cssStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin::Inspector, InspectorDOMAgent::documentURLString(document), this);
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
        return nullptr;
    }
    return it->value.get();
}

Inspector::Protocol::CSS::StyleSheetOrigin InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    Inspector::Protocol::CSS::StyleSheetOrigin origin = Inspector::Protocol::CSS::StyleSheetOrigin::Regular;
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->nodeName() == "#document")
        origin = Inspector::Protocol::CSS::StyleSheetOrigin::User;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = Inspector::Protocol::CSS::StyleSheetOrigin::Inspector;
    }
    return origin;
}

PassRefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(StyleRule* styleRule, StyleResolver& styleResolver)
{
    if (!styleRule)
        return nullptr;

    // StyleRules returned by StyleResolver::styleRulesForElement lack parent pointers since that infomation is not cheaply available.
    // Since the inspector wants to walk the parent chain, we construct the full wrappers here.
    CSSStyleRule* cssomWrapper = styleResolver.inspectorCSSOMWrappers().getWrapperForRuleInSheets(styleRule, styleResolver.document().styleSheetCollection());
    if (!cssomWrapper)
        return nullptr;
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(cssomWrapper->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(cssomWrapper) : nullptr;
}

PassRefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    ASSERT(rule->parentStyleSheet());
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(rule->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(rule) : nullptr;
}

PassRefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSRule>> InspectorCSSAgent::buildArrayForRuleList(CSSRuleList* ruleList)
{
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSRule>> result = Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSRule>::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        RefPtr<Inspector::Protocol::CSS::CSSRule> ruleObject = buildObjectForRule(rule);
        if (!ruleObject)
            continue;
        result->addItem(ruleObject);
    }
    return result.release();
}

PassRefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>> InspectorCSSAgent::buildArrayForMatchedRuleList(const Vector<RefPtr<StyleRule>>& matchedRules, StyleResolver& styleResolver, Element* element)
{
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>> result = Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>::create();

    for (unsigned i = 0; i < matchedRules.size(); ++i) {
        if (!matchedRules[i]->isStyleRule())
            continue;
        StyleRule* matchedStyleRule = static_cast<StyleRule*>(matchedRules[i].get());
        RefPtr<Inspector::Protocol::CSS::CSSRule> ruleObject = buildObjectForRule(matchedStyleRule, styleResolver);
        if (!ruleObject)
            continue;
        RefPtr<Inspector::Protocol::Array<int>> matchingSelectors = Inspector::Protocol::Array<int>::create();
        const CSSSelectorList& selectorList = matchedStyleRule->selectorList();
        long index = 0;
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            bool matched = element->matches(selector->selectorText(), IGNORE_EXCEPTION);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }
        RefPtr<Inspector::Protocol::CSS::RuleMatch> match = Inspector::Protocol::CSS::RuleMatch::create()
            .setRule(ruleObject)
            .setMatchingSelectors(matchingSelectors);
        result->addItem(match);
    }

    return result.release();
}

PassRefPtr<Inspector::Protocol::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(Element* element)
{
    if (!element->isStyledElement())
        return nullptr;

    // FIXME: Ugliness below.
    StyleProperties* attributeStyle = const_cast<StyleProperties*>(toStyledElement(element)->presentationAttributeStyle());
    if (!attributeStyle)
        return nullptr;

    ASSERT_WITH_SECURITY_IMPLICATION(attributeStyle->isMutable());
    MutableStyleProperties* mutableAttributeStyle = static_cast<MutableStyleProperties*>(attributeStyle);

    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle->ensureCSSStyleDeclaration(), nullptr);
    return inspectorStyle->buildObjectForStyle();
}

PassRefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>> InspectorCSSAgent::buildArrayForRegions(ErrorString* errorString, PassRefPtr<NodeList> regionList, int documentNodeId)
{
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>> regions = Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>::create();

    for (unsigned i = 0; i < regionList->length(); ++i) {
        Inspector::Protocol::CSS::Region::RegionOverset regionOverset;

        switch (toElement(regionList->item(i))->regionOversetState()) {
        case RegionFit:
            regionOverset = Inspector::Protocol::CSS::Region::RegionOverset::Fit;
            break;
        case RegionEmpty:
            regionOverset = Inspector::Protocol::CSS::Region::RegionOverset::Empty;
            break;
        case RegionOverset:
            regionOverset = Inspector::Protocol::CSS::Region::RegionOverset::Overset;
            break;
        case RegionUndefined:
            continue;
        default:
            ASSERT_NOT_REACHED();
            continue;
        }

        RefPtr<Inspector::Protocol::CSS::Region> region = Inspector::Protocol::CSS::Region::create()
            .setRegionOverset(regionOverset)
            // documentNodeId was previously asserted
            .setNodeId(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, regionList->item(i)));

        regions->addItem(region);
    }

    return regions.release();
}

PassRefPtr<Inspector::Protocol::CSS::NamedFlow> InspectorCSSAgent::buildObjectForNamedFlow(ErrorString* errorString, WebKitNamedFlow* webkitNamedFlow, int documentNodeId)
{
    RefPtr<NodeList> contentList = webkitNamedFlow->getContent();
    RefPtr<Inspector::Protocol::Array<int>> content = Inspector::Protocol::Array<int>::create();

    for (unsigned i = 0; i < contentList->length(); ++i) {
        // documentNodeId was previously asserted
        content->addItem(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, contentList->item(i)));
    }

    RefPtr<Inspector::Protocol::CSS::NamedFlow> namedFlow = Inspector::Protocol::CSS::NamedFlow::create()
        .setDocumentNodeId(documentNodeId)
        .setName(webkitNamedFlow->name().string())
        .setOverset(webkitNamedFlow->overset())
        .setContent(content)
        .setRegions(buildArrayForRegions(errorString, webkitNamedFlow->getRegions(), documentNodeId));

    return namedFlow.release();
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    if (document)
        m_documentToInspectorStyleSheet.remove(document);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    if (!node)
        return;

    int nodeId = m_domAgent->boundNodeId(node);
    if (nodeId)
        m_nodeIdToForcedPseudoState.remove(nodeId);

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheet.remove(it->value->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element)
{
    if (!element)
        return;

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    it->value->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheet* styleSheet)
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::resetPseudoStates()
{
    HashSet<Document*> documentsToChange;
    for (NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.begin(), end = m_nodeIdToForcedPseudoState.end(); it != end; ++it) {
        if (Element* element = toElement(m_domAgent->nodeForId(it->key)))
            documentsToChange.add(&element->document());
    }

    m_nodeIdToForcedPseudoState.clear();
    for (HashSet<Document*>::iterator it = documentsToChange.begin(), end = documentsToChange.end(); it != end; ++it)
        (*it)->styleResolverChanged(RecalcStyleImmediately);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
