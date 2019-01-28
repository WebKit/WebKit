/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "CSSParserFastPaths.h"
#include "CSSParserMode.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "FontCache.h"
#include "Frame.h"
#include "HTMLHeadElement.h"
#include "HTMLStyleElement.h"
#include "InspectorHistory.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Node.h"
#include "NodeList.h"
#include "PseudoElement.h"
#include "SVGStyleElement.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetList.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

using namespace Inspector;

enum ForcePseudoClassFlags {
    PseudoClassNone = 0,
    PseudoClassHover = 1 << 0,
    PseudoClassFocus = 1 << 1,
    PseudoClassActive = 1 << 2,
    PseudoClassVisited = 1 << 3
};

static unsigned computePseudoClassMask(const JSON::Array& pseudoClassArray)
{
    static NeverDestroyed<String> active(MAKE_STATIC_STRING_IMPL("active"));
    static NeverDestroyed<String> hover(MAKE_STATIC_STRING_IMPL("hover"));
    static NeverDestroyed<String> focus(MAKE_STATIC_STRING_IMPL("focus"));
    static NeverDestroyed<String> visited(MAKE_STATIC_STRING_IMPL("visited"));
    if (!pseudoClassArray.length())
        return PseudoClassNone;

    unsigned result = PseudoClassNone;
    for (auto& pseudoClassValue : pseudoClassArray) {
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

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    StyleSheetAction(InspectorStyleSheet* styleSheet)
        : InspectorHistory::Action()
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
        : InspectorCSSAgent::StyleSheetAction(styleSheet)
        , m_text(text)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        auto result = m_styleSheet->text();
        if (result.hasException())
            return result.releaseException();
        m_oldText = result.releaseReturnValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        auto result = m_styleSheet->setText(m_oldText);
        if (result.hasException())
            return result.releaseException();
        m_styleSheet->reparseStyleSheet(m_oldText);
        return { };
    }

    ExceptionOr<void> redo() final
    {
        auto result = m_styleSheet->setText(m_text);
        if (result.hasException())
            return result.releaseException();
        m_styleSheet->reparseStyleSheet(m_text);
        return { };
    }

    String mergeId() final
    {
        return "SetStyleSheetText " + m_styleSheet->id();
    }

    void merge(std::unique_ptr<Action> action) override
    {
        ASSERT(action->mergeId() == mergeId());
        m_text = static_cast<SetStyleSheetTextAction&>(*action).m_text;
    }

    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetStyleTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleTextAction);
public:
    SetStyleTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& text)
        : InspectorCSSAgent::StyleSheetAction(styleSheet)
        , m_cssId(cssId)
        , m_text(text)
    {
    }

    ExceptionOr<void> perform() override
    {
        return redo();
    }

    ExceptionOr<void> undo() override
    {
        return m_styleSheet->setStyleText(m_cssId, m_oldText, nullptr);
    }

    ExceptionOr<void> redo() override
    {
        return m_styleSheet->setStyleText(m_cssId, m_text, &m_oldText);
    }

    String mergeId() override
    {
        ASSERT(m_styleSheet->id() == m_cssId.styleSheetId());
        return makeString("SetStyleText ", m_styleSheet->id(), ':', m_cssId.ordinal());
    }

    void merge(std::unique_ptr<Action> action) override
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
        : InspectorCSSAgent::StyleSheetAction(styleSheet)
        , m_cssId(cssId)
        , m_selector(selector)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        auto result = m_styleSheet->ruleSelector(m_cssId);
        if (result.hasException())
            return result.releaseException();
        m_oldSelector = result.releaseReturnValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_oldSelector);
    }

    ExceptionOr<void> redo() final
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_selector);
    }

    InspectorCSSId m_cssId;
    String m_selector;
    String m_oldSelector;
};

class InspectorCSSAgent::AddRuleAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& selector)
        : InspectorCSSAgent::StyleSheetAction(styleSheet)
        , m_selector(selector)
    {
    }

    InspectorCSSId newRuleId() const { return m_newId; }

