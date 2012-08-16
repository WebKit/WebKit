/*
 *  Copyright (C) 2012 Igalia S.L.
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
#include "TextCheckerEnchant.h"

#if ENABLE(SPELLCHECK)

#include <pango/pango.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

static const size_t maximumNumberOfSuggestions = 10;

static void enchantDictDescribeCallback(const char* const languageTag, const char* const, const char* const, const char* const, void* data)
{
    Vector<CString>* dictionaries = static_cast<Vector<CString>*>(data);
    dictionaries->append(languageTag);
}

static bool wordEndIsAContractionApostrophe(const char* string, long offset)
{
    if (g_utf8_get_char(g_utf8_offset_to_pointer(string, offset)) != g_utf8_get_char("'"))
        return false;

    // If this is the last character in the string, it cannot be the apostrophe part of a contraction.
    if (offset == g_utf8_strlen(string, -1))
        return false;

    return g_unichar_isalpha(g_utf8_get_char(g_utf8_offset_to_pointer(string, offset + 1)));
}

TextCheckerEnchant::TextCheckerEnchant()
    : m_broker(enchant_broker_init())
    , m_enchantDictionaries(0)
{
}

TextCheckerEnchant::~TextCheckerEnchant()
{
    if (!m_broker)
        return;

    freeEnchantBrokerDictionaries();
    enchant_broker_free(m_broker);
}

void TextCheckerEnchant::ignoreWord(const String& word)
{
    for (Vector<EnchantDict*>::const_iterator iter = m_enchantDictionaries.begin(); iter != m_enchantDictionaries.end(); ++iter)
        enchant_dict_add_to_session(*iter, word.utf8().data(), -1);
}

void TextCheckerEnchant::learnWord(const String& word)
{
    for (Vector<EnchantDict*>::const_iterator iter = m_enchantDictionaries.begin(); iter != m_enchantDictionaries.end(); ++iter)
        enchant_dict_add_to_personal(*iter, word.utf8().data(), -1);
}

void TextCheckerEnchant::checkSpellingOfString(const String& string, int& misspellingLocation, int& misspellingLength)
{
    if (m_enchantDictionaries.isEmpty())
        return;
    Vector<EnchantDict*>::const_iterator dictIter = m_enchantDictionaries.begin();

    PangoLanguage* language(pango_language_get_default());
    size_t numberOfCharacters = string.length();
    GOwnPtr<PangoLogAttr> attrs(g_new(PangoLogAttr, numberOfCharacters + 1));

    CString utf8String = string.utf8();
    const char* cString = utf8String.data();

    // pango_get_log_attrs uses an aditional position at the end of the text.
    pango_get_log_attrs(cString, -1, -1, language, attrs.get(), numberOfCharacters + 1);

    for (size_t i = 0; i < numberOfCharacters + 1; i++) {
        // We go through each character until we find an is_word_start,
        // then we get into an inner loop to find the is_word_end corresponding
        // to it.
        if (attrs.get()[i].is_word_start) {
            int start = i;
            int end = i;
            int wordLength;

            while (attrs.get()[end].is_word_end < 1  || wordEndIsAContractionApostrophe(cString, end))
                end++;

            wordLength = end - start;
            // Set the iterator to be at the current word end, so we don't
            // check characters twice.
            i = end;

            gchar* cstart = g_utf8_offset_to_pointer(cString, start);
            gint bytes = static_cast<gint>(g_utf8_offset_to_pointer(cString, end) - cstart);
            GOwnPtr<gchar> word(g_new0(gchar, bytes + 1));

            g_utf8_strncpy(word.get(), cstart, wordLength);

            for (; dictIter != m_enchantDictionaries.end(); ++dictIter) {
                if (enchant_dict_check(*dictIter, word.get(), bytes)) {
                    misspellingLocation = start;
                    misspellingLength = wordLength;
                } else {
                    // Stop checking, this word is ok in at least one dict.
                    misspellingLocation = -1;
                    misspellingLength = 0;
                    break;
                }
            }
        }
    }
}

Vector<String> TextCheckerEnchant::getGuessesForWord(const String& word)
{
    Vector<String> guesses;
    if (m_enchantDictionaries.isEmpty())
        return guesses;

    for (Vector<EnchantDict*>::const_iterator iter = m_enchantDictionaries.begin(); iter != m_enchantDictionaries.end(); ++iter) {
        size_t numberOfSuggestions;
        size_t i;

        gchar** suggestions = enchant_dict_suggest(*iter, word.utf8().data(), -1, &numberOfSuggestions);
        if (numberOfSuggestions <= 0)
            continue;

        if (numberOfSuggestions > maximumNumberOfSuggestions)
            numberOfSuggestions = maximumNumberOfSuggestions;

        for (i = 0; i < numberOfSuggestions; i++)
            guesses.append(String(suggestions[i]));

        enchant_dict_free_suggestions(*iter, suggestions);
    }

    return guesses;
}

void TextCheckerEnchant::updateSpellCheckingLanguages(const String& languages)
{
    Vector<EnchantDict*> spellDictionaries;

    if (!languages.isEmpty()) {
        Vector<String> languagesVector;
        languages.split(static_cast<UChar>(','), languagesVector);
        for (Vector<String>::const_iterator iter = languagesVector.begin(); iter != languagesVector.end(); ++iter) {
            CString currentLanguage = iter->utf8();
            if (enchant_broker_dict_exists(m_broker, currentLanguage.data())) {
                EnchantDict* dict = enchant_broker_request_dict(m_broker, currentLanguage.data());
                spellDictionaries.append(dict);
            }
        }
    } else {
        const char* language = pango_language_to_string(pango_language_get_default());
        if (enchant_broker_dict_exists(m_broker, language)) {
            EnchantDict* dict = enchant_broker_request_dict(m_broker, language);
            spellDictionaries.append(dict);
        } else {
            // No dictionaries selected, we get one from the list.
            Vector<CString> allDictionaries;
            enchant_broker_list_dicts(m_broker, enchantDictDescribeCallback, &allDictionaries);
            if (!allDictionaries.isEmpty()) {
                EnchantDict* dict = enchant_broker_request_dict(m_broker, allDictionaries[0].data());
                spellDictionaries.append(dict);
            }
        }
    }
    freeEnchantBrokerDictionaries();
    m_enchantDictionaries = spellDictionaries;
}

String TextCheckerEnchant::getSpellCheckingLanguages()
{
    if (m_enchantDictionaries.isEmpty())
        return String();

    // Get a Vector<CString> with the list of languages in use.
    Vector<CString> currentDictionaries;
    for (Vector<EnchantDict*>::const_iterator iter = m_enchantDictionaries.begin(); iter != m_enchantDictionaries.end(); ++iter)
        enchant_dict_describe(*iter, enchantDictDescribeCallback, &currentDictionaries);

    // Build the result String;
    StringBuilder builder;
    for (Vector<CString>::const_iterator iter = currentDictionaries.begin(); iter != currentDictionaries.end(); ++iter) {
        if (iter != currentDictionaries.begin())
            builder.append(",");
        builder.append(String::fromUTF8(iter->data()));
    }
    return builder.toString();
}

void TextCheckerEnchant::freeEnchantBrokerDictionaries()
{
    for (Vector<EnchantDict*>::const_iterator iter = m_enchantDictionaries.begin(); iter != m_enchantDictionaries.end(); ++iter)
        enchant_broker_free_dict(m_broker, *iter);
}

#endif // ENABLE(SPELLCHECK)

