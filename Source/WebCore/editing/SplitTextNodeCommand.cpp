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
#include "SplitTextNodeCommand.h"

#include "CompositeEditCommand.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Text.h"
#include <wtf/Assertions.h>

namespace WebCore {

SplitTextNodeCommand::SplitTextNodeCommand(Ref<Text>&& text, int offset)
    : SimpleEditCommand(text->document())
    , m_text2(WTFMove(text))
    , m_offset(offset)
{
    // NOTE: Various callers rely on the fact that the original node becomes
    // the second node (i.e. the new node is inserted before the existing one).
    // That is not a fundamental dependency (i.e. it could be re-coded), but
    // rather is based on how this code happens to work.
    ASSERT(m_text2->length() > 0);
    ASSERT(m_offset > 0);
    ASSERT(m_offset < m_text2->length());
}

void SplitTextNodeCommand::doApply()
{
    ContainerNode* parent = m_text2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;

    auto result = m_text2->substringData(0, m_offset);
    if (result.hasException())
        return;
    auto prefixText = result.releaseReturnValue();
    if (prefixText.isEmpty())
        return;

    m_text1 = Text::create(document(), WTFMove(prefixText));
    ASSERT(m_text1);
    document().markers().copyMarkers(m_text2, { 0, m_offset }, *m_text1);

    insertText1AndTrimText2();
}

void SplitTextNodeCommand::doUnapply()
{
    if (!m_text1 || !m_text1->hasEditableStyle())
        return;

    ASSERT(&m_text1->document() == &document());

    String prefixText = m_text1->data();

    m_text2->insertData(0, prefixText);

    document().markers().copyMarkers(*m_text1, { 0, prefixText.length() }, m_text2);
    m_text1->remove();
}

void SplitTextNodeCommand::doReapply()
{
    if (!m_text1)
        return;

    ContainerNode* parent = m_text2->parentNode();
    if (!parent || !parent->hasEditableStyle())
        return;

    insertText1AndTrimText2();
}

void SplitTextNodeCommand::insertText1AndTrimText2()
{
    if (m_text2->parentNode()->insertBefore(*m_text1, m_text2.ptr()).hasException())
        return;
    m_text2->deleteData(0, m_offset);
}

#ifndef NDEBUG

void SplitTextNodeCommand::getNodesInCommand(HashSet<Ref<Node>>& nodes)
{
    addNodeAndDescendants(m_text1.get(), nodes);
    addNodeAndDescendants(m_text2.ptr(), nodes);
}

#endif
    
} // namespace WebCore
