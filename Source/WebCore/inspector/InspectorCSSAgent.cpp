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
#include "InstrumentingAgents.h"
#include "NamedFlowCollection.h"
#include "Node.h"
#include "NodeList.h"
#include "RenderNamedFlowFragment.h"
#include "SVGStyleElement.h"
#include "SelectorChecker.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleSheetList.h"
#include "WebKitNamedFlow.h"
#include <inspector/InspectorProtocolObjects.h>
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

static unsigned computePseudoClassMask(const InspectorArray& pseudoClassArray)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(String, active, (ASCIILiteral("active")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, hover, (ASCIILiteral("hover")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, focus, (ASCIILiteral("focus")));
    DEPRECATED_DEFINE_STATIC_LOCAL(String, visited, (ASCIILiteral("visited")));
    if (!pseudoClassArray.length())
        return PseudoClassNone;

    unsigned result = PseudoClassNone;
    for (size_t i = 0; i < pseudoClassArray.length(); ++i) {
        RefPtr<InspectorValue> pseudoClassValue = pseudoClassArray.get(i);
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
    void timerFired();

private:
    InspectorCSSAgent* m_cssAgent;
    Timer m_timer;
    HashMap<WebKitNamedFlow*, int> m_namedFlows;
};

ChangeRegionOversetTask::ChangeRegionOversetTask(InspectorCSSAgent* cssAgent)
    : m_cssAgent(cssAgent)
    , m_timer(*this, &ChangeRegionOversetTask::timerFired)
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

void ChangeRegionOversetTask::timerFired()
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
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule& rule)
{
    if (!is<CSSStyleRule>(rule))
        return nullptr;
    return downcast<CSSStyleRule>(&rule);
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

void InspectorCSSAgent::didCreateFrontendAndBackend(Inspector::FrontendChannel* frontendChannel, Inspector::BackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<CSSFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = CSSBackendDispatcher::create(backendDispatcher, this);
}

void InspectorCSSAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher = nullptr;

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

void InspectorCSSAgent::enable(ErrorString&)
{
    m_instrumentingAgents->setInspectorCSSAgent(this);
}

void InspectorCSSAgent::disable(ErrorString&)
{
    m_instrumentingAgents->setInspectorCSSAgent(nullptr);
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->mediaQueryResultChanged();
}

void InspectorCSSAgent::didCreateNamedFlow(Document& document, WebKitNamedFlow& namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(&document);
    if (!documentNodeId)
        return;

    ErrorString unused;
    m_frontendDispatcher->namedFlowCreated(buildObjectForNamedFlow(unused, &namedFlow, documentNodeId));
}

void InspectorCSSAgent::willRemoveNamedFlow(Document& document, WebKitNamedFlow& namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(&document);
    if (!documentNodeId)
        return;

    if (m_changeRegionOversetTask)
        m_changeRegionOversetTask->unschedule(&namedFlow);

    m_frontendDispatcher->namedFlowRemoved(documentNodeId, namedFlow.name().string());
}

void InspectorCSSAgent::didChangeRegionOverset(Document& document, WebKitNamedFlow& namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(&document);
    if (!documentNodeId)
        return;

    if (!m_changeRegionOversetTask)
        m_changeRegionOversetTask = std::make_unique<ChangeRegionOversetTask>(this);
    m_changeRegionOversetTask->scheduleFor(&namedFlow, documentNodeId);
}

void InspectorCSSAgent::regionOversetChanged(WebKitNamedFlow* namedFlow, int documentNodeId)
{
    if (namedFlow->flowState() == WebKitNamedFlow::FlowStateNull)
        return;

    ErrorString unused;
    Ref<WebKitNamedFlow> protect(*namedFlow);

    m_frontendDispatcher->regionOversetChanged(buildObjectForNamedFlow(unused, namedFlow, documentNodeId));
}

void InspectorCSSAgent::didRegisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement, Node* nextContentElement)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(&document);
    if (!documentNodeId)
        return;

    ErrorString unused;
    int contentElementNodeId = m_domAgent->pushNodeToFrontend(unused, documentNodeId, &contentElement);
    int nextContentElementNodeId = nextContentElement ? m_domAgent->pushNodeToFrontend(unused, documentNodeId, nextContentElement) : 0;
    m_frontendDispatcher->registeredNamedFlowContentElement(documentNodeId, namedFlow.name().string(), contentElementNodeId, nextContentElementNodeId);
}

