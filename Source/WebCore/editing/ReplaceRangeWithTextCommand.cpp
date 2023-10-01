/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "ReplaceRangeWithTextCommand.h"

#include "AlternativeTextController.h"
#include "DataTransfer.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "LocalFrame.h"
#include "ReplaceSelectionCommand.h"
#include "SetSelectionCommand.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include "markup.h"

namespace WebCore {

ReplaceRangeWithTextCommand::ReplaceRangeWithTextCommand(const SimpleRange& rangeToBeReplaced, const String& text)
    : CompositeEditCommand(rangeToBeReplaced.start.document(), EditAction::InsertReplacement)
    , m_rangeToBeReplaced(rangeToBeReplaced)
    , m_text(text)
{
}

bool ReplaceRangeWithTextCommand::willApplyCommand()
{
    m_textFragment = createFragmentFromText(m_rangeToBeReplaced, m_text);
    return CompositeEditCommand::willApplyCommand();
}

void ReplaceRangeWithTextCommand::doApply()
{
    VisibleSelection selection { m_rangeToBeReplaced };

    if (!document().selection().shouldChangeSelection(selection))
        return;

    if (!characterCount(m_rangeToBeReplaced))
        return;

    applyCommandToComposite(SetSelectionCommand::create(selection, FrameSelection::defaultSetSelectionOptions()));
    applyCommandToComposite(ReplaceSelectionCommand::create(document(), m_textFragment.copyRef(), ReplaceSelectionCommand::MatchStyle, EditAction::Paste));
}

String ReplaceRangeWithTextCommand::inputEventData() const
{
    if (isEditingTextAreaOrTextInput())
        return m_text;

    return CompositeEditCommand::inputEventData();
}

RefPtr<DataTransfer> ReplaceRangeWithTextCommand::inputEventDataTransfer() const
{
    if (!isEditingTextAreaOrTextInput())
        return DataTransfer::createForInputEvent(m_text, serializeFragment(*protectedTextFragment(), SerializedNodes::SubtreeIncludingNode));

    return CompositeEditCommand::inputEventDataTransfer();
}

Vector<RefPtr<StaticRange>> ReplaceRangeWithTextCommand::targetRanges() const
{
    return { 1, StaticRange::create(m_rangeToBeReplaced) };
}

} // namespace WebCore
