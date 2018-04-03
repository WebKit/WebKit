/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(SPELLCHECK)

#include <enchant.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

class TextCheckerEnchant {
    WTF_MAKE_NONCOPYABLE(TextCheckerEnchant); WTF_MAKE_FAST_ALLOCATED;
    friend class WTF::NeverDestroyed<TextCheckerEnchant>;
public:
    static TextCheckerEnchant& singleton();

    void ignoreWord(const String&);
    void learnWord(const String&);
    void checkSpellingOfString(const String&, int& misspellingLocation, int& misspellingLength);
    Vector<String> getGuessesForWord(const String&);
    void updateSpellCheckingLanguages(const Vector<String>& languages);
    Vector<String> loadedSpellCheckingLanguages() const;
    bool hasDictionary() const { return !m_enchantDictionaries.isEmpty(); }
    Vector<String> availableSpellCheckingLanguages() const;

private:
    TextCheckerEnchant();
    ~TextCheckerEnchant() = delete;

    void checkSpellingOfWord(const String&, int start, int end, int& misspellingLocation, int& misspellingLength);

    struct EnchantDictDeleter {
        void operator()(EnchantDict*) const;
    };

    using UniqueEnchantDict = std::unique_ptr<EnchantDict, EnchantDictDeleter>;

    EnchantBroker* m_broker;
    Vector<UniqueEnchantDict> m_enchantDictionaries;
};

} // namespace WebCore

#endif // ENABLE(SPELLCHECK)
