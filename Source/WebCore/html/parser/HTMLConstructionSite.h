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

#include "FragmentScriptingPermission.h"
#include "HTMLElementStack.h"
#include "HTMLFormattingElementList.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace WebCore {

struct HTMLConstructionSiteTask {
    enum Operation {
        Insert,
        InsertAlreadyParsedChild,
        Reparent,
        TakeAllChildrenAndReparent,
    };

    explicit HTMLConstructionSiteTask(Operation op)
        : operation(op)
        , selfClosing(false)
    {
    }

    ContainerNode* oldParent()
    {
        // It's sort of ugly, but we store the |oldParent| in the |child| field
        // of the task so that we don't bloat the HTMLConstructionSiteTask
        // object in the common case of the Insert operation.
        return downcast<ContainerNode>(child.get());
    }

    Operation operation;
    RefPtr<ContainerNode> parent;
    RefPtr<Node> nextChild;
    RefPtr<Node> child;
    bool selfClosing;
};

} // namespace WebCore

namespace WTF {
template<> struct VectorTraits<WebCore::HTMLConstructionSiteTask> : SimpleClassVectorTraits { };
} // namespace WTF

namespace WebCore {

enum WhitespaceMode {
    AllWhitespace,
    NotAllWhitespace,
    WhitespaceUnknown
};

class AtomHTMLToken;
struct CustomElementConstructionData;
class Document;
class Element;
class HTMLFormElement;
class JSCustomElementInterface;
class WhitespaceCache;

class HTMLConstructionSite {
    WTF_MAKE_NONCOPYABLE(HTMLConstructionSite);
public:
    HTMLConstructionSite(Document&, ParserContentPolicy, unsigned maximumDOMTreeDepth);
    HTMLConstructionSite(DocumentFragment&, ParserContentPolicy, unsigned maximumDOMTreeDepth);
    ~HTMLConstructionSite();

    void executeQueuedTasks();

    void setDefaultCompatibilityMode();
    void finishedParsing();

    void insertDoctype(AtomHTMLToken&&);
    void insertComment(AtomHTMLToken&&);
    void insertCommentOnDocument(AtomHTMLToken&&);
    void insertCommentOnHTMLHtmlElement(AtomHTMLToken&&);
    void insertHTMLElement(AtomHTMLToken&&);
    std::unique_ptr<CustomElementConstructionData> insertHTMLElementOrFindCustomElementInterface(AtomHTMLToken&&);
    void insertCustomElement(Ref<Element>&&, const AtomString& localName, Vector<Attribute>&&);
    void insertSelfClosingHTMLElement(AtomHTMLToken&&);
    void insertFormattingElement(AtomHTMLToken&&);
    void insertHTMLHeadElement(AtomHTMLToken&&);
    void insertHTMLBodyElement(AtomHTMLToken&&);
    void insertHTMLFormElement(AtomHTMLToken&&, bool isDemoted = false);
    void insertScriptElement(AtomHTMLToken&&);
    void insertTextNode(const String&, WhitespaceMode = WhitespaceUnknown);
    void insertForeignElement(AtomHTMLToken&&, const AtomString& namespaceURI);

    void insertHTMLHtmlStartTagBeforeHTML(AtomHTMLToken&&);
    void insertHTMLHtmlStartTagInBody(AtomHTMLToken&&);
    void insertHTMLBodyStartTagInBody(AtomHTMLToken&&);

    void reparent(HTMLElementStack::ElementRecord& newParent, HTMLElementStack::ElementRecord& child);
    // insertAlreadyParsedChild assumes that |child| has already been parsed (i.e., we're just
    // moving it around in the tree rather than parsing it for the first time). That means
    // this function doesn't call beginParsingChildren / finishParsingChildren.
    void insertAlreadyParsedChild(HTMLStackItem& newParent, HTMLElementStack::ElementRecord& child);
    void takeAllChildrenAndReparent(HTMLStackItem& newParent, HTMLElementStack::ElementRecord& oldParent);

    Ref<HTMLStackItem> createElementFromSavedToken(HTMLStackItem&);

    bool shouldFosterParent() const;
    void fosterParent(Ref<Node>&&);

    std::optional<unsigned> indexOfFirstUnopenFormattingElement() const;
    void reconstructTheActiveFormattingElements();

    void generateImpliedEndTags();
    void generateImpliedEndTagsWithExclusion(const AtomString& tagName);

    bool inQuirksMode() { return m_inQuirksMode; }

