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

#ifndef InspectorStyleSheet_h
#define InspectorStyleSheet_h

#include "CSSPropertySourceData.h"
#include "Document.h"
#include "InspectorValues.h"
#include "PlatformString.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

class ParsedStyleSheet;

namespace WebCore {

class CSSRuleList;
class CSSStyleDeclaration;
class Element;
class Node;

#if ENABLE(INSPECTOR)

class InspectorStyleSheet : public RefCounted<InspectorStyleSheet> {
public:
    static PassRefPtr<InspectorStyleSheet> create(const String& id, CSSStyleSheet* pageStyleSheet, const String& origin, const String& documentURL)
    {
        return adoptRef(new InspectorStyleSheet(id, pageStyleSheet, origin, documentURL));
    }

    InspectorStyleSheet(const String& id, CSSStyleSheet* pageStyleSheet, const String& origin, const String& documentURL);
    virtual ~InspectorStyleSheet();

    const String& id() const { return m_id; }
    CSSStyleSheet* pageStyleSheet() const { return m_pageStyleSheet; }
    bool setText(const String&);
    bool setRuleSelector(const String& ruleId, const String& selector);
    CSSStyleRule* addRule(const String& selector);
    CSSStyleRule* ruleForId(const String&) const;
    PassRefPtr<InspectorObject> buildObjectForStyleSheet();
    PassRefPtr<InspectorObject> buildObjectForRule(CSSStyleRule*);
    PassRefPtr<InspectorObject> buildObjectForStyle(CSSStyleDeclaration*);
    virtual CSSStyleDeclaration* styleForId(const String&) const;
    virtual bool setStyleText(const String& styleId, const String& text);

protected:
    bool canBind() const { return m_origin != "userAgent" && m_origin != "user"; }
    String fullRuleOrStyleId(CSSStyleDeclaration* style) const { return id() + ":" + ruleOrStyleId(style); }
    String ruleOrStyleId(CSSStyleDeclaration* style) const { unsigned index = ruleIndexByStyle(style); return index == UINT_MAX ? "" : String::number(index); }
    virtual Document* ownerDocument() const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration* style) const;
    virtual unsigned ruleIndexByStyle(CSSStyleDeclaration*) const;
    virtual bool ensureParsedDataReady();

private:
    bool text(String* result) const;
    bool ensureText() const;
    bool ensureSourceData(Node* ownerNode);
    void innerSetStyleSheetText(const String& newText);
    bool innerSetStyleText(CSSStyleDeclaration*, const String&);
    bool styleSheetTextWithChangedStyle(CSSStyleDeclaration*, const String& newStyleText, String* result);
    CSSStyleRule* findPageRuleWithStyle(CSSStyleDeclaration*);
    String fullRuleId(CSSStyleRule* rule) const;
    String fullStyleId(CSSStyleDeclaration* style) const { return fullRuleOrStyleId(style); }
    void revalidateStyle(CSSStyleDeclaration*);
    bool styleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    PassRefPtr<InspectorArray> buildArrayForRuleList(CSSRuleList*);

    String m_id;
    CSSStyleSheet* m_pageStyleSheet;
    String m_origin;
    String m_documentURL;
    bool m_isRevalidating;
    ParsedStyleSheet* m_parsedStyleSheet;
};

class InspectorStyleSheetForInlineStyle : public InspectorStyleSheet {
public:
    static PassRefPtr<InspectorStyleSheetForInlineStyle> create(const String& id, Element* element, const String& origin)
    {
        return adoptRef(new InspectorStyleSheetForInlineStyle(id, element, origin));
    }

    InspectorStyleSheetForInlineStyle(const String& id, Element* element, const String& origin);
    virtual CSSStyleDeclaration* styleForId(const String& id) const { ASSERT(id == "0"); UNUSED_PARAM(id); return inlineStyle(); }
    virtual bool setStyleText(const String& styleId, const String& text);

protected:
    virtual Document* ownerDocument() const;
    virtual RefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration* style) const { ASSERT(style == inlineStyle()); UNUSED_PARAM(style); return m_ruleSourceData; }
    virtual unsigned ruleIndexByStyle(CSSStyleDeclaration*) const { return 0; }
    virtual bool ensureParsedDataReady();

private:
    CSSStyleDeclaration* inlineStyle() const;
    bool getStyleAttributeRanges(RefPtr<CSSStyleSourceData>* result);

    Element* m_element;
    RefPtr<CSSRuleSourceData> m_ruleSourceData;
};

#endif

} // namespace WebCore

#endif // !defined(InspectorStyleSheet_h)
