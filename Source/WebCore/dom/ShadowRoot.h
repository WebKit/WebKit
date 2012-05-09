/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowRoot_h
#define ShadowRoot_h

#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "TreeScope.h"
#include <wtf/DoublyLinkedList.h>

namespace WebCore {

class Document;
class DOMSelection;
class HTMLContentElement;
class HTMLContentSelector;
class InsertionPoint;
class ElementShadow;

class ShadowRoot : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    friend class WTF::DoublyLinkedListNode<ShadowRoot>;
public:
    static PassRefPtr<ShadowRoot> create(Element*, ExceptionCode&);

    // FIXME: We will support multiple shadow subtrees, however current implementation does not work well
    // if a shadow root is dynamically created. So we prohibit multiple shadow subtrees
    // in several elements for a while.
    // See https://bugs.webkit.org/show_bug.cgi?id=77503 and related bugs.
    enum ShadowRootCreationPurpose {
        CreatingUserAgentShadowRoot,
        CreatingAuthorShadowRoot,
    };
    static PassRefPtr<ShadowRoot> create(Element*, ShadowRootCreationPurpose, ExceptionCode& = ASSERT_NO_EXCEPTION);

    void recalcShadowTreeStyle(StyleChange);

    void setNeedsReattachHostChildrenAndShadow();
    void clearNeedsReattachHostChildrenAndShadow();
    bool needsReattachHostChildrenAndShadow();

    InsertionPoint* insertionPointFor(Node*) const;
    void hostChildrenChanged();

    virtual bool applyAuthorStyles() const OVERRIDE;
    void setApplyAuthorStyles(bool);

    Element* host() const { return shadowHost(); }
    ElementShadow* owner() const;

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionCode&);

    Element* activeElement() const;

    DOMSelection* selection();

    ShadowRoot* youngerShadowRoot() const { return prev(); }
    ShadowRoot* olderShadowRoot() const { return next(); }

    bool isYoungest() const { return !youngerShadowRoot(); }
    bool isOldest() const { return !olderShadowRoot(); }

    bool hasInsertionPoint() const;

    virtual void attach();

    bool isUsedForRendering() const;
    InsertionPoint* assignedTo() const;
    void setAssignedTo(InsertionPoint*);

private:
    ShadowRoot(Document*);
    virtual ~ShadowRoot();

    virtual String nodeName() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType) const;

    ShadowRoot* m_prev;
    ShadowRoot* m_next;
    bool m_applyAuthorStyles : 1;
    InsertionPoint* m_insertionPointAssignedTo;
};

inline InsertionPoint* ShadowRoot::assignedTo() const
{
    return m_insertionPointAssignedTo;
}

inline void ShadowRoot::setAssignedTo(InsertionPoint* insertionPoint)
{
    ASSERT(!m_insertionPointAssignedTo || !insertionPoint);
    m_insertionPointAssignedTo = insertionPoint;
}

inline bool ShadowRoot::isUsedForRendering() const
{
    return isYoungest() || assignedTo();
}

inline Element* ShadowRoot::activeElement() const
{
    if (Node* node = treeScope()->focusedNode())
        return node->isElementNode() ? toElement(node) : 0;
    return 0;
}

inline const ShadowRoot* toShadowRoot(const Node* node)
{
    ASSERT(!node || node->isShadowRoot());
    return static_cast<const ShadowRoot*>(node);
}

inline ShadowRoot* toShadowRoot(Node* node)
{
    return const_cast<ShadowRoot*>(toShadowRoot(static_cast<const Node*>(node)));
}

} // namespace

#endif