private:
    ExceptionOr<void> perform() final
    {
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_styleSheet->deleteRule(m_newId);
    }

    ExceptionOr<void> redo() final
    {
        auto result = m_styleSheet->addRule(m_selector);
        if (result.hasException())
            return result.releaseException();
        m_newId = m_styleSheet->ruleId(result.releaseReturnValue());
        return { };
    }

    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule& rule)
{
    if (!is<CSSStyleRule>(rule))
        return nullptr;
    return downcast<CSSStyleRule>(&rule);
}

InspectorCSSAgent::InspectorCSSAgent(WebAgentContext& context, InspectorDOMAgent* domAgent)
    : InspectorAgentBase("CSS"_s, context)
    , m_frontendDispatcher(std::make_unique<CSSFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(CSSBackendDispatcher::create(context.backendDispatcher, this))
    , m_domAgent(domAgent)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    ASSERT(!m_domAgent);
    reset();
}

void InspectorCSSAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorCSSAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    resetNonPersistentData();

    String unused;
    disable(unused);
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(nullptr);
    m_domAgent = nullptr;
}

void InspectorCSSAgent::reset()
{
    // FIXME: Should we be resetting on main frame navigations?
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
    m_documentToKnownCSSStyleSheets.clear();
    resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData()
{
    resetPseudoStates();
}

void InspectorCSSAgent::enable(ErrorString&)
{
    m_instrumentingAgents.setInspectorCSSAgent(this);

    for (auto* document : m_domAgent->documents())
        activeStyleSheetsUpdated(*document);
}

void InspectorCSSAgent::disable(ErrorString&)
{
    m_instrumentingAgents.setInspectorCSSAgent(nullptr);
}

void InspectorCSSAgent::documentDetached(Document& document)
{
    Vector<CSSStyleSheet*> emptyList;
    setActiveStyleSheetsForDocument(document, emptyList);

    m_documentToKnownCSSStyleSheets.remove(&document);
    m_documentToInspectorStyleSheet.remove(&document);
    m_documentsWithForcedPseudoStates.remove(&document);
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    m_frontendDispatcher->mediaQueryResultChanged();
}

void InspectorCSSAgent::activeStyleSheetsUpdated(Document& document)
{
    Vector<CSSStyleSheet*> cssStyleSheets;
    collectAllDocumentStyleSheets(document, cssStyleSheets);

    setActiveStyleSheetsForDocument(document, cssStyleSheets);
}

void InspectorCSSAgent::setActiveStyleSheetsForDocument(Document& document, Vector<CSSStyleSheet*>& activeStyleSheets)
{
    HashSet<CSSStyleSheet*>& previouslyKnownActiveStyleSheets = m_documentToKnownCSSStyleSheets.add(&document, HashSet<CSSStyleSheet*>()).iterator->value;

    HashSet<CSSStyleSheet*> removedStyleSheets(previouslyKnownActiveStyleSheets);
    Vector<CSSStyleSheet*> addedStyleSheets;
    for (auto& activeStyleSheet : activeStyleSheets) {
        if (removedStyleSheets.contains(activeStyleSheet))
            removedStyleSheets.remove(activeStyleSheet);
        else
            addedStyleSheets.append(activeStyleSheet);
    }

    for (auto* cssStyleSheet : removedStyleSheets) {
        previouslyKnownActiveStyleSheets.remove(cssStyleSheet);
        RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(cssStyleSheet);
        if (m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
            String id = unbindStyleSheet(inspectorStyleSheet.get());
            m_frontendDispatcher->styleSheetRemoved(id);
        }
    }

    for (auto* cssStyleSheet : addedStyleSheets) {
        previouslyKnownActiveStyleSheets.add(cssStyleSheet);
        if (!m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet)) {
            InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(cssStyleSheet);
            m_frontendDispatcher->styleSheetAdded(inspectorStyleSheet->buildObjectForStyleSheetInfo());
        }
    }
}

