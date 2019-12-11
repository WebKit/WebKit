/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SplitElementCommand.h"

#include "Element.h"
#include "HTMLNames.h"
#include <wtf/Assertions.h>

namespace WebCore {

SplitElementCommand::SplitElementCommand(Ref<Element>&& element, Ref<Node>&& atChild)
    : SimpleEditCommand(element->document())
    , m_element2(WTFMove(element))
    , m_atChild(WTFMove(atChild))
{
    ASSERT(m_atChild->parentNode() == m_element2.ptr());
}

void SplitElementCommand::executeApply()
{
    if (m_atChild->parentNode() != m_element2.ptr())
        return;
    
    Vector<Ref<Node>> children;
    for (Node* node = m_element2->firstChild(); node != m_atChild.ptr(); node = node->nextSibling())
        children.append(*node);

    auto* parent = m_element2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;
    if (parent->insertBefore(*m_element1, m_element2.ptr()).hasException())
        return;

    // Delete id attribute from the second element because the same id cannot be used for more than one element
    m_element2->removeAttribute(HTMLNames::idAttr);

    for (auto& child : children)
        m_element1->appendChild(child);
}
    
void SplitElementCommand::doApply()
{
    m_element1 = m_element2->cloneElementWithoutChildren(document());
    
    executeApply();
}

void SplitElementCommand::doUnapply()
{
    if (!m_element1 || !m_element1->hasEditableStyle() || !m_element2->hasEditableStyle())
        return;

    Vector<Ref<Node>> children;
    for (Node* node = m_element1->firstChild(); node; node = node->nextSibling())
        children.append(*node);

    RefPtr<Node> refChild = m_element2->firstChild();

    for (auto& child : children)
        m_element2->insertBefore(child, refChild.get());

    // Recover the id attribute of the original element.
    const AtomString& id = m_element1->getIdAttribute();
    if (!id.isNull())
        m_element2->setIdAttribute(id);

    m_element1->remove();
}

void SplitElementCommand::doReapply()
{
    if (!m_element1)
        return;
    
    executeApply();
}

#ifndef NDEBUG
void SplitElementCommand::getNodesInCommand(HashSet<Node*>& nodes)
{
    addNodeAndDescendants(m_element1.get(), nodes);
    addNodeAndDescendants(m_element2.ptr(), nodes);
    addNodeAndDescendants(m_atChild.ptr(), nodes);
}
#endif
    
} // namespace WebCore
