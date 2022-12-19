/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "CommonAtomStrings.h"
#include "ContainerNode.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "ElementAncestorIterator.h"
#include "ElementChildIterator.h"
#include "ElementRareData.h"
#include "EventTarget.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "FontPlatformData.h"
#include "Frame.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLStyleElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorHistory.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "Node.h"
#include "NodeList.h"
#include "PseudoElement.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"
#include "SVGStyleElement.h"
#include "SelectorChecker.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

using namespace Inspector;

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

class InspectorCSSAgent::SetRuleHeaderTextAction final : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleHeaderTextAction);
public:
    SetRuleHeaderTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& newHeaderText)
        : InspectorCSSAgent::StyleSheetAction(styleSheet)
        , m_cssId(cssId)
        , m_newHeaderText(newHeaderText)
    {
    }

private:
    ExceptionOr<void> perform() final
    {
        auto result = m_styleSheet->ruleHeaderText(m_cssId);
        if (result.hasException())
            return result.releaseException();
        m_oldHeaderText = result.releaseReturnValue();
        return redo();
    }

    ExceptionOr<void> undo() final
    {
        return m_styleSheet->setRuleHeaderText(m_cssId, m_oldHeaderText);
    }

    ExceptionOr<void> redo() final
    {
        return m_styleSheet->setRuleHeaderText(m_cssId, m_newHeaderText);
    }

    InspectorCSSId m_cssId;
    String m_newHeaderText;
    String m_oldHeaderText;
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
        m_newId = m_styleSheet->ruleOrStyleId(result.releaseReturnValue());
        return { };
    }

    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

