/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "TextChecker.h"

#include "NotImplemented.h"
#include "TextCheckerState.h"

using namespace WebCore;
 
namespace WebKit {

static TextCheckerState textCheckerState;

const TextCheckerState& TextChecker::state()
{
    notImplemented();
    return textCheckerState;
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    notImplemented();
    return false;
}

void TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
    notImplemented();
}

void TextChecker::setGrammarCheckingEnabled(bool isGrammarCheckingEnabled)
{
    notImplemented();
}

int64_t TextChecker::uniqueSpellDocumentTag()
{
    notImplemented();
    return 0;
}

void TextChecker::closeSpellDocumentWithTag(int64_t)
{
    notImplemented();
}

Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(int64_t spellDocumentTag, const UChar* text, int length, uint64_t checkingTypes)
{
    notImplemented();
    return Vector<WebCore::TextCheckingResult>();
}

void TextChecker::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    notImplemented();
}

void TextChecker::getGuessesForWord(int64_t spellDocumentTag, const String& word, const String& context, Vector<String>& guesses)
{
    notImplemented();
}

void TextChecker::learnWord(const String& word)
{
    notImplemented();
}

void TextChecker::ignoreWord(int64_t spellDocumentTag, const String& word)
{
    notImplemented();
}

} // namespace WebKit
