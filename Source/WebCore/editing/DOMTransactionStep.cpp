/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL GOOGLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(UNDO_MANAGER)

#include "DOMTransactionStep.h"

#include "CharacterData.h"
#include "ContainerNode.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "Node.h"

namespace WebCore {

NodeInsertingDOMTransactionStep::NodeInsertingDOMTransactionStep(Node* node, Node* child)
    : m_node(node)
    , m_refChild(child->nextSibling())
    , m_child(child)
{
}

PassRefPtr<NodeInsertingDOMTransactionStep> NodeInsertingDOMTransactionStep::create(Node* node, Node* child)
{
    return adoptRef(new NodeInsertingDOMTransactionStep(node, child));
}

void NodeInsertingDOMTransactionStep::unapply()
{
    if (m_child && m_child->parentNode() != m_node.get())
        return;
    if (m_refChild && m_refChild->parentNode() != m_node.get())
        return;
    if (m_refChild && m_refChild->previousSibling() != m_child.get())
        return;
    
    ExceptionCode ec;
    m_node->removeChild(m_child.get(), ec);
}

void NodeInsertingDOMTransactionStep::reapply()
{
    if (m_child && m_child->parentNode())
        return;
    if (m_refChild && m_refChild->parentNode() != m_node.get())
        return;
    
    ExceptionCode ec;
    m_node->insertBefore(m_child, m_refChild.get(), ec);
}

NodeRemovingDOMTransactionStep::NodeRemovingDOMTransactionStep(Node* node, Node* child)
    : m_node(node)
    , m_refChild(child->nextSibling())
    , m_child(child)
{
}

PassRefPtr<NodeRemovingDOMTransactionStep> NodeRemovingDOMTransactionStep::create(Node* node, Node* child)
{
    return adoptRef(new NodeRemovingDOMTransactionStep(node, child));
}

void NodeRemovingDOMTransactionStep::unapply()
{
    if (m_child && m_child->parentNode())
        return;
    if (m_refChild && m_refChild->parentNode() != m_node.get())
        return;

    ExceptionCode ec;
    m_node->insertBefore(m_child, m_refChild.get(), ec);
}

void NodeRemovingDOMTransactionStep::reapply()
{
    if (m_child && m_child->parentNode() != m_node.get())
        return;
    if (m_refChild && m_refChild->parentNode() != m_node.get())
        return;
    if (m_refChild && m_refChild->previousSibling() != m_child.get())
        return;
    
    ExceptionCode ec;
    m_node->removeChild(m_child.get(), ec);
}

DataReplacingDOMTransactionStep::DataReplacingDOMTransactionStep(CharacterData* node, unsigned offset, unsigned count, const String& data, const String& replacedData)
    : m_node(node)
    , m_offset(offset)
    , m_count(count)
    , m_data(data)
    , m_replacedData(replacedData)
{
}

PassRefPtr<DataReplacingDOMTransactionStep> DataReplacingDOMTransactionStep::create(CharacterData* node, unsigned offset, unsigned count, const String& data, const String& replacedData)
{
    return adoptRef(new DataReplacingDOMTransactionStep(node, offset, count, data, replacedData));
}

void DataReplacingDOMTransactionStep::unapply()
{
    if (m_node->length() < m_offset)
        return;
    
    ExceptionCode ec;
    m_node->replaceData(m_offset, m_data.length(), m_replacedData, ec);
}

void DataReplacingDOMTransactionStep::reapply()
{
    if (m_node->length() < m_offset)
        return;
    
    ExceptionCode ec;
    m_node->replaceData(m_offset, m_count, m_data, ec);
}

AttrChangingDOMTransactionStep::AttrChangingDOMTransactionStep(Element* element, const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
    : m_element(element)
    , m_name(name)
    , m_oldValue(oldValue)
    , m_newValue(newValue)
{
}

PassRefPtr<AttrChangingDOMTransactionStep> AttrChangingDOMTransactionStep::create(Element* element, const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    return adoptRef(new AttrChangingDOMTransactionStep(element, name, oldValue, newValue));
}

void AttrChangingDOMTransactionStep::unapply()
{
    m_element->setAttribute(m_name, m_oldValue);
}

void AttrChangingDOMTransactionStep::reapply()
{
    m_element->setAttribute(m_name, m_newValue);
}

}

#endif
