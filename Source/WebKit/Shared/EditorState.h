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

enum TypingAttributes {
    AttributeNone = 0,
    AttributeBold = 1,
    AttributeItalics = 2,
    AttributeUnderline = 4,
    AttributeStrikeThrough = 8
};

enum TextAlignment {
    NoAlignment = 0,
    LeftAlignment = 1,
    RightAlignment = 2,
    CenterAlignment = 3,
    JustifiedAlignment = 4,
};

enum ListType {
    NoList = 0,
    OrderedList,
    UnorderedList
};

struct EditorState {
    EditorStateIdentifier identifier;
    String originIdentifierForPasteboard;
    bool shouldIgnoreSelectionChanges { false };
    bool selectionIsNone { true }; // This will be false when there is a caret selection.
    bool selectionIsRange { false };
    bool selectionIsRangeInsideImageOverlay { false };
    bool isContentEditable { false };
    bool isContentRichlyEditable { false };
    bool isInPasswordField { false };
    bool isInPlugin { false };
    bool hasComposition { false };
    bool triggeredByAccessibilitySelectionChange { false };
#if PLATFORM(MAC)
    bool canEnableAutomaticSpellingCorrection { true };
#endif
    bool isMissingPostLayoutData { true };

    struct PostLayoutData {
        uint32_t typingAttributes { AttributeNone };
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK) || PLATFORM(WPE)
        WebCore::IntRect caretRectAtStart;
#endif
#if PLATFORM(COCOA)
        uint64_t selectedTextLength { 0 };
        uint32_t textAlignment { NoAlignment };
        WebCore::Color textColor { WebCore::Color::black };
        uint32_t enclosingListType { NoList };
        WebCore::WritingDirection baseWritingDirection { WebCore::WritingDirection::Natural };
#endif
#if PLATFORM(IOS_FAMILY)
        WebCore::IntRect selectionClipRect;
        WebCore::IntRect caretRectAtEnd;
        Vector<WebCore::SelectionGeometry> selectionGeometries;
        Vector<WebCore::SelectionGeometry> markedTextRects;
        String markedText;
        WebCore::IntRect markedTextCaretRectAtStart;
        WebCore::IntRect markedTextCaretRectAtEnd;
        String wordAtSelection;
        UChar32 characterAfterSelection { 0 };
        UChar32 characterBeforeSelection { 0 };
        UChar32 twoCharacterBeforeSelection { 0 };
#if USE(DICTATION_ALTERNATIVES)
        Vector<WebCore::DictationContext> dictationContextsForSelection;
#endif
        bool isReplaceAllowed { false };
        bool hasContent { false };
        bool isStableStateUpdate { false };
        bool insideFixedPosition { false };
        bool hasPlainText { false };
        bool editableRootIsTransparentOrFullyClipped { false };
        WebCore::Color caretColor;
        bool atStartOfSentence { false };
        bool selectionStartIsAtParagraphBoundary { false };
        bool selectionEndIsAtParagraphBoundary { false };
        std::optional<WebCore::ElementContext> selectedEditableImage;
#endif
#if PLATFORM(MAC)
        WebCore::IntRect selectionBoundingRect;
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

        void encode(IPC::Encoder&) const;
        static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, PostLayoutData&);
    };

    const PostLayoutData& postLayoutData() const;
    PostLayoutData& postLayoutData();

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, EditorState&);

private:
    PostLayoutData m_postLayoutData;
};

inline auto EditorState::postLayoutData() -> PostLayoutData&
{
    ASSERT_WITH_MESSAGE(!isMissingPostLayoutData, "Attempt to access post layout data before receiving it");
    return m_postLayoutData;
}

inline auto EditorState::postLayoutData() const -> const PostLayoutData&
{
    ASSERT_WITH_MESSAGE(!isMissingPostLayoutData, "Attempt to access post layout data before receiving it");
    return m_postLayoutData;
}

WTF::TextStream& operator<<(WTF::TextStream&, const EditorState&);

} // namespace WebKit