bool InspectorCSSAgent::forcePseudoState(const Element& element, CSSSelector::PseudoClassType pseudoClassType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(&element);
    if (!nodeId)
        return false;

    unsigned forcedPseudoState = m_nodeIdToForcedPseudoState.get(nodeId);
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

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString& errorString, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>>& matchedCSSRules, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>>& pseudoIdMatches, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>>& inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    Element* originalElement = element;
    PseudoId elementPseudoId = element->pseudoId();
    if (elementPseudoId != PseudoId::None) {
        element = downcast<PseudoElement>(*element).hostElement();
        if (!element) {
            errorString = "Pseudo element has no parent"_s;
            return;
        }
    }

    // Matched rules.
    StyleResolver& styleResolver = element->styleResolver();
    auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, elementPseudoId, StyleResolver::AllCSSRules);
    matchedCSSRules = buildArrayForMatchedRuleList(matchedRules, styleResolver, *element, elementPseudoId);

    if (!originalElement->isPseudoElement()) {
        // Pseudo elements.
        if (!includePseudo || *includePseudo) {
            auto pseudoElements = JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>::create();
            for (PseudoId pseudoId = PseudoId::FirstPublicPseudoId; pseudoId < PseudoId::AfterLastInternalPseudoId; pseudoId = static_cast<PseudoId>(static_cast<unsigned>(pseudoId) + 1)) {
                auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
                if (!matchedRules.isEmpty()) {
                    auto matches = Inspector::Protocol::CSS::PseudoIdMatches::create()
                        .setPseudoId(static_cast<int>(pseudoId))
                        .setMatches(buildArrayForMatchedRuleList(matchedRules, styleResolver, *element, pseudoId))
                        .release();
                    pseudoElements->addItem(WTFMove(matches));
                }
            }

            pseudoIdMatches = WTFMove(pseudoElements);
        }

        // Inherited styles.
        if (!includeInherited || *includeInherited) {
            auto entries = JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>::create();
            Element* parentElement = element->parentElement();
            while (parentElement) {
                StyleResolver& parentStyleResolver = parentElement->styleResolver();
                auto parentMatchedRules = parentStyleResolver.styleRulesForElement(parentElement, StyleResolver::AllCSSRules);
                auto entry = Inspector::Protocol::CSS::InheritedStyleEntry::create()
                    .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules, styleResolver, *parentElement, PseudoId::None))
                    .release();
                if (is<StyledElement>(*parentElement) && downcast<StyledElement>(*parentElement).cssomStyle().length()) {
                    auto& styleSheet = asInspectorStyleSheet(downcast<StyledElement>(*parentElement));
                    entry->setInlineStyle(styleSheet.buildObjectForStyle(styleSheet.styleForId(InspectorCSSId(styleSheet.id(), 0))));
                }

                entries->addItem(WTFMove(entry));
                parentElement = parentElement->parentElement();
            }

            inheritedEntries = WTFMove(entries);
        }
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString& errorString, int nodeId, RefPtr<Inspector::Protocol::CSS::CSSStyle>& inlineStyle, RefPtr<Inspector::Protocol::CSS::CSSStyle>& attributesStyle)
{
    auto* element = elementForId(errorString, nodeId);
    if (!is<StyledElement>(element))
        return;

    auto& styledElement = downcast<StyledElement>(*element);
    auto& styleSheet = asInspectorStyleSheet(styledElement);
    inlineStyle = styleSheet.buildObjectForStyle(&styledElement.cssomStyle());
    if (auto attributes = buildObjectForAttributesStyle(styledElement))
        attributesStyle = WTFMove(attributes);
    else
        attributesStyle = nullptr;
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString& errorString, int nodeId, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>>& style)
{
    auto* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    auto computedStyleInfo = CSSComputedStyleDeclaration::create(*element, true);
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), WTFMove(computedStyleInfo), nullptr);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::getAllStyleSheets(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>>& styleInfos)
{
    styleInfos = JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>::create();

    Vector<InspectorStyleSheet*> inspectorStyleSheets;
    collectAllStyleSheets(inspectorStyleSheets);
    for (auto* inspectorStyleSheet : inspectorStyleSheets)
        styleInfos->addItem(inspectorStyleSheet->buildObjectForStyleSheetInfo());
}

