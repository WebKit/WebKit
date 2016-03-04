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

namespace WebCore {

class AuthorStyleSheets;
class HTMLSlotElement;
class SlotAssignment;

class ShadowRoot final : public DocumentFragment, public TreeScope {
public:
    enum class Type : uint8_t {
        UserAgent = 0,
        Closed,
        Open,
    };

    static Ref<ShadowRoot> create(Document& document, Type type)
    {
        return adoptRef(*new ShadowRoot(document, type));
    }

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    static Ref<ShadowRoot> create(Document& document, std::unique_ptr<SlotAssignment>&& assignment)
    {
        return adoptRef(*new ShadowRoot(document, WTFMove(assignment)));
    }
#endif

    virtual ~ShadowRoot();

    StyleResolver& styleResolver();
    AuthorStyleSheets& authorStyleSheets();
    
    void updateStyle();
    void resetStyleResolver();

    bool resetStyleInheritance() const { return m_resetStyleInheritance; }
    void setResetStyleInheritance(bool);

    Element* host() const { return m_host; }
    void setHost(Element* host) { m_host = host; }

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionCode&);

    Element* activeElement() const;

    Type type() const { return m_type; }

    void removeAllEventListeners() override;

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    HTMLSlotElement* findAssignedSlot(const Node&);

    void addSlotElementByName(const AtomicString&, HTMLSlotElement&);
    void removeSlotElementByName(const AtomicString&, HTMLSlotElement&);

    void invalidateSlotAssignments();
    void invalidateDefaultSlotAssignments();

    const Vector<Node*>* assignedNodesForSlot(const HTMLSlotElement&);
#endif

protected:
    ShadowRoot(Document&, Type);

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    ShadowRoot(Document&, std::unique_ptr<SlotAssignment>&&);
#endif

    // FIXME: This shouldn't happen. https://bugs.webkit.org/show_bug.cgi?id=88834
    bool isOrphan() const { return !m_host; }

private:
    bool childTypeAllowed(NodeType) const override;

    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;

    bool m_resetStyleInheritance { false };
    Type m_type { Type::UserAgent };

    Element* m_host { nullptr };

    std::unique_ptr<StyleResolver> m_styleResolver;
    std::unique_ptr<AuthorStyleSheets> m_authorStyleSheets;

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    std::unique_ptr<SlotAssignment> m_slotAssignment;
#endif
};

inline Element* ShadowRoot::activeElement() const
{
    return treeScope().focusedElement();
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

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShadowRoot)
    static bool isType(const WebCore::Node& node) { return node.isShadowRoot(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
