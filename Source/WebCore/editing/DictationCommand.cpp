/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DictationCommand.h"

#include "AlternativeTextController.h"
#include "Document.h"
#include "DocumentMarkerController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "Text.h"

namespace WebCore {

class DictationCommandLineOperation {
public:
    DictationCommandLineOperation(DictationCommand* dictationCommand)
    : m_dictationCommand(dictationCommand)
    { }
    
    void operator()(size_t lineOffset, size_t lineLength, bool isLastLine) const
    {
        if (lineLength > 0)
            m_dictationCommand->insertTextRunWithoutNewlines(lineOffset, lineLength);
        if (!isLastLine)
            m_dictationCommand->insertParagraphSeparator();
    }
private:
    DictationCommand* m_dictationCommand;
};

class DictationMarkerSupplier : public TextInsertionMarkerSupplier {
public:
    static Ref<DictationMarkerSupplier> create(const Vector<DictationAlternative>& alternatives)
    {
        return adoptRef(*new DictationMarkerSupplier(alternatives));
    }

    void addMarkersToTextNode(Text& textNode, unsigned offsetOfInsertion, const String& textToBeInserted) override
    {
        auto& markerController = textNode.document().markers();
        for (auto& alternative : m_alternatives) {
            DocumentMarker::DictationData data { alternative.context, textToBeInserted.substring(alternative.range.location, alternative.range.length) };
            markerController.addMarker(textNode, alternative.range.location + offsetOfInsertion, alternative.range.length, DocumentMarker::DictationAlternatives, WTFMove(data));
            markerController.addMarker(textNode, alternative.range.location + offsetOfInsertion, alternative.range.length, DocumentMarker::SpellCheckingExemption);
        }
    }

protected:
    DictationMarkerSupplier(const Vector<DictationAlternative>& alternatives)
    : m_alternatives(alternatives)
    {
    }
private:
    Vector<DictationAlternative> m_alternatives;
};

DictationCommand::DictationCommand(Document& document, const String& text, const Vector<DictationAlternative>& alternatives)
    : TextInsertionBaseCommand(document)
    , m_textToInsert(text)
    , m_alternatives(alternatives)
{
}

void DictationCommand::insertText(Document& document, const String& text, const Vector<DictationAlternative>& alternatives, const VisibleSelection& selectionForInsertion)
{
    RefPtr<Frame> frame = document.frame();
    ASSERT(frame);

    VisibleSelection currentSelection = frame->selection().selection();

    String newText = dispatchBeforeTextInsertedEvent(text, selectionForInsertion, false);

    RefPtr<DictationCommand> cmd;
    if (newText == text)
        cmd = DictationCommand::create(document, newText, alternatives);
    else
        // If the text was modified before insertion, the location of dictation alternatives
        // will not be valid anymore. We will just drop the alternatives.
        cmd = DictationCommand::create(document, newText, Vector<DictationAlternative>());
    applyTextInsertionCommand(frame.get(), *cmd, selectionForInsertion, currentSelection);
}

void DictationCommand::doApply()
{
    DictationCommandLineOperation operation(this);
    forEachLineInString(m_textToInsert, operation);
    postTextStateChangeNotification(AXTextEditTypeDictation, m_textToInsert);
}

void DictationCommand::insertTextRunWithoutNewlines(size_t lineStart, size_t lineLength)
{
    Vector<DictationAlternative> alternativesInLine;
    collectDictationAlternativesInRange(lineStart, lineLength, alternativesInLine);
    auto command = InsertTextCommand::createWithMarkerSupplier(document(), m_textToInsert.substring(lineStart, lineLength), DictationMarkerSupplier::create(alternativesInLine), EditAction::Dictation);
    applyCommandToComposite(WTFMove(command), endingSelection());
}

void DictationCommand::insertParagraphSeparator()
{
    if (!canAppendNewLineFeedToSelection(endingSelection()))
        return;

    applyCommandToComposite(InsertParagraphSeparatorCommand::create(document(), false, false, EditAction::Dictation));
}

void DictationCommand::collectDictationAlternativesInRange(size_t rangeStart, size_t rangeLength, Vector<DictationAlternative>& alternatives)
{
    for (auto& alternative : m_alternatives) {
        if (alternative.range.location >= rangeStart && (alternative.range.location + alternative.range.length) <= rangeStart + rangeLength)
            alternatives.append({ { alternative.range.location - rangeStart, alternative.range.length }, alternative.context });
    }

}

}
