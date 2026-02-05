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

#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

TextStream& operator<<(TextStream& ts, const EditorState& editorState)
{
    if (editorState.shouldIgnoreSelectionChanges)
        ts.dumpProperty("shouldIgnoreSelectionChanges"_s, editorState.shouldIgnoreSelectionChanges);
    if (!editorState.selectionIsNone)
        ts.dumpProperty("selectionIsNone"_s, editorState.selectionIsNone);
    if (editorState.selectionIsRange)
        ts.dumpProperty("selectionIsRange"_s, editorState.selectionIsRange);
    if (editorState.isContentEditable)
        ts.dumpProperty("isContentEditable"_s, editorState.isContentEditable);
    if (editorState.isContentRichlyEditable)
        ts.dumpProperty("isContentRichlyEditable"_s, editorState.isContentRichlyEditable);
    if (editorState.isInPasswordField)
        ts.dumpProperty("isInPasswordField"_s, editorState.isInPasswordField);
    if (editorState.hasComposition)
        ts.dumpProperty("hasComposition"_s, editorState.hasComposition);
    if (editorState.triggeredByAccessibilitySelectionChange)
        ts.dumpProperty("triggeredByAccessibilitySelectionChange"_s, editorState.triggeredByAccessibilitySelectionChange);
    if (editorState.isInPlugin)
        ts.dumpProperty("isInPlugin"_s, editorState.isInPlugin);
#if PLATFORM(MAC)
    if (!editorState.canEnableAutomaticSpellingCorrection)
        ts.dumpProperty("canEnableAutomaticSpellingCorrection"_s, editorState.canEnableAutomaticSpellingCorrection);
#endif

    ts.dumpProperty("hasPostLayoutData"_s, editorState.hasPostLayoutData());

    if (editorState.hasPostLayoutData()) {
        TextStream::GroupScope scope(ts);
        ts << "postLayoutData"_s;
        if (!editorState.postLayoutData->typingAttributes.isEmpty())
            ts.dumpProperty("typingAttributes"_s, editorState.postLayoutData->typingAttributes.toRaw());
#if PLATFORM(COCOA)
        if (editorState.postLayoutData->selectedTextLength)
            ts.dumpProperty("selectedTextLength"_s, editorState.postLayoutData->selectedTextLength);
        if (editorState.postLayoutData->textAlignment != TextAlignment::Natural)
            ts.dumpProperty("textAlignment"_s, enumToUnderlyingType(editorState.postLayoutData->textAlignment));
        if (editorState.postLayoutData->textColor.isValid())
            ts.dumpProperty("textColor"_s, editorState.postLayoutData->textColor);
        if (editorState.postLayoutData->enclosingListType != ListType::None)
            ts.dumpProperty("enclosingListType"_s, enumToUnderlyingType(editorState.postLayoutData->enclosingListType));
        if (editorState.postLayoutData->baseWritingDirection != WebCore::WritingDirection::Natural)
            ts.dumpProperty("baseWritingDirection"_s, static_cast<uint8_t>(editorState.postLayoutData->baseWritingDirection));
        if (editorState.postLayoutData->canEnableWritingSuggestions)
            ts.dumpProperty("canEnableWritingSuggestions"_s, editorState.postLayoutData->canEnableWritingSuggestions);
        if (editorState.postLayoutData->insideFixedPosition)
            ts.dumpProperty("insideFixedPosition"_s, editorState.postLayoutData->insideFixedPosition);
#endif // PLATFORM(COCOA)
#if PLATFORM(IOS_FAMILY)
        if (editorState.postLayoutData->markedText.length())
            ts.dumpProperty("markedText"_s, editorState.postLayoutData->markedText);
        if (editorState.postLayoutData->wordAtSelection.length())
            ts.dumpProperty("wordAtSelection"_s, editorState.postLayoutData->wordAtSelection);
        if (editorState.postLayoutData->characterAfterSelection)
            ts.dumpProperty("characterAfterSelection"_s, editorState.postLayoutData->characterAfterSelection);
        if (editorState.postLayoutData->characterBeforeSelection)
            ts.dumpProperty("characterBeforeSelection"_s, editorState.postLayoutData->characterBeforeSelection);
        if (editorState.postLayoutData->twoCharacterBeforeSelection)
            ts.dumpProperty("twoCharacterBeforeSelection"_s, editorState.postLayoutData->twoCharacterBeforeSelection);

        if (editorState.postLayoutData->isReplaceAllowed)
            ts.dumpProperty("isReplaceAllowed"_s, editorState.postLayoutData->isReplaceAllowed);
        if (editorState.postLayoutData->hasContent)
            ts.dumpProperty("hasContent"_s, editorState.postLayoutData->hasContent);
        ts.dumpProperty("isStableStateUpdate"_s, editorState.postLayoutData->isStableStateUpdate);
        if (editorState.postLayoutData->caretColor.isValid())
            ts.dumpProperty("caretColor"_s, editorState.postLayoutData->caretColor);
        if (editorState.postLayoutData->hasCaretColorAuto)
            ts.dumpProperty("hasCaretColorAuto"_s, editorState.postLayoutData->hasCaretColorAuto);
#endif
#if PLATFORM(MAC)
        if (editorState.postLayoutData->selectionBoundingRect != IntRect())
            ts.dumpProperty("selectionBoundingRect"_s, editorState.postLayoutData->selectionBoundingRect);
        if (editorState.postLayoutData->candidateRequestStartPosition)
            ts.dumpProperty("candidateRequestStartPosition"_s, editorState.postLayoutData->candidateRequestStartPosition);
        if (editorState.postLayoutData->paragraphContextForCandidateRequest.length())
            ts.dumpProperty("paragraphContextForCandidateRequest"_s, editorState.postLayoutData->paragraphContextForCandidateRequest);
        if (editorState.postLayoutData->stringForCandidateRequest.length())
            ts.dumpProperty("stringForCandidateRequest"_s, editorState.postLayoutData->stringForCandidateRequest);
#endif

        if (editorState.postLayoutData->canCut)
            ts.dumpProperty("canCut"_s, editorState.postLayoutData->canCut);
        if (editorState.postLayoutData->canCopy)
            ts.dumpProperty("canCopy"_s, editorState.postLayoutData->canCopy);
        if (editorState.postLayoutData->canPaste)
            ts.dumpProperty("canPaste"_s, editorState.postLayoutData->canPaste);
    }

    ts.dumpProperty("hasVisualData"_s, editorState.hasVisualData());

    if (editorState.hasVisualData()) {
        TextStream::GroupScope scope(ts);
        ts << "visualData"_s;
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
        if (editorState.visualData->caretRectAtStart != IntRect())
            ts.dumpProperty("caretRectAtStart"_s, editorState.visualData->caretRectAtStart);
#endif
#if PLATFORM(COCOA)
        if (editorState.visualData->caretRectAtEnd != IntRect())
            ts.dumpProperty("caretRectAtEnd"_s, editorState.visualData->caretRectAtEnd);
        if (!editorState.visualData->selectionGeometries.isEmpty())
            ts.dumpProperty("selectionGeometries"_s, editorState.visualData->selectionGeometries);
#endif
#if PLATFORM(IOS_FAMILY)
        if (editorState.visualData->selectionClipRect != IntRect())
            ts.dumpProperty("selectionClipRect"_s, editorState.visualData->selectionClipRect);
        if (editorState.visualData->editableRootBounds != IntRect())
            ts.dumpProperty("editableRootBounds"_s, editorState.visualData->editableRootBounds);
        if (!editorState.visualData->markedTextRects.isEmpty())
            ts.dumpProperty("markedTextRects"_s, editorState.visualData->markedTextRects);
        if (editorState.visualData->markedTextCaretRectAtStart != IntRect())
            ts.dumpProperty("markedTextCaretRectAtStart"_s, editorState.visualData->markedTextCaretRectAtStart);
        if (editorState.visualData->markedTextCaretRectAtEnd != IntRect())
            ts.dumpProperty("markedTextCaretRectAtEnd"_s, editorState.visualData->markedTextCaretRectAtEnd);
#endif
    }
    return ts;
}

