/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelector.h"
#include "ContentSecurityPolicy.h"
#include "InspectorStyleSheet.h"
#include "InspectorWebAgentBase.h"
#include "SecurityContext.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class CSSFrontendDispatcher;
}

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class EventTarget;
class Node;
class NodeList;
class RenderObject;
class Settings;
class StyleRule;
class WeakPtrImplWithEventTargetData;

namespace Style {
class Resolver;
}

class InspectorCSSAgent final : public InspectorAgentBase , public Inspector::CSSBackendDispatcherHandler , public InspectorStyleSheet::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorCSSAgent(PageAgentContext&);
    ~InspectorCSSAgent();

    class InlineStyleOverrideScope {
    public:
        InlineStyleOverrideScope(SecurityContext& context)
            : m_contentSecurityPolicy(context.contentSecurityPolicy())
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(true);
        }

        ~InlineStyleOverrideScope()
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(false);
        }

    private:
        ContentSecurityPolicy* m_contentSecurityPolicy;
    };

    static std::optional<Inspector::Protocol::CSS::PseudoId> protocolValueForPseudoId(PseudoId);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // CSSBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable();
    Inspector::Protocol::ErrorStringOr<void> disable();
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>>> getComputedStyleForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::Font>> getFontDataForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<std::tuple<RefPtr<Inspector::Protocol::CSS::CSSStyle> /* inlineStyle */, RefPtr<Inspector::Protocol::CSS::CSSStyle> /* attributesStyle */>> getInlineStylesForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<std::tuple<RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>>, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>>, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>>>> getMatchedStylesForNode(Inspector::Protocol::DOM::NodeId, std::optional<bool>&& includePseudo, std::optional<bool>&& includeInherited);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>>> getAllStyleSheets();
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::CSSStyleSheetBody>> getStyleSheet(const Inspector::Protocol::CSS::StyleSheetId&);
    Inspector::Protocol::ErrorStringOr<String> getStyleSheetText(const Inspector::Protocol::CSS::StyleSheetId&);
    Inspector::Protocol::ErrorStringOr<void> setStyleSheetText(const Inspector::Protocol::CSS::StyleSheetId&, const String& text);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::CSSStyle>> setStyleText(Ref<JSON::Object>&& styleId, const String& text);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::CSSRule>> setRuleSelector(Ref<JSON::Object>&& ruleId, const String& selector);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::Grouping>> setGroupingHeaderText(Ref<JSON::Object>&& ruleId, const String& headerText);
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::CSS::StyleSheetId> createStyleSheet(const Inspector::Protocol::Network::FrameId&);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::CSS::CSSRule>> addRule(const Inspector::Protocol::CSS::StyleSheetId&, const String& selector);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>>> getSupportedCSSProperties();
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> getSupportedSystemFontFamilyNames();
    Inspector::Protocol::ErrorStringOr<void> forcePseudoState(Inspector::Protocol::DOM::NodeId, Ref<JSON::Array>&& forcedPseudoClasses);
    Inspector::Protocol::ErrorStringOr<void> setLayoutContextTypeChangedMode(Inspector::Protocol::CSS::LayoutContextTypeChangedMode);

    // InspectorStyleSheet::Listener
    void styleSheetChanged(InspectorStyleSheet*);

    // InspectorInstrumentation
    void documentDetached(Document&);
    void mediaQueryResultChanged();
    void activeStyleSheetsUpdated(Document&);
    bool forcePseudoState(const Element&, CSSSelector::PseudoClassType);
    void didChangeRendererForDOMNode(Node&);
    void didAddEventListener(EventTarget&);
    void willRemoveEventListener(EventTarget&);

    // InspectorDOMAgent hooks
    void didRemoveDOMNode(Node&, Inspector::Protocol::DOM::NodeId);
    void didModifyDOMAttr(Element&);

    enum class LayoutFlag : uint8_t {
        Rendered = 1 << 0,
        Flex = 1 << 1,
        Grid = 1 << 2,
        Event = 1 << 3,
        Scrollable = 1 << 4,
    };
    OptionSet<LayoutFlag> layoutFlagsForNode(Node&);
    RefPtr<JSON::ArrayOf<String /* Inspector::Protocol::CSS::LayoutFlag */>> protocolLayoutFlagsForNode(Node&);

    void reset();

