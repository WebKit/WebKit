/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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
#include "SpellingCorrectionCommand.h"

#include "AlternativeTextController.h"
#include "DataTransfer.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "Frame.h"
#include "ReplaceSelectionCommand.h"
#include "SetSelectionCommand.h"
#include "StaticRange.h"
#include "TextIterator.h"
#include "markup.h"

namespace WebCore {

#if USE(AUTOCORRECTION_PANEL)
// On Mac OS X, we use this command to keep track of user undoing a correction for the first time.
// This information is needed by spell checking service to update user specific data.
class SpellingCorrectionRecordUndoCommand : public SimpleEditCommand {
public:
    static Ref<SpellingCorrectionRecordUndoCommand> create(Document& document, const String& corrected, const String& correction)
    {
        return adoptRef(*new SpellingCorrectionRecordUndoCommand(document, corrected, correction));
    }
private:
    SpellingCorrectionRecordUndoCommand(Document& document, const String& corrected, const String& correction)
        : SimpleEditCommand(document)
        , m_corrected(corrected)
        , m_correction(correction)
        , m_hasBeenUndone(false)
    {
    }

    void doApply() override
    {
    }

    void doUnapply() override
    {
        if (!m_hasBeenUndone) {
            document().editor().unappliedSpellCorrection(startingSelection(), m_corrected, m_correction);
            m_hasBeenUndone = true;
        }
        
    }

#ifndef NDEBUG
    void getNodesInCommand(HashSet<Node*>&) override
    {
    }
#endif

    String m_corrected;
    String m_correction;
    bool m_hasBeenUndone;
};
#endif

SpellingCorrectionCommand::SpellingCorrectionCommand(const SimpleRange& rangeToBeCorrected, const String& correction)
    : CompositeEditCommand(rangeToBeCorrected.start.container->document(), EditAction::InsertReplacement)
    , m_rangeToBeCorrected(rangeToBeCorrected)
    , m_selectionToBeCorrected(m_rangeToBeCorrected)
    , m_correction(correction)
{
}

Ref<SpellingCorrectionCommand> SpellingCorrectionCommand::create(const SimpleRange& rangeToBeCorrected, const String& correction)
{
    return adoptRef(*new SpellingCorrectionCommand(rangeToBeCorrected, correction));
}

bool SpellingCorrectionCommand::willApplyCommand()
{
    m_correctionFragment = createFragmentFromText(m_rangeToBeCorrected, m_correction);
    return CompositeEditCommand::willApplyCommand();
}

void SpellingCorrectionCommand::doApply()
{
    m_corrected = plainText(m_rangeToBeCorrected);
    if (!m_corrected.length())
        return;

    if (!document().selection().shouldChangeSelection(m_selectionToBeCorrected))
        return;

    applyCommandToComposite(SetSelectionCommand::create(m_selectionToBeCorrected, FrameSelection::defaultSetSelectionOptions() | FrameSelection::SpellCorrectionTriggered));
#if USE(AUTOCORRECTION_PANEL)
    applyCommandToComposite(SpellingCorrectionRecordUndoCommand::create(document(), m_corrected, m_correction));
#endif

    applyCommandToComposite(ReplaceSelectionCommand::create(document(), WTFMove(m_correctionFragment), ReplaceSelectionCommand::MatchStyle, EditAction::Paste));
}

String SpellingCorrectionCommand::inputEventData() const
{
    if (isEditingTextAreaOrTextInput())
        return m_correction;

    return CompositeEditCommand::inputEventData();
}

Vector<RefPtr<StaticRange>> SpellingCorrectionCommand::targetRanges() const
{
    return { 1, StaticRange::create(m_rangeToBeCorrected) };
}

RefPtr<DataTransfer> SpellingCorrectionCommand::inputEventDataTransfer() const
{
    if (!isEditingTextAreaOrTextInput())
        return DataTransfer::createForInputEvent(m_correction, serializeFragment(*m_correctionFragment, SerializedNodes::SubtreeIncludingNode));

    return CompositeEditCommand::inputEventDataTransfer();
}

bool SpellingCorrectionCommand::shouldRetainAutocorrectionIndicator() const
{
    return true;
}

} // namespace WebCore
