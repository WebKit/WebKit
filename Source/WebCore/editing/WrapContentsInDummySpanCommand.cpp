/*
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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
#include "WrapContentsInDummySpanCommand.h"

#include "ApplyStyleCommand.h"
#include "NodeDocument.h"

namespace WebCore {

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(Element& element)
    : SimpleEditCommand(element.document())
    , m_element(element)
{
}

void WrapContentsInDummySpanCommand::executeApply()
{
    Vector<Ref<Node>> children;
    for (RefPtr child = m_element->firstChild(); child; child = child->nextSibling())
        children.append(*child);

    RefPtr dummySpan = m_dummySpan;
    for (auto& child : children)
        dummySpan->appendChild(child);

    m_element->appendChild(*dummySpan);
}

void WrapContentsInDummySpanCommand::doApply()
{
    m_dummySpan = createStyleSpanElement(document());
    
    executeApply();
}
    
void WrapContentsInDummySpanCommand::doUnapply()
{
    if (!m_dummySpan)
        return;

    if (!m_element->hasEditableStyle())
        return;

    Vector<Ref<Node>> children;
    RefPtr dummySpan = m_dummySpan;
    for (RefPtr child = dummySpan->firstChild(); child; child = child->nextSibling())
        children.append(*child);

    for (auto& child : children)
        m_element->appendChild(child);

    dummySpan->remove();
}

void WrapContentsInDummySpanCommand::doReapply()
{
    if (!m_dummySpan || !m_element->hasEditableStyle())
        return;

    executeApply();
}

#ifndef NDEBUG
void WrapContentsInDummySpanCommand::getNodesInCommand(NodeSet& nodes)
{
    addNodeAndDescendants(m_element.ptr(), nodes);
    addNodeAndDescendants(protect(m_dummySpan).get(), nodes);
}
#endif

} // namespace WebCore
