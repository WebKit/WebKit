/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2012 Intel Corporation
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
#include "ewk_text_checker.h"

#if ENABLE(SPELLCHECK)

#include "TextCheckerEnchant.h"
#include "WKAPICast.h"
#include "WKMutableArray.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKTextChecker.h"
#include "WebPageProxy.h"
#include "WebString.h"
#include "ewk_settings.h"
#include "ewk_text_checker_private.h"
#include <Eina.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

/**
 * @brief Structure to store client callback functions.
 *
 * @internal
 */
struct ClientCallbacks {
    Ewk_Text_Checker_Unique_Spell_Document_Tag_Get_Cb unique_spell_document_tag_get;
    Ewk_Text_Checker_Unique_Spell_Document_Tag_Close_Cb unique_spell_document_tag_close;
    Ewk_Text_Checker_String_Spelling_Check_Cb string_spelling_check;
    Ewk_Text_Checker_Word_Guesses_Get_Cb word_guesses_get;
    Ewk_Text_Checker_Word_Learn_Cb word_learn;
    Ewk_Text_Checker_Word_Ignore_Cb word_ignore;
};

static inline TextCheckerEnchant* textCheckerEnchant()
{
    static OwnPtr<TextCheckerEnchant> textCheckerEnchant = TextCheckerEnchant::create();
    return textCheckerEnchant.get();
}

static inline ClientCallbacks& clientCallbacks()
{
    DEFINE_STATIC_LOCAL(ClientCallbacks, clientCallbacks, ());
    return clientCallbacks;
}

static bool isContinuousSpellCheckingEnabled(const void*)
{
    return ewk_settings_continuous_spell_checking_enabled_get();
}

static void setContinuousSpellCheckingEnabled(bool enabled, const void*)
{
    ewk_settings_continuous_spell_checking_enabled_set(enabled);
}

static uint64_t uniqueSpellDocumentTag(WKPageRef page, const void*)
{
    if (clientCallbacks().unique_spell_document_tag_get)
        return clientCallbacks().unique_spell_document_tag_get(toImpl(page)->viewWidget());

    return 0;
}

static void closeSpellDocumentWithTag(uint64_t tag, const void*)
{
    if (clientCallbacks().unique_spell_document_tag_close)
        clientCallbacks().unique_spell_document_tag_close(tag);
}

static void checkSpellingOfString(uint64_t tag, WKStringRef text, int32_t* misspellingLocation, int32_t* misspellingLength, const void*)
{
    if (clientCallbacks().string_spelling_check)
        clientCallbacks().string_spelling_check(tag, toImpl(text)->string().utf8().data(), misspellingLocation, misspellingLength);
    else
        textCheckerEnchant()->checkSpellingOfString(toImpl(text)->string(), *misspellingLocation, *misspellingLength);
}

static WKArrayRef guessesForWord(uint64_t tag, WKStringRef word, const void*)
{
    WKMutableArrayRef suggestionsForWord = WKMutableArrayCreate();

    if (clientCallbacks().word_guesses_get) {
        Eina_List* list = clientCallbacks().word_guesses_get(tag, toImpl(word)->string().utf8().data());
        void* item;

        EINA_LIST_FREE(list, item) {
            WKRetainPtr<WKStringRef> suggestion(AdoptWK, WKStringCreateWithUTF8CString(static_cast<const char*>(item)));
            WKArrayAppendItem(suggestionsForWord, suggestion.get());
            free(item);
        }
    } else {
        const Vector<String>& guesses = textCheckerEnchant()->getGuessesForWord(toImpl(word)->string());
        size_t numberOfGuesses = guesses.size();
        for (size_t i = 0; i < numberOfGuesses; ++i) {
            WKRetainPtr<WKStringRef> suggestion(AdoptWK, WKStringCreateWithUTF8CString(guesses[i].utf8().data()));
            WKArrayAppendItem(suggestionsForWord, suggestion.get());
        }
    }

    return suggestionsForWord;
}

static void learnWord(uint64_t tag, WKStringRef word, const void*)
{
    if (clientCallbacks().word_learn)
        clientCallbacks().word_learn(tag, toImpl(word)->string().utf8().data());
    else
        textCheckerEnchant()->learnWord(toImpl(word)->string());
}

static void ignoreWord(uint64_t tag, WKStringRef word, const void*)
{
    if (clientCallbacks().word_ignore)
        clientCallbacks().word_ignore(tag, toImpl(word)->string().utf8().data());
    else
        textCheckerEnchant()->ignoreWord(toImpl(word)->string());
}

namespace Ewk_Text_Checker {

Vector<String> availableSpellCheckingLanguages()
{
    return textCheckerEnchant()->availableSpellCheckingLanguages();
}

void updateSpellCheckingLanguages(const Vector<String>& languages)
{
    textCheckerEnchant()->updateSpellCheckingLanguages(languages);
}

Vector<String> loadedSpellCheckingLanguages()
{
    return textCheckerEnchant()->loadedSpellCheckingLanguages();
}

bool hasDictionary()
{
    return textCheckerEnchant()->hasDictionary();
}

/**
 * Initializes spellcheck feature.
 *
 * @internal
 *
 * The default spellcheck feature is based on Enchant library.
 * Client may use own spellcheck implementation previously set
 * through the callback functions.
 */
void initialize()
{
    static bool didInitializeTextCheckerClient = false;
    if (didInitializeTextCheckerClient)
        return;

    WKTextCheckerClient textCheckerClient = {
        kWKTextCheckerClientCurrentVersion,
        0, // clientInfo
        0, // isContinuousSpellCheckingAllowed
        isContinuousSpellCheckingEnabled,
        setContinuousSpellCheckingEnabled,
        0, // isGrammarCheckingEnabled
        0, // setGrammarCheckingEnabled
        uniqueSpellDocumentTag,
        closeSpellDocumentWithTag,
        checkSpellingOfString,
        0, // checkGrammarOfString
        0, // spellingUIIsShowing
        0, // toggleSpellingUIIsShowing
        0, // updateSpellingUIWithMisspelledWord
        0, // updateSpellingUIWithGrammarString
        guessesForWord,
        learnWord,
        ignoreWord
    };
    WKTextCheckerSetClient(&textCheckerClient);

    didInitializeTextCheckerClient = true;
}

} // namespace Ewk_Text_Checker

#define EWK_TEXT_CHECKER_CALLBACK_SET(TYPE_NAME, NAME)  \
void ewk_text_checker_##NAME##_cb_set(TYPE_NAME cb)     \
{                                                       \
    clientCallbacks().NAME = cb;      \
}

#else

// Defines an empty API to do not break build.
#define EWK_TEXT_CHECKER_CALLBACK_SET(TYPE_NAME, NAME)  \
void ewk_text_checker_##NAME##_cb_set(TYPE_NAME)        \
{                                                       \
}
#endif // ENABLE(SPELLCHECK)

EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_Unique_Spell_Document_Tag_Get_Cb, unique_spell_document_tag_get)
EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_Unique_Spell_Document_Tag_Close_Cb, unique_spell_document_tag_close)
EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_String_Spelling_Check_Cb, string_spelling_check)
EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_Word_Guesses_Get_Cb, word_guesses_get)
EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_Word_Learn_Cb, word_learn)
EWK_TEXT_CHECKER_CALLBACK_SET(Ewk_Text_Checker_Word_Ignore_Cb, word_ignore)
