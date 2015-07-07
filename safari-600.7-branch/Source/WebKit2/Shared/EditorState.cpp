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

#include "config.h"
#include "EditorState.h"

#include "Arguments.h"
#include "WebCoreArgumentCoders.h"

#if PLATFORM(IOS)
#include <WebCore/SelectionRect.h>
#endif

namespace WebKit {

void EditorState::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << shouldIgnoreCompositionSelectionChange;
    encoder << selectionIsNone;
    encoder << selectionIsRange;
    encoder << isContentEditable;
    encoder << isContentRichlyEditable;
    encoder << isInPasswordField;
    encoder << isInPlugin;
    encoder << hasComposition;

#if PLATFORM(IOS)
    encoder << isReplaceAllowed;
    encoder << hasContent;
    encoder << characterAfterSelection;
    encoder << characterBeforeSelection;
    encoder << twoCharacterBeforeSelection;
    encoder << caretRectAtStart;
    encoder << caretRectAtEnd;
    encoder << selectionRects;
    encoder << selectedTextLength;
    encoder << wordAtSelection;
    encoder << firstMarkedRect;
    encoder << lastMarkedRect;
    encoder << markedText;
    encoder << typingAttributes;
#endif

#if PLATFORM(GTK)
    encoder << cursorRect;
#endif
}

bool EditorState::decode(IPC::ArgumentDecoder& decoder, EditorState& result)
{
    if (!decoder.decode(result.shouldIgnoreCompositionSelectionChange))
        return false;

    if (!decoder.decode(result.selectionIsNone))
        return false;

    if (!decoder.decode(result.selectionIsRange))
        return false;

    if (!decoder.decode(result.isContentEditable))
        return false;

    if (!decoder.decode(result.isContentRichlyEditable))
        return false;

    if (!decoder.decode(result.isInPasswordField))
        return false;

    if (!decoder.decode(result.isInPlugin))
        return false;

    if (!decoder.decode(result.hasComposition))
        return false;

#if PLATFORM(IOS)
    if (!decoder.decode(result.isReplaceAllowed))
        return false;
    if (!decoder.decode(result.hasContent))
        return false;
    if (!decoder.decode(result.characterAfterSelection))
        return false;
    if (!decoder.decode(result.characterBeforeSelection))
        return false;
    if (!decoder.decode(result.twoCharacterBeforeSelection))
        return false;
    if (!decoder.decode(result.caretRectAtStart))
        return false;
    if (!decoder.decode(result.caretRectAtEnd))
        return false;
    if (!decoder.decode(result.selectionRects))
        return false;
    if (!decoder.decode(result.selectedTextLength))
        return false;
    if (!decoder.decode(result.wordAtSelection))
        return false;
    if (!decoder.decode(result.firstMarkedRect))
        return false;
    if (!decoder.decode(result.lastMarkedRect))
        return false;
    if (!decoder.decode(result.markedText))
        return false;
    if (!decoder.decode(result.typingAttributes))
        return false;
#endif

#if PLATFORM(GTK)
    if (!decoder.decode(result.cursorRect))
        return false;
#endif

    return true;
}

}
