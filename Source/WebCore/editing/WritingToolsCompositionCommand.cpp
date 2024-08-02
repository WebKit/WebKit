/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WritingToolsCompositionCommand.h"

#include "FrameSelection.h"
#include "ReplaceSelectionCommand.h"
#include "TextIterator.h"

namespace WebCore {

WritingToolsCompositionCommand::WritingToolsCompositionCommand(Ref<Document>&& document, const SimpleRange& endingContextRange)
    : CompositeEditCommand(WTFMove(document), EditAction::InsertReplacement)
    , m_endingContextRange(endingContextRange)
    , m_currentContextRange(endingContextRange)
{
}

void WritingToolsCompositionCommand::replaceContentsOfRangeWithFragment(RefPtr<DocumentFragment>&& fragment, const SimpleRange& range, MatchStyle matchStyle, State state)
{
    auto contextRange = m_endingContextRange;

    auto contextRangeCount = characterCount(contextRange);
    auto resolvedCharacterRange = characterRange(contextRange, range);

    OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::PreventNesting, ReplaceSelectionCommand::SanitizeFragment, ReplaceSelectionCommand::SelectReplacement };
    if (matchStyle == MatchStyle::Yes)
        options.add(ReplaceSelectionCommand::MatchStyle);

    applyCommandToComposite(ReplaceSelectionCommand::create(protectedDocument(), WTFMove(fragment), options, EditAction::InsertReplacement), range);

    // Restore the context range to what it previously was, while taking into account the newly replaced contents.
    auto newContextRange = rangeExpandedAroundRangeByCharacters(endingSelection(), resolvedCharacterRange.location, contextRangeCount - (resolvedCharacterRange.location + resolvedCharacterRange.length));
    if (!newContextRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_currentContextRange = *newContextRange;

    if (state == State::Complete) {
        // When the command is signaled to be "complete", this commits the entire command as a whole to the undo/redo stack.
        this->apply();
        m_endingContextRange = *newContextRange;
    }
}

} // namespace WebCore
