/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "SetSelectionCommand.h"

#include "CompositeEditCommand.h"
#include "Document.h"
#include "Frame.h"

namespace WebCore {

SetSelectionCommand::SetSelectionCommand(const VisibleSelection& selection, OptionSet<FrameSelection::SetSelectionOption> options)
    : SimpleEditCommand(selection.base().anchorNode()->document())
    , m_options(options)
    , m_selectionToSet(selection)
{
}

void SetSelectionCommand::doApply()
{
    FrameSelection& selection = document().selection();

    if (selection.shouldChangeSelection(m_selectionToSet) && !m_selectionToSet.isNoneOrOrphaned()) {
        selection.setSelection(m_selectionToSet, m_options);
        setEndingSelection(m_selectionToSet);
    }
}

void SetSelectionCommand::doUnapply()
{
    FrameSelection& selection = document().selection();

    if (selection.shouldChangeSelection(startingSelection()) && !startingSelection().isNoneOrOrphaned())
        selection.setSelection(startingSelection(), m_options);
}

} // namespace WebCore
