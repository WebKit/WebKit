/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @brief covers API from ewk_text_checker.h
 * @file test_ewk2_text_checker.cpp
 */

#include "config.h"

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static const uint64_t defaultDocumentTag = 123;
static const char expectedMisspelledWord[] = "aa";
static const Evas_Object* defaultView = 0;
static bool isSettingEnabled = false;
static Ecore_Timer* timeoutTimer = 0;
static double defaultTimeoutInSeconds = 0.5;

static bool wasContextMenuShown = false;
static const char noGuessesString[] = "No Guesses Found";
static const char ignoreSpellingString[] = "Ignore Spelling";
static const char learnSpellingString[] = "Learn Spelling";

/**
 * Structure keeps information which callbacks were called.
 * Its values are reset before each test.
 */
static struct {
    bool spellDocumentTag : 1;
    bool spellDocumentTagClose : 1;
    bool spellingCheck : 1;
    bool wordGuesses : 1;
    bool wordLearn : 1;
    bool wordIgnore : 1;
} callbacksExecutionStats;

static void resetCallbacksExecutionStats()
{
    callbacksExecutionStats.spellDocumentTag = false;
    callbacksExecutionStats.spellDocumentTagClose = false;
    callbacksExecutionStats.spellingCheck = false;
    callbacksExecutionStats.wordGuesses = false;
    callbacksExecutionStats.wordLearn = false;
    callbacksExecutionStats.wordIgnore = false;
}

/**
 * Handle the timeout, it may happen for the asynchronous tests.
 *
 * @internal
 *
 * @return the ECORE_CALLBACK_CANCEL flag to delete the timer automatically
 */
static Eina_Bool onTimeout(void*)
{
    ecore_main_loop_quit();
    return ECORE_CALLBACK_CANCEL;
}

/**
 * This callback tests whether the client's callback is called when the spell checking setting was changed.
 *
 * @internal
 *
 * Verify the new setting value (passes in the @a flag parameter) if it equals to the previously set.
 *
 * @internal
 *
 * @param flag the new setting value
 */
static void onSettingChange(Eina_Bool flag)
{
    EXPECT_EQ(isSettingEnabled, flag);

    ecore_timer_del(timeoutTimer);
    timeoutTimer = 0;
    ecore_main_loop_quit();
}

/**
 * Returns unique tag (an identifier).
 *
 * @internal
 *
 * It will be used for onSpellingCheck, onWordGuesses etc. to notify
 * the client on which object (associated to the tag) the spelling is being invoked.
 *
 * @param ewkView the view object to get unique tag
 *
 * @return unique tag for the given @a ewkView object
 */
static uint64_t onSpellDocumentTag(const Evas_Object* ewkView)
{
    EXPECT_EQ(defaultView, ewkView);
    callbacksExecutionStats.spellDocumentTag = true;

    return defaultDocumentTag;
}

/**
 * The view which is associated to the @a tag has been destroyed.
 *
 * @internal
 *
 * @param tag the tag to be closed
 */
static void onSpellDocumentTagClose(uint64_t tag)
{
    ASSERT_EQ(defaultDocumentTag, tag);
    callbacksExecutionStats.spellDocumentTagClose = true;
}

/**
 * Checks spelling for the given @a text.
 *
 * @internal
 *
 * @param tag unique tag to notify the client on which object the spelling is being performed
 * @param text the text containing the words to spellcheck
 * @param misspelling_location a pointer to store the beginning of the misspelled @a text, @c -1 if the @a text is correct
 * @param misspelling_length a pointer to store the length of misspelled @a text, @c 0 if the @a text is correct
 */
