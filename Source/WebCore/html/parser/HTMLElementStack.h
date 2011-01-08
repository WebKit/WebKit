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

#ifndef HTMLElementStack_h
#define HTMLElementStack_h

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class QualifiedName;

// NOTE: The HTML5 spec uses a backwards (grows downward) stack.  We're using
// more standard (grows upwards) stack terminology here.
class HTMLElementStack : public Noncopyable {
public:
    HTMLElementStack();
    ~HTMLElementStack();

    class ElementRecord : public Noncopyable {
    public:
        ~ElementRecord(); // Public for ~PassOwnPtr()
    
        Element* element() const { return m_element.get(); }
        void replaceElement(PassRefPtr<Element>);

        bool isAbove(ElementRecord*) const;

        ElementRecord* next() const { return m_next.get(); }

    private:
        friend class HTMLElementStack;

        ElementRecord(PassRefPtr<Element>, PassOwnPtr<ElementRecord>);

        PassOwnPtr<ElementRecord> releaseNext() { return m_next.release(); }
        void setNext(PassOwnPtr<ElementRecord> next) { m_next = next; }

        RefPtr<Element> m_element;
        OwnPtr<ElementRecord> m_next;
    };

    // Inlining this function is a (small) performance win on the parsing
    // benchmark.
    Element* top() const
    {
        ASSERT(m_top->element());
        return m_top->element();
    }

    Element* oneBelowTop() const;
    ElementRecord* topRecord() const;
    Element* bottom() const;
    ElementRecord* find(Element*) const;
    ElementRecord* topmost(const AtomicString& tagName) const;

    void insertAbove(PassRefPtr<Element>, ElementRecord*);

    void push(PassRefPtr<Element>);
    void pushHTMLHtmlElement(PassRefPtr<Element>);
    void pushHTMLHeadElement(PassRefPtr<Element>);
    void pushHTMLBodyElement(PassRefPtr<Element>);

    void pop();
    void popUntil(const AtomicString& tagName);
    void popUntil(Element*);
    void popUntilPopped(const AtomicString& tagName);
    void popUntilPopped(Element*);
    void popUntilNumberedHeaderElementPopped();
    void popUntilTableScopeMarker(); // "clear the stack back to a table context" in the spec.
    void popUntilTableBodyScopeMarker(); // "clear the stack back to a table body context" in the spec.
    void popUntilTableRowScopeMarker(); // "clear the stack back to a table row context" in the spec.
    void popUntilForeignContentScopeMarker();
    void popHTMLHeadElement();
    void popHTMLBodyElement();
    void popAll();

    void remove(Element*);
    void removeHTMLHeadElement(Element*);

    bool contains(Element*) const;
    bool contains(const AtomicString& tagName) const;

    bool inScope(Element*) const;
    bool inScope(const AtomicString& tagName) const;
    bool inScope(const QualifiedName&) const;
    bool inListItemScope(const AtomicString& tagName) const;
    bool inListItemScope(const QualifiedName&) const;
    bool inTableScope(const AtomicString& tagName) const;
    bool inTableScope(const QualifiedName&) const;
    bool inButtonScope(const AtomicString& tagName) const;
    bool inButtonScope(const QualifiedName&) const;
    bool inSelectScope(const AtomicString& tagName) const;
    bool inSelectScope(const QualifiedName&) const;

    bool hasOnlyHTMLElementsInScope() const;
    bool hasNumberedHeaderElementInScope() const;

    bool hasOnlyOneElement() const;
    bool secondElementIsHTMLBodyElement() const;

    Element* htmlElement() const;
    Element* headElement() const;
    Element* bodyElement() const;

#ifndef NDEBUG
    void show();
#endif

private:
    void pushCommon(PassRefPtr<Element>);
    void popCommon();
    void removeNonTopCommon(Element*);

    OwnPtr<ElementRecord> m_top;

    // We remember <html>, <head> and <body> as they are pushed.  Their
    // ElementRecords keep them alive.  <html> is never popped.
    // FIXME: We don't currently require type-specific information about
    // these elements so we haven't yet bothered to plumb the types all the
    // way down through createElement, etc.
    Element* m_htmlElement;
    Element* m_headElement;
    Element* m_bodyElement;
};

} // namespace WebCore

#endif // HTMLElementStack_h
