/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "HTMLStackItem.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Ref.h>

namespace WebCore {

class ContainerNode;
class Element;
class QualifiedName;

// NOTE: The HTML5 spec uses a backwards (grows downward) stack.  We're using
// more standard (grows upwards) stack terminology here.
class HTMLElementStack {
    WTF_MAKE_NONCOPYABLE(HTMLElementStack); WTF_MAKE_FAST_ALLOCATED;
public:
    HTMLElementStack() = default;
    ~HTMLElementStack();

    class ElementRecord {
        WTF_MAKE_NONCOPYABLE(ElementRecord); WTF_MAKE_FAST_ALLOCATED;
    public:
        ElementRecord(Ref<HTMLStackItem>&&, std::unique_ptr<ElementRecord>);
        ~ElementRecord();
    
        Element& element() const { return m_item->element(); }
        ContainerNode& node() const { return m_item->node(); }
        const AtomString& namespaceURI() const { return m_item->namespaceURI(); }
        HTMLStackItem& stackItem() { return m_item.get(); }
        const HTMLStackItem& stackItem() const { return m_item.get(); }

        void replaceElement(Ref<HTMLStackItem>&&);

        bool isAbove(ElementRecord&) const;

        ElementRecord* next() const { return m_next.get(); }

    private:
        friend class HTMLElementStack;

        std::unique_ptr<ElementRecord> releaseNext() { return WTFMove(m_next); }
        void setNext(std::unique_ptr<ElementRecord> next) { m_next = WTFMove(next); }

        Ref<HTMLStackItem> m_item;
        std::unique_ptr<ElementRecord> m_next;
    };

    unsigned stackDepth() const { return m_stackDepth; }

    // Inlining this function is a (small) performance win on the parsing
    // benchmark.
    Element& top() const
    {
        return m_top->element();
    }

    ContainerNode& topNode() const
    {
        return m_top->node();
    }

    HTMLStackItem& topStackItem() const
    {
        return m_top->stackItem();
    }

    HTMLStackItem* oneBelowTop() const;
    ElementRecord& topRecord() const;
    ElementRecord* find(Element&) const;
    ElementRecord* furthestBlockForFormattingElement(Element&) const;
    ElementRecord* topmost(const AtomString& tagName) const;

    void insertAbove(Ref<HTMLStackItem>&&, ElementRecord&);

    void push(Ref<HTMLStackItem>&&);
    void pushRootNode(Ref<HTMLStackItem>&&);
    void pushHTMLHtmlElement(Ref<HTMLStackItem>&&);
    void pushHTMLHeadElement(Ref<HTMLStackItem>&&);
    void pushHTMLBodyElement(Ref<HTMLStackItem>&&);

    void pop();
    void popUntil(const AtomString& tagName);
    void popUntil(Element&);
    void popUntilPopped(const AtomString& tagName);
    void popUntilPopped(const QualifiedName& tagName) { popUntilPopped(tagName.localName()); }

    void popUntilPopped(Element&);
    void popUntilNumberedHeaderElementPopped();
    void popUntilTableScopeMarker(); // "clear the stack back to a table context" in the spec.
    void popUntilTableBodyScopeMarker(); // "clear the stack back to a table body context" in the spec.
    void popUntilTableRowScopeMarker(); // "clear the stack back to a table row context" in the spec.
    void popUntilForeignContentScopeMarker();
    void popHTMLHeadElement();
    void popHTMLBodyElement();
    void popAll();

    static bool isMathMLTextIntegrationPoint(HTMLStackItem&);
    static bool isHTMLIntegrationPoint(HTMLStackItem&);

    void remove(Element&);
    void removeHTMLHeadElement(Element&);

    bool contains(Element&) const;
    bool contains(const AtomString& tagName) const;

    bool inScope(Element&) const;
    bool inScope(const AtomString& tagName) const;
    bool inScope(const QualifiedName&) const;
    bool inListItemScope(const AtomString& tagName) const;
    bool inListItemScope(const QualifiedName&) const;
    bool inTableScope(const AtomString& tagName) const;
    bool inTableScope(const QualifiedName&) const;
    bool inButtonScope(const AtomString& tagName) const;
    bool inButtonScope(const QualifiedName&) const;
    bool inSelectScope(const AtomString& tagName) const;
    bool inSelectScope(const QualifiedName&) const;

    bool hasNumberedHeaderElementInScope() const;

    bool hasOnlyOneElement() const;
    bool secondElementIsHTMLBodyElement() const;
    bool hasTemplateInHTMLScope() const;
    Element& htmlElement() const;
    Element& headElement() const;
    Element& bodyElement() const;

    ContainerNode& rootNode() const;

#if ENABLE(TREE_DEBUGGING)
    void show();
#endif

private:
    void pushCommon(Ref<HTMLStackItem>&&);
    void pushRootNodeCommon(Ref<HTMLStackItem>&&);
    void popCommon();
    void removeNonTopCommon(Element&);

    std::unique_ptr<ElementRecord> m_top;

    // We remember the root node, <head> and <body> as they are pushed. Their
    // ElementRecords keep them alive. The root node is never popped.
    // FIXME: We don't currently require type-specific information about
    // these elements so we haven't yet bothered to plumb the types all the
    // way down through createElement, etc.
    ContainerNode* m_rootNode { nullptr };
    Element* m_headElement { nullptr };
    Element* m_bodyElement { nullptr };
    unsigned m_stackDepth { 0 };
};
    
} // namespace WebCore
