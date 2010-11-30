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

#include "Document.h"
#include "InspectorDOMAgent.h"
#include "InspectorStyleSheet.h"
#include "InspectorValues.h"
#include "PlatformString.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSRule;
class CSSRuleList;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Element;
class InspectorFrontend;
class NameNodeMap;
class Node;

#if ENABLE(INSPECTOR)

class InspectorCSSAgent : public InspectorDOMAgent::DOMListener {
public:
    static CSSStyleSheet* parentStyleSheet(StyleBase*);
    static CSSStyleRule* asCSSStyleRule(StyleBase*);

    InspectorCSSAgent();
    ~InspectorCSSAgent();

    void setDOMAgent(InspectorDOMAgent* domAgent);

    void reset();
    void getStylesForNode2(long nodeId, RefPtr<InspectorValue>* result);
    void getInlineStyleForNode2(long nodeId, RefPtr<InspectorValue>* style);
    void getComputedStyleForNode2(long nodeId, RefPtr<InspectorValue>* style);
    void getAllStyles2(RefPtr<InspectorArray>* styles);
    void getStyleSheet2(const String& styleSheetId, RefPtr<InspectorValue>* result);
    void getStyleSheetText2(const String& styleSheetId, String* url, String* result);
    void setStyleSheetText2(const String& styleSheetId, const String& text, bool* success);
    void setPropertyText2(const RefPtr<InspectorObject>& styleId, long propertyIndex, const String& text, bool overwrite, RefPtr<InspectorValue>* result);
    void toggleProperty2(const RefPtr<InspectorObject>& styleId, long propertyIndex, bool disable, RefPtr<InspectorValue>* result);
    void setRuleSelector2(const RefPtr<InspectorObject>& ruleId, const String& selector, RefPtr<InspectorValue>* result);
    void addRule2(const long contextNodeId, const String& selector, RefPtr<InspectorValue>* result);
    void getSupportedCSSProperties(RefPtr<InspectorArray>* result);
    void querySelectorAll(const long nodeId, const String& selector, RefPtr<InspectorArray>* result);

private:
    typedef HashMap<String, RefPtr<InspectorStyleSheet> > IdToInspectorStyleSheet;
    typedef HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet> > CSSStyleSheetToInspectorStyleSheet;
    typedef HashMap<Node*, RefPtr<InspectorStyleSheetForInlineStyle> > NodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    typedef HashMap<RefPtr<Document>, RefPtr<InspectorStyleSheet> > DocumentToViaInspectorStyleSheet; // "via inspector" stylesheets

    static Element* inlineStyleElement(CSSStyleDeclaration*);

    InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);
    Element* elementForId(long nodeId);

    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    InspectorStyleSheet* viaInspectorStyleSheet(Document*, bool createIfAbsent);
    InspectorStyleSheet* styleSheetForId(const String&);
    String detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    PassRefPtr<InspectorArray> buildArrayForRuleList(CSSRuleList* ruleList);
    PassRefPtr<InspectorObject> buildObjectForAttributeStyles(Element* element);

    // InspectorDOMAgent::DOMListener interface
    virtual void didRemoveDocument(Document*);
    virtual void didRemoveDOMNode(Node*);

    InspectorDOMAgent* m_domAgent;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    CSSStyleSheetToInspectorStyleSheet m_cssStyleSheetToInspectorStyleSheet;
    NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
    DocumentToViaInspectorStyleSheet m_documentToInspectorStyleSheet;

    long m_lastStyleSheetId;
    long m_lastRuleId;
    long m_lastStyleId;
};

#endif

} // namespace WebCore

#endif // !defined(InspectorCSSAgent_h)