void InspectorCSSAgent::collectAllStyleSheets(Vector<InspectorStyleSheet*>& result)
{
    Vector<CSSStyleSheet*> cssStyleSheets;
    for (auto* document : m_domAgent->documents())
        collectAllDocumentStyleSheets(*document, cssStyleSheets);

    for (auto* cssStyleSheet : cssStyleSheets)
        result.append(bindStyleSheet(cssStyleSheet));
}

void InspectorCSSAgent::collectAllDocumentStyleSheets(Document& document, Vector<CSSStyleSheet*>& result)
{
    auto cssStyleSheets = document.styleScope().activeStyleSheetsForInspector();
    for (auto& cssStyleSheet : cssStyleSheets)
        collectStyleSheets(cssStyleSheet.get(), result);
}

void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, Vector<CSSStyleSheet*>& result)
{
    result.append(styleSheet);

    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (is<CSSImportRule>(*rule)) {
            if (CSSStyleSheet* importedStyleSheet = downcast<CSSImportRule>(*rule).styleSheet())
                collectStyleSheets(importedStyleSheet, result);
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

    auto text = inspectorStyleSheet->text();
    if (!text.hasException())
        *result = text.releaseReturnValue();
}

void InspectorCSSAgent::setStyleSheetText(ErrorString& errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    auto result = m_domAgent->history()->perform(std::make_unique<SetStyleSheetTextAction>(inspectorStyleSheet, text));
    if (result.hasException())
        errorString = InspectorDOMAgent::toErrorString(result.releaseException());
}

void InspectorCSSAgent::setStyleText(ErrorString& errorString, const JSON::Object& fullStyleId, const String& text, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    auto performResult = m_domAgent->history()->perform(std::make_unique<SetStyleTextAction>(inspectorStyleSheet, compoundId, text));
    if (performResult.hasException()) {
        errorString = InspectorDOMAgent::toErrorString(performResult.releaseException());
        return;
    }

    result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
}

void InspectorCSSAgent::setRuleSelector(ErrorString& errorString, const JSON::Object& fullRuleId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    InspectorCSSId compoundId(fullRuleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    auto performResult = m_domAgent->history()->perform(std::make_unique<SetRuleSelectorAction>(inspectorStyleSheet, compoundId, selector));
    if (performResult.hasException()) {
        errorString = InspectorDOMAgent::toErrorString(performResult.releaseException());
        return;
    }

    result = inspectorStyleSheet->buildObjectForRule(inspectorStyleSheet->ruleForId(compoundId), nullptr);
}

void InspectorCSSAgent::createStyleSheet(ErrorString& errorString, const String& frameId, String* styleSheetId)
{
    Frame* frame = m_domAgent->pageAgent()->frameForId(frameId);
    if (!frame) {
        errorString = "No frame for given id found"_s;
        return;
    }

    Document* document = frame->document();
    if (!document) {
        errorString = "No document for frame"_s;
        return;
    }

    InspectorStyleSheet* inspectorStyleSheet = createInspectorStyleSheetForDocument(*document);
    if (!inspectorStyleSheet) {
        errorString = "Could not create stylesheet for the frame."_s;
        return;
    }

    *styleSheetId = inspectorStyleSheet->id();
}

InspectorStyleSheet* InspectorCSSAgent::createInspectorStyleSheetForDocument(Document& document)
{
    if (!document.isHTMLDocument() && !document.isSVGDocument())
        return nullptr;

    auto styleElement = HTMLStyleElement::create(document);
    styleElement->setAttributeWithoutSynchronization(HTMLNames::typeAttr, AtomicString("text/css", AtomicString::ConstructFromLiteral));

    ContainerNode* targetNode;
    // HEAD is absent in ImageDocuments, for example.
    if (auto* head = document.head())
        targetNode = head;
    else if (auto* body = document.bodyOrFrameset())
        targetNode = body;
    else
        return nullptr;

    // Inserting this <style> into the document will trigger activeStyleSheetsUpdated
    // and we will create an InspectorStyleSheet for this <style>'s CSSStyleSheet.
    // Set this flag, so when we create it, we put it into the via inspector map.
    m_creatingViaInspectorStyleSheet = true;
    InlineStyleOverrideScope overrideScope(document);
    auto appendResult = targetNode->appendChild(styleElement);
    document.styleScope().flushPendingUpdate();
    m_creatingViaInspectorStyleSheet = false;
    if (appendResult.hasException())
        return nullptr;

    auto iterator = m_documentToInspectorStyleSheet.find(&document);
    ASSERT(iterator != m_documentToInspectorStyleSheet.end());
    if (iterator == m_documentToInspectorStyleSheet.end())
        return nullptr;

    auto& inspectorStyleSheetsForDocument = iterator->value;
    ASSERT(!inspectorStyleSheetsForDocument.isEmpty());
    if (inspectorStyleSheetsForDocument.isEmpty())
        return nullptr;

    return inspectorStyleSheetsForDocument.last().get();
}

void InspectorCSSAgent::addRule(ErrorString& errorString, const String& styleSheetId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet) {
        errorString = "No target stylesheet found"_s;
        return;
    }

    auto action = std::make_unique<AddRuleAction>(inspectorStyleSheet, selector);
    auto& rawAction = *action;
    auto performResult = m_domAgent->history()->perform(WTFMove(action));
    if (performResult.hasException()) {
        errorString = InspectorDOMAgent::toErrorString(performResult.releaseException());
        return;
    }

    InspectorCSSId ruleId = rawAction.newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule, nullptr);
}

