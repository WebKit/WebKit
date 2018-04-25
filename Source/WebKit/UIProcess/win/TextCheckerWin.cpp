/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "TextChecker.h"

#include "TextCheckerState.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

TextCheckerState& checkerState()
{
    static TextCheckerState textCheckerState;
    return textCheckerState;
}

const TextCheckerState& TextChecker::state()
{
    return checkerState();
}

static bool testingModeEnabled = false;

void TextChecker::setTestingMode(bool enabled)
{
    testingModeEnabled = enabled;
}

bool TextChecker::isTestingMode()
{
    return testingModeEnabled;
}

bool TextChecker::isContinuousSpellCheckingAllowed()
{
    return false;
}

void TextChecker::setContinuousSpellCheckingEnabled(bool)
{
}

void TextChecker::setGrammarCheckingEnabled(bool)
{
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool)
{
}

void TextChecker::grammarCheckingEnabledStateChanged(bool)
{
}

int64_t TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    return 0;
}

void TextChecker::closeSpellDocumentWithTag(int64_t)
{
}

void TextChecker::checkSpellingOfString(int64_t /* spellDocumentTag */, StringView, int32_t&, int32_t&)
{
}

void TextChecker::checkGrammarOfString(int64_t /* spellDocumentTag */, StringView /* text */, Vector<WebCore::GrammarDetail>& /* grammarDetails */, int32_t& /* badGrammarLocation */, int32_t& /* badGrammarLength */)
{
}

bool TextChecker::spellingUIIsShowing()
{
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
}

void TextChecker::updateSpellingUIWithMisspelledWord(int64_t /* spellDocumentTag */, const String& /* misspelledWord */)
{
}

void TextChecker::updateSpellingUIWithGrammarString(int64_t /* spellDocumentTag */, const String& /* badGrammarPhrase */, const GrammarDetail& /* grammarDetail */)
{
}

void TextChecker::getGuessesForWord(int64_t /* spellDocumentTag */, const String&, const String& /* context */, int32_t /* insertionPoint */, Vector<String>&, bool)
{
}

void TextChecker::learnWord(int64_t /* spellDocumentTag */, const String&)
{
}

void TextChecker::ignoreWord(int64_t /* spellDocumentTag */, const String&)
{
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&&, int32_t)
{
}

#if USE(UNIFIED_TEXT_CHECKING)
Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(int64_t, StringView, int32_t, uint64_t, bool)
{
    return Vector<TextCheckingResult>();
}
#endif // USE(UNIFIED_TEXT_CHECKING)

} // namespace WebKit
