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

#include "CompositeEditCommand.h"
#include "Element.h"
#include "ElementInlines.h"
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
    
    Ref element2 = m_element2;
    Vector<Ref<Node>> children;
    for (RefPtr node = element2->firstChild(); node != m_atChild.ptr(); node = node->nextSibling())
        children.append(*node);

    RefPtr parent = element2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;
    RefPtr element1 = m_element1;
    if (parent->insertBefore(*element1, element2.copyRef()).hasException())
        return;

    // Delete id attribute from the second element because the same id cannot be used for more than one element
    element2->removeAttribute(HTMLNames::idAttr);

    for (auto& child : children)
        element1->appendChild(child);
}
    
void SplitElementCommand::doApply()
{
    m_element1 = protectedElement2()->cloneElementWithoutChildren(protectedDocument());
    
    executeApply();
}

void SplitElementCommand::doUnapply()
{
    RefPtr element1 = m_element1;
    Ref element2 = m_element2;
    if (!element1 || !element1->hasEditableStyle() || !element2->hasEditableStyle())
        return;

    Vector<Ref<Node>> children;
    for (Node* node = element1->firstChild(); node; node = node->nextSibling())
        children.append(*node);

    RefPtr<Node> refChild = element2->firstChild();

    for (auto& child : children)
        element2->insertBefore(child, refChild.copyRef());

    // Recover the id attribute of the original element.
    const AtomString& id = element1->getIdAttribute();
    if (!id.isNull())
        element2->setIdAttribute(id);

    element1->remove();
}

void SplitElementCommand::doReapply()
{
    if (!m_element1)
        return;
    
    executeApply();
}

#ifndef NDEBUG
void SplitElementCommand::getNodesInCommand(HashSet<Ref<Node>>& nodes)
{
    addNodeAndDescendants(protectedElement1().get(), nodes);
    addNodeAndDescendants(protectedElement2().ptr(), nodes);
    addNodeAndDescendants(Ref { m_atChild }.ptr(), nodes);
}
#endif
    
} // namespace WebCore