InspectorCSSAgent::InspectorCSSAgent(PageAgentContext& context)
    : InspectorAgentBase("CSS"_s, context)
    , m_frontendDispatcher(makeUnique<CSSFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(CSSBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
    , m_nodesWithPendingLayoutFlagsChangeDispatchTimer(*this, &InspectorCSSAgent::nodesWithPendingLayoutFlagsChangeDispatchTimerFired)
{
}

InspectorCSSAgent::~InspectorCSSAgent() = default;

void InspectorCSSAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorCSSAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

void InspectorCSSAgent::reset()
{
    // FIXME: Should we be resetting on main frame navigations?
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
    m_documentToKnownCSSStyleSheets.clear();
    m_lastLayoutFlagsForNode.clear();
    m_nodesWithPendingLayoutFlagsChange.clear();
    if (m_nodesWithPendingLayoutFlagsChangeDispatchTimer.isActive())
        m_nodesWithPendingLayoutFlagsChangeDispatchTimer.stop();
    m_layoutContextTypeChangedMode = Protocol::CSS::LayoutContextTypeChangedMode::Observed;
    resetPseudoStates();
}

Protocol::ErrorStringOr<void> InspectorCSSAgent::enable()
{
    if (m_instrumentingAgents.enabledCSSAgent() == this)
        return { };

    m_instrumentingAgents.setEnabledCSSAgent(this);

    if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
        for (auto* document : domAgent->documents())
            activeStyleSheetsUpdated(*document);
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorCSSAgent::disable()
{
    m_instrumentingAgents.setEnabledCSSAgent(nullptr);

    reset();

    return { };
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
            auto id = unbindStyleSheet(inspectorStyleSheet.get());
            m_frontendDispatcher->styleSheetRemoved(id);
        }
    }

    for (auto* cssStyleSheet : addedStyleSheets) {
        previouslyKnownActiveStyleSheets.add(cssStyleSheet);
        if (!m_cssStyleSheetToInspectorStyleSheet.contains(cssStyleSheet)) {
            InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(cssStyleSheet);
            if (auto header = inspectorStyleSheet->buildObjectForStyleSheetInfo())
                m_frontendDispatcher->styleSheetAdded(header.releaseNonNull());
        }
    }
}

bool InspectorCSSAgent::forcePseudoState(const Element& element, CSSSelector::PseudoClassType pseudoClassType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return false;

    auto nodeId = domAgent->boundNodeId(&element);
    if (!nodeId)
        return false;

    return m_nodeIdToForcedPseudoState.get(nodeId).contains(pseudoClassType);
}

std::optional<Protocol::CSS::PseudoId> InspectorCSSAgent::protocolValueForPseudoId(PseudoId pseudoId)
{
    switch (pseudoId) {
    case PseudoId::FirstLine:
        return Protocol::CSS::PseudoId::FirstLine;
    case PseudoId::FirstLetter:
        return Protocol::CSS::PseudoId::FirstLetter;
    case PseudoId::Marker:
        return Protocol::CSS::PseudoId::Marker;
    case PseudoId::Backdrop:
        return Protocol::CSS::PseudoId::Backdrop;
    case PseudoId::Before:
        return Protocol::CSS::PseudoId::Before;
    case PseudoId::After:
        return Protocol::CSS::PseudoId::After;
    case PseudoId::Selection:
        return Protocol::CSS::PseudoId::Selection;
    case PseudoId::Highlight:
        return Protocol::CSS::PseudoId::Highlight;
    case PseudoId::Scrollbar:
        return Protocol::CSS::PseudoId::Scrollbar;
    case PseudoId::ScrollbarThumb:
        return Protocol::CSS::PseudoId::ScrollbarThumb;
    case PseudoId::ScrollbarButton:
        return Protocol::CSS::PseudoId::ScrollbarButton;
    case PseudoId::ScrollbarTrack:
        return Protocol::CSS::PseudoId::ScrollbarTrack;
    case PseudoId::ScrollbarTrackPiece:
        return Protocol::CSS::PseudoId::ScrollbarTrackPiece;
    case PseudoId::ScrollbarCorner:
        return Protocol::CSS::PseudoId::ScrollbarCorner;
    case PseudoId::Resizer:
        return Protocol::CSS::PseudoId::Resizer;

    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

Protocol::ErrorStringOr<std::tuple<RefPtr<JSON::ArrayOf<Protocol::CSS::RuleMatch>>, RefPtr<JSON::ArrayOf<Protocol::CSS::PseudoIdMatches>>, RefPtr<JSON::ArrayOf<Protocol::CSS::InheritedStyleEntry>>>> InspectorCSSAgent::getMatchedStylesForNode(Protocol::DOM::NodeId nodeId, std::optional<bool>&& includePseudo, std::optional<bool>&& includeInherited)
{
    Protocol::ErrorString errorString;

    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!element->isConnected())
        return makeUnexpected("Element for given nodeId was not connected to DOM tree."_s);

    Element* originalElement = element;
    PseudoId elementPseudoId = element->pseudoId();
    if (elementPseudoId != PseudoId::None) {
        element = downcast<PseudoElement>(*element).hostElement();
        if (!element)
            return makeUnexpected("Missing parent of pseudo-element node for given nodeId"_s);
    }

    // Matched rules.
    auto& styleResolver = element->styleResolver();
    auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, elementPseudoId, Style::Resolver::AllCSSRules);
    auto matchedCSSRules = buildArrayForMatchedRuleList(matchedRules, styleResolver, *element, elementPseudoId);
    RefPtr<JSON::ArrayOf<Protocol::CSS::PseudoIdMatches>> pseudoElements;
    RefPtr<JSON::ArrayOf<Protocol::CSS::InheritedStyleEntry>> inherited;

    if (!originalElement->isPseudoElement()) {
        if (!includePseudo || *includePseudo) {
            pseudoElements = JSON::ArrayOf<Protocol::CSS::PseudoIdMatches>::create();
            for (PseudoId pseudoId = PseudoId::FirstPublicPseudoId; pseudoId < PseudoId::AfterLastInternalPseudoId; pseudoId = static_cast<PseudoId>(static_cast<unsigned>(pseudoId) + 1)) {
                // `*::marker` selectors are only applicable to elements with `display: list-item`.
                if (pseudoId == PseudoId::Marker && element->computedStyle()->display() != DisplayType::ListItem)
                    continue;
                if (auto protocolPseudoId = protocolValueForPseudoId(pseudoId)) {
                    auto matchedRules = styleResolver.pseudoStyleRulesForElement(element, pseudoId, Style::Resolver::AllCSSRules);
                    if (!matchedRules.isEmpty()) {
                        auto matches = Protocol::CSS::PseudoIdMatches::create()
                            .setPseudoId(protocolPseudoId.value())
                            .setMatches(buildArrayForMatchedRuleList(matchedRules, styleResolver, *element, pseudoId))
                            .release();
                        pseudoElements->addItem(WTFMove(matches));
                    }
                }
            }
        }

        if (!includeInherited || *includeInherited) {
            inherited = JSON::ArrayOf<Protocol::CSS::InheritedStyleEntry>::create();
            for (auto& ancestor : ancestorsOfType<Element>(*element)) {
                auto& parentStyleResolver = ancestor.styleResolver();
                auto parentMatchedRules = parentStyleResolver.styleRulesForElement(&ancestor, Style::Resolver::AllCSSRules);
                auto entry = Protocol::CSS::InheritedStyleEntry::create()
                    .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules, styleResolver, ancestor, PseudoId::None))
                    .release();
                if (is<StyledElement>(ancestor) && downcast<StyledElement>(ancestor).cssomStyle().length()) {
                    auto& styleSheet = asInspectorStyleSheet(downcast<StyledElement>(ancestor));
                    entry->setInlineStyle(styleSheet.buildObjectForStyle(styleSheet.styleForId(InspectorCSSId(styleSheet.id(), 0))));
                }
                inherited->addItem(WTFMove(entry));
            }
        }
    }

    return { { WTFMove(matchedCSSRules), WTFMove(pseudoElements), WTFMove(inherited) } };
}