void InspectorCSSAgent::getSupportedCSSProperties(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>>& cssProperties)
{
    auto properties = JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>::create();
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
        CSSPropertyID propertyID = convertToCSSPropertyID(i);
        if (isInternalCSSProperty(propertyID) || !isEnabledCSSProperty(propertyID))
            continue;

        auto property = Inspector::Protocol::CSS::CSSPropertyInfo::create()
            .setName(getPropertyNameString(propertyID))
            .release();

        auto aliases = CSSProperty::aliasesForProperty(propertyID);
        if (!aliases.isEmpty()) {
            auto aliasesArray = JSON::ArrayOf<String>::create();
            for (auto& alias : aliases)
                aliasesArray->addItem(alias);
            property->setAliases(WTFMove(aliasesArray));
        }

        const StylePropertyShorthand& shorthand = shorthandForProperty(propertyID);
        if (shorthand.length()) {
            auto longhands = JSON::ArrayOf<String>::create();
            for (unsigned j = 0; j < shorthand.length(); ++j) {
                CSSPropertyID longhandID = shorthand.properties()[j];
                if (isEnabledCSSProperty(longhandID))
                    longhands->addItem(getPropertyNameString(longhandID));
            }
            property->setLonghands(WTFMove(longhands));
        }

        if (CSSParserFastPaths::isKeywordPropertyID(propertyID)) {
            auto values = JSON::ArrayOf<String>::create();
            for (int j = firstCSSValueKeyword; j <= lastCSSValueKeyword; ++j) {
                CSSValueID valueID = convertToCSSValueID(j);
                if (CSSParserFastPaths::isValidKeywordPropertyAndValue(propertyID, valueID, HTMLStandardMode))
                    values->addItem(getValueNameString(valueID));
            }
            if (values->length())
                property->setValues(WTFMove(values));
        }

        if (CSSProperty::isInheritedProperty(propertyID))
            property->setInherited(true);

        properties->addItem(WTFMove(property));
    }
    cssProperties = WTFMove(properties);
}

