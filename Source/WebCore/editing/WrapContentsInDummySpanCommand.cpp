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
#include "WrapContentsInDummySpanCommand.h"

#include "ApplyStyleCommand.h"

namespace WebCore {

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(Element& element)
    : SimpleEditCommand(element.document())
    , m_element(element)
{
}

void WrapContentsInDummySpanCommand::executeApply()
{
    Vector<Ref<Node>> children;
    Ref element = protectedElement();
    for (RefPtr child = element->firstChild(); child; child = child->nextSibling())
        children.append(*child);

    auto dummySpan = protectedDummySpan();
    for (auto& child : children)
        dummySpan->appendChild(child);

    element->appendChild(*dummySpan);
}

void WrapContentsInDummySpanCommand::doApply()
{
    m_dummySpan = createStyleSpanElement(protectedDocument());
    
    executeApply();
}
    
void WrapContentsInDummySpanCommand::doUnapply()
{
    if (!m_dummySpan)
        return;

    Ref element = protectedElement();
    if (!element->hasEditableStyle())
        return;

    Vector<Ref<Node>> children;
    auto dummySpan = protectedDummySpan();
    for (RefPtr child = dummySpan->firstChild(); child; child = child->nextSibling())
        children.append(*child);

    for (auto& child : children)
        element->appendChild(child);

    dummySpan->remove();
}

void WrapContentsInDummySpanCommand::doReapply()
{
    if (!m_dummySpan || !protectedElement()->hasEditableStyle())
        return;

    executeApply();
}

#ifndef NDEBUG
void WrapContentsInDummySpanCommand::getNodesInCommand(HashSet<Ref<Node>>& nodes)
{
    addNodeAndDescendants(protectedElement().ptr(), nodes);
    addNodeAndDescendants(protectedDummySpan().get(), nodes);
}
#endif
    
} // namespace WebCore