static void onSpellingCheck(uint64_t tag, const char* text, int32_t* misspellingLocation, int32_t* misspellingLength)
{
    ASSERT_EQ(defaultDocumentTag, tag);
    ASSERT_STREQ(expectedMisspelledWord, text);

    ASSERT_TRUE(misspellingLocation);
    ASSERT_TRUE(misspellingLength);

    // The client is able to show the misselled text through its location (the beginning of misspelling)
    // and length (the end of misspelling).
    *misspellingLocation = 0;
    *misspellingLength = strlen(expectedMisspelledWord);

    callbacksExecutionStats.spellingCheck = true;
}

/**
 * Gets a list of suggested spellings for a misspelled @a word.
 *
 * @internal
 *
 * @param tag unique tag to notify the client on which object the spelling is being performed
 * @param word the word to get guesses
 * @return a list of dynamically allocated strings (as char*) and
 *         caller is responsible for destroying them.
 */
static Eina_List* onWordGuesses(uint64_t tag, const char* word)
{
    EXPECT_EQ(defaultDocumentTag, tag);
    EXPECT_STREQ(expectedMisspelledWord, word);

    Eina_List* suggestionsForWord = 0;
    // FIXME: Fill the Eina_List with suggestions for the misspelled word.
    callbacksExecutionStats.wordGuesses = true;

    return suggestionsForWord;
}

/**
 * Adds the @a word to the spell checker dictionary.
 *
 * @internal
 *
 * @param tag unique tag to notify the client on which object the spelling is being performed
 * @param word the word to add
 */
static void onWordLearn(uint64_t tag, const char* word)
{
    ASSERT_EQ(defaultDocumentTag, tag);
    ASSERT_STREQ(expectedMisspelledWord, word);
    callbacksExecutionStats.wordLearn = true;
}

/**
 * Tells the spell checker to ignore a given @a word.
 *
 * @internal
 *
 * @param tag unique tag to notify the client on which object the spelling is being performed
 * @param word the word to ignore
 */
static void onWordIgnore(uint64_t tag, const char* word)
{
    ASSERT_EQ(defaultDocumentTag, tag);
    ASSERT_STREQ(expectedMisspelledWord, word);
    callbacksExecutionStats.wordIgnore = true;
}

static Eina_Bool onContextMenuShow(Ewk_View_Smart_Data*, Evas_Coord, Evas_Coord, Ewk_Context_Menu* contextMenu)
{
    const Eina_List* contextMenuItems = ewk_context_menu_items_get(contextMenu);

    bool noGuessesAvailable = false;
    bool isIgnoreSpellingAvailable = false;
    bool isLearnSpellingAvailable = false;

    const Eina_List* listIterator;
    void* itemData;
    EINA_LIST_FOREACH(contextMenuItems, listIterator, itemData) {
        Ewk_Context_Menu_Item* item = static_cast<Ewk_Context_Menu_Item*>(itemData);
        if (!strcmp(ewk_context_menu_item_title_get(item), noGuessesString))
            noGuessesAvailable = true;
        else if (!strcmp(ewk_context_menu_item_title_get(item), ignoreSpellingString))
            isIgnoreSpellingAvailable = true;
        else if (!strcmp(ewk_context_menu_item_title_get(item), learnSpellingString))
            isLearnSpellingAvailable = true;
    }

    EXPECT_FALSE(noGuessesAvailable);
    EXPECT_TRUE(isIgnoreSpellingAvailable);
    EXPECT_TRUE(isLearnSpellingAvailable);

    wasContextMenuShown = true;
    return true;
}