void InspectorCSSAgent::getSupportedSystemFontFamilyNames(ErrorString&, RefPtr<JSON::ArrayOf<String>>& fontFamilyNames)
{
    auto families = JSON::ArrayOf<String>::create();

    Vector<String> systemFontFamilies = FontCache::singleton().systemFontFamilies();
    for (const auto& familyName : systemFontFamilies)
        families->addItem(familyName);

    fontFamilyNames = WTFMove(families);
}

void InspectorCSSAgent::forcePseudoState(ErrorString& errorString, int nodeId, const JSON::Array& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    // Return early if the forced pseudo state was already set correctly.
    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses);
    if (forcedPseudoState) {
        auto iterator = m_nodeIdToForcedPseudoState.add(nodeId, 0).iterator;
        if (forcedPseudoState == iterator->value)
            return;
        iterator->value = forcedPseudoState;
        m_documentsWithForcedPseudoStates.add(&element->document());
    } else {
        if (!m_nodeIdToForcedPseudoState.remove(nodeId))
            return;
        if (m_nodeIdToForcedPseudoState.isEmpty())
            m_documentsWithForcedPseudoStates.clear();
    }

    element->document().styleScope().didChangeStyleSheetEnvironment();
}

InspectorStyleSheetForInlineStyle& InspectorCSSAgent::asInspectorStyleSheet(StyledElement& element)
{
    return m_nodeToInspectorStyleSheet.ensure(&element, [this, &element] {
        String newStyleSheetId = String::number(m_lastStyleSheetId++);
        auto inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(m_domAgent->pageAgent(), newStyleSheetId, element, Inspector::Protocol::CSS::StyleSheetOrigin::Regular, this);
        m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet.copyRef());
        return inspectorStyleSheet;
    }).iterator->value;
}

Element* InspectorCSSAgent::elementForId(ErrorString& errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        errorString = "No node with given id found"_s;
        return nullptr;
    }
    if (!is<Element>(*node)) {
        errorString = "Not an element node"_s;
        return nullptr;
    }
    return downcast<Element>(node);
}

String InspectorCSSAgent::unbindStyleSheet(InspectorStyleSheet* inspectorStyleSheet)
{
    String id = inspectorStyleSheet->id();
    m_idToInspectorStyleSheet.remove(id);
    if (inspectorStyleSheet->pageStyleSheet())
        m_cssStyleSheetToInspectorStyleSheet.remove(inspectorStyleSheet->pageStyleSheet());
    return id;
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
        if (m_creatingViaInspectorStyleSheet) {
            auto& inspectorStyleSheetsForDocument = m_documentToInspectorStyleSheet.add(document, Vector<RefPtr<InspectorStyleSheet>>()).iterator->value;
            inspectorStyleSheetsForDocument.append(inspectorStyleSheet);
        }
    }
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::assertStyleSheetForId(ErrorString& errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        errorString = "No stylesheet with given id found"_s;
        return nullptr;
    }
    return it->value.get();
}

