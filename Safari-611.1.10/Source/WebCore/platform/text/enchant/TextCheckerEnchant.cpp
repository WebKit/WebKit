/*
 *  Copyright (C) 2012 Igalia S.L.
 *  Copyright (C) 2012 Samsung Electronics
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

#include <unicode/ubrk.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/TextBreakIterator.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

TextCheckerEnchant& TextCheckerEnchant::singleton()
{
    static NeverDestroyed<TextCheckerEnchant> textChecker;
    return textChecker;
}

void TextCheckerEnchant::EnchantDictDeleter::operator()(EnchantDict* dictionary) const
{
    enchant_broker_free_dict(TextCheckerEnchant::singleton().m_broker, dictionary);
}

TextCheckerEnchant::TextCheckerEnchant()
    : m_broker(enchant_broker_init())
{
}

void TextCheckerEnchant::ignoreWord(const String& word)
{
    auto utf8Word = word.utf8();
    for (auto& dictionary : m_enchantDictionaries)
        enchant_dict_add_to_session(dictionary.get(), utf8Word.data(), utf8Word.length());
}

void TextCheckerEnchant::learnWord(const String& word)
{
    auto utf8Word = word.utf8();
    for (auto& dictionary : m_enchantDictionaries)
        enchant_dict_add(dictionary.get(), utf8Word.data(), utf8Word.length());
}

void TextCheckerEnchant::checkSpellingOfWord(const String& word, int start, int end, int& misspellingLocation, int& misspellingLength)
{
    CString string = word.substring(start, end - start).utf8();

    for (auto& dictionary : m_enchantDictionaries) {
        if (!enchant_dict_check(dictionary.get(), string.data(), string.length())) {
            // Stop checking, this word is ok in at least one dict.
            misspellingLocation = -1;
            misspellingLength = 0;
            return;
        }
    }

    misspellingLocation = start;
    misspellingLength = end - start;
}

void TextCheckerEnchant::checkSpellingOfString(const String& string, int& misspellingLocation, int& misspellingLength)
{
    // Assume that the words in the string are spelled correctly.
    misspellingLocation = -1;
    misspellingLength = 0;

    if (!hasDictionary())
        return;

    UBreakIterator* iter = wordBreakIterator(string);
    if (!iter)
        return;

    int start = ubrk_first(iter);
    for (int end = ubrk_next(iter); end != UBRK_DONE; end = ubrk_next(iter)) {
        if (isWordTextBreak(iter)) {
            checkSpellingOfWord(string, start, end, misspellingLocation, misspellingLength);
            // Stop checking the next words If the current word is misspelled, to do not overwrite its misspelled location and length.
            if (misspellingLength)
                return;
        }
        start = end;
    }
}

Vector<String> TextCheckerEnchant::getGuessesForWord(const String& word)
{
    if (!hasDictionary())
        return { };

    static const size_t maximumNumberOfSuggestions = 10;

    Vector<String> guesses;
    auto utf8Word = word.utf8();
    for (auto& dictionary : m_enchantDictionaries) {
        size_t numberOfSuggestions;
        size_t i;

        char** suggestions = enchant_dict_suggest(dictionary.get(), utf8Word.data(), utf8Word.length(), &numberOfSuggestions);
        if (numberOfSuggestions <= 0)
            continue;

        if (numberOfSuggestions > maximumNumberOfSuggestions)
            numberOfSuggestions = maximumNumberOfSuggestions;

        for (i = 0; i < numberOfSuggestions; i++)
            guesses.append(String::fromUTF8(suggestions[i]));

        enchant_dict_free_string_list(dictionary.get(), suggestions);
    }

    return guesses;
}

void TextCheckerEnchant::updateSpellCheckingLanguages(const Vector<String>& languages)
{
    Vector<UniqueEnchantDict> spellDictionaries;
    if (!languages.isEmpty()) {
        for (auto& language : languages) {
            CString currentLanguage = language.utf8();
            if (enchant_broker_dict_exists(m_broker, currentLanguage.data())) {
                if (auto* dict = enchant_broker_request_dict(m_broker, currentLanguage.data()))
                    spellDictionaries.append(dict);
            }
        }
    } else {
        // Languages are not specified by user, try to get default language.
        CString language = defaultLanguage().utf8();
        if (enchant_broker_dict_exists(m_broker, language.data())) {
            if (auto* dict = enchant_broker_request_dict(m_broker, language.data()))
                spellDictionaries.append(dict);
        } else {
            // No dictionaries selected, we get the first one from the list.
            CString dictLanguage;
            enchant_broker_list_dicts(m_broker, [](const char* const languageTag, const char* const, const char* const, const char* const, void* data) {
                auto* dictLanguage = static_cast<CString*>(data);
                if (dictLanguage->isNull())
                    *dictLanguage = languageTag;
            }, &dictLanguage);
            if (!dictLanguage.isNull()) {
                if (auto* dict = enchant_broker_request_dict(m_broker, dictLanguage.data()))
                    spellDictionaries.append(dict);
            }
        }
    }
    m_enchantDictionaries = WTFMove(spellDictionaries);
}

Vector<String> TextCheckerEnchant::loadedSpellCheckingLanguages() const
{
    if (!hasDictionary())
        return { };

    Vector<String> languages;
    for (auto& dictionary : m_enchantDictionaries) {
        enchant_dict_describe(dictionary.get(), [](const char* const languageTag, const char* const, const char* const, const char* const, void* data) {
            auto* languages = static_cast<Vector<String>*>(data);
            languages->append(String::fromUTF8(languageTag));
        }, &languages);
    }
    return languages;
}

Vector<String> TextCheckerEnchant::availableSpellCheckingLanguages() const
{
    Vector<String> languages;
    enchant_broker_list_dicts(m_broker, [](const char* const languageTag, const char* const, const char* const, const char* const, void* data) {
        auto* languages = static_cast<Vector<String>*>(data);
        languages->append(String::fromUTF8(languageTag));
    }, &languages);
    return languages;
}

} // namespace WebCore

#endif // ENABLE(SPELLCHECK)