    bool isEmpty() const { return !m_openElements.stackDepth(); }
    Element& currentElement() const { return m_openElements.top(); }
    ContainerNode& currentNode() const { return m_openElements.topNode(); }
    HTMLStackItem& currentStackItem() const { return m_openElements.topStackItem(); }
    HTMLStackItem* oneBelowTop() const { return m_openElements.oneBelowTop(); }
    Document& ownerDocumentForCurrentNode();
    HTMLElementStack& openElements() const { return m_openElements; }
    HTMLFormattingElementList& activeFormattingElements() const { return m_activeFormattingElements; }
    bool currentIsRootNode() { return &m_openElements.topNode() == &m_openElements.rootNode(); }

    Element& head() const { return m_head->element(); }
    HTMLStackItem* headStackItem() const { return m_head.get(); }

    void setForm(HTMLFormElement*);
    HTMLFormElement* form() const { return m_form.get(); }
    RefPtr<HTMLFormElement> takeForm();

    ParserContentPolicy parserContentPolicy() { return m_parserContentPolicy; }

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    bool isTelephoneNumberParsingEnabled() { return m_document.isTelephoneNumberParsingEnabled(); }
#endif

    class RedirectToFosterParentGuard {
        WTF_MAKE_NONCOPYABLE(RedirectToFosterParentGuard);
    public:
        explicit RedirectToFosterParentGuard(HTMLConstructionSite& tree)
            : m_redirectAttachToFosterParentChange(tree.m_redirectAttachToFosterParent, true)
        { }

    private:
        SetForScope<bool> m_redirectAttachToFosterParentChange;
    };

    static bool isFormattingTag(const AtomString&);

private:
    // In the common case, this queue will have only one task because most
    // tokens produce only one DOM mutation.
    typedef Vector<HTMLConstructionSiteTask, 1> TaskQueue;

    void setCompatibilityMode(DocumentCompatibilityMode);
    void setCompatibilityModeFromDoctype(const String& name, const String& publicId, const String& systemId);

    void attachLater(ContainerNode& parent, Ref<Node>&& child, bool selfClosing = false);

    void findFosterSite(HTMLConstructionSiteTask&);

    RefPtr<Element> createHTMLElementOrFindCustomElementInterface(AtomHTMLToken&, JSCustomElementInterface**);
    Ref<Element> createHTMLElement(AtomHTMLToken&);
    Ref<Element> createElement(AtomHTMLToken&, const AtomString& namespaceURI);

    void mergeAttributesFromTokenIntoElement(AtomHTMLToken&&, Element&);
    void dispatchDocumentElementAvailableIfNeeded();

    Document& m_document;
    
    // This is the root ContainerNode to which the parser attaches all newly
    // constructed nodes. It points to a DocumentFragment when parsing fragments
    // and a Document in all other cases.
    ContainerNode& m_attachmentRoot;
    
    RefPtr<HTMLStackItem> m_head;
    RefPtr<HTMLFormElement> m_form;
    mutable HTMLElementStack m_openElements;
    mutable HTMLFormattingElementList m_activeFormattingElements;

    TaskQueue m_taskQueue;

    ParserContentPolicy m_parserContentPolicy;
    bool m_isParsingFragment;

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-intable
    // In the "in table" insertion mode, we sometimes get into a state where
    // "whenever a node would be inserted into the current node, it must instead
    // be foster parented."  This flag tracks whether we're in that state.
    bool m_redirectAttachToFosterParent;

    unsigned m_maximumDOMTreeDepth;

    bool m_inQuirksMode;

    WhitespaceCache& m_whitespaceCache;
};

class WhitespaceCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WhitespaceCache() = default;

    AtomString lookup(const String&, WhitespaceMode);

private:
    template<WhitespaceMode> uint64_t codeForString(const String&);

    constexpr static uint64_t overflowWhitespaceCode = static_cast<uint64_t>(-1);
    constexpr static size_t maximumCachedStringLength = 128;

    // Parallel arrays storing a 64 bit code and an index into m_atoms for the
    // most recently atomized whitespace-only string of a given length. The
    // indices into these two arrays are the string length minus 1, so the code
    // for a whitespace-only string of length 2 is stored at m_codes[1], etc.
    uint64_t m_codes[maximumCachedStringLength] { 0 };
    uint8_t m_indexes[maximumCachedStringLength] { 0 };

    Vector<AtomString, maximumCachedStringLength> m_atoms;
};

} // namespace WebCore