Protocol::ErrorStringOr<std::tuple<RefPtr<Protocol::CSS::CSSStyle> /* inlineStyle */, RefPtr<Protocol::CSS::CSSStyle> /* attributesStyle */>> InspectorCSSAgent::getInlineStylesForNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    auto* element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!is<StyledElement>(element))
        return { { nullptr, nullptr } };

    auto& styledElement = downcast<StyledElement>(*element);
    auto& styleSheet = asInspectorStyleSheet(styledElement);
    return { { styleSheet.buildObjectForStyle(&styledElement.cssomStyle()), buildObjectForAttributesStyle(styledElement) } };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::CSS::CSSComputedStyleProperty>>> InspectorCSSAgent::getComputedStyleForNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    auto* element = elementForId(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    if (!element->isConnected())
        return makeUnexpected("Element for given nodeId was not connected to DOM tree."_s);

    auto computedStyleInfo = CSSComputedStyleDeclaration::create(*element, true);
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), WTFMove(computedStyleInfo), nullptr);
    return inspectorStyle->buildArrayForComputedStyle();
}

static Ref<Protocol::CSS::Font> buildObjectForFont(const Font& font)
{
    auto& fontPlatformData = font.platformData();
    
    auto resultVariationAxes = JSON::ArrayOf<Protocol::CSS::FontVariationAxis>::create();
    for (auto& variationAxis : fontPlatformData.variationAxes(ShouldLocalizeAxisNames::Yes)) {
        auto axis = Protocol::CSS::FontVariationAxis::create()
            .setTag(variationAxis.tag())
            .setMinimumValue(variationAxis.minimumValue())
            .setMaximumValue(variationAxis.maximumValue())
            .setDefaultValue(variationAxis.defaultValue())
            .release();
        
        if (variationAxis.name().length() && variationAxis.name() != variationAxis.tag())
            axis->setName(variationAxis.name());
        
        resultVariationAxes->addItem(WTFMove(axis));
    }

    auto protocolFont = Protocol::CSS::Font::create()
        .setDisplayName(font.platformData().familyName())
        .setVariationAxes(WTFMove(resultVariationAxes))
        .release();

    protocolFont->setSynthesizedBold(fontPlatformData.syntheticBold());
    protocolFont->setSynthesizedOblique(fontPlatformData.syntheticOblique());

    return protocolFont;
}

Protocol::ErrorStringOr<Ref<Protocol::CSS::Font>> InspectorCSSAgent::getFontDataForNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;
    auto* node = nodeForId(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    
    auto* computedStyle = node->computedStyle();
    if (!computedStyle)
        return makeUnexpected("No computed style for node."_s);
    
    return buildObjectForFont(computedStyle->fontCascade().primaryFont());
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::CSS::CSSStyleSheetHeader>>> InspectorCSSAgent::getAllStyleSheets()
{
    auto headers = JSON::ArrayOf<Protocol::CSS::CSSStyleSheetHeader>::create();

    Vector<InspectorStyleSheet*> inspectorStyleSheets;
    collectAllStyleSheets(inspectorStyleSheets);
    for (auto* inspectorStyleSheet : inspectorStyleSheets) {
        if (auto header = inspectorStyleSheet->buildObjectForStyleSheetInfo())
            headers->addItem(header.releaseNonNull());
    }

    return headers;
}

void InspectorCSSAgent::collectAllStyleSheets(Vector<InspectorStyleSheet*>& result)
{
    Vector<CSSStyleSheet*> cssStyleSheets;
    if (auto* domAgent = m_instrumentingAgents.persistentDOMAgent()) {
        for (auto* document : domAgent->documents())
            collectAllDocumentStyleSheets(*document, cssStyleSheets);
    }

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
        if (auto* rule = dynamicDowncast<CSSImportRule>(styleSheet->item(i))) {
            if (CSSStyleSheet* importedStyleSheet = rule->styleSheet())
                collectStyleSheets(importedStyleSheet, result);
        }
    }
}

