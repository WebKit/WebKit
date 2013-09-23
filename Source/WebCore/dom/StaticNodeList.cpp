/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StaticNodeList.h"

namespace WebCore {

unsigned StaticNodeList::length() const
{
    return m_nodes.size();
}

Node* StaticNodeList::item(unsigned index) const
{
    if (index < m_nodes.size())
        return &const_cast<Node&>(m_nodes[index].get());
    return nullptr;
}

Node* StaticNodeList::namedItem(const AtomicString& elementId) const
{
    for (unsigned i = 0, length = m_nodes.size(); i < length; ++i) {
        Node& node = const_cast<Node&>(m_nodes[i].get());
        if (node.isElementNode() && toElement(node).getIdAttribute() == elementId)
            return &node;
    }
    return nullptr;
}

unsigned StaticElementList::length() const
{
    return m_elements.size();
}

Node* StaticElementList::item(unsigned index) const
{
    if (index < m_elements.size())
        return &const_cast<Element&>(m_elements[index].get());
    return nullptr;
}

Node* StaticElementList::namedItem(const AtomicString& elementId) const
{
    for (unsigned i = 0, length = m_elements.size(); i < length; ++i) {
        Element& element = const_cast<Element&>(m_elements[i].get());
        if (element.getIdAttribute() == elementId)
            return &element;
    }
    return nullptr;
}

} // namespace WebCore
