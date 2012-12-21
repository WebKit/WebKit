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

#include "ContainerNode.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "TreeScope.h"
#include <wtf/DoublyLinkedList.h>

namespace WebCore {

class Document;
class DOMSelection;
class ElementShadow;
class InsertionPoint;
class ShadowRootContentDistributionData;

class ShadowRoot : public DocumentFragment, public TreeScope, public DoublyLinkedListNode<ShadowRoot> {
    friend class WTF::DoublyLinkedListNode<ShadowRoot>;
public:
    static PassRefPtr<ShadowRoot> create(Element*, ExceptionCode&);

    // FIXME: We will support multiple shadow subtrees, however current implementation does not work well
    // if a shadow root is dynamically created. So we prohibit multiple shadow subtrees
    // in several elements for a while.
    // See https://bugs.webkit.org/show_bug.cgi?id=77503 and related bugs.
    enum ShadowRootType {
        UserAgentShadowRoot,
        AuthorShadowRoot
    };
    static PassRefPtr<ShadowRoot> create(Element*, ShadowRootType, ExceptionCode& = ASSERT_NO_EXCEPTION);

    void recalcStyle(StyleChange);

    virtual bool applyAuthorStyles() const OVERRIDE;
    void setApplyAuthorStyles(bool);
    virtual bool resetStyleInheritance() const OVERRIDE;
    void setResetStyleInheritance(bool);

    Element* host() const;
    void setHost(Element*);
    ElementShadow* owner() const;

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionCode&);

    Element* activeElement() const;

    ShadowRoot* youngerShadowRoot() const { return prev(); }
    ShadowRoot* olderShadowRoot() const { return next(); }

    bool isYoungest() const { return !youngerShadowRoot(); }
    bool isOldest() const { return !olderShadowRoot(); }

    bool hasInsertionPoint() const;

    virtual void attach();

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    bool isUsedForRendering() const;
    InsertionPoint* assignedTo() const;
    void setAssignedTo(InsertionPoint*);

    bool hasShadowInsertionPoint() const;
    bool hasContentElement() const;

    void registerInsertionPoint(InsertionPoint*);
    void unregisterInsertionPoint(InsertionPoint*);
    
    void registerElementShadow();
    void unregisterElementShadow();
    bool hasElementShadow() const;
    unsigned countElementShadow() const;

    const Vector<RefPtr<InsertionPoint> >& insertionPointList();

    virtual void registerScopedHTMLStyleChild() OVERRIDE;
    virtual void unregisterScopedHTMLStyleChild() OVERRIDE;

    ShadowRootType type() const { return m_isAuthorShadowRoot ? AuthorShadowRoot : UserAgentShadowRoot; }
    bool isAccessible() const { return type() == AuthorShadowRoot; }

    PassRefPtr<Node> cloneNode(bool, ExceptionCode&);

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

private:
    explicit ShadowRoot(Document*);
    virtual ~ShadowRoot();
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType) const;
    virtual void childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta) OVERRIDE;
    virtual bool documentFragmentIsShadowRoot() const OVERRIDE { return true; }

    void setType(ShadowRootType type) { m_isAuthorShadowRoot = type == AuthorShadowRoot; }

    ShadowRootContentDistributionData* distributionData() { return m_distributionData.get(); }
    const ShadowRootContentDistributionData* distributionData() const { return m_distributionData.get(); }
    ShadowRootContentDistributionData* ensureDistributionData();

    ShadowRoot* m_prev;
    ShadowRoot* m_next;
    OwnPtr<ShadowRootContentDistributionData> m_distributionData;
    unsigned m_numberOfStyles : 28;
    unsigned m_applyAuthorStyles : 1;
    unsigned m_resetStyleInheritance : 1;
    unsigned m_isAuthorShadowRoot : 1;
    unsigned m_registeredWithParentShadowRoot : 1;
};

inline Element* ShadowRoot::host() const
{
    return toElement(parentOrHostNode());
}

inline void ShadowRoot::setHost(Element* host)
{
    setParentOrHostNode(host);
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