void InspectorCSSAgent::didUnregisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(&document);
    if (!documentNodeId)
        return;

    ErrorString unused;
    int contentElementNodeId = m_domAgent->pushNodeToFrontend(unused, documentNodeId, &contentElement);
    if (!contentElementNodeId) {
        // We've already notified that the DOM node was removed from the DOM, so there's no need to send another event.
        return;
    }
    m_frontendDispatcher->unregisteredNamedFlowContentElement(documentNodeId, namedFlow.name().string(), contentElementNodeId);
}

bool InspectorCSSAgent::forcePseudoState(Element& element, CSSSelector::PseudoClassType pseudoClassType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(&element);
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

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString& errorString, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>>& matchedCSSRules, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>>& pseudoIdMatches, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>>& inheritedEntries)
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
        auto pseudoElements = Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
            if (!matchedRules.isEmpty()) {
                auto matches = Inspector::Protocol::CSS::PseudoIdMatches::create()
                    .setPseudoId(static_cast<int>(pseudoId))
                    .setMatches(buildArrayForMatchedRuleList(matchedRules, styleResolver, element))
                    .release();
                pseudoElements->addItem(WTF::move(matches));
            }
        }

        pseudoIdMatches = WTF::move(pseudoElements);
    }

    // Inherited styles.
    if (!includeInherited || *includeInherited) {
        auto entries = Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            StyleResolver& parentStyleResolver = parentElement->document().ensureStyleResolver();
            auto parentMatchedRules = parentStyleResolver.styleRulesForElement(parentElement, StyleResolver::AllCSSRules);
            auto entry = Inspector::Protocol::CSS::InheritedStyleEntry::create()
                .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules, styleResolver, parentElement))
                .release();
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    entry->setInlineStyle(styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            entries->addItem(WTF::move(entry));
            parentElement = parentElement->parentElement();
        }

        inheritedEntries = WTF::move(entries);
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString& errorString, int nodeId, RefPtr<Inspector::Protocol::CSS::CSSStyle>& inlineStyle, RefPtr<Inspector::Protocol::CSS::CSSStyle>& attributesStyle)
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

void InspectorCSSAgent::getComputedStyleForNode(ErrorString& errorString, int nodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSComputedStyleProperty>>& style)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(element, true);
    Ref<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, nullptr);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::getAllStyleSheets(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>>& styleInfos)
{
    styleInfos = Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>::create();
    Vector<Document*> documents = m_domAgent->documents();
    for (Vector<Document*>::iterator it = documents.begin(); it != documents.end(); ++it) {
        StyleSheetList& list = (*it)->styleSheets();
        for (unsigned i = 0; i < list.length(); ++i) {
            StyleSheet& styleSheet = *list.item(i);
            if (is<CSSStyleSheet>(styleSheet))
                collectStyleSheets(&downcast<CSSStyleSheet>(styleSheet), styleInfos.get());
        }
    }
}

void InspectorCSSAgent::getStyleSheet(ErrorString& errorString, const String& styleSheetId, RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody>& styleSheetObject)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    styleSheetObject = inspectorStyleSheet->buildObjectForStyleSheet();
}

