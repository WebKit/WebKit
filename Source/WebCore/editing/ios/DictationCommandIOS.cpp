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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DictationCommandIOS.h"

#if PLATFORM(IOS_FAMILY)

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Element.h"
#include "Position.h"
#include "Range.h"
#include "SmartReplace.h"
#include "TextIterator.h"
#include "VisibleUnits.h"

namespace WebCore {

DictationCommandIOS::DictationCommandIOS(Document& document, Vector<Vector<String>>&& dictationPhrases, id metadata)
    : CompositeEditCommand(document, EditAction::Dictation)
    , m_dictationPhrases(WTFMove(dictationPhrases))
    , m_metadata(metadata)
{
}

Ref<DictationCommandIOS> DictationCommandIOS::create(Document& document, Vector<Vector<String>>&& dictationPhrases, id metadata)
{
    return adoptRef(*new DictationCommandIOS(document, WTFMove(dictationPhrases), metadata));
}

void DictationCommandIOS::doApply()
{
    uint64_t resultLength = 0;
    for (auto& interpretations : m_dictationPhrases) {
        const String& firstInterpretation = interpretations[0];
        resultLength += firstInterpretation.length();
        inputText(firstInterpretation, true);

        if (interpretations.size() > 1) {
            auto alternatives = interpretations;
            alternatives.remove(0);
            addMarker(*endingSelection().toNormalizedRange(), DocumentMarker::DictationPhraseWithAlternatives, WTFMove(alternatives));
        }

        setEndingSelection(VisibleSelection(endingSelection().visibleEnd()));
    }

    // FIXME: Add the result marker using a Position cached before results are inserted, instead of relying on character counts.

    auto endPosition = endingSelection().visibleEnd();
    auto end = makeBoundaryPoint(endPosition);
    auto* root = endPosition.rootEditableElement();
    if (!end || !root)
        return;

    auto endOffset = characterCount({ { *root, 0 }, WTFMove(*end) });
    if (endOffset < resultLength)
        return;

    auto resultRange = resolveCharacterRange(makeRangeSelectingNodeContents(*root), { endOffset - resultLength, endOffset });
    addMarker(resultRange, DocumentMarker::DictationResult, m_metadata);
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
