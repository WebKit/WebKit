/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011-2013 Samsung Electronics
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

#include "TextBreakIterator.h"
#include "TextCheckerState.h"
#include "WebProcessPool.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/TextCheckerEnchant.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

#if ENABLE(SPELLCHECK)
static WebCore::TextCheckerEnchant& enchantTextChecker()
{
    static NeverDestroyed<WebCore::TextCheckerEnchant> checker;
    return checker;
}
#endif

TextCheckerState& checkerState()
{
    static TextCheckerState textCheckerState;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        textCheckerState.isContinuousSpellCheckingEnabled = false;
        textCheckerState.isGrammarCheckingEnabled = false;
    });

    return textCheckerState;
}

const TextCheckerState& TextChecker::state()
{
    return checkerState();
}

#if ENABLE(SPELLCHECK)
static void updateStateForAllProcessPools()
{
    for (const auto& processPool : WebProcessPool::allProcessPools())
        processPool->textCheckerStateChanged();
}
#endif

bool TextChecker::isContinuousSpellCheckingAllowed()
{
#if ENABLE(SPELLCHECK)
    return true;
#else
    return false;
#endif
}

void TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
#if ENABLE(SPELLCHECK)
    if (checkerState().isContinuousSpellCheckingEnabled == isContinuousSpellCheckingEnabled)
        return;
    checkerState().isContinuousSpellCheckingEnabled = isContinuousSpellCheckingEnabled;
    updateStateForAllProcessPools();
#else
    UNUSED_PARAM(isContinuousSpellCheckingEnabled);
#endif
}

void TextChecker::setGrammarCheckingEnabled(bool isGrammarCheckingEnabled)
{
#if ENABLE(SPELLCHECK)
    if (checkerState().isGrammarCheckingEnabled == isGrammarCheckingEnabled)
        return;
    checkerState().isGrammarCheckingEnabled = isGrammarCheckingEnabled;
    updateStateForAllProcessPools();
#else
    UNUSED_PARAM(isGrammarCheckingEnabled);
#endif
}

void TextChecker::continuousSpellCheckingEnabledStateChanged(bool enabled)
{
#if ENABLE(SPELLCHECK)
    checkerState().isContinuousSpellCheckingEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

void TextChecker::grammarCheckingEnabledStateChanged(bool enabled)
{
#if ENABLE(SPELLCHECK)
    checkerState().isGrammarCheckingEnabled = enabled;
#else
    UNUSED_PARAM(enabled);
#endif
}

int64_t TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    return 0;
}

void TextChecker::closeSpellDocumentWithTag(int64_t /* tag */)
{
}

