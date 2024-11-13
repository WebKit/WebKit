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

#pragma once

#include "ArgumentCoders.h"
#include "IdentifierTypes.h"
#include <WebCore/Color.h>
#include <WebCore/ElementContext.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/WritingDirection.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/SelectionGeometry.h>
#endif

#if USE(DICTATION_ALTERNATIVES)
#include <WebCore/DictationContext.h>
#endif

namespace WTF {
class TextStream;
};

namespace WebKit {

enum class TypingAttribute : uint8_t {
    Bold          = 1 << 0,
    Italics       = 1 << 1,
    Underline     = 1 << 2,
    StrikeThrough = 1 << 3,
};

enum class TextAlignment : uint8_t {
    Natural,
    Left,
    Right,
    Center,
    Justified,
};

enum class ListType : uint8_t {
    None,
    OrderedList,
    UnorderedList
};

struct EditorState {
    void move(float x, float y);

    EditorStateIdentifier identifier;
    bool shouldIgnoreSelectionChanges { false };
    bool selectionIsNone { true }; // This will be false when there is a caret selection.
    bool selectionIsRange { false };
    bool selectionIsRangeInsideImageOverlay { false };
    bool selectionIsRangeInAutoFilledAndViewableField { false };
    bool isContentEditable { false };
    bool isContentRichlyEditable { false };
    bool isInPasswordField { false };
    bool isInPlugin { false };
    bool hasComposition { false };
    bool triggeredByAccessibilitySelectionChange { false };
#if PLATFORM(MAC)
    bool canEnableAutomaticSpellingCorrection { true };
#endif

    struct PostLayoutData {
        OptionSet<TypingAttribute> typingAttributes;
#if PLATFORM(COCOA)
        uint64_t selectedTextLength { 0 };
        TextAlignment textAlignment { TextAlignment::Natural };
        WebCore::Color textColor { WebCore::Color::black }; // FIXME: Maybe this should be on VisualData?
        ListType enclosingListType { ListType::None };
        WebCore::WritingDirection baseWritingDirection { WebCore::WritingDirection::Natural };
        bool selectionIsTransparentOrFullyClipped { false };
        bool canEnableWritingSuggestions { false };
#endif
#if PLATFORM(IOS_FAMILY)
        String markedText;
        String wordAtSelection;
        char32_t characterAfterSelection { 0 };
        char32_t characterBeforeSelection { 0 };
        char32_t twoCharacterBeforeSelection { 0 };
#if USE(DICTATION_ALTERNATIVES)
        Vector<WebCore::DictationContext> dictationContextsForSelection;
#endif
        bool isReplaceAllowed { false };
        bool hasContent { false };
        bool isStableStateUpdate { false };
        bool insideFixedPosition { false };
        bool hasPlainText { false };
        WebCore::Color caretColor; // FIXME: Maybe this should be on VisualData?
        bool hasCaretColorAuto { false };
        bool atStartOfSentence { false };
        bool selectionStartIsAtParagraphBoundary { false };
        bool selectionEndIsAtParagraphBoundary { false };
        bool hasGrammarDocumentMarkers { false };
        std::optional<WebCore::ElementContext> selectedEditableImage;
#endif // PLATFORM(IOS_FAMILY)
#if PLATFORM(MAC)
        WebCore::IntRect selectionBoundingRect; // FIXME: Maybe this should be on VisualData?
        uint64_t candidateRequestStartPosition { 0 };
        String paragraphContextForCandidateRequest;
        String stringForCandidateRequest;
#endif
#if PLATFORM(GTK) || PLATFORM(WPE)
        String surroundingContext;
        uint64_t surroundingContextCursorPosition { 0 };
        uint64_t surroundingContextSelectionPosition { 0 };
#endif

        std::optional<WebCore::FontAttributes> fontAttributes;

        bool canCut { false };
        bool canCopy { false };
        bool canPaste { false };
    };

    bool hasPostLayoutData() const { return !!postLayoutData; }

    // Visual data is only updated in sync with rendering updates.
    struct VisualData {
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || PLATFORM(WPE)
        WebCore::IntRect caretRectAtStart;
#endif
#if PLATFORM(IOS_FAMILY)
        WebCore::IntRect selectionClipRect;
        WebCore::IntRect editableRootBounds;
        WebCore::IntRect caretRectAtEnd;
        Vector<WebCore::SelectionGeometry> selectionGeometries;
        Vector<WebCore::SelectionGeometry> markedTextRects;
        WebCore::IntRect markedTextCaretRectAtStart;
        WebCore::IntRect markedTextCaretRectAtEnd;
        std::optional<WebCore::PlatformLayerIdentifier> enclosingLayerID;
        std::optional<WebCore::ScrollingNodeID> enclosingScrollingNodeID;
        WebCore::ScrollPosition enclosingScrollPosition;
#endif // PLATFORM(IOS_FAMILY)
    };

    bool hasVisualData() const { return !!visualData; }

    bool hasPostLayoutAndVisualData() const { return hasPostLayoutData() && hasVisualData(); }

    std::optional<PostLayoutData> postLayoutData;
    std::optional<VisualData> visualData;

    void clipOwnedRectExtentsToNumericLimits();

private:
    friend TextStream& operator<<(TextStream&, const EditorState&);
};

WTF::TextStream& operator<<(WTF::TextStream&, const EditorState&);

} // namespace WebKit