Protocol::ErrorStringOr<Ref<Protocol::CSS::CSSStyleSheetBody>> InspectorCSSAgent::getStyleSheet(const Protocol::CSS::StyleSheetId& styleSheetId)
{
    Protocol::ErrorString errorString;

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto styleSheet = inspectorStyleSheet->buildObjectForStyleSheet();
    if (!styleSheet)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return styleSheet.releaseNonNull();
}

Protocol::ErrorStringOr<String> InspectorCSSAgent::getStyleSheetText(const Protocol::CSS::StyleSheetId& styleSheetId)
{
    Protocol::ErrorString errorString;

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto text = inspectorStyleSheet->text();
    if (text.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(text.releaseException()));

    return text.releaseReturnValue();
}

Protocol::ErrorStringOr<void> InspectorCSSAgent::setStyleSheetText(const Protocol::CSS::StyleSheetId& styleSheetId, const String& text)
{
    Protocol::ErrorString errorString;

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto result = domAgent->history()->perform(makeUnique<SetStyleSheetTextAction>(inspectorStyleSheet, text));
    if (result.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(result.releaseException()));

    return { };
}

Protocol::ErrorStringOr<Ref<Protocol::CSS::CSSStyle>> InspectorCSSAgent::setStyleText(Ref<JSON::Object>&& styleId, const String& text)
{
    Protocol::ErrorString errorString;

    InspectorCSSId compoundId(styleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetStyleTextAction>(inspectorStyleSheet, compoundId, text));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    return inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
}

Protocol::ErrorStringOr<Ref<Protocol::CSS::CSSRule>> InspectorCSSAgent::setRuleSelector(Ref<JSON::Object>&& ruleId, const String& selector)
{
    Protocol::ErrorString errorString;

    InspectorCSSId compoundId(ruleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetRuleHeaderTextAction>(inspectorStyleSheet, compoundId, selector));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    auto rule = inspectorStyleSheet->buildObjectForRule(dynamicDowncast<CSSStyleRule>(inspectorStyleSheet->ruleForId(compoundId)));
    if (!rule)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return rule.releaseNonNull();
}

Protocol::ErrorStringOr<Ref<Protocol::CSS::Grouping>> InspectorCSSAgent::setGroupingHeaderText(Ref<JSON::Object>&& ruleId, const String& headerText)
{
    Protocol::ErrorString errorString;

    InspectorCSSId compoundId(WTFMove(ruleId));
    ASSERT(!compoundId.isEmpty());

    auto* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto performResult = domAgent->history()->perform(makeUnique<SetRuleHeaderTextAction>(inspectorStyleSheet, compoundId, headerText));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    if (auto rule = inspectorStyleSheet->buildObjectForGrouping(inspectorStyleSheet->ruleForId(compoundId)))
        return rule.releaseNonNull();

    ASSERT_NOT_REACHED();
    return makeUnexpected("Internal error: missing grouping payload"_s);
}


Protocol::ErrorStringOr<Protocol::CSS::StyleSheetId> InspectorCSSAgent::createStyleSheet(const Protocol::Network::FrameId& frameId)
{
    Protocol::ErrorString errorString;

    auto* pageAgent = m_instrumentingAgents.enabledPageAgent();
    if (!pageAgent)
        return makeUnexpected("Page domain must be enabled"_s);

    auto* frame = pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    Document* document = frame->document();
    if (!document)
        return makeUnexpected("Missing document of frame for given frameId"_s);

    InspectorStyleSheet* inspectorStyleSheet = createInspectorStyleSheetForDocument(*document);
    if (!inspectorStyleSheet)
        return makeUnexpected("Could not create style sheet for document of frame for given frameId"_s);

    return inspectorStyleSheet->id();
}

