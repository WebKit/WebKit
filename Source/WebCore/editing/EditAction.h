/*
 * Copyright (C) 2004-2018 Apple Inc.  All rights reserved.
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

#pragma once

namespace WebCore {

enum class EditAction : uint8_t {
    Unspecified,
    Insert,
    InsertReplacement,
    InsertFromDrop,
    SetColor,
    SetBackgroundColor,
    TurnOffKerning,
    TightenKerning,
    LoosenKerning,
    UseStandardKerning,
    TurnOffLigatures,
    UseStandardLigatures,
    UseAllLigatures,
    RaiseBaseline,
    LowerBaseline,
    SetTraditionalCharacterShape,
    SetFont,
    ChangeAttributes,
    AlignLeft,
    AlignRight,
    Center,
    Justify,
    SetWritingDirection,
    Subscript,
    Superscript,
    Underline,
    Outline,
    Unscript,
    DeleteByDrag,
    Cut,
    Bold,
    Italics,
    Delete,
    Dictation,
    Paste,
    PasteFont,
    PasteRuler,
    TypingDeleteSelection,
    TypingDeleteBackward,
    TypingDeleteForward,
    TypingDeleteWordBackward,
    TypingDeleteWordForward,
    TypingDeleteLineBackward,
    TypingDeleteLineForward,
    TypingDeletePendingComposition,
    TypingDeleteFinalComposition,
    TypingInsertText,
    TypingInsertLineBreak,
    TypingInsertParagraph,
    TypingInsertPendingComposition,
    TypingInsertFinalComposition,
    CreateLink,
    Unlink,
    FormatBlock,
    InsertOrderedList,
    InsertUnorderedList,
    ConvertToOrderedList,
    ConvertToUnorderedList,
    Indent,
    Outdent
};

} // namespace WebCore
