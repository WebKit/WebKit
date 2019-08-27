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
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
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
class Node;
class NodeList;
class StyleResolver;
class StyleRule;

class InspectorCSSAgent final : public InspectorAgentBase , public Inspector::CSSBackendDispatcherHandler , public InspectorStyleSheet::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorCSSAgent(WebAgentContext&);
    virtual ~InspectorCSSAgent();

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

    static CSSStyleRule* asCSSStyleRule(CSSRule&);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // CSSBackendDispatcherHandler
    void enable(ErrorString&);
    void disable(ErrorString&);
    void getComputedStyleForNode(ErrorString&, int nodeId, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSComputedStyleProperty>>&);
    void getInlineStylesForNode(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::CSS::CSSStyle>& inlineStyle, RefPtr<Inspector::Protocol::CSS::CSSStyle>& attributes);
    void getMatchedStylesForNode(ErrorString&, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>>& matchedCSSRules, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::PseudoIdMatches>>&, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::InheritedStyleEntry>>& inheritedEntries);
    void getAllStyleSheets(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSStyleSheetHeader>>& styleSheetInfos);
    void getStyleSheet(ErrorString&, const String& styleSheetId, RefPtr<Inspector::Protocol::CSS::CSSStyleSheetBody>& result);
    void getStyleSheetText(ErrorString&, const String& styleSheetId, String* result);
    void setStyleSheetText(ErrorString&, const String& styleSheetId, const String& text);
    void setStyleText(ErrorString&, const JSON::Object& styleId, const String& text, RefPtr<Inspector::Protocol::CSS::CSSStyle>& result);
    void setRuleSelector(ErrorString&, const JSON::Object& ruleId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result);
    void createStyleSheet(ErrorString&, const String& frameId, String* styleSheetId);
    void addRule(ErrorString&, const String& styleSheetId, const String& selector, RefPtr<Inspector::Protocol::CSS::CSSRule>& result);
    void getSupportedCSSProperties(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::CSSPropertyInfo>>& result);
    void getSupportedSystemFontFamilyNames(ErrorString&, RefPtr<JSON::ArrayOf<String>>& result);
    void forcePseudoState(ErrorString&, int nodeId, const JSON::Array& forcedPseudoClasses);

    // InspectorStyleSheet::Listener
    void styleSheetChanged(InspectorStyleSheet*);

    // InspectorInstrumentation
    void documentDetached(Document&);
    void mediaQueryResultChanged();
    void activeStyleSheetsUpdated(Document&);
    bool forcePseudoState(const Element&, CSSSelector::PseudoClassType);

    // InspectorDOMAgent hooks
    void didRemoveDOMNode(Node&, int nodeId);
    void didModifyDOMAttr(Element&);

    void reset();

private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class SetStyleTextAction;
    class SetRuleSelectorAction;
    class AddRuleAction;

    typedef HashMap<String, RefPtr<InspectorStyleSheet>> IdToInspectorStyleSheet;
    typedef HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet>> CSSStyleSheetToInspectorStyleSheet;
    typedef HashMap<RefPtr<Document>, Vector<RefPtr<InspectorStyleSheet>>> DocumentToViaInspectorStyleSheet; // "via inspector" stylesheets
    typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

    InspectorStyleSheetForInlineStyle& asInspectorStyleSheet(StyledElement&);
    Element* elementForId(ErrorString&, int nodeId);

    void collectAllStyleSheets(Vector<InspectorStyleSheet*>&);
    void collectAllDocumentStyleSheets(Document&, Vector<CSSStyleSheet*>&);
    void collectStyleSheets(CSSStyleSheet*, Vector<CSSStyleSheet*>&);
    void setActiveStyleSheetsForDocument(Document&, Vector<CSSStyleSheet*>& activeStyleSheets);

    String unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    InspectorStyleSheet* assertStyleSheetForId(ErrorString&, const String&);
    InspectorStyleSheet* createInspectorStyleSheetForDocument(Document&);
    Inspector::Protocol::CSS::StyleSheetOrigin detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(StyleRule*, StyleResolver&, Element&);
    RefPtr<Inspector::Protocol::CSS::CSSRule> buildObjectForRule(CSSStyleRule*);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::CSS::RuleMatch>> buildArrayForMatchedRuleList(const Vector<RefPtr<StyleRule>>&, StyleResolver&, Element&, PseudoId);
    RefPtr<Inspector::Protocol::CSS::CSSStyle> buildObjectForAttributesStyle(StyledElement&);


    void resetPseudoStates();

    std::unique_ptr<Inspector::CSSFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::CSSBackendDispatcher> m_backendDispatcher;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    CSSStyleSheetToInspectorStyleSheet m_cssStyleSheetToInspectorStyleSheet;
    HashMap<Node*, Ref<InspectorStyleSheetForInlineStyle>> m_nodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    DocumentToViaInspectorStyleSheet m_documentToInspectorStyleSheet;
    HashMap<Document*, HashSet<CSSStyleSheet*>> m_documentToKnownCSSStyleSheets;
    NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;
    HashSet<Document*> m_documentsWithForcedPseudoStates;

    int m_lastStyleSheetId { 1 };
    bool m_creatingViaInspectorStyleSheet { false };
};

} // namespace WebCore