InspectorStyleSheet* InspectorCSSAgent::createInspectorStyleSheetForDocument(Document& document)
{
    if (!document.isHTMLDocument() && !document.isSVGDocument())
        return nullptr;

    auto styleElement = HTMLStyleElement::create(document);
    styleElement->setAttributeWithoutSynchronization(HTMLNames::typeAttr, cssContentTypeAtom());

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

Protocol::ErrorStringOr<Ref<Protocol::CSS::CSSRule>> InspectorCSSAgent::addRule(const Protocol::CSS::StyleSheetId& styleSheetId, const String& selector)
{
    Protocol::ErrorString errorString;

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return makeUnexpected(errorString);

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    auto action = makeUnique<AddRuleAction>(inspectorStyleSheet, selector);
    auto& rawAction = *action;
    auto performResult = domAgent->history()->perform(WTFMove(action));
    if (performResult.hasException())
        return makeUnexpected(InspectorDOMAgent::toErrorString(performResult.releaseException()));

    auto rule = inspectorStyleSheet->buildObjectForRule(dynamicDowncast<CSSStyleRule>(inspectorStyleSheet->ruleForId(rawAction.newRuleId())));
    if (!rule)
        return makeUnexpected("Internal error: missing style sheet"_s);

    return rule.releaseNonNull();
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::CSS::CSSPropertyInfo>>> InspectorCSSAgent::getSupportedCSSProperties()
{
    auto cssProperties = JSON::ArrayOf<Protocol::CSS::CSSPropertyInfo>::create();

    for (auto propertyID : allCSSProperties()) {
        if (!isExposed(propertyID, &m_inspectedPage.settings()))
            continue;

        auto property = Protocol::CSS::CSSPropertyInfo::create()
            .setName(nameString(propertyID))
            .release();

        auto aliases = CSSProperty::aliasesForProperty(propertyID);
        if (!aliases.isEmpty()) {
            auto aliasesArray = JSON::ArrayOf<String>::create();
            for (auto& alias : aliases)
                aliasesArray->addItem(alias);
            property->setAliases(WTFMove(aliasesArray));
        }

        auto shorthand = shorthandForProperty(propertyID);
        if (shorthand.length()) {
            auto longhands = JSON::ArrayOf<String>::create();
            for (auto longhand : shorthand) {
                if (isExposed(longhand, &m_inspectedPage.settings()))
                    longhands->addItem(nameString(longhand));
            }
            if (longhands->length())
                property->setLonghands(WTFMove(longhands));
        }

        if (CSSParserFastPaths::isKeywordFastPathEligibleStyleProperty(propertyID)) {
            auto values = JSON::ArrayOf<String>::create();
            for (auto valueID : allCSSValueKeywords()) {
                if (CSSParserFastPaths::isKeywordValidForStyleProperty(propertyID, valueID, strictCSSParserContext()))
                    values->addItem(nameString(valueID));
            }
            if (values->length())
                property->setValues(WTFMove(values));
        }

        if (CSSProperty::isInheritedProperty(propertyID))
            property->setInherited(true);

        cssProperties->addItem(WTFMove(property));
    }

    return cssProperties;
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> InspectorCSSAgent::getSupportedSystemFontFamilyNames()
{
    auto fontFamilyNames = JSON::ArrayOf<String>::create();

    Vector<String> systemFontFamilies = FontCache::forCurrentThread().systemFontFamilies();
    for (const auto& familyName : systemFontFamilies)
        fontFamilyNames->addItem(familyName);

    return fontFamilyNames;
}

Protocol::ErrorStringOr<void> InspectorCSSAgent::forcePseudoState(Protocol::DOM::NodeId nodeId, Ref<JSON::Array>&& forcedPseudoClasses)
{
    Protocol::ErrorString errorString;

    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    Element* element = domAgent->assertElement(errorString, nodeId);
    if (!element)
        return makeUnexpected(errorString);

    PseudoClassHashSet forcedPseudoClassesToSet;
    for (const auto& pseudoClassValue : forcedPseudoClasses.get()) {
        auto pseudoClassString = pseudoClassValue->asString();
        if (!pseudoClassString)
            return makeUnexpected("Unexpected non-string value in given forcedPseudoClasses"_s);

        auto pseudoClass = Protocol::Helpers::parseEnumValueFromString<Protocol::CSS::ForceablePseudoClass>(pseudoClassString);
        if (!pseudoClass)
            return makeUnexpected(makeString("Unknown forcedPseudoClass: ", pseudoClassString));

        switch (*pseudoClass) {
        case Protocol::CSS::ForceablePseudoClass::Active:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassActive);
            break;

        case Protocol::CSS::ForceablePseudoClass::Hover:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassHover);
            break;

        case Protocol::CSS::ForceablePseudoClass::Focus:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassFocus);
            break;

        case Protocol::CSS::ForceablePseudoClass::FocusVisible:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassFocusVisible);
            break;

        case Protocol::CSS::ForceablePseudoClass::FocusWithin:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassFocusWithin);
            break;

        case Protocol::CSS::ForceablePseudoClass::Target:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassTarget);
            break;

        case Protocol::CSS::ForceablePseudoClass::Visited:
            forcedPseudoClassesToSet.add(CSSSelector::PseudoClassVisited);
            break;
        }
    }

    if (!forcedPseudoClassesToSet.isEmpty()) {
        m_nodeIdToForcedPseudoState.set(nodeId, WTFMove(forcedPseudoClassesToSet));
        m_documentsWithForcedPseudoStates.add(&element->document());
    } else {
        if (!m_nodeIdToForcedPseudoState.remove(nodeId))
            return { };
        if (m_nodeIdToForcedPseudoState.isEmpty())
            m_documentsWithForcedPseudoStates.clear();
    }

    element->document().styleScope().didChangeStyleSheetEnvironment();

    return { };
}

