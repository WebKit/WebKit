/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "EditingOperations.h"

#import <Foundation/Foundation.h>

NSArray *editingOperations()
{
    return @[
        @"alignCenter:",
        @"alignJustified:",
        @"alignLeft:",
        @"alignRight:",
        @"capitalizeWord:",
        @"centerSelectionInVisibleArea:",
        @"changeCaseOfLetter:",
        @"checkSpelling:",
        @"complete:",
        @"copy:",
        @"copyFont:",
        @"cut:",
        @"delete:",
        @"deleteBackward:",
        @"deleteBackwardByDecomposingPreviousCharacter:",
        @"deleteForward:",
        @"deleteToBeginningOfLine:",
        @"deleteToBeginningOfParagraph:",
        @"deleteToEndOfLine:",
        @"deleteToEndOfParagraph:",
        @"deleteToMark:",
        @"deleteWordBackward:",
        @"deleteWordForward:",
        @"ignoreSpelling:",
        @"indent:",
        @"insertBacktab:",
        @"insertLineBreak:",
        @"insertNewline:",
        @"insertNewlineIgnoringFieldEditor:",
        @"insertParagraphSeparator:",
        @"insertTab:",
        @"insertTabIgnoringFieldEditor:",
        @"insertTable:",
        @"lowercaseWord:",
        @"moveBackward:",
        @"moveBackwardAndModifySelection:",
        @"moveDown:",
        @"moveDownAndModifySelection:",
        @"moveForward:",
        @"moveForwardAndModifySelection:",
        @"moveLeft:",
        @"moveLeftAndModifySelection:",
        @"moveParagraphBackwardAndModifySelection:",
        @"moveParagraphForwardAndModifySelection:",
        @"moveRight:",
        @"moveRightAndModifySelection:",
        @"moveToBeginningOfDocument:",
        @"moveToBeginningOfDocumentAndModifySelection:",
        @"moveToBeginningOfSentence:",
        @"moveToBeginningOfSentenceAndModifySelection:",
        @"moveToBeginningOfLine:",
        @"moveToBeginningOfLineAndModifySelection:",
        @"moveToBeginningOfParagraph:",
        @"moveToBeginningOfParagraphAndModifySelection:",
        @"moveToEndOfDocument:",
        @"moveToEndOfDocumentAndModifySelection:",
        @"moveToEndOfSentence:",
        @"moveToEndOfSentenceAndModifySelection:",
        @"moveToEndOfLine:",
        @"moveToEndOfLineAndModifySelection:",
        @"moveToEndOfParagraph:",
        @"moveToEndOfParagraphAndModifySelection:",
        @"moveUp:",
        @"moveUpAndModifySelection:",
        @"moveWordBackward:",
        @"moveWordBackwardAndModifySelection:",
        @"moveWordForward:",
        @"moveWordForwardAndModifySelection:",
        @"moveWordLeft:",
        @"moveWordLeftAndModifySelection:",
        @"moveWordRight:",
        @"moveWordRightAndModifySelection:",
        @"outdent:",
        @"outline:",
        @"pageDown:",
        @"pageDownAndModifySelection:",
        @"pageUp:",
        @"pageUpAndModifySelection:",
        @"paste:",
        @"pasteAsPlainText:",
        @"pasteAsRichText:",
        @"pasteFont:",
        @"scrollLineDown:",
        @"scrollLineUp:",
        @"scrollPageDown:",
        @"scrollPageUp:",
        @"selectAll:",
        @"selectSentence:",
        @"selectLine:",
        @"selectParagraph:",
        @"selectToMark:",
        @"selectWord:",
        @"setMark:",
        @"showGuessPanel:",
        @"startSpeaking:",
        @"stopSpeaking:",
        @"subscript:",
        @"superscript:",
        @"swapWithMark:",
        @"takeFindStringFromSelection:",
        @"toggleContinuousSpellChecking:",
        @"toggleSmartInsertDelete:",
        @"transpose:",
        @"transposeWords:",
        @"underline:",
        @"unscript:",
        @"uppercaseWord:",
        @"yank:",
        @"yankAndSelect:"
    ];
}
