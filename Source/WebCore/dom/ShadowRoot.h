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

#pragma once

#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "ShadowRootMode.h"

namespace WebCore {

class HTMLSlotElement;
class SlotAssignment;
class StyleSheetList;

class ShadowRoot final : public DocumentFragment, public TreeScope {
    WTF_MAKE_ISO_ALLOCATED(ShadowRoot);
public:
    static Ref<ShadowRoot> create(Document& document, ShadowRootMode type)
    {
        return adoptRef(*new ShadowRoot(document, type));
    }

    static Ref<ShadowRoot> create(Document& document, std::unique_ptr<SlotAssignment>&& assignment)
    {
        return adoptRef(*new ShadowRoot(document, WTFMove(assignment)));
    }

    virtual ~ShadowRoot();

    using TreeScope::rootNode;

    Style::Scope& styleScope();
    StyleSheetList& styleSheets();

    bool resetStyleInheritance() const { return m_resetStyleInheritance; }
    void setResetStyleInheritance(bool);

    Element* host() const { return m_host; }
    void setHost(Element* host) { m_host = host; }

    String innerHTML() const;
    ExceptionOr<void> setInnerHTML(const String&);

    Element* activeElement() const;

    ShadowRootMode mode() const { return m_type; }
    bool shouldFireSlotchangeEvent() const { return m_type != ShadowRootMode::UserAgent && !m_hasBegunDeletingDetachedChildren; }

    void removeAllEventListeners() override;

    HTMLSlotElement* findAssignedSlot(const Node&);

    void renameSlotElement(HTMLSlotElement&, const AtomicString& oldName, const AtomicString& newName);
    void addSlotElementByName(const AtomicString&, HTMLSlotElement&);
    void removeSlotElementByName(const AtomicString&, HTMLSlotElement&, ContainerNode& oldParentOfRemovedTree);
    void slotFallbackDidChange(HTMLSlotElement&);
    void resolveSlotsBeforeNodeInsertionOrRemoval();
    void willRemoveAllChildren(ContainerNode&);

    void didRemoveAllChildrenOfShadowHost();
    void didChangeDefaultSlot();
    void hostChildElementDidChange(const Element&);
    void hostChildElementDidChangeSlotAttribute(Element&, const AtomicString& oldValue, const AtomicString& newValue);

    const Vector<Node*>* assignedNodesForSlot(const HTMLSlotElement&);

    void moveShadowRootToNewParentScope(TreeScope&, Document&);
    void moveShadowRootToNewDocument(Document&);

protected:
    ShadowRoot(Document&, ShadowRootMode);

    ShadowRoot(Document&, std::unique_ptr<SlotAssignment>&&);

private:
    bool childTypeAllowed(NodeType) const override;

    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;

    Node::InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode& insertionPoint) override;

    void childrenChanged(const ChildChange&) override;

    bool m_resetStyleInheritance { false };
    bool m_hasBegunDeletingDetachedChildren { false };
    ShadowRootMode m_type { ShadowRootMode::UserAgent };

    Element* m_host { nullptr };
    RefPtr<StyleSheetList> m_styleSheetList;

    std::unique_ptr<Style::Scope> m_styleScope;
    std::unique_ptr<SlotAssignment> m_slotAssignment;
};

inline Element* ShadowRoot::activeElement() const
{
    return treeScope().focusedElementInScope();
}

inline ShadowRoot* Node::shadowRoot() const
{
    if (!is<Element>(*this))
        return nullptr;
    return downcast<Element>(*this).shadowRoot();
}

inline ContainerNode* Node::parentOrShadowHostNode() const
{
    ASSERT(isMainThreadOrGCThread());
    if (is<ShadowRoot>(*this))
        return downcast<ShadowRoot>(*this).host();
    return parentNode();
}

inline bool hasShadowRootParent(const Node& node)
{
    return node.parentNode() && node.parentNode()->isShadowRoot();
}

Vector<ShadowRoot*> assignedShadowRootsIfSlotted(const Node&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShadowRoot)
    static bool isType(const WebCore::Node& node) { return node.isShadowRoot(); }
SPECIALIZE_TYPE_TRAITS_END()
