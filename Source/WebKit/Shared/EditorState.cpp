/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "EditorState.h"

#include "WebCoreArgumentCoders.h"
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

TextStream& operator<<(TextStream& ts, const EditorState& editorState)
{
    if (editorState.shouldIgnoreSelectionChanges)
        ts.dumpProperty("shouldIgnoreSelectionChanges", editorState.shouldIgnoreSelectionChanges);
    if (!editorState.selectionIsNone)
        ts.dumpProperty("selectionIsNone", editorState.selectionIsNone);
    if (editorState.selectionIsRange)
        ts.dumpProperty("selectionIsRange", editorState.selectionIsRange);
    if (editorState.isContentEditable)
        ts.dumpProperty("isContentEditable", editorState.isContentEditable);
    if (editorState.isContentRichlyEditable)
        ts.dumpProperty("isContentRichlyEditable", editorState.isContentRichlyEditable);
    if (editorState.isInPasswordField)
        ts.dumpProperty("isInPasswordField", editorState.isInPasswordField);
    if (editorState.isInPlugin)
        ts.dumpProperty("isInPlugin", editorState.isInPlugin);
    if (editorState.hasComposition)
        ts.dumpProperty("hasComposition", editorState.hasComposition);
    if (editorState.triggeredByAccessibilitySelectionChange)
        ts.dumpProperty("triggeredByAccessibilitySelectionChange", editorState.triggeredByAccessibilitySelectionChange);
#if PLATFORM(MAC)
    if (!editorState.canEnableAutomaticSpellingCorrection)
        ts.dumpProperty("canEnableAutomaticSpellingCorrection", editorState.canEnableAutomaticSpellingCorrection);
#endif

    ts.dumpProperty("hasPostLayoutData", editorState.hasPostLayoutData());

    if (editorState.hasPostLayoutData()) {
        TextStream::GroupScope scope(ts);
        ts << "postLayoutData";
        if (editorState.postLayoutData->typingAttributes != AttributeNone)
            ts.dumpProperty("typingAttributes", editorState.postLayoutData->typingAttributes);
#if PLATFORM(COCOA)
        if (editorState.postLayoutData->selectedTextLength)
            ts.dumpProperty("selectedTextLength", editorState.postLayoutData->selectedTextLength);
        if (editorState.postLayoutData->textAlignment != NoAlignment)
            ts.dumpProperty("textAlignment", editorState.postLayoutData->textAlignment);
        if (editorState.postLayoutData->textColor.isValid())
            ts.dumpProperty("textColor", editorState.postLayoutData->textColor);
        if (editorState.postLayoutData->enclosingListType != NoList)
            ts.dumpProperty("enclosingListType", editorState.postLayoutData->enclosingListType);
        if (editorState.postLayoutData->baseWritingDirection != WebCore::WritingDirection::Natural)
            ts.dumpProperty("baseWritingDirection", static_cast<uint8_t>(editorState.postLayoutData->baseWritingDirection));
#endif // PLATFORM(COCOA)
#if PLATFORM(IOS_FAMILY)
        if (editorState.postLayoutData->markedText.length())
            ts.dumpProperty("markedText", editorState.postLayoutData->markedText);
        if (editorState.postLayoutData->wordAtSelection.length())
            ts.dumpProperty("wordAtSelection", editorState.postLayoutData->wordAtSelection);
        if (editorState.postLayoutData->characterAfterSelection)
            ts.dumpProperty("characterAfterSelection", editorState.postLayoutData->characterAfterSelection);
        if (editorState.postLayoutData->characterBeforeSelection)
            ts.dumpProperty("characterBeforeSelection", editorState.postLayoutData->characterBeforeSelection);
        if (editorState.postLayoutData->twoCharacterBeforeSelection)
            ts.dumpProperty("twoCharacterBeforeSelection", editorState.postLayoutData->twoCharacterBeforeSelection);

        if (editorState.postLayoutData->isReplaceAllowed)
            ts.dumpProperty("isReplaceAllowed", editorState.postLayoutData->isReplaceAllowed);
        if (editorState.postLayoutData->hasContent)
            ts.dumpProperty("hasContent", editorState.postLayoutData->hasContent);
        ts.dumpProperty("isStableStateUpdate", editorState.postLayoutData->isStableStateUpdate);
        if (editorState.postLayoutData->insideFixedPosition)
            ts.dumpProperty("insideFixedPosition", editorState.postLayoutData->insideFixedPosition);
        if (editorState.postLayoutData->caretColor.isValid())
            ts.dumpProperty("caretColor", editorState.postLayoutData->caretColor);
#endif
#if PLATFORM(MAC)
        if (editorState.postLayoutData->selectionBoundingRect != IntRect())
            ts.dumpProperty("selectionBoundingRect", editorState.postLayoutData->selectionBoundingRect);
        if (editorState.postLayoutData->candidateRequestStartPosition)
            ts.dumpProperty("candidateRequestStartPosition", editorState.postLayoutData->candidateRequestStartPosition);
        if (editorState.postLayoutData->paragraphContextForCandidateRequest.length())
            ts.dumpProperty("paragraphContextForCandidateRequest", editorState.postLayoutData->paragraphContextForCandidateRequest);
        if (editorState.postLayoutData->stringForCandidateRequest.length())
            ts.dumpProperty("stringForCandidateRequest", editorState.postLayoutData->stringForCandidateRequest);
#endif

        if (editorState.postLayoutData->canCut)
            ts.dumpProperty("canCut", editorState.postLayoutData->canCut);
        if (editorState.postLayoutData->canCopy)
            ts.dumpProperty("canCopy", editorState.postLayoutData->canCopy);
        if (editorState.postLayoutData->canPaste)
            ts.dumpProperty("canPaste", editorState.postLayoutData->canPaste);
    }

    ts.dumpProperty("hasVisualData", editorState.hasVisualData());

    if (editorState.hasVisualData()) {
        TextStream::GroupScope scope(ts);
        ts << "visualData";
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || PLATFORM(WPE)
        if (editorState.visualData->caretRectAtStart != IntRect())
            ts.dumpProperty("caretRectAtStart", editorState.visualData->caretRectAtStart);
#endif
#if PLATFORM(IOS_FAMILY)
        if (editorState.visualData->selectionClipRect != IntRect())
            ts.dumpProperty("selectionClipRect", editorState.visualData->selectionClipRect);
        if (editorState.visualData->caretRectAtEnd != IntRect())
            ts.dumpProperty("caretRectAtEnd", editorState.visualData->caretRectAtEnd);
        if (!editorState.visualData->selectionGeometries.isEmpty())
            ts.dumpProperty("selectionGeometries", editorState.visualData->selectionGeometries);
        if (!editorState.visualData->markedTextRects.isEmpty())
            ts.dumpProperty("markedTextRects", editorState.visualData->markedTextRects);
        if (editorState.visualData->markedTextCaretRectAtStart != IntRect())
            ts.dumpProperty("markedTextCaretRectAtStart", editorState.visualData->markedTextCaretRectAtStart);
        if (editorState.visualData->markedTextCaretRectAtEnd != IntRect())
            ts.dumpProperty("markedTextCaretRectAtEnd", editorState.visualData->markedTextCaretRectAtEnd);
#endif
    }
    return ts;
}

} // namespace WebKit
