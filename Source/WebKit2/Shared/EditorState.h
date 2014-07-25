/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef EditorState_h
#define EditorState_h

#include "ArgumentCoders.h"
#include <WebCore/IntRect.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#include <WebCore/SelectionRect.h>
#endif

namespace WebKit {

enum TypingAttributes {
    AttributeNone = 0,
    AttributeBold = 1,
    AttributeItalics = 2,
    AttributeUnderline = 4
};

struct EditorState {
    EditorState()
        : shouldIgnoreCompositionSelectionChange(false)
        , selectionIsNone(true)
        , selectionIsRange(false)
        , isContentEditable(false)
        , isContentRichlyEditable(false)
        , isInPasswordField(false)
        , isInPlugin(false)
        , hasComposition(false)
#if PLATFORM(IOS)
        , isReplaceAllowed(false)
        , hasContent(false)
        , characterAfterSelection(0)
        , characterBeforeSelection(0)
        , twoCharacterBeforeSelection(0)
        , selectedTextLength(0)
        , typingAttributes(AttributeNone)
#endif
    {
    }

    bool shouldIgnoreCompositionSelectionChange;

    bool selectionIsNone; // This will be false when there is a caret selection.
    bool selectionIsRange;
    bool isContentEditable;
    bool isContentRichlyEditable;
    bool isInPasswordField;
    bool isInPlugin;
    bool hasComposition;

#if PLATFORM(IOS)
    bool isReplaceAllowed;
    bool hasContent;
    UChar32 characterAfterSelection;
    UChar32 characterBeforeSelection;
    UChar32 twoCharacterBeforeSelection;
    WebCore::IntRect caretRectAtStart;
    WebCore::IntRect caretRectAtEnd;
    Vector<WebCore::SelectionRect> selectionRects;
    uint64_t selectedTextLength;
    String wordAtSelection;
    WebCore::IntRect firstMarkedRect;
    WebCore::IntRect lastMarkedRect;
    String markedText;
    uint32_t typingAttributes;
#endif

#if PLATFORM(GTK)
    WebCore::IntRect cursorRect;
#endif

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, EditorState&);
};

}

#endif // EditorState_h