static std::optional<InspectorCSSAgent::LayoutFlag> layoutFlagContextType(RenderObject* renderer)
{
    if (auto* renderFlexibleBox = dynamicDowncast<RenderFlexibleBox>(renderer)) {
        // Subclasses of RenderFlexibleBox (buttons, selection inputs, etc.) should not be considered flex containers,
        // as it is an internal implementation detail.
        if (renderFlexibleBox->isFlexibleBoxImpl())
            return std::nullopt;
        return InspectorCSSAgent::LayoutFlag::Flex;
    }
    if (is<RenderGrid>(renderer))
        return InspectorCSSAgent::LayoutFlag::Grid;
    return std::nullopt;
}

static bool layoutFlagsContainLayoutContextType(OptionSet<InspectorCSSAgent::LayoutFlag> layoutFlags)
{
    return layoutFlags.containsAny({
        InspectorCSSAgent::LayoutFlag::Flex,
        InspectorCSSAgent::LayoutFlag::Grid,
    });
}

static bool hasJSEventListener(Node& node)
{
    if (const auto* eventTargetData = node.eventTargetData()) {
        for (const auto& type : eventTargetData->eventListenerMap.eventTypes()) {
            for (const auto& listener : node.eventListeners(type)) {
                if (listener->callback().type() == EventListener::JSEventListenerType)
                    return true;
            }
        }
    }

    return false;
}

OptionSet<InspectorCSSAgent::LayoutFlag> InspectorCSSAgent::layoutFlagsForNode(Node& node)
{
    auto* renderer = node.renderer();

    OptionSet<LayoutFlag> layoutFlags;

    if (renderer) {
        layoutFlags.add(InspectorCSSAgent::LayoutFlag::Rendered);

        if (is<Document>(node)) {
            // We display document scrollability on the document element's node in the frontend. Other browsers show
            // scrollability on document.scrollingElement(), but that makes it impossible to see when both the document
            // and the <body> are scrollable in quirks mode.
        } else if (is<HTMLHtmlElement>(node)) {
            if (auto* frameView = node.document().view()) {
                if (frameView->isScrollable())
                    layoutFlags.add(InspectorCSSAgent::LayoutFlag::Scrollable);
            }
        } else if (is<RenderBox>(*renderer) && downcast<RenderBox>(*renderer).canBeScrolledAndHasScrollableArea())
            layoutFlags.add(InspectorCSSAgent::LayoutFlag::Scrollable);
    }

    if (auto contextType = layoutFlagContextType(renderer))
        layoutFlags.add(*contextType);

    if (hasJSEventListener(node))
        layoutFlags.add(InspectorCSSAgent::LayoutFlag::Event);

    return layoutFlags;
}

static RefPtr<JSON::ArrayOf<String /* Protocol::CSS::LayoutFlag */>> toProtocol(OptionSet<InspectorCSSAgent::LayoutFlag> layoutFlags)
{
    if (layoutFlags.isEmpty())
        return nullptr;

    auto protocolLayoutFlags = JSON::ArrayOf<String /* Protocol::CSS::LayoutFlag */>::create();
    if (layoutFlags.contains(InspectorCSSAgent::LayoutFlag::Rendered))
        protocolLayoutFlags->addItem(Protocol::Helpers::getEnumConstantValue(Protocol::CSS::LayoutFlag::Rendered));
    if (layoutFlags.contains(InspectorCSSAgent::LayoutFlag::Scrollable))
        protocolLayoutFlags->addItem(Protocol::Helpers::getEnumConstantValue(Protocol::CSS::LayoutFlag::Scrollable));
    if (layoutFlags.contains(InspectorCSSAgent::LayoutFlag::Flex))
        protocolLayoutFlags->addItem(Protocol::Helpers::getEnumConstantValue(Protocol::CSS::LayoutFlag::Flex));
    if (layoutFlags.contains(InspectorCSSAgent::LayoutFlag::Grid))
        protocolLayoutFlags->addItem(Protocol::Helpers::getEnumConstantValue(Protocol::CSS::LayoutFlag::Grid));
    if (layoutFlags.contains(InspectorCSSAgent::LayoutFlag::Event))
        protocolLayoutFlags->addItem(Protocol::Helpers::getEnumConstantValue(Protocol::CSS::LayoutFlag::Event));
    return protocolLayoutFlags;
}

RefPtr<JSON::ArrayOf<String /* Protocol::CSS::LayoutFlag */>> InspectorCSSAgent::protocolLayoutFlagsForNode(Node& node)
{
    auto layoutFlags = layoutFlagsForNode(node);
    if (!layoutFlags.isEmpty())
        m_lastLayoutFlagsForNode.set(node, layoutFlags);
    return toProtocol(layoutFlags);
}