/**
 * Test whether the default language is loaded independently of
 * continuous spell checking setting.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_spell_checking_languages_get)
{
    ewk_text_checker_continuous_spell_checking_enabled_set(false);
    // The language is being loaded on the idler, wait for it.
    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);
    ecore_main_loop_begin();

    Eina_List* loadedLanguages = ewk_text_checker_spell_checking_languages_get();
    // No dictionary is available/installed.
    if (!loadedLanguages)
        return;

    EXPECT_EQ(1, eina_list_count(loadedLanguages));

    void* data;
    EINA_LIST_FREE(loadedLanguages, data)
        eina_stringshare_del(static_cast<Eina_Stringshare*>(data));

    // Repeat the checking when continuous spell checking setting is on.
    ewk_text_checker_continuous_spell_checking_enabled_set(true);
    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);
    ecore_main_loop_begin();

    loadedLanguages = ewk_text_checker_spell_checking_languages_get();
    if (!loadedLanguages)
        return;

    EXPECT_EQ(1, eina_list_count(loadedLanguages));

    EINA_LIST_FREE(loadedLanguages, data)
        eina_stringshare_del(static_cast<Eina_Stringshare*>(data));
}

/**
 * Test whether the context menu spelling items (suggestions, learn and ignore spelling)
 * are available when continuous spell checking is off.
 */
TEST_F(EWK2UnitTestBase, context_menu_spelling_items_availability)
{
    ewk_text_checker_continuous_spell_checking_enabled_set(false);
    ewkViewClass()->context_menu_show = onContextMenuShow;

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));
    mouseClick(10, 20, 3 /* Right button */);

    while (!wasContextMenuShown)
        ecore_main_loop_iterate();
}

/**
 * Test setter/getter for the continuous spell checking:
 *  - ewk_text_checker_continuous_spell_checking_enabled_get
 *  - ewk_text_checker_continuous_spell_checking_enabled_set
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_continuous_spell_checking_enabled)
{
    ewk_text_checker_continuous_spell_checking_enabled_set(true);
#if ENABLE(SPELLCHECK)
    EXPECT_TRUE(ewk_text_checker_continuous_spell_checking_enabled_get());
#else
    EXPECT_FALSE(ewk_text_checker_continuous_spell_checking_enabled_get());
#endif

    ewk_text_checker_continuous_spell_checking_enabled_set(false);
    EXPECT_FALSE(ewk_text_checker_continuous_spell_checking_enabled_get());
}

/**
 * Test whether the callback is called when the spell checking setting was changed by WebKit.
 * Changing of this setting at the WebKit level can be made as a result of modifying
 * options in a Context Menu by a user.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_continuous_spell_checking_change_cb_set)
{
    ewk_text_checker_continuous_spell_checking_change_cb_set(onSettingChange);

    isSettingEnabled = ewk_text_checker_continuous_spell_checking_enabled_get();
    isSettingEnabled = !isSettingEnabled;
    // The notifications about the setting change shouldn't be sent if the change was made
    // on the client's request (public API).
    ewk_text_checker_continuous_spell_checking_enabled_set(isSettingEnabled);

    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);

    // Start proccesing Ecore events, the notification about the change of the spell
    // checking setting is called on idler.
    // We can't call ecore_main_loop_iterate because it doesn't process the idlers.
    ecore_main_loop_begin();

    EXPECT_TRUE(timeoutTimer);

    /* The callback should be called if the context menu "Check Spelling While Typing" option
       was toggled by the user.
    FIXME:
    1) Invoke the context menu on the input field.
    2) Choose the sub menu "Spelling and Grammar" option (not implemented for WK2).
    3) Toggle "Check Spelling While Typing" option.
    4) Check whether the callback is called. */

    /* The callback shouldn't be called if the user has invalidated it.
    FIXME:
    1) Call ewk_text_checker_continuous_spell_checking_change_cb_set(0) to notify the WebKit that
       the client is not interested in the setting change.
    2) Invoke the context menu on the input field.
    3) Choose the sub menu "Spelling and Grammar" option (not implemented for WK2).
    4) Toggle "Check Spelling While Typing" option.
    5) Check whether the callback was called. */
}

