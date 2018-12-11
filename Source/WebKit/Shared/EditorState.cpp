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

#include "WebCoreArgumentCoders.h"
#include <wtf/text/TextStream.h>

using namespace WebCore;

namespace WebKit {

void EditorState::encode(IPC::Encoder& encoder) const
{
    encoder << shouldIgnoreSelectionChanges;
    encoder << selectionIsNone;
    encoder << selectionIsRange;
    encoder << isContentEditable;
    encoder << isContentRichlyEditable;
    encoder << isInPasswordField;
    encoder << isInPlugin;
    encoder << hasComposition;
    encoder << isMissingPostLayoutData;

    if (!isMissingPostLayoutData)
        m_postLayoutData.encode(encoder);

#if PLATFORM(IOS_FAMILY)
    encoder << firstMarkedRect;
    encoder << lastMarkedRect;
    encoder << markedText;
#endif

    encoder << originIdentifierForPasteboard;
}

bool EditorState::decode(IPC::Decoder& decoder, EditorState& result)
{
    if (!decoder.decode(result.shouldIgnoreSelectionChanges))
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

    if (!decoder.decode(result.isMissingPostLayoutData))
        return false;

    if (!result.isMissingPostLayoutData) {
        if (!PostLayoutData::decode(decoder, result.postLayoutData()))
            return false;
    }

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(result.firstMarkedRect))
        return false;
    if (!decoder.decode(result.lastMarkedRect))
        return false;
    if (!decoder.decode(result.markedText))
        return false;
#endif

    if (!decoder.decode(result.originIdentifierForPasteboard))
        return false;

    return true;
}

void EditorState::PostLayoutData::encode(IPC::Encoder& encoder) const
{
    encoder << typingAttributes;
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK)
    encoder << caretRectAtStart;
#endif
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    encoder << selectionClipRect;
    encoder << selectedTextLength;
    encoder << textAlignment;
    encoder << textColor;
    encoder << enclosingListType;
#endif
#if PLATFORM(IOS_FAMILY)
    encoder << caretRectAtEnd;
    encoder << selectionRects;
    encoder << wordAtSelection;
    encoder << characterAfterSelection;
    encoder << characterBeforeSelection;
    encoder << twoCharacterBeforeSelection;
    encoder << isReplaceAllowed;
    encoder << hasContent;
    encoder << isStableStateUpdate;
    encoder << insideFixedPosition;
    encoder << hasPlainText;
    encoder << elementIsTransparent;
    encoder << caretColor;
#endif
#if PLATFORM(MAC)
    encoder << candidateRequestStartPosition;
    encoder << paragraphContextForCandidateRequest;
    encoder << stringForCandidateRequest;
#endif
    encoder << fontAttributes;
    encoder << canCut;
    encoder << canCopy;
    encoder << canPaste;
}

bool EditorState::PostLayoutData::decode(IPC::Decoder& decoder, PostLayoutData& result)
{
    if (!decoder.decode(result.typingAttributes))
        return false;
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK)
    if (!decoder.decode(result.caretRectAtStart))
        return false;
#endif
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    if (!decoder.decode(result.selectionClipRect))
        return false;
    if (!decoder.decode(result.selectedTextLength))
        return false;
    if (!decoder.decode(result.textAlignment))
        return false;
    if (!decoder.decode(result.textColor))
        return false;
    if (!decoder.decode(result.enclosingListType))
        return false;
#endif
#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(result.caretRectAtEnd))
        return false;
    if (!decoder.decode(result.selectionRects))
        return false;
    if (!decoder.decode(result.wordAtSelection))
        return false;
    if (!decoder.decode(result.characterAfterSelection))
        return false;
    if (!decoder.decode(result.characterBeforeSelection))
        return false;
    if (!decoder.decode(result.twoCharacterBeforeSelection))
        return false;
    if (!decoder.decode(result.isReplaceAllowed))
        return false;
    if (!decoder.decode(result.hasContent))
        return false;
    if (!decoder.decode(result.isStableStateUpdate))
        return false;
    if (!decoder.decode(result.insideFixedPosition))
        return false;
    if (!decoder.decode(result.hasPlainText))
        return false;
    if (!decoder.decode(result.elementIsTransparent))
        return false;
    if (!decoder.decode(result.caretColor))
        return false;
#endif
#if PLATFORM(MAC)
    if (!decoder.decode(result.candidateRequestStartPosition))
        return false;

    if (!decoder.decode(result.paragraphContextForCandidateRequest))
        return false;

    if (!decoder.decode(result.stringForCandidateRequest))
        return false;
#endif

    std::optional<std::optional<FontAttributes>> optionalFontAttributes;
    decoder >> optionalFontAttributes;
    if (!optionalFontAttributes)
        return false;

    result.fontAttributes = optionalFontAttributes.value();

    if (!decoder.decode(result.canCut))
        return false;
    if (!decoder.decode(result.canCopy))
        return false;
    if (!decoder.decode(result.canPaste))
        return false;

    return true;
}

