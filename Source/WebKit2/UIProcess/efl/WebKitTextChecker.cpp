/*
 * Copyright (C) 2012 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitTextChecker.h"

#if ENABLE(SPELLCHECK)

#include "NotImplemented.h"
#include "WKAPICast.h"
#include "WKMutableArray.h"
#include "WKRetainPtr.h"
#include "WebPageProxy.h"
#include "WebString.h"
#include "ewk_settings.h"
#include "ewk_text_checker_private.h"
#include <Eina.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

namespace WebKit {

static OwnPtr<WebCore::TextCheckerEnchant> textCheckerEnchant = WebCore::TextCheckerEnchant::create();

static Ewk_Text_Checker* ewkTextCheckerCallbacks = ewk_text_checker_callbacks_get();

bool isContinuousSpellCheckingEnabled(const void*)
{
    return ewk_settings_continuous_spell_checking_enabled_get();
}

void setContinuousSpellCheckingEnabled(bool enabled, const void*)
{
    ewk_settings_continuous_spell_checking_enabled_set(enabled);
}

uint64_t uniqueSpellDocumentTag(WKPageRef page, const void*)
{
    if (ewkTextCheckerCallbacks->unique_spell_document_tag_get)
        return ewkTextCheckerCallbacks->unique_spell_document_tag_get(toImpl(page)->viewWidget());

    return 0;
}

void closeSpellDocumentWithTag(uint64_t tag, const void*)
{
    if (ewkTextCheckerCallbacks->unique_spell_document_tag_close)
        ewkTextCheckerCallbacks->unique_spell_document_tag_close(tag);
}

void checkSpellingOfString(uint64_t tag, WKStringRef text, int32_t* misspellingLocation, int32_t* misspellingLength, const void*)
{
    if (ewkTextCheckerCallbacks->string_spelling_check)
        ewkTextCheckerCallbacks->string_spelling_check(tag, toImpl(text)->string().utf8().data(), misspellingLocation, misspellingLength);
    else
        textCheckerEnchant->checkSpellingOfString(toImpl(text)->string(), *misspellingLocation, *misspellingLength);
}

WKArrayRef guessesForWord(uint64_t tag, WKStringRef word, const void*)
{
    WKMutableArrayRef suggestionsForWord = WKMutableArrayCreate();

    if (ewkTextCheckerCallbacks->word_guesses_get) {
        Eina_List* list = ewkTextCheckerCallbacks->word_guesses_get(tag, toImpl(word)->string().utf8().data());
        void* item;

        EINA_LIST_FREE(list, item) {
            WKRetainPtr<WKStringRef> suggestion(AdoptWK, WKStringCreateWithUTF8CString(static_cast<char*>(item)));
            WKArrayAppendItem(suggestionsForWord, suggestion.get());
            free(item);
        }
    } else {
        Vector<String> guesses = textCheckerEnchant->getGuessesForWord(toImpl(word)->string());
        size_t numberOfGuesses = guesses.size();
        for (size_t i = 0; i < numberOfGuesses; ++i) {
            WKRetainPtr<WKStringRef> suggestion(AdoptWK, WKStringCreateWithUTF8CString(guesses[i].utf8().data()));
            WKArrayAppendItem(suggestionsForWord, suggestion.get());
        }
    }

    return suggestionsForWord;
}

void learnWord(uint64_t tag, WKStringRef word, const void*)
{
    if (ewkTextCheckerCallbacks->word_learn)
        ewkTextCheckerCallbacks->word_learn(tag, toImpl(word)->string().utf8().data());
    else
        textCheckerEnchant->learnWord(toImpl(word)->string());
}

void ignoreWord(uint64_t tag, WKStringRef word, const void*)
{
    if (ewkTextCheckerCallbacks->word_ignore)
        ewkTextCheckerCallbacks->word_ignore(tag, toImpl(word)->string().utf8().data());
    else
        textCheckerEnchant->ignoreWord(toImpl(word)->string());
}

Vector<String> availableSpellCheckingLanguages()
{
    return textCheckerEnchant->availableSpellCheckingLanguages();
}

void updateSpellCheckingLanguages(const Vector<String>& languages)
{
    textCheckerEnchant->updateSpellCheckingLanguages(languages);
}

Vector<String> loadedSpellCheckingLanguages()
{
    return textCheckerEnchant->loadedSpellCheckingLanguages();
}

} // namespace WebKit

#endif // ENABLE(SPELLCHECK)
