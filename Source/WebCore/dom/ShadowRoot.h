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
#include "ContentDistributor.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "TreeScope.h"

namespace WebCore {

class ShadowRoot final : public DocumentFragment, public TreeScope {
public:
    enum ShadowRootType {
        UserAgentShadowRoot = 0,
    };

    static PassRefPtr<ShadowRoot> create(Document& document, ShadowRootType type)
    {
        return adoptRef(new ShadowRoot(document, type));
    }

    virtual ~ShadowRoot();

    virtual bool applyAuthorStyles() const override { return m_applyAuthorStyles; }
    void setApplyAuthorStyles(bool);
    bool resetStyleInheritance() const { return m_resetStyleInheritance; }
    void setResetStyleInheritance(bool);

    Element* hostElement() const { return m_hostElement; }
    void setHostElement(Element* hostElement) { m_hostElement = hostElement; }

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionCode&);

    Element* activeElement() const;

    ShadowRootType type() const { return static_cast<ShadowRootType>(m_type); }

    PassRefPtr<Node> cloneNode(bool, ExceptionCode&);

    ContentDistributor& distributor() { return m_distributor; }
    void invalidateDistribution() { m_distributor.invalidateDistribution(hostElement()); }

    virtual void removeAllEventListeners() override;

private:
    ShadowRoot(Document&, ShadowRootType);

    virtual void dropChildren() override;
    virtual bool childTypeAllowed(NodeType) const override;
    virtual void childrenChanged(const ChildChange&) override;

    // ShadowRoots should never be cloned.
    virtual PassRefPtr<Node> cloneNode(bool) override { return 0; }

    // FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
    bool isOrphan() const { return !hostElement(); }

    unsigned m_applyAuthorStyles : 1;
    unsigned m_resetStyleInheritance : 1;
    unsigned m_type : 1;

    Element* m_hostElement;

    ContentDistributor m_distributor;
};

inline Element* ShadowRoot::activeElement() const
{
    return treeScope().focusedElement();
}

inline const ShadowRoot* toShadowRoot(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isShadowRoot());
    return static_cast<const ShadowRoot*>(node);
}

inline ShadowRoot* toShadowRoot(Node* node)
{
    return const_cast<ShadowRoot*>(toShadowRoot(static_cast<const Node*>(node)));
}

inline ShadowRoot* Node::shadowRoot() const
{
    if (!isElementNode())
        return 0;
    return toElement(this)->shadowRoot();
}

inline ContainerNode* Node::parentOrShadowHostNode() const
{
    ASSERT(isMainThreadOrGCThread());
    if (isShadowRoot())
        return toShadowRoot(this)->hostElement();
    return parentNode();
}

inline bool hasShadowRootParent(const Node& node)
{
    return node.parentNode() && node.parentNode()->isShadowRoot();
}

} // namespace

#endif
