/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "remove_node_attribute_command.h"

#include "xml/dom_elementimpl.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::QualifiedName;

namespace khtml {

RemoveNodeAttributeCommand::RemoveNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, const QualifiedName& attribute)
    : EditCommand(document), m_element(element), m_attribute(attribute)
{
    ASSERT(m_element);
    m_element->ref();
}

RemoveNodeAttributeCommand::~RemoveNodeAttributeCommand()
{
    ASSERT(m_element);
    m_element->deref();
}

void RemoveNodeAttributeCommand::doApply()
{
    ASSERT(m_element);

    m_oldValue = m_element->getAttribute(m_attribute);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->removeAttribute(m_attribute, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeAttributeCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(!m_oldValue.isNull());

    int exceptionCode = 0;
    m_element->setAttribute(m_attribute, m_oldValue.impl(), exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
