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

#include "TextCheckerState.h"
#include "WebProcessPool.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/TextCheckerEnchant.h>
#include <unicode/ubrk.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebKit {
using namespace WebCore;

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
    
static bool testingModeEnabled = false;
    
void TextChecker::setTestingMode(bool enabled)
{
    testingModeEnabled = enabled;
}

bool TextChecker::isTestingMode()
{
    return testingModeEnabled;
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

bool TextChecker::setContinuousSpellCheckingEnabled(bool isContinuousSpellCheckingEnabled)
{
#if ENABLE(SPELLCHECK)
    if (checkerState().isContinuousSpellCheckingEnabled == isContinuousSpellCheckingEnabled)
        return false;
    checkerState().isContinuousSpellCheckingEnabled = isContinuousSpellCheckingEnabled;
    updateStateForAllProcessPools();
#else
    UNUSED_PARAM(isContinuousSpellCheckingEnabled);
#endif
    return true;
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

SpellDocumentTag TextChecker::uniqueSpellDocumentTag(WebPageProxy*)
{
    return { };
}

void TextChecker::closeSpellDocumentWithTag(SpellDocumentTag)
{
}

void TextChecker::checkSpellingOfString(SpellDocumentTag, StringView text, int32_t& misspellingLocation, int32_t& misspellingLength)
{
#if ENABLE(SPELLCHECK)
    misspellingLocation = -1;
    misspellingLength = 0;
    TextCheckerEnchant::singleton().checkSpellingOfString(text.toStringWithoutCopying(), misspellingLocation, misspellingLength);
#else
    UNUSED_PARAM(text);
    UNUSED_PARAM(misspellingLocation);
    UNUSED_PARAM(misspellingLength);
#endif
}

void TextChecker::checkGrammarOfString(SpellDocumentTag, StringView /* text */, Vector<WebCore::GrammarDetail>& /* grammarDetails */, int32_t& /* badGrammarLocation */, int32_t& /* badGrammarLength */)
{
}

bool TextChecker::spellingUIIsShowing()
{
    return false;
}

void TextChecker::toggleSpellingUIIsShowing()
{
}

void TextChecker::updateSpellingUIWithMisspelledWord(SpellDocumentTag, const String& /* misspelledWord */)
{
}

void TextChecker::updateSpellingUIWithGrammarString(SpellDocumentTag, const String& /* badGrammarPhrase */, const GrammarDetail& /* grammarDetail */)
{
}

void TextChecker::getGuessesForWord(SpellDocumentTag, const String& word, const String& /* context */, int32_t /* insertionPoint */, Vector<String>& guesses, bool)
{
#if ENABLE(SPELLCHECK)
    guesses = TextCheckerEnchant::singleton().getGuessesForWord(word);
#else
    UNUSED_PARAM(word);
    UNUSED_PARAM(guesses);
#endif
}

void TextChecker::learnWord(SpellDocumentTag, const String& word)
{
#if ENABLE(SPELLCHECK)
    TextCheckerEnchant::singleton().learnWord(word);
#else
    UNUSED_PARAM(word);
#endif
}

void TextChecker::ignoreWord(SpellDocumentTag, const String& word)
{
#if ENABLE(SPELLCHECK)
    TextCheckerEnchant::singleton().ignoreWord(word);
#else
    UNUSED_PARAM(word);
#endif
}

void TextChecker::requestCheckingOfString(Ref<TextCheckerCompletion>&& completion, int32_t insertionPoint)
{
#if ENABLE(SPELLCHECK)
    TextCheckingRequestData request = completion->textCheckingRequestData();
    ASSERT(request.sequence() != unrequestedTextCheckingSequence);
    ASSERT(request.checkingTypes());

    completion->didFinishCheckingText(checkTextOfParagraph(completion->spellDocumentTag(), request.text(), insertionPoint, request.checkingTypes(), false));
#else
    UNUSED_PARAM(completion);
#endif
}

#if USE(UNIFIED_TEXT_CHECKING) && ENABLE(SPELLCHECK)
static unsigned nextWordOffset(StringView text, unsigned currentOffset)
{
    // FIXME: avoid creating textIterator object here, it could be passed as a parameter.
    //        ubrk_isBoundary() leaves the iterator pointing to the first boundary position at
    //        or after "offset" (ubrk_isBoundary side effect).
    //        For many word separators, the method doesn't properly determine the boundaries
    //        without resetting the iterator.
    UBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return currentOffset;

    unsigned wordOffset = currentOffset;
    while (wordOffset < text.length() && ubrk_isBoundary(textIterator, wordOffset))
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
Vector<TextCheckingResult> TextChecker::checkTextOfParagraph(SpellDocumentTag spellDocumentTag, StringView text, int32_t insertionPoint, OptionSet<TextCheckingType> checkingTypes, bool)
{
    UNUSED_PARAM(insertionPoint);
#if ENABLE(SPELLCHECK)
    if (!checkingTypes.contains(TextCheckingType::Spelling))
        return { };

    UBreakIterator* textIterator = wordBreakIterator(text);
    if (!textIterator)
        return { };

    // Omit the word separators at the beginning/end of the text to don't unnecessarily
    // involve the client to check spelling for them.
    unsigned offset = nextWordOffset(text, 0);
    unsigned lengthStrip = text.length();
    while (lengthStrip > 0 && ubrk_isBoundary(textIterator, lengthStrip - 1))
        --lengthStrip;

    Vector<TextCheckingResult> paragraphCheckingResult;
    while (offset < lengthStrip) {
        int32_t misspellingLocation = -1;
        int32_t misspellingLength = 0;
        checkSpellingOfString(spellDocumentTag, text.substring(offset, lengthStrip - offset), misspellingLocation, misspellingLength);
        if (!misspellingLength)
            break;

        TextCheckingResult misspellingResult;
        misspellingResult.type = TextCheckingType::Spelling;
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
    TextCheckerEnchant::singleton().updateSpellCheckingLanguages(languages);
#else
    UNUSED_PARAM(languages);
#endif
}

Vector<String> TextChecker::loadedSpellCheckingLanguages()
{
#if ENABLE(SPELLCHECK)
    return TextCheckerEnchant::singleton().loadedSpellCheckingLanguages();
#else
    return Vector<String>();
#endif
}

} // namespace WebKit
