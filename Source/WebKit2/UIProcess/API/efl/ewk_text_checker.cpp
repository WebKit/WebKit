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
#include "ewk_text_checker.h"

#if ENABLE(SPELLCHECK)

#include "WKTextChecker.h"
#include "WebKitTextChecker.h"
#include "ewk_text_checker_private.h"

using namespace WebKit;

// Initializes the client's functions to @c 0 to be sure that they are not defined.
static Ewk_Text_Checker ewkTextCheckerCallbacks = {
    0, // unique_spell_document_tag_get
    0, // unique_spell_document_tag_close
    0, // string_spelling_check
    0, // word_guesses_get
    0, // word_learn
    0 // word_ignore
};

#define EWK_TEXT_CHECKER_CALLBACK_SET(TYPE_NAME, NAME)  \
void ewk_text_checker_##NAME##_cb_set(TYPE_NAME cb)     \
{                                                       \
    ewkTextCheckerCallbacks.NAME = cb;                  \
}

/**
 * Attaches spellchecker feature.
 *
 * @internal
 *
 * The default spellchecker feature is based on Enchant library.
 * Client may use own implementation of spellchecker previously set
 * through the callback functions.
 */
void ewk_text_checker_client_attach()
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

/*
 * Gets the client's callbacks.
 *
 * @internal
 *
 * The client't callbacks are not defined by default.
 * If the client hasn't set the callback, the corresponding callback will
 * return @c 0 and the default WebKit implementation will be used for this
 * functionality.
 *
 * @return the struct with the client's callbacks.
 */
Ewk_Text_Checker* ewk_text_checker_callbacks_get()
{
    return &ewkTextCheckerCallbacks;
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