void EditorState::clipOwnedRectExtentsToNumericLimits()
{
    auto sanitizePostLayoutData = [](auto& postLayoutData) {
#if PLATFORM(MAC)
        postLayoutData.selectionBoundingRect = postLayoutData.selectionBoundingRect.toRectWithExtentsClippedToNumericLimits();
#else
        UNUSED_PARAM(postLayoutData);
#endif
    };
    if (hasPostLayoutData())
        sanitizePostLayoutData(*postLayoutData);

    auto sanitizeVisualData = [](auto& visualData) {
#if PLATFORM(COCOA)
        visualData.caretRectAtEnd = visualData.caretRectAtEnd.toRectWithExtentsClippedToNumericLimits();
#endif
#if PLATFORM(IOS_FAMILY)
        visualData.selectionClipRect = visualData.selectionClipRect.toRectWithExtentsClippedToNumericLimits();
        visualData.editableRootBounds = visualData.editableRootBounds.toRectWithExtentsClippedToNumericLimits();
        visualData.markedTextCaretRectAtStart = visualData.markedTextCaretRectAtStart.toRectWithExtentsClippedToNumericLimits();
        visualData.markedTextCaretRectAtEnd = visualData.markedTextCaretRectAtEnd.toRectWithExtentsClippedToNumericLimits();
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
        visualData.caretRectAtStart = visualData.caretRectAtStart.toRectWithExtentsClippedToNumericLimits();
#else
        UNUSED_PARAM(visualData);
#endif
    };
    if (hasVisualData())
        sanitizeVisualData(*visualData);
}

void EditorState::move(float x, float y)
{
    if (!hasVisualData())
        return;

    if (!x && !y)
        return;

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    int roundedX = std::round(x);
    int roundedY = std::round(y);
    visualData->caretRectAtStart.move(roundedX, roundedY);
#endif

#if PLATFORM(COCOA)
    visualData->caretRectAtEnd.move(roundedX, roundedY);

    for (auto& geometry : visualData->selectionGeometries)
        geometry.move(x, y);
#endif

#if PLATFORM(IOS_FAMILY)
    visualData->selectionClipRect.move(roundedX, roundedY);
    visualData->editableRootBounds.move(roundedX, roundedY);
    visualData->markedTextCaretRectAtStart.move(roundedX, roundedY);
    visualData->markedTextCaretRectAtEnd.move(roundedX, roundedY);
    for (auto& geometry : visualData->markedTextRects)
        geometry.move(x, y);
#endif // PLATFORM(IOS_FAMILY)

    clipOwnedRectExtentsToNumericLimits();
}

} // namespace WebKit