static void pushChildrenNodesToFrontendIfLayoutFlagIsRelevant(InspectorDOMAgent& domAgent, ContainerNode& node)
{
    for (auto& child : childrenOfType<Element>(node))
        pushChildrenNodesToFrontendIfLayoutFlagIsRelevant(domAgent, child);
    
    if (layoutFlagContextType(node.renderer()))
        domAgent.pushNodeToFrontend(&node);
}

Protocol::ErrorStringOr<void> InspectorCSSAgent::setLayoutContextTypeChangedMode(Protocol::CSS::LayoutContextTypeChangedMode mode)
{
    if (m_layoutContextTypeChangedMode == mode)
        return { };
    
    m_layoutContextTypeChangedMode = mode;
    
    if (mode == Protocol::CSS::LayoutContextTypeChangedMode::All) {
        auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
        if (!domAgent)
            return makeUnexpected("DOM domain must be enabled"_s);

        for (auto* document : domAgent->documents())
            pushChildrenNodesToFrontendIfLayoutFlagIsRelevant(*domAgent, *document);
    }
    
    return { };
}

void InspectorCSSAgent::didChangeRendererForDOMNode(Node& node)
{
    nodeHasLayoutFlagsChange(node);
}

void InspectorCSSAgent::didAddEventListener(EventTarget& target)
{
    if (auto* node = dynamicDowncast<Node>(target))
        nodeHasLayoutFlagsChange(*node);
}
void InspectorCSSAgent::willRemoveEventListener(EventTarget& target)
{
    if (auto* node = dynamicDowncast<Node>(target))
        nodeHasLayoutFlagsChange(*node);
}

void InspectorCSSAgent::nodeHasLayoutFlagsChange(Node& node)
{
    m_nodesWithPendingLayoutFlagsChange.add(node);
    if (!m_nodesWithPendingLayoutFlagsChangeDispatchTimer.isActive())
        m_nodesWithPendingLayoutFlagsChangeDispatchTimer.startOneShot(0_s);
}

void InspectorCSSAgent::nodesWithPendingLayoutFlagsChangeDispatchTimerFired()
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent)
        return;

    for (auto&& node : std::exchange(m_nodesWithPendingLayoutFlagsChange, { })) {
        auto layoutFlags = layoutFlagsForNode(node);
        auto lastLayoutFlags = m_lastLayoutFlagsForNode.get(node);
        if (lastLayoutFlags == layoutFlags)
            continue;

        auto nodeId = domAgent->boundNodeId(&node);
        if (!nodeId && m_layoutContextTypeChangedMode == Protocol::CSS::LayoutContextTypeChangedMode::All && layoutFlagsContainLayoutContextType(layoutFlags)) {
            // FIXME: <https://webkit.org/b/189687> Preserve DOM.NodeId if a node is removed and re-added
            nodeId = domAgent->identifierForNode(node);
        }
        if (!nodeId)
            continue;

        m_lastLayoutFlagsForNode.set(node, layoutFlags);
        m_frontendDispatcher->nodeLayoutFlagsChanged(nodeId, toProtocol(layoutFlags));
    }
}

InspectorStyleSheetForInlineStyle& InspectorCSSAgent::asInspectorStyleSheet(StyledElement& element)
{
    return m_nodeToInspectorStyleSheet.ensure(&element, [this, &element] {
        String newStyleSheetId = String::number(m_lastStyleSheetId++);
        auto inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(m_instrumentingAgents.enabledPageAgent(), newStyleSheetId, element, Protocol::CSS::StyleSheetOrigin::Author, this);
        m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet.copyRef());
        return inspectorStyleSheet;
    }).iterator->value;
}

Element* InspectorCSSAgent::elementForId(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return nullptr;
    }

    return domAgent->assertElement(errorString, nodeId);
}

Node* InspectorCSSAgent::nodeForId(Protocol::ErrorString& errorString, Protocol::DOM::NodeId nodeId)
{
    auto* domAgent = m_instrumentingAgents.persistentDOMAgent();
    if (!domAgent) {
        errorString = "DOM domain must be enabled"_s;
        return nullptr;
    }

    return domAgent->assertNode(errorString, nodeId);
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
        inspectorStyleSheet = InspectorStyleSheet::create(m_instrumentingAgents.enabledPageAgent(), id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document), this);
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
        if (m_creatingViaInspectorStyleSheet) {
            auto& inspectorStyleSheetsForDocument = m_documentToInspectorStyleSheet.add(document, Vector<RefPtr<InspectorStyleSheet>>()).iterator->value;
            inspectorStyleSheetsForDocument.append(inspectorStyleSheet);
        }
    }
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::assertStyleSheetForId(Protocol::ErrorString& errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        errorString = "Missing style sheet for given styleSheetId"_s;
        return nullptr;
    }
    return it->value.get();
}

