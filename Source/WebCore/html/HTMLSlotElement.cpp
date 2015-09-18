/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "HTMLSlotElement.h"

#include "ElementChildIterator.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

Ref<HTMLSlotElement> HTMLSlotElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLSlotElement(tagName, document));
}

HTMLSlotElement::HTMLSlotElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(slotTag));
}

HTMLSlotElement::InsertionNotificationRequest HTMLSlotElement::insertedInto(ContainerNode& insertionPoint)
{
    auto insertionResult = HTMLElement::insertedInto(insertionPoint);
    ASSERT_UNUSED(insertionResult, insertionResult == InsertionDone);

    if (auto shadowRoot = containingShadowRoot())
        shadowRoot->addSlotElementByName(fastGetAttribute(nameAttr), *this);

    return InsertionDone;
}

void HTMLSlotElement::removedFrom(ContainerNode& insertionPoint)
{
    // Can't call containingShadowRoot() here since this node has already been disconnected from the parent.
    if (isInShadowTree()) {
        auto& oldShadowRoot = downcast<ShadowRoot>(insertionPoint.treeScope().rootNode());
        oldShadowRoot.removeSlotElementByName(fastGetAttribute(nameAttr), *this);
    }

    HTMLElement::removedFrom(insertionPoint);
}

void HTMLSlotElement::attributeChanged(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason reason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);

    if (name == nameAttr) {
        if (auto* shadowRoot = containingShadowRoot()) {
            shadowRoot->removeSlotElementByName(oldValue, *this);
            shadowRoot->addSlotElementByName(newValue, *this);
        }
    }
}

Vector<RefPtr<Node>> HTMLSlotElement::getDistributedNodes() const
{
    Vector<RefPtr<Node>> distributedNodes;

    if (auto shadowRoot = containingShadowRoot()) {
        if (auto assignedNodes = shadowRoot->assignedNodesForSlot(*this)) {
            for (auto* node : *assignedNodes)
                distributedNodes.append(node);
        }
    }

    return distributedNodes;
}

}
