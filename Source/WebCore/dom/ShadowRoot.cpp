/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ShadowRoot.h"

#include "AuthorStyleSheets.h"
#include "CSSStyleSheet.h"
#include "ElementTraversal.h"
#include "RenderElement.h"
#include "RuntimeEnabledFeatures.h"
#include "SlotAssignment.h"
#include "StyleResolver.h"
#include "markup.h"

namespace WebCore {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope {
    unsigned countersAndFlags[1];
    void* styleResolver;
    void* authorStyleSheets;
    void* host;
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    void* slotAssignment;
#endif
};

COMPILE_ASSERT(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), shadowroot_should_stay_small);

ShadowRoot::ShadowRoot(Document& document, Type type)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_type(type)
{
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)

ShadowRoot::ShadowRoot(Document& document, std::unique_ptr<SlotAssignment>&& slotAssignment)
    : DocumentFragment(document, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_type(Type::UserAgent)
    , m_slotAssignment(WTFMove(slotAssignment))
{
}

#endif

ShadowRoot::~ShadowRoot()
{
    // We cannot let ContainerNode destructor call willBeDeletedFrom()
    // for this ShadowRoot instance because TreeScope destructor
    // clears Node::m_treeScope thus ContainerNode is no longer able
    // to access it Document reference after that.
    willBeDeletedFrom(document());

    // We must remove all of our children first before the TreeScope destructor
    // runs so we don't go through TreeScopeAdopter for each child with a
    // destructed tree scope in each descendant.
    removeDetachedChildren();
}

StyleResolver& ShadowRoot::styleResolver()
{
    if (m_type == Type::UserAgent)
        return document().userAgentShadowTreeStyleResolver();

    if (!m_styleResolver) {
        // FIXME: We could share style resolver with shadow roots that have identical style.
        m_styleResolver = std::make_unique<StyleResolver>(document());
        if (m_authorStyleSheets)
            m_styleResolver->appendAuthorStyleSheets(m_authorStyleSheets->activeStyleSheets());
    }
    return *m_styleResolver;
}

void ShadowRoot::resetStyleResolver()
{
    m_styleResolver = nullptr;
}

AuthorStyleSheets& ShadowRoot::authorStyleSheets()
{
    if (!m_authorStyleSheets)
        m_authorStyleSheets = std::make_unique<AuthorStyleSheets>(*this);
    return *m_authorStyleSheets;
}

void ShadowRoot::updateStyle()
{
    bool shouldRecalcStyle = false;

    if (m_authorStyleSheets) {
        // FIXME: Make optimized updated work.
        shouldRecalcStyle = m_authorStyleSheets->updateActiveStyleSheets(AuthorStyleSheets::FullUpdate);
    }

    if (shouldRecalcStyle)
        setNeedsStyleRecalc();
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(*this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionCode& ec)
{
    if (isOrphan()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    if (RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, host(), AllowScriptingContent, ec))
        replaceChildrenWithFragment(*this, fragment.releaseNonNull(), ec);
}

bool ShadowRoot::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        return false;
    }
}

void ShadowRoot::setResetStyleInheritance(bool value)
{
    if (isOrphan())
        return;

    if (value != m_resetStyleInheritance) {
        m_resetStyleInheritance = value;
        if (host())
            setNeedsStyleRecalc();
    }
}

Ref<Node> ShadowRoot::cloneNodeInternal(Document&, CloningOperation)
{
    RELEASE_ASSERT_NOT_REACHED();
    return *static_cast<Node*>(nullptr); // ShadowRoots should never be cloned.
}

void ShadowRoot::removeAllEventListeners()
{
    DocumentFragment::removeAllEventListeners();
    for (Node* node = firstChild(); node; node = NodeTraversal::next(*node))
        node->removeAllEventListeners();
}

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)

HTMLSlotElement* ShadowRoot::findAssignedSlot(const Node& node)
{
    ASSERT(node.parentNode() == host());
    if (!m_slotAssignment)
        return nullptr;
    return m_slotAssignment->findAssignedSlot(node, *this);
}

void ShadowRoot::addSlotElementByName(const AtomicString& name, HTMLSlotElement& slot)
{
    if (!m_slotAssignment)
        m_slotAssignment = std::make_unique<SlotAssignment>();

    return m_slotAssignment->addSlotElementByName(name, slot, *this);
}

void ShadowRoot::removeSlotElementByName(const AtomicString& name, HTMLSlotElement& slot)
{
    return m_slotAssignment->removeSlotElementByName(name, slot, *this);
}

void ShadowRoot::invalidateSlotAssignments()
{
    if (m_slotAssignment)
        m_slotAssignment->invalidate(*this);
}

void ShadowRoot::invalidateDefaultSlotAssignments()
{
    if (m_slotAssignment)
        m_slotAssignment->invalidateDefaultSlot(*this);
}

const Vector<Node*>* ShadowRoot::assignedNodesForSlot(const HTMLSlotElement& slot)
{
    if (!m_slotAssignment)
        return nullptr;
    return m_slotAssignment->assignedNodesForSlot(slot, *this);
}

#endif

}
