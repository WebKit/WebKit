/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLConstructionSite_h
#define HTMLConstructionSite_h

#include "FragmentScriptingPermission.h"
#include "HTMLElementStack.h"
#include "HTMLFormattingElementList.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AtomicHTMLToken;
class Document;
class Element;

class HTMLConstructionSite : public Noncopyable {
public:
    HTMLConstructionSite(Document*, FragmentScriptingPermission);
    ~HTMLConstructionSite();

    void insertDoctype(AtomicHTMLToken&);
    void insertComment(AtomicHTMLToken&);
    void insertCommentOnDocument(AtomicHTMLToken&);
    void insertCommentOnHTMLHtmlElement(AtomicHTMLToken&);
    void insertElement(AtomicHTMLToken&);
    void insertSelfClosingElement(AtomicHTMLToken&);
    void insertFormattingElement(AtomicHTMLToken&);
    void insertHTMLHtmlElement(AtomicHTMLToken&);
    void insertHTMLHeadElement(AtomicHTMLToken&);
    void insertHTMLBodyElement(AtomicHTMLToken&);
    void insertScriptElement(AtomicHTMLToken&);
    void insertTextNode(AtomicHTMLToken&);

    void insertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken&);
    void insertHTMLHtmlStartTagInBody(AtomicHTMLToken&);
    void insertHTMLBodyStartTagInBody(AtomicHTMLToken&);

    PassRefPtr<Element> createElement(AtomicHTMLToken&);

    void fosterParent(Element*);

    bool indexOfFirstUnopenFormattingElement(unsigned& firstUnopenElementIndex) const;
    void reconstructTheActiveFormattingElements();

    void generateImpliedEndTags();
    void generateImpliedEndTagsWithExclusion(const AtomicString& tagName);

    Element* currentElement() const { return m_openElements.top(); }
    HTMLElementStack* openElements() const { return &m_openElements; }
    HTMLFormattingElementList* activeFormattingElements() const { return &m_activeFormattingElements; }

    Element* head() const { return m_head.get(); }

    Element* form() const { return m_form.get(); }
    PassRefPtr<Element> takeForm() { return m_form.release(); }

    void setForm(PassRefPtr<Element> form) { m_form = form; }

private:
    template<typename ChildType>
    PassRefPtr<ChildType> attach(Node* parent, PassRefPtr<ChildType> prpChild);

    PassRefPtr<Element> createElementAndAttachToCurrent(AtomicHTMLToken&);
    void mergeAttributesFromTokenIntoElement(AtomicHTMLToken&, Element*);

    Document* m_document;
    RefPtr<Element> m_head;
    RefPtr<Element> m_form;
    mutable HTMLElementStack m_openElements;
    mutable HTMLFormattingElementList m_activeFormattingElements;
    FragmentScriptingPermission m_fragmentScriptingPermission;
};

}

#endif