void TextChecker::checkSpellingOfString(int64_t /* spellDocumentTag */, StringView text, int32_t& misspellingLocation, int32_t& misspellingLength)
{
#if ENABLE(SPELLCHECK)
    misspellingLocation = -1;
    misspellingLength = 0;
    enchantTextChecker().checkSpellingOfString(text.toStringWithoutCopying(), misspellingLocation, misspellingLength);
#else
    UNUSED_PARAM(text);
    UNUSED_PARAM(misspellingLocation);
    UNUSED_PARAM(misspellingLength);
#endif
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

void TextChecker::getGuessesForWord(int64_t /* spellDocumentTag */, const String& word, const String& /* context */, int32_t /* insertionPoint */, Vector<String>& guesses)
{
#if ENABLE(SPELLCHECK)
    guesses = enchantTextChecker().getGuessesForWord(word);
#else
    UNUSED_PARAM(word);
    UNUSED_PARAM(guesses);
#endif
}

void TextChecker::learnWord(int64_t /* spellDocumentTag */, const String& word)
{
#if ENABLE(SPELLCHECK)
    enchantTextChecker().learnWord(word);
#else
    UNUSED_PARAM(word);
#endif
}

void TextChecker::ignoreWord(int64_t /* spellDocumentTag */, const String& word)
{
#if ENABLE(SPELLCHECK)
    enchantTextChecker().ignoreWord(word);
#else
    UNUSED_PARAM(word);
#endif
}

void TextChecker::requestCheckingOfString(PassRefPtr<TextCheckerCompletion> completion, int32_t insertionPoint)
{
#if ENABLE(SPELLCHECK)
    if (!completion)
        return;

    TextCheckingRequestData request = completion->textCheckingRequestData();
    ASSERT(request.sequence() != unrequestedTextCheckingSequence);
    ASSERT(request.mask() != TextCheckingTypeNone);

    completion->didFinishCheckingText(checkTextOfParagraph(completion->spellDocumentTag(), request.text(), insertionPoint, request.mask()));
#else
    UNUSED_PARAM(completion);
#endif
}

#if USE(UNIFIED_TEXT_CHECKING) && ENABLE(SPELLCHECK)
static unsigned nextWordOffset(StringView text, unsigned currentOffset)
{
    // FIXME: avoid creating textIterator object here, it could be passed as a parameter.
    //        isTextBreak() leaves the iterator pointing to the first boundary position at
    //        or after "offset" (ubrk_isBoundary side effect).
    //        For many word separators, the method doesn't properly determine the boundaries
    //        without resetting the iterator.
    TextBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return currentOffset;

    unsigned wordOffset = currentOffset;
    while (wordOffset < text.length() && isTextBreak(textIterator, wordOffset))
        ++wordOffset;

    // Do not treat the word's boundary as a separator.
    if (!currentOffset && wordOffset == 1)
        return currentOffset;

    // Omit multiple separators.
    if ((wordOffset - currentOffset) > 1)
        --wordOffset;

    return wordOffset;
}
#endif

#if USE(UNIFIED_TEXT_CHECKING)
Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(int64_t spellDocumentTag, StringView text, int32_t insertionPoint, uint64_t checkingTypes)
{
    UNUSED_PARAM(insertionPoint);
#if ENABLE(SPELLCHECK)
    if (!(checkingTypes & TextCheckingTypeSpelling))
        return Vector<TextCheckingResult>();

    TextBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return Vector<TextCheckingResult>();

    // Omit the word separators at the beginning/end of the text to don't unnecessarily
    // involve the client to check spelling for them.
    unsigned offset = nextWordOffset(text, 0);
    unsigned lengthStrip = text.length();
    while (lengthStrip > 0 && isTextBreak(textIterator, lengthStrip - 1))
        --lengthStrip;

    Vector<TextCheckingResult> paragraphCheckingResult;
    while (offset < lengthStrip) {
        int32_t misspellingLocation = -1;
        int32_t misspellingLength = 0;
        checkSpellingOfString(spellDocumentTag, text.substring(offset, lengthStrip - offset), misspellingLocation, misspellingLength);
        if (!misspellingLength)
            break;

        TextCheckingResult misspellingResult;
        misspellingResult.type = TextCheckingTypeSpelling;
        misspellingResult.location = offset + misspellingLocation;
        misspellingResult.length = misspellingLength;
        paragraphCheckingResult.append(misspellingResult);
        offset += misspellingLocation + misspellingLength;
        // Generally, we end up checking at the word separator, move to the adjacent word.
        offset = nextWordOffset(text.substring(0, lengthStrip), offset);
    }
    return paragraphCheckingResult;
#else
    UNUSED_PARAM(spellDocumentTag);
    UNUSED_PARAM(text);
    UNUSED_PARAM(checkingTypes);
    return Vector<TextCheckingResult>();
#endif // ENABLE(SPELLCHECK)
}
#endif // USE(UNIFIED_TEXT_CHECKING)

void TextChecker::setSpellCheckingLanguages(const Vector<String>& languages)
{
#if ENABLE(SPELLCHECK)
    enchantTextChecker().updateSpellCheckingLanguages(languages);
#else
    UNUSED_PARAM(languages);
#endif
}

Vector<String> TextChecker::loadedSpellCheckingLanguages()
{
#if ENABLE(SPELLCHECK)
    return enchantTextChecker().loadedSpellCheckingLanguages();
#else
    return Vector<String>();
#endif
}

} // namespace WebKit