/**
 * This unit test sets all available/installed dictionaries and verifies them
 * if they are in use.
 * All the dictionaries from the list can be set to perform spellchecking.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_spell_checking_available_languages_get)
{
    Eina_List* availableLanguages = ewk_text_checker_spell_checking_available_languages_get();
    // No dictionary is available/installed or the SPELLCHECK macro is disabled.
    if (!availableLanguages)
        return;

    // Helper to create one string with comma separated languages.
    void* actual = 0;
    WTF::StringBuilder languages;
    Eina_List* listIterator = 0;
    unsigned lastIndex = eina_list_count(availableLanguages) - 1;
    unsigned i = 0;
    EINA_LIST_FOREACH(availableLanguages, listIterator, actual) {
        languages.append(static_cast<const char*>(actual));
        // Add the comma after all but the last language IDs.
        if (i++ != lastIndex)
            languages.append(',');
    }

    // Set all available languages.
    ewk_text_checker_spell_checking_languages_set(languages.toString().utf8().data());

    // Languages are being loaded on the idler, wait for them.
    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);
    ecore_main_loop_begin();

    // Get the languages in use.
    Eina_List* loadedLanguages = ewk_text_checker_spell_checking_languages_get();
    ASSERT_EQ(eina_list_count(loadedLanguages), eina_list_count(availableLanguages));

    i = 0;
    void* expected = 0;
    // Verify whether the loaded languages list is equal to the available languages list.
    EINA_LIST_FOREACH(loadedLanguages, listIterator, actual) {
        expected = eina_list_nth(availableLanguages, i++);
        EXPECT_STREQ(static_cast<const char*>(expected), static_cast<const char*>(actual));
    }

    // Delete the lists.
    EINA_LIST_FREE(availableLanguages, actual)
        eina_stringshare_del(static_cast<const char*>(actual));

    EINA_LIST_FREE(loadedLanguages, actual)
        eina_stringshare_del(static_cast<const char*>(actual));
}

/**
 * Here we test the following scenarios:
 *  - setting the default language,
 *  - if two arbitrarily selected dictionaries are set correctly.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_spell_checking_languages)
{
    // Set the default language.
    ewk_text_checker_spell_checking_languages_set(0);

    // Languages are being loaded on the idler, wait for them.
    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);
    ecore_main_loop_begin();

    Eina_List* loadedLanguages = ewk_text_checker_spell_checking_languages_get();
    // No dictionary is available/installed or the SPELLCHECK macro is disabled.
    if (!loadedLanguages)
        return;

    ASSERT_EQ(1, eina_list_count(loadedLanguages));

    // Delete the list.
    void* actual = 0;
    EINA_LIST_FREE(loadedLanguages, actual)
        eina_stringshare_del(static_cast<const char*>(actual));

    // Get the first and last language from installed dictionaries.
    Eina_List* availableLanguages = ewk_text_checker_spell_checking_available_languages_get();
    unsigned numberOfAvailableLanguages = eina_list_count(availableLanguages);
    // We assume that user has installed at lest two dictionaries.
    if (numberOfAvailableLanguages < 2)
        return;

    const char* firstExpected = static_cast<const char*>(eina_list_nth(availableLanguages, 0));
    const char* lastExpected = static_cast<const char*>(eina_list_data_get(eina_list_last(availableLanguages)));

    // Case sensitivity of dictionaries doesn't affect on loading the dictionaries,
    // the Enchant library will 'normalize' them.
    WTF::StringBuilder languages;
    languages.append(String(firstExpected).upper());
    languages.append(',');
    languages.append(String(lastExpected).lower());

    // Set both languages (the first and the last) from the list.
    ewk_text_checker_spell_checking_languages_set(languages.toString().utf8().data());

    timeoutTimer = ecore_timer_add(defaultTimeoutInSeconds, onTimeout, 0);
    ecore_main_loop_begin();

    loadedLanguages = ewk_text_checker_spell_checking_languages_get();
    ASSERT_EQ(2, eina_list_count(loadedLanguages));

    EXPECT_STREQ(firstExpected, static_cast<const char*>(eina_list_nth(loadedLanguages, 0)));
    EXPECT_STREQ(lastExpected, static_cast<const char*>(eina_list_nth(loadedLanguages, 1)));

    // Delete the lists.
    EINA_LIST_FREE(availableLanguages, actual)
        eina_stringshare_del(static_cast<const char*>(actual));

    EINA_LIST_FREE(loadedLanguages, actual)
        eina_stringshare_del(static_cast<const char*>(actual));
}

/**
 * Test whether the client's callbacks aren't called (if not specified).
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker)
{
    resetCallbacksExecutionStats();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    // If user doesn't specify callback functions responsible for spelling
    // the default WebKit implementation (based on the Enchant library) will be used.
    ASSERT_FALSE(callbacksExecutionStats.spellDocumentTag);
    ASSERT_FALSE(callbacksExecutionStats.spellDocumentTagClose);
    ASSERT_FALSE(callbacksExecutionStats.spellingCheck);

    // It doesn't make sense to verify others callbacks (onWordGuesses,
    // onWordLearn, onWordIgnore) as they need the context menu feature
    // which is not implemented for WK2-EFL.
}

/**
 * Test whether the client's callbacks (onSpellDocumentTag, onSpellDocumentTagClose) are called.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_unique_spell_document_tag)
{
    resetCallbacksExecutionStats();
    defaultView = webView();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ewk_text_checker_unique_spell_document_tag_get_cb_set(onSpellDocumentTag);
    ewk_text_checker_unique_spell_document_tag_close_cb_set(onSpellDocumentTagClose);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    // Check whether the callback was called.
    ASSERT_TRUE(callbacksExecutionStats.spellDocumentTag);
    // It's not possible to check whether onSpellDocumentTagClose was called here, because
    // it's invoked when WebPage is being destroyed.
    // It should be verified for example when view is freed.
}

/**
 * Test whether the client's callback (onSpellingCheck) is called when
 * the word to input field was put.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_string_spelling_check_cb_set)
{
    resetCallbacksExecutionStats();
    defaultView = webView();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ewk_text_checker_string_spelling_check_cb_set(onSpellingCheck);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    // Check whether the callback was called.
    ASSERT_TRUE(callbacksExecutionStats.spellingCheck);
}

/**
 * Test whether the client's callback (onWordGuesses) is called when
 * the context menu was shown on the misspelled word.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_word_guesses_get_cb_set)
{
    resetCallbacksExecutionStats();
    defaultView = webView();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ewk_text_checker_word_guesses_get_cb_set(onWordGuesses);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    /* FIXME:
        1) Invoke the context menu on the misspelled word (not implemented for WK2),
           the word has to be selected first.
        2) Fill the suggestion list in the onWordGuesses callback.
        3) Compare context menu suggestions to the suggestion list.
        4) Check whether the callback was called. */    
}

/**
 * Test whether the client's callback (onWordLearn) is called when
 * the context menu option "Learn spelling" was choosen.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_word_learn_cb_set)
{
    resetCallbacksExecutionStats();
    defaultView = webView();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ewk_text_checker_word_learn_cb_set(onWordLearn);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    /* FIXME:
        1) Invoke the context menu on the misspelled word (not implemented for WK2),
           the word has to be selected first.
        2) Check whether the callback was called. */
}

/**
 * Test whether the client's callback (onWordIgnore) is called when
 * the context menu option "Ignore spelling" was choosen.
 */
TEST_F(EWK2UnitTestBase, ewk_text_checker_word_ignore_cb_set)
{
    resetCallbacksExecutionStats();
    defaultView = webView();
    ewk_text_checker_continuous_spell_checking_enabled_set(true);

    ewk_text_checker_word_ignore_cb_set(onWordIgnore);

    ASSERT_TRUE(loadUrlSync(environment->urlForResource("spelling_test.html").data()));

    /* FIXME:
        1) Invoke the context menu on the misspelled word (not implemented for WK2),
           the word has to be selected first.
        2) Check whether the callback was called. */
}
