/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "webkitspellcheckerenchant.h"

#if ENABLE(SPELLCHECK)

#include "webkitspellchecker.h"
#include <enchant.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>

/**
 * SECTION:webkitspellcheckerenchant
 * @short_description: the default spell checking implementation for WebKitGTK+.
 *
 * #WebKitSpellCheckerEnchant is the default spell checking implementation for
 * WebKitGTK+. It uses the Enchant dictionaries installed on the system to
 * correct spelling.
 */
static EnchantBroker* broker = 0;

struct _WebKitSpellCheckerEnchantPrivate {
    GSList* enchantDicts;
};

static void webkit_spell_checker_enchant_spell_checker_interface_init(WebKitSpellCheckerInterface* interface);

G_DEFINE_TYPE_WITH_CODE(WebKitSpellCheckerEnchant, webkit_spell_checker_enchant, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(WEBKIT_TYPE_SPELL_CHECKER,
                                              webkit_spell_checker_enchant_spell_checker_interface_init))

static void createEnchantBrokerIfNeeded()
{
    if (!broker)
        broker = enchant_broker_init();
}

static void freeSpellCheckingLanguage(gpointer data, gpointer)
{
    createEnchantBrokerIfNeeded();

    enchant_broker_free_dict(broker, static_cast<EnchantDict*>(data));
}

static void webkit_spell_checker_enchant_finalize(GObject* object)
{
    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(object)->priv;

    g_slist_foreach(priv->enchantDicts, freeSpellCheckingLanguage, 0);
    g_slist_free(priv->enchantDicts);

    WEBKIT_SPELL_CHECKER_ENCHANT(object)->priv->~WebKitSpellCheckerEnchantPrivate();
}

static void webkit_spell_checker_enchant_class_init(WebKitSpellCheckerEnchantClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);

    objectClass->finalize = webkit_spell_checker_enchant_finalize;

    g_type_class_add_private(klass, sizeof(WebKitSpellCheckerEnchantPrivate));
}

static void webkit_spell_checker_enchant_init(WebKitSpellCheckerEnchant* checker)
{
    WebKitSpellCheckerEnchantPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(checker, WEBKIT_TYPE_SPELL_CHECKER_ENCHANT, WebKitSpellCheckerEnchantPrivate);
    checker->priv = priv;
    new (priv) WebKitSpellCheckerEnchantPrivate();

    priv->enchantDicts = 0;
}

static void checkSpellingOfString(WebKitSpellChecker* checker, const char* string, int* misspellingLocation, int* misspellingLength)
{
    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(checker)->priv;

    GSList* dicts = priv->enchantDicts;
    if (!dicts)
        return;

    int length = g_utf8_strlen(string, -1);

    PangoLanguage* language(pango_language_get_default());
    GOwnPtr<PangoLogAttr> attrs(g_new(PangoLogAttr, length + 1));

    // pango_get_log_attrs uses an aditional position at the end of the text.
    pango_get_log_attrs(string, -1, -1, language, attrs.get(), length + 1);

    for (int i = 0; i < length + 1; i++) {
        // We go through each character until we find an is_word_start,
        // then we get into an inner loop to find the is_word_end corresponding
        // to it.
        if (attrs.get()[i].is_word_start) {
            int start = i;
            int end = i;
            int wordLength;

            while (attrs.get()[end].is_word_end < 1)
                end++;

            wordLength = end - start;
            // Set the iterator to be at the current word end, so we don't
            // check characters twice.
            i = end;

            gchar* cstart = g_utf8_offset_to_pointer(string, start);
            gint bytes = static_cast<gint>(g_utf8_offset_to_pointer(string, end) - cstart);
            GOwnPtr<gchar> word(g_new0(gchar, bytes + 1));

            g_utf8_strncpy(word.get(), cstart, wordLength);

            for (; dicts; dicts = dicts->next) {
                EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);
                if (enchant_dict_check(dict, word.get(), wordLength)) {
                    *misspellingLocation = start;
                    *misspellingLength = wordLength;
                } else {
                    // Stop checking, this word is ok in at least one dict.
                    *misspellingLocation = -1;
                    *misspellingLength = 0;
                    break;
                }
            }
        }
    }
}

