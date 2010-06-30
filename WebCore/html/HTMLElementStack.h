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

namespace WebCore {

class AtomicString;
class Element;

class HTMLElementStack : public Noncopyable {
public:
    HTMLElementStack();
    ~HTMLElementStack();

    Element* top() const;

    void push(PassRefPtr<Element>);
    void pushHTMLHtmlElement(PassRefPtr<Element>);
    void pushHTMLHeadElement(PassRefPtr<Element>);
    void pushHTMLBodyElement(PassRefPtr<Element>);

    void pop();
    void popUntil(const AtomicString& tagName);
    void popHTMLHeadElement();

    void remove(Element*);
    void removeHTMLHeadElement(Element*);

    bool contains(Element*) const;

    bool inScope(Element*) const;
    bool inScope(const AtomicString& tagName) const;
    bool inListItemScope(const AtomicString& tagName) const;
    bool inTableScope(const AtomicString& tagName) const;

    Element* htmlElement();
    Element* headElement();
    Element* bodyElement();

    // Public so free functions can use it, but defined privately.
    class ElementRecord;
private:
    void pushCommon(PassRefPtr<Element>);
    void popCommon();
    void removeNonFirstCommon(Element*);

    OwnPtr<ElementRecord> m_top;

    // We remember <html>, <head> and <body> as they are pushed.  Their
    // ElementRecords keep them alive.  <html> and <body> are never popped.
    // FIXME: We don't currently require type-specific information about
    // these elements so we haven't yet bothered to plumb the types all the
    // way down through createElement, etc.
    Element* m_htmlElement;
    Element* m_headElement;
    Element* m_bodyElement;
};

} // namespace WebCore

#endif // HTMLElementStack_h
