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

#ifndef InspectorCSSAgent_h
#define InspectorCSSAgent_h

#include "CSSSelector.h"
#include "ContentSecurityPolicy.h"
#include "InspectorDOMAgent.h"
#include "InspectorStyleSheet.h"
#include "InspectorWebAgentBase.h"
#include "SecurityContext.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorValues.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
class CSSFrontendDispatcher;
}

namespace WebCore {

class CSSRule;
class CSSStyleRule;
class CSSStyleSheet;
class ChangeRegionOversetTask;
class Document;
class Element;
class InstrumentingAgents;
class Node;
class NodeList;
class StyleResolver;
class StyleRule;

class InspectorCSSAgent final
    : public InspectorAgentBase
    , public InspectorDOMAgent::DOMListener
    , public Inspector::CSSBackendDispatcherHandler
    , public InspectorStyleSheet::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
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

    InspectorCSSAgent(InstrumentingAgents*, InspectorDOMAgent*);
    virtual ~InspectorCSSAgent();

    static CSSStyleRule* asCSSStyleRule(CSSRule&);

    virtual void didCreateFrontendAndBackend(Inspector::FrontendChannel*, Inspector::BackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;
    virtual void discardAgent() override;
    virtual void enable(ErrorString&) override;
    virtual void disable(ErrorString&) override;
    void regionOversetChanged(WebKitNamedFlow*, int documentNodeId);
    void reset();

    // InspectorInstrumentation callbacks.
    void documentDetached(Document&);
    void mediaQueryResultChanged();
    void activeStyleSheetsUpdated(Document&);
    void didCreateNamedFlow(Document&, WebKitNamedFlow&);
    void willRemoveNamedFlow(Document&, WebKitNamedFlow&);
    void didChangeRegionOverset(Document&, WebKitNamedFlow&);
    void didRegisterNamedFlowContentElement(Document&, WebKitNamedFlow&, Node& contentElement, Node* nextContentElement = nullptr);
    void didUnregisterNamedFlowContentElement(Document&, WebKitNamedFlow&, Node& contentElement);
    bool forcePseudoState(Element&, CSSSelector::PseudoClassType);

    virtual void getComputedStyleForNode(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSComputedStyleProperty>>&) override;
    virtual void getInlineStylesForNode(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::CSS::CSSStyle>& inlineStyle, RefPtr<Inspector::Protocol::CSS::CSSStyle>& attributes) override;
    virtual void getMatchedStylesForNode(ErrorString&, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>>& matchedCSSRules, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::PseudoIdMatches>>& pseudoIdMatches, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::InheritedStyleEntry>>& inheritedEntries) override;
    virtual void getAllStyleSheets(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSStyleSheetHeader>>& styleSheetInfos) override;
    virtual void getStyleSheet(ErrorString&, const String& styleSheetId, RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody>& result) override;
    virtual void getStyleSheetText(ErrorString&, const String& styleSheetId, String* result) override;
    virtual void setStyleSheetText(ErrorString&, const String& styleSheetId, const String& text) override;
    virtual void setStyleText(ErrorString&, const Inspector::InspectorObject& styleId, const String& text, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result) override;
    virtual void setRuleSelector(ErrorString&, const Inspector::InspectorObject& ruleId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result) override;
    virtual void createStyleSheet(ErrorString&, const String& frameId, String* styleSheetId) override;
    virtual void addRule(ErrorString&, const String& styleSheetId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result) override;
    virtual void getSupportedCSSProperties(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::CSSPropertyInfo>>& result) override;
    virtual void getSupportedSystemFontFamilyNames(ErrorString&, RefPtr<Inspector::Protocol::Array<String>>& result) override;
    virtual void forcePseudoState(ErrorString&, int nodeId, const Inspector::InspectorArray& forcedPseudoClasses) override;
    virtual void getNamedFlowCollection(ErrorString&, int documentNodeId, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::NamedFlow>>& result) override;

private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class SetStyleTextAction;
    class SetRuleSelectorAction;
    class AddRuleAction;

    typedef HashMap<String, RefPtr<InspectorStyleSheet>> IdToInspectorStyleSheet;
    typedef HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet>> CSSStyleSheetToInspectorStyleSheet;
    typedef HashMap<Node*, RefPtr<InspectorStyleSheetForInlineStyle>> NodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    typedef HashMap<RefPtr<Document>, Vector<RefPtr<InspectorStyleSheet>>> DocumentToViaInspectorStyleSheet; // "via inspector" stylesheets
    typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

    void resetNonPersistentData();
    InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);
    Element* elementForId(ErrorString&, int nodeId);
    int documentNodeWithRequestedFlowsId(Document*);

    void collectAllStyleSheets(Vector<InspectorStyleSheet*>&);
    void collectAllDocumentStyleSheets(Document&, Vector<CSSStyleSheet*>&);
    void collectStyleSheets(CSSStyleSheet*, Vector<CSSStyleSheet*>&);
    void setActiveStyleSheetsForDocument(Document&, Vector<CSSStyleSheet*>& activeStyleSheets);

    String unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    InspectorStyleSheet* assertStyleSheetForId(ErrorString&, const String&);
    InspectorStyleSheet* createInspectorStyleSheetForDocument(Document&);
    Inspector::Protocol::CSS::StyleSheetOrigin detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(StyleRule*, StyleResolver&, Element*);
    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::RuleMatch>> buildArrayForMatchedRuleList(const Vector<RefPtr<StyleRule>>&, StyleResolver&, Element*, PseudoId);
    RefPtr<Inspector::Protocol::CSS::CSSStyle> buildObjectForAttributesStyle(Element*);
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::CSS::Region>> buildArrayForRegions(ErrorString&, RefPtr<NodeList>&&, int documentNodeId);
    RefPtr<Inspector::Protocol::CSS::NamedFlow> buildObjectForNamedFlow(ErrorString&, WebKitNamedFlow*, int documentNodeId);

    // InspectorDOMAgent::DOMListener implementation
    virtual void didRemoveDocument(Document*) override;
    virtual void didRemoveDOMNode(Node*) override;
    virtual void didModifyDOMAttr(Element*) override;

    // InspectorCSSAgent::Listener implementation
    virtual void styleSheetChanged(InspectorStyleSheet*) override;

    void resetPseudoStates();

    std::unique_ptr<Inspector::CSSFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CSSBackendDispatcher> m_backendDispatcher;
    InspectorDOMAgent* m_domAgent;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    CSSStyleSheetToInspectorStyleSheet m_cssStyleSheetToInspectorStyleSheet;
    NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
    DocumentToViaInspectorStyleSheet m_documentToInspectorStyleSheet;
    HashMap<Document*, HashSet<CSSStyleSheet*>> m_documentToKnownCSSStyleSheets;
    NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;
    HashSet<int> m_namedFlowCollectionsRequested;
    std::unique_ptr<ChangeRegionOversetTask> m_changeRegionOversetTask;

    int m_lastStyleSheetId;
    bool m_creatingViaInspectorStyleSheet { false };
};

} // namespace WebCore

#endif // !defined(InspectorCSSAgent_h)