private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class SetStyleTextAction;
    class SetRuleHeaderTextAction;
    class AddRuleAction;

    typedef HashMap<Inspector::Protocol::CSS::StyleSheetId, RefPtr<InspectorStyleSheet>> IdToInspectorStyleSheet;
    typedef HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet>> CSSStyleSheetToInspectorStyleSheet;
    typedef HashMap<RefPtr<Document>, Vector<RefPtr<InspectorStyleSheet>>> DocumentToViaInspectorStyleSheet; // "via inspector" stylesheets
    typedef HashSet<CSSSelector::PseudoClassType, IntHash<CSSSelector::PseudoClassType>, WTF::StrongEnumHashTraits<CSSSelector::PseudoClassType>> PseudoClassHashSet;

    InspectorStyleSheetForInlineStyle& asInspectorStyleSheet(StyledElement&);
    Element* elementForId(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    Node* nodeForId(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);

    void collectAllStyleSheets(Vector<InspectorStyleSheet*>&);
    void collectAllDocumentStyleSheets(Document&, Vector<CSSStyleSheet*>&);
    void collectStyleSheets(CSSStyleSheet*, Vector<CSSStyleSheet*>&);
    void setActiveStyleSheetsForDocument(Document&, Vector<CSSStyleSheet*>& activeStyleSheets);

    Inspector::Protocol::CSS::StyleSheetId unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    InspectorStyleSheet* assertStyleSheetForId(Inspector::Protocol::ErrorString&, const Inspector::Protocol::CSS::StyleSheetId&);
    InspectorStyleSheet* createInspectorStyleSheetForDocument(Document&);
    Inspector::Protocol::CSS::StyleSheetOrigin detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(const StyleRule*, Style::Resolver&, Element&);
    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    Ref<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>> buildArrayForMatchedRuleList(const Vector<RefPtr<const StyleRule>>&, Style::Resolver&, Element&, PseudoId);
    RefPtr<Inspector::Protocol::CSS::CSSStyle> buildObjectForAttributesStyle(StyledElement&);

    void nodeHasLayoutFlagsChange(Node&);
    void nodesWithPendingLayoutFlagsChangeDispatchTimerFired();

    void resetPseudoStates();

    std::unique_ptr<Inspector::CSSFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CSSBackendDispatcher> m_backendDispatcher;

    Page& m_inspectedPage;
    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    CSSStyleSheetToInspectorStyleSheet m_cssStyleSheetToInspectorStyleSheet;
    HashMap<Node*, Ref<InspectorStyleSheetForInlineStyle>> m_nodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    DocumentToViaInspectorStyleSheet m_documentToInspectorStyleSheet;
    HashMap<Document*, HashSet<CSSStyleSheet*>> m_documentToKnownCSSStyleSheets;
    HashMap<Inspector::Protocol::DOM::NodeId, PseudoClassHashSet> m_nodeIdToForcedPseudoState;
    HashSet<Document*> m_documentsWithForcedPseudoStates;

    int m_lastStyleSheetId { 1 };
    bool m_creatingViaInspectorStyleSheet { false };

    WeakHashMap<Node, OptionSet<LayoutFlag>, WeakPtrImplWithEventTargetData> m_lastLayoutFlagsForNode;
    WeakHashSet<Node, WeakPtrImplWithEventTargetData> m_nodesWithPendingLayoutFlagsChange;
    Timer m_nodesWithPendingLayoutFlagsChangeDispatchTimer;

    Inspector::Protocol::CSS::LayoutContextTypeChangedMode m_layoutContextTypeChangedMode { Inspector::Protocol::CSS::LayoutContextTypeChangedMode::Observed };
};

} // namespace WebCore