Protocol::CSS::StyleSheetOrigin InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return Protocol::CSS::StyleSheetOrigin::Inspector;

    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        return Protocol::CSS::StyleSheetOrigin::UserAgent;

    if (pageStyleSheet && pageStyleSheet->contents().isUserStyleSheet())
        return Protocol::CSS::StyleSheetOrigin::User;

    auto iterator = m_documentToInspectorStyleSheet.find(ownerDocument);
    if (iterator != m_documentToInspectorStyleSheet.end()) {
        for (auto& inspectorStyleSheet : iterator->value) {
            if (pageStyleSheet == inspectorStyleSheet->pageStyleSheet())
                return Protocol::CSS::StyleSheetOrigin::Inspector;
        }
    }

    return Protocol::CSS::StyleSheetOrigin::Author;
}

RefPtr<Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(const StyleRule* styleRule, Style::Resolver& styleResolver, Element& element)
{
    ASSERT(element.isConnected());

    if (!styleRule)
        return nullptr;

    // StyleRules returned by Style::Resolver::styleRulesForElement lack parent pointers since that infomation is not cheaply available.
    // Since the inspector wants to walk the parent chain, we construct the full wrappers here.
    styleResolver.inspectorCSSOMWrappers().collectDocumentWrappers(styleResolver.document().extensionStyleSheets());
    styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(Style::Scope::forNode(element));

    // Possiblity of :host styles if this element has a shadow root.
    if (ShadowRoot* shadowRoot = element.shadowRoot())
        styleResolver.inspectorCSSOMWrappers().collectScopeWrappers(shadowRoot->styleScope());

    CSSStyleRule* cssomWrapper = styleResolver.inspectorCSSOMWrappers().getWrapperForRuleInSheets(styleRule);
    return buildObjectForRule(cssomWrapper);
}

RefPtr<Protocol::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule)
{
    if (!rule)
        return nullptr;

    ASSERT(rule->parentStyleSheet());
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(rule->parentStyleSheet());
    return inspectorStyleSheet ? inspectorStyleSheet->buildObjectForRule(rule) : nullptr;
}

Ref<JSON::ArrayOf<Protocol::CSS::RuleMatch>> InspectorCSSAgent::buildArrayForMatchedRuleList(const Vector<RefPtr<const StyleRule>>& matchedRules, Style::Resolver& styleResolver, Element& element, PseudoId pseudoId)
{
    auto result = JSON::ArrayOf<Protocol::CSS::RuleMatch>::create();

    SelectorChecker::CheckingContext context(SelectorChecker::Mode::CollectingRules);
    context.pseudoId = pseudoId != PseudoId::None ? pseudoId : element.pseudoId();
    SelectorChecker selectorChecker(element.document());

    for (auto& matchedRule : matchedRules) {
        RefPtr<Protocol::CSS::CSSRule> ruleObject = buildObjectForRule(matchedRule.get(), styleResolver, element);
        if (!ruleObject)
            continue;

        auto matchingSelectors = JSON::ArrayOf<int>::create();
        const CSSSelectorList& selectorList = matchedRule->selectorList();
        int index = 0;
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            bool matched = selectorChecker.match(*selector, element, context);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }

        auto match = Protocol::CSS::RuleMatch::create()
            .setRule(ruleObject.releaseNonNull())
            .setMatchingSelectors(WTFMove(matchingSelectors))
            .release();
        result->addItem(WTFMove(match));
    }

    return result;
}

RefPtr<Protocol::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(StyledElement& element)
{
    // FIXME: Ugliness below.
    auto* attributeStyle = const_cast<StyleProperties*>(element.presentationalHintStyle());
    if (!attributeStyle)
        return nullptr;

    auto& mutableAttributeStyle = downcast<MutableStyleProperties>(*attributeStyle);
    auto inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle.ensureCSSStyleDeclaration(), nullptr);
    return inspectorStyle->buildObjectForStyle();
}

void InspectorCSSAgent::didRemoveDOMNode(Node& node, Protocol::DOM::NodeId nodeId)
{
    // This can be called in response to GC.
    m_nodeIdToForcedPseudoState.remove(nodeId);

    auto sheet = m_nodeToInspectorStyleSheet.take(&node);
    if (!sheet)
        return;
    m_idToInspectorStyleSheet.remove(sheet->id());
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