Inspector::Protocol::CSS::StyleSheetOrigin InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return Inspector::Protocol::CSS::StyleSheetOrigin::Inspector;

    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        return Inspector::Protocol::CSS::StyleSheetOrigin::UserAgent;

    if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->nodeName() == "#document")
        return Inspector::Protocol::CSS::StyleSheetOrigin::User;

    auto iterator = m_documentToInspectorStyleSheet.find(ownerDocument);
    if (iterator != m_documentToInspectorStyleSheet.end()) {
        for (auto& inspectorStyleSheet : iterator->value) {
            if (pageStyleSheet == inspectorStyleSheet->pageStyleSheet())
                return Inspector::Protocol::CSS::StyleSheetOrigin::Inspector;
        }
    }

    return Inspector::Protocol::CSS::StyleSheetOrigin::Regular;
}

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(StyleRule* styleRule, StyleResolver& styleResolver, Element& element)
{
    if (!styleRule)
        return nullptr;

    // StyleRules returned by StyleResolver::styleRulesForElement lack parent pointers since that infomation is not cheaply available.
    // Since the inspector wants to walk the parent chain, we construct the full wrappers here.
    styleResolver.inspectorCSSOMWrappers().collectDocumentWrappers(styleResolver.document().extensionStyleSheets());
    styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(Style::Scope::forNode(element));

    // Possiblity of :host styles if this element has a shadow root.
    if (ShadowRoot* shadowRoot = element.shadowRoot())
        styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(shadowRoot->styleScope());

    CSSStyleRule* cssomWrapper = styleResolver.inspectorCSSOMWrappers().getWrapperForRuleInSheets(styleRule);
    if (!cssomWrapper)
        return nullptr;

    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(cssomWrapper->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(cssomWrapper, &element) : nullptr;
}

RefPtr<Inspector::Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    ASSERT(rule->parentStyleSheet());
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(rule->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(rule, nullptr) : nullptr;
}

RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>> InspectorCSSAgent::buildArrayForMatchedRuleList(const Vector<RefPtr<StyleRule>>& matchedRules, StyleResolver& styleResolver, Element& element, PseudoId pseudoId)
{
    auto result = JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>::create();

    SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRules);
    context.pseudoId = pseudoId != PseudoId::None ? pseudoId : element.pseudoId();
    SelectorChecker selectorChecker(element.document());

    for (auto& matchedRule : matchedRules) {
        RefPtr<Inspector::Protocol::CSS::CSSRule> ruleObject = buildObjectForRule(matchedRule.get(), styleResolver, element);
        if (!ruleObject)
            continue;

        auto matchingSelectors = JSON::ArrayOf<int>::create();
        const CSSSelectorList& selectorList = matchedRule->selectorList();
        int index = 0;
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            unsigned ignoredSpecificity;
            bool matched = selectorChecker.match(*selector, element, context, ignoredSpecificity);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }

        auto match = Inspector::Protocol::CSS::RuleMatch::create()
            .setRule(WTFMove(ruleObject))
            .setMatchingSelectors(WTFMove(matchingSelectors))
            .release();
        result->addItem(WTFMove(match));
    }

    return WTFMove(result);
}

RefPtr<Inspector::Protocol::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(StyledElement& element)
{
    // FIXME: Ugliness below.
    auto* attributeStyle = const_cast<StyleProperties*>(element.presentationAttributeStyle());
    if (!attributeStyle)
        return nullptr;

    auto& mutableAttributeStyle = downcast<MutableStyleProperties>(*attributeStyle);
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle.ensureCSSStyleDeclaration(), nullptr);
    return inspectorStyle->buildObjectForStyle();
}

void InspectorCSSAgent::didRemoveDOMNode(Node& node, int nodeId)
{
    m_nodeIdToForcedPseudoState.remove(nodeId);

    auto sheet = m_nodeToInspectorStyleSheet.take(&node);
    if (!sheet)
        return;
    m_idToInspectorStyleSheet.remove(sheet.value()->id());
}

void InspectorCSSAgent::didModifyDOMAttr(Element& element)
{
    auto sheet = m_nodeToInspectorStyleSheet.get(&element);
    if (!sheet)
        return;
    sheet->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheet* styleSheet)
{
    m_frontendDispatcher->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::resetPseudoStates()
{
    for (auto& document : m_documentsWithForcedPseudoStates)
        document->styleScope().didChangeStyleSheetEnvironment();

    m_nodeIdToForcedPseudoState.clear();
    m_documentsWithForcedPseudoStates.clear();
}

} // namespace WebCore