void InspectorCSSAgent::getStyleSheetText(ErrorString& errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->getText(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString& errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    m_domAgent->history()->perform(std::make_unique<SetStyleSheetTextAction>(inspectorStyleSheet, text), ec);
    errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::setStyleText(ErrorString& errorString, const InspectorObject& fullStyleId, const String& text, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result)
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
    errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::setRuleSelector(ErrorString& errorString, const InspectorObject& fullRuleId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    InspectorCSSId compoundId(fullRuleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    ExceptionCode ec = 0;
    bool success = m_domAgent->history()->perform(std::make_unique<SetRuleSelectorAction>(inspectorStyleSheet, compoundId, selector), ec);

    if (success)
        result = inspectorStyleSheet->buildObjectForRule(inspectorStyleSheet->ruleForId(compoundId), nullptr);
    errorString = InspectorDOMAgent::toErrorString(ec);
}

void InspectorCSSAgent::addRule(ErrorString& errorString, const int contextNodeId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    Node* node = m_domAgent->assertNode(errorString, contextNodeId);
    if (!node)
        return;

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(&node->document(), true);
    if (!inspectorStyleSheet) {
        errorString = ASCIILiteral("No target stylesheet found");
        return;
    }

    ExceptionCode ec = 0;
    auto action = std::make_unique<AddRuleAction>(inspectorStyleSheet, selector);
    AddRuleAction* rawAction = action.get();
    bool success = m_domAgent->history()->perform(WTF::move(action), ec);
    if (!success) {
        errorString = InspectorDOMAgent::toErrorString(ec);
        return;
    }

    InspectorCSSId ruleId = rawAction->newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule, nullptr);
}

void InspectorCSSAgent::getSupportedCSSProperties(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>>& cssProperties)
{
    auto properties = Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>::create();
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
        CSSPropertyID id = convertToCSSPropertyID(i);
        auto property = Inspector::Protocol::CSS::CSSPropertyInfo::create()
            .setName(getPropertyNameString(id))
            .release();

        const StylePropertyShorthand& shorthand = shorthandForProperty(id);
        if (!shorthand.length()) {
            properties->addItem(WTF::move(property));
            continue;
        }
        auto longhands = Inspector::Protocol::Array<String>::create();
        for (unsigned j = 0; j < shorthand.length(); ++j) {
            CSSPropertyID longhandID = shorthand.properties()[j];
            longhands->addItem(getPropertyNameString(longhandID));
        }
        property->setLonghands(WTF::move(longhands));
        properties->addItem(WTF::move(property));
    }
    cssProperties = WTF::move(properties);
}

void InspectorCSSAgent::forcePseudoState(ErrorString& errorString, int nodeId, const InspectorArray& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses);
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

void InspectorCSSAgent::getNamedFlowCollection(ErrorString& errorString, int documentNodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>>& result)
{
    Document* document = m_domAgent->assertDocument(errorString, documentNodeId);
    if (!document)
        return;

    m_namedFlowCollectionsRequested.add(documentNodeId);

    Vector<RefPtr<WebKitNamedFlow>> namedFlowsVector = document->namedFlows().namedFlows();
    auto namedFlows = Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>::create();

    for (Vector<RefPtr<WebKitNamedFlow>>::iterator it = namedFlowsVector.begin(); it != namedFlowsVector.end(); ++it)
        namedFlows->addItem(buildObjectForNamedFlow(errorString, it->get(), documentNodeId));

    result = WTF::move(namedFlows);
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

Element* InspectorCSSAgent::elementForId(ErrorString& errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        errorString = ASCIILiteral("No node with given id found");
        return nullptr;
    }
    if (!is<Element>(*node)) {
        errorString = ASCIILiteral("Not an element node");
        return nullptr;
    }
    return downcast<Element>(node);
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
        if (is<CSSImportRule>(*rule)) {
            if (CSSStyleSheet* importedStyleSheet = downcast<CSSImportRule>(*rule).styleSheet())
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
        if (auto* head = document->head())
            targetNode = head;
        else if (auto* body = document->bodyOrFrameset())
            targetNode = body;
        else
            return nullptr;

        InlineStyleOverrideScope overrideScope(document);
        targetNode->appendChild(styleElement, ec);
    }
    if (ec)
        return nullptr;

    CSSStyleSheet* cssStyleSheet = nullptr;
    if (styleElement->isHTMLElement())
        cssStyleSheet = downcast<HTMLStyleElement>(*styleElement).sheet();
    else if (styleElement->isSVGElement())
        cssStyleSheet = downcast<SVGStyleElement>(*styleElement).sheet();

    if (!cssStyleSheet)
        return nullptr;

    String id = String::number(m_lastStyleSheetId++);
    inspectorStyleSheet = InspectorStyleSheet::create(m_domAgent->pageAgent(), id, cssStyleSheet, Inspector::Protocol::CSS::StyleSheetOrigin::Inspector, InspectorDOMAgent::documentURLString(document), this);
    m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
    m_cssStyleSheetToInspectorStyleSheet.set(cssStyleSheet, inspectorStyleSheet);
    m_documentToInspectorStyleSheet.set(document, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::assertStyleSheetForId(ErrorString& errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        errorString = ASCIILiteral("No style sheet with given id found");
        return nullptr;
    }
    return it->value.get();
}

Inspector::Protocol::CSS::StyleSheetOrigin InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    auto origin = Inspector::Protocol::CSS::StyleSheetOrigin::Regular;
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

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(StyleRule* styleRule, StyleResolver& styleResolver, Element* element)
{
    if (!styleRule)
        return nullptr;

    // StyleRules returned by StyleResolver::styleRulesForElement lack parent pointers since that infomation is not cheaply available.
    // Since the inspector wants to walk the parent chain, we construct the full wrappers here.
    CSSStyleRule* cssomWrapper = styleResolver.inspectorCSSOMWrappers().getWrapperForRuleInSheets(styleRule, styleResolver.document().styleSheetCollection());
    if (!cssomWrapper)
        return nullptr;
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(cssomWrapper->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(cssomWrapper, element) : nullptr;
}

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    ASSERT(rule->parentStyleSheet());
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(rule->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(rule, nullptr) : nullptr;
}

RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>> InspectorCSSAgent::buildArrayForMatchedRuleList(const Vector<RefPtr<StyleRule>>& matchedRules, StyleResolver& styleResolver, Element* element)
{
    auto result = Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>::create();

    SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRules);
    SelectorChecker selectorChecker(element->document());

    for (auto& matchedRule : matchedRules) {
        RefPtr<Inspector::Protocol::CSS::CSSRule> ruleObject = buildObjectForRule(matchedRule.get(), styleResolver, element);
        if (!ruleObject)
            continue;

        auto matchingSelectors = Inspector::Protocol::Array<int>::create();
        const CSSSelectorList& selectorList = matchedRule->selectorList();
        long index = 0;
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            unsigned ignoredSpecificity;
            bool matched = selectorChecker.match(selector, element, context, ignoredSpecificity);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }

        auto match = Inspector::Protocol::CSS::RuleMatch::create()
            .setRule(WTF::move(ruleObject))
            .setMatchingSelectors(WTF::move(matchingSelectors))
            .release();
        result->addItem(WTF::move(match));
    }

    return WTF::move(result);
}

RefPtr<Inspector::Protocol::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(Element* element)
{
    ASSERT(element);
    if (!is<StyledElement>(*element))
        return nullptr;

    // FIXME: Ugliness below.
    StyleProperties* attributeStyle = const_cast<StyleProperties*>(downcast<StyledElement>(element)->presentationAttributeStyle());
    if (!attributeStyle)
        return nullptr;

    ASSERT_WITH_SECURITY_IMPLICATION(attributeStyle->isMutable());
    MutableStyleProperties* mutableAttributeStyle = static_cast<MutableStyleProperties*>(attributeStyle);

    Ref<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle->ensureCSSStyleDeclaration(), nullptr);
    return inspectorStyle->buildObjectForStyle();
}

RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>> InspectorCSSAgent::buildArrayForRegions(ErrorString& errorString, RefPtr<NodeList>&& regionList, int documentNodeId)
{
    auto regions = Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>::create();

    for (unsigned i = 0; i < regionList->length(); ++i) {
        Inspector::Protocol::CSS::Region::RegionOverset regionOverset;

        switch (downcast<Element>(regionList->item(i))->regionOversetState()) {
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

        auto region = Inspector::Protocol::CSS::Region::create()
            .setRegionOverset(regionOverset)
            // documentNodeId was previously asserted
            .setNodeId(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, regionList->item(i)))
            .release();

        regions->addItem(WTF::move(region));
    }

    return WTF::move(regions);
}

RefPtr<Inspector::Protocol::CSS::NamedFlow> InspectorCSSAgent::buildObjectForNamedFlow(ErrorString& errorString, WebKitNamedFlow* webkitNamedFlow, int documentNodeId)
{
    RefPtr<NodeList> contentList = webkitNamedFlow->getContent();
    auto content = Inspector::Protocol::Array<int>::create();

    for (unsigned i = 0; i < contentList->length(); ++i) {
        // documentNodeId was previously asserted
        content->addItem(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, contentList->item(i)));
    }

    return Inspector::Protocol::CSS::NamedFlow::create()
        .setDocumentNodeId(documentNodeId)
        .setName(webkitNamedFlow->name().string())
        .setOverset(webkitNamedFlow->overset())
        .setContent(WTF::move(content))
        .setRegions(buildArrayForRegions(errorString, webkitNamedFlow->getRegions(), documentNodeId))
        .release();
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
        if (Element* element = downcast<Element>(m_domAgent->nodeForId(it->key)))
            documentsToChange.add(&element->document());
    }

    m_nodeIdToForcedPseudoState.clear();
    for (HashSet<Document*>::iterator it = documentsToChange.begin(), end = documentsToChange.end(); it != end; ++it)
        (*it)->styleResolverChanged(RecalcStyleImmediately);
}

} // namespace WebCore