static char** getGuessesForWord(WebKitSpellChecker* checker, const char* word, const char* context)
{
    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(checker)->priv;

    GSList* dicts = priv->enchantDicts;
    char** guesses = 0;

    for (; dicts; dicts = dicts->next) {
        size_t numberOfSuggestions;
        size_t i;

        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);
        gchar** suggestions = enchant_dict_suggest(dict, word, -1, &numberOfSuggestions);

        if (numberOfSuggestions > 0) {
            if (numberOfSuggestions > 10)
                numberOfSuggestions = 10;

            guesses = static_cast<char**>(g_malloc0((numberOfSuggestions + 1) * sizeof(char*)));
            for (i = 0; i < numberOfSuggestions && i < 10; i++)
                guesses[i] = g_strdup(suggestions[i]);

            guesses[i] = 0;

            enchant_dict_free_suggestions(dict, suggestions);
        }
    }

    return guesses;
}

static void getAvailableDictionariesCallback(const char* const languageTag, const char* const, const char* const, const char* const, void* data)
{
    Vector<CString>* dicts = static_cast<Vector<CString>*>(data);

    dicts->append(languageTag);
}

static void updateSpellCheckingLanguages(WebKitSpellChecker* checker, const char* languages)
{
    GSList* spellDictionaries = 0;

    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(checker)->priv;

    createEnchantBrokerIfNeeded();

    if (languages) {
        char** langs = g_strsplit(languages, ",", -1);
        for (int i = 0; langs[i]; i++) {
            if (enchant_broker_dict_exists(broker, langs[i])) {
                EnchantDict* dict = enchant_broker_request_dict(broker, langs[i]);
                spellDictionaries = g_slist_append(spellDictionaries, dict);
            }
        }
        g_strfreev(langs);
    } else {
        const char* language = pango_language_to_string(gtk_get_default_language());
        if (enchant_broker_dict_exists(broker, language)) {
            EnchantDict* dict = enchant_broker_request_dict(broker, language);
            spellDictionaries = g_slist_append(spellDictionaries, dict);
        } else {
            // No dictionaries selected, we get one from the list.
            Vector<CString> allDictionaries;
            enchant_broker_list_dicts(broker, getAvailableDictionariesCallback, &allDictionaries);
            if (!allDictionaries.isEmpty()) {
                EnchantDict* dict = enchant_broker_request_dict(broker, allDictionaries[0].data());
                spellDictionaries = g_slist_append(spellDictionaries, dict);
            }
        }
    }
    g_slist_foreach(priv->enchantDicts, freeSpellCheckingLanguage, 0);
    g_slist_free(priv->enchantDicts);
    priv->enchantDicts = spellDictionaries;
}

static char* getAutocorrectSuggestionsForMisspelledWord(WebKitSpellChecker* checker, const char* word)
{
    return 0;
}

static void learnWord(WebKitSpellChecker* checker, const char* word)
{
    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(checker)->priv;
    GSList* dicts = priv->enchantDicts;

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_personal(dict, word, -1);
    }
}

static void ignoreWord(WebKitSpellChecker* checker, const char* word)
{
    WebKitSpellCheckerEnchantPrivate* priv = WEBKIT_SPELL_CHECKER_ENCHANT(checker)->priv;
    GSList* dicts = priv->enchantDicts;

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_session(dict, word, -1);
    }
}

static void webkit_spell_checker_enchant_spell_checker_interface_init(WebKitSpellCheckerInterface* interface)
{
    interface->check_spelling_of_string = checkSpellingOfString;
    interface->get_guesses_for_word = getGuessesForWord;
    interface->update_spell_checking_languages = updateSpellCheckingLanguages;
    interface->get_autocorrect_suggestions_for_misspelled_word = getAutocorrectSuggestionsForMisspelledWord;
    interface->learn_word = learnWord;
    interface->ignore_word = ignoreWord;
}

#endif /* ENABLE(SPELLCHECK) */

