/*
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2009 Diego Escalante Urrelo <diegoe@gnome.org>
 *  Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *  Copyright (C) 2009, 2010 Igalia S.L.
 *  Copyright (C) 2010, Martin Robinson <mrobinson@webkit.org>
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
#include "TextCheckerClientEnchant.h"

#include "NotImplemented.h"
#include "webkitwebsettingsprivate.h"
#include "webkitwebviewprivate.h"
#include <enchant.h>
#include <glib.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

EnchantBroker* TextCheckerClientEnchant::broker = 0;

TextCheckerClientEnchant::TextCheckerClientEnchant(WebKitWebView* webView)
    : m_webView(webView)
    , m_enchantDicts(0)
{
}

TextCheckerClientEnchant::~TextCheckerClientEnchant()
{
    g_slist_foreach(m_enchantDicts, freeSpellCheckingLanguage, 0);
    g_slist_free(m_enchantDicts);
}

void TextCheckerClientEnchant::ignoreWordInSpellDocument(const String& text)
{
    GSList* dicts = m_enchantDicts;

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_session(dict, text.utf8().data(), -1);
    }
}

void TextCheckerClientEnchant::learnWord(const String& text)
{
    GSList* dicts = m_enchantDicts;

    for (; dicts; dicts = dicts->next) {
        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);

        enchant_dict_add_to_personal(dict, text.utf8().data(), -1);
    }
}

void TextCheckerClientEnchant::checkSpellingOfString(const UChar* text, int length, int* misspellingLocation, int* misspellingLength)
{
    GSList* dicts = m_enchantDicts;
    if (!dicts)
        return;

    GOwnPtr<gchar> utf8Text(g_utf16_to_utf8(const_cast<gunichar2*>(text), length, 0, 0, 0));
    int utf8Length = g_utf8_strlen(utf8Text.get(), -1);

    PangoLanguage* language(pango_language_get_default());
    GOwnPtr<PangoLogAttr> attrs(g_new(PangoLogAttr, utf8Length + 1));

    // pango_get_log_attrs uses an aditional position at the end of the text.
    pango_get_log_attrs(utf8Text.get(), -1, -1, language, attrs.get(), utf8Length + 1);

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

            gchar* cstart = g_utf8_offset_to_pointer(utf8Text.get(), start);
            gint bytes = static_cast<gint>(g_utf8_offset_to_pointer(utf8Text.get(), end) - cstart);
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

String TextCheckerClientEnchant::getAutoCorrectSuggestionForMisspelledWord(const String& inputWord)
{
    // This method can be implemented using customized algorithms for the particular browser.
    // Currently, it computes an empty string.
    return String();
}

void TextCheckerClientEnchant::checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void TextCheckerClientEnchant::getGuessesForWord(const String& word, const String& context, WTF::Vector<String>& guesses)
{
    GSList* dicts = m_enchantDicts;
    guesses.clear();

    for (; dicts; dicts = dicts->next) {
        size_t numberOfSuggestions;
        size_t i;

        EnchantDict* dict = static_cast<EnchantDict*>(dicts->data);
        gchar** suggestions = enchant_dict_suggest(dict, word.utf8().data(), -1, &numberOfSuggestions);

        for (i = 0; i < numberOfSuggestions && i < 10; i++)
            guesses.append(String::fromUTF8(suggestions[i]));

        if (numberOfSuggestions > 0)
            enchant_dict_free_suggestions(dict, suggestions);
    }
}

static void getAvailableDictionariesCallback(const char* const languageTag, const char* const, const char* const, const char* const, void* data)
{
    Vector<CString>* dicts = static_cast<Vector<CString>*>(data);

    dicts->append(languageTag);
}

void TextCheckerClientEnchant::updateSpellCheckingLanguage(const char* spellCheckingLanguages)
{
    EnchantDict* dict;
    GSList* spellDictionaries = 0;

    if (!broker)
        broker = enchant_broker_init();

    if (spellCheckingLanguages) {
        char** langs = g_strsplit(spellCheckingLanguages, ",", -1);
        for (int i = 0; langs[i]; i++) {
            if (enchant_broker_dict_exists(broker, langs[i])) {
                dict = enchant_broker_request_dict(broker, langs[i]);
                spellDictionaries = g_slist_append(spellDictionaries, dict);
            }
        }
        g_strfreev(langs);
    } else {
        const char* language = pango_language_to_string(gtk_get_default_language());
        if (enchant_broker_dict_exists(broker, language)) {
            dict = enchant_broker_request_dict(broker, language);
            spellDictionaries = g_slist_append(spellDictionaries, dict);
        } else {
            // No dictionaries selected, we get one from the list
            Vector<CString> allDictionaries;
            enchant_broker_list_dicts(broker, getAvailableDictionariesCallback, &allDictionaries);
            if (!allDictionaries.isEmpty()) {
                dict = enchant_broker_request_dict(broker, allDictionaries[0].data());
                spellDictionaries = g_slist_append(spellDictionaries, dict);
            }
        }
    }
    g_slist_foreach(m_enchantDicts, freeSpellCheckingLanguage, 0);
    g_slist_free(m_enchantDicts);
    m_enchantDicts = spellDictionaries;
}

void TextCheckerClientEnchant::freeSpellCheckingLanguage(gpointer data, gpointer)
{
    if (!broker)
        broker = enchant_broker_init();

    EnchantDict* dict = static_cast<EnchantDict*>(data);
    enchant_broker_free_dict(broker, dict);
}

}