TextStream& operator<<(TextStream& ts, const EditorState& editorState)
{
#if PLATFORM(IOS_FAMILY)
    if (editorState.firstMarkedRect != IntRect())
        ts.dumpProperty("firstMarkedRect", editorState.firstMarkedRect);
    if (editorState.lastMarkedRect != IntRect())
        ts.dumpProperty("lastMarkedRect", editorState.lastMarkedRect);
    if (editorState.markedText.length())
        ts.dumpProperty("markedText", editorState.markedText);
#endif

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
    if (editorState.isMissingPostLayoutData)
        ts.dumpProperty("isMissingPostLayoutData", editorState.isMissingPostLayoutData);

    if (editorState.isMissingPostLayoutData)
        return ts;

    TextStream::GroupScope scope(ts);
    ts << "postLayoutData";
    if (editorState.postLayoutData().typingAttributes != AttributeNone)
        ts.dumpProperty("typingAttributes", editorState.postLayoutData().typingAttributes);
#if PLATFORM(IOS_FAMILY) || PLATFORM(GTK)
    if (editorState.postLayoutData().caretRectAtStart != IntRect())
        ts.dumpProperty("caretRectAtStart", editorState.postLayoutData().caretRectAtStart);
#endif
#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
    if (editorState.postLayoutData().selectionClipRect != IntRect())
        ts.dumpProperty("selectionClipRect", editorState.postLayoutData().selectionClipRect);
    if (editorState.postLayoutData().selectedTextLength)
        ts.dumpProperty("selectedTextLength", editorState.postLayoutData().selectedTextLength);
    if (editorState.postLayoutData().textAlignment != NoAlignment)
        ts.dumpProperty("textAlignment", editorState.postLayoutData().textAlignment);
    if (editorState.postLayoutData().textColor.isValid())
        ts.dumpProperty("textColor", editorState.postLayoutData().textColor);
    if (editorState.postLayoutData().enclosingListType != NoList)
        ts.dumpProperty("enclosingListType", editorState.postLayoutData().enclosingListType);
#endif
#if PLATFORM(IOS_FAMILY)
    if (editorState.postLayoutData().caretRectAtEnd != IntRect())
        ts.dumpProperty("caretRectAtEnd", editorState.postLayoutData().caretRectAtEnd);
    if (editorState.postLayoutData().selectionRects.size())
        ts.dumpProperty("selectionRects", editorState.postLayoutData().selectionRects);
    if (editorState.postLayoutData().wordAtSelection.length())
        ts.dumpProperty("wordAtSelection", editorState.postLayoutData().wordAtSelection);
    if (editorState.postLayoutData().characterAfterSelection)
        ts.dumpProperty("characterAfterSelection", editorState.postLayoutData().characterAfterSelection);
    if (editorState.postLayoutData().characterBeforeSelection)
        ts.dumpProperty("characterBeforeSelection", editorState.postLayoutData().characterBeforeSelection);
    if (editorState.postLayoutData().twoCharacterBeforeSelection)
        ts.dumpProperty("twoCharacterBeforeSelection", editorState.postLayoutData().twoCharacterBeforeSelection);

    if (editorState.postLayoutData().isReplaceAllowed)
        ts.dumpProperty("isReplaceAllowed", editorState.postLayoutData().isReplaceAllowed);
    if (editorState.postLayoutData().hasContent)
        ts.dumpProperty("hasContent", editorState.postLayoutData().hasContent);
    ts.dumpProperty("isStableStateUpdate", editorState.postLayoutData().isStableStateUpdate);
    if (editorState.postLayoutData().insideFixedPosition)
        ts.dumpProperty("insideFixedPosition", editorState.postLayoutData().insideFixedPosition);
    if (editorState.postLayoutData().caretColor.isValid())
        ts.dumpProperty("caretColor", editorState.postLayoutData().caretColor);
#endif
#if PLATFORM(MAC)
    if (editorState.postLayoutData().candidateRequestStartPosition)
        ts.dumpProperty("candidateRequestStartPosition", editorState.postLayoutData().candidateRequestStartPosition);
    if (editorState.postLayoutData().paragraphContextForCandidateRequest.length())
        ts.dumpProperty("paragraphContextForCandidateRequest", editorState.postLayoutData().paragraphContextForCandidateRequest);
    if (editorState.postLayoutData().stringForCandidateRequest.length())
        ts.dumpProperty("stringForCandidateRequest", editorState.postLayoutData().stringForCandidateRequest);
#endif

    if (editorState.postLayoutData().canCut)
        ts.dumpProperty("canCut", editorState.postLayoutData().canCut);
    if (editorState.postLayoutData().canCopy)
        ts.dumpProperty("canCopy", editorState.postLayoutData().canCopy);
    if (editorState.postLayoutData().canPaste)
        ts.dumpProperty("canPaste", editorState.postLayoutData().canPaste);

    return ts;
}

} // namespace WebKit
