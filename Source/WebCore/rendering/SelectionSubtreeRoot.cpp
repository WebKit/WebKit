/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "SelectionSubtreeRoot.h"

#include "Document.h"
#include "Position.h"
#include "Range.h"
#include "VisibleSelection.h"

namespace WebCore {

SelectionSubtreeRoot::SelectionSubtreeRoot()
    : m_selectionStart(nullptr)
    , m_selectionStartPos(-1)
    , m_selectionEnd(nullptr)
    , m_selectionEndPos(-1)
{
}

void SelectionSubtreeRoot::adjustForVisibleSelection(Document& document)
{
    if (selectionClear())
        return;

    // Create a range based on the cached end points
    Position startPosition = createLegacyEditingPosition(m_selectionStart->node(), m_selectionStartPos);
    Position endPosition = createLegacyEditingPosition(m_selectionEnd->node(), m_selectionEndPos);

    RefPtr<Range> range = Range::create(document, startPosition.parentAnchoredEquivalent(), endPosition.parentAnchoredEquivalent());
    VisibleSelection selection(range.get());
    Position startPos = selection.start();
    Position candidate = startPos.downstream();
    if (candidate.isCandidate())
        startPos = candidate;

    Position endPos = selection.end();
    candidate = endPos.upstream();
    if (candidate.isCandidate())
        endPos = candidate;

    m_selectionStart = nullptr;
    m_selectionStartPos = -1;
    m_selectionEnd = nullptr;
    m_selectionEndPos = -1;

    if (startPos.isNotNull()
        && endPos.isNotNull()
        && selection.visibleStart() != selection.visibleEnd()
        && startPos.deprecatedNode()->renderer()->flowThreadContainingBlock() == endPos.deprecatedNode()->renderer()->flowThreadContainingBlock()) {
        m_selectionStart = startPos.deprecatedNode()->renderer();
        m_selectionStartPos = startPos.deprecatedEditingOffset();
        m_selectionEnd = endPos.deprecatedNode()->renderer();
        m_selectionEndPos = endPos.deprecatedEditingOffset();
    }
}

} // namespace WebCore
