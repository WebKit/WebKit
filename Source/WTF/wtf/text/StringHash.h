/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/CompactPtr.h>
#include <wtf/HashTraits.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringHasher.h>
#include <wtf/text/StringView.h>

namespace WTF {

    inline bool HashTraits<String>::isEmptyValue(const String& value)
    {
        return value.isNull();
    }

    inline void HashTraits<String>::customDeleteBucket(String& value)
    {
        // See unique_ptr's customDeleteBucket() for an explanation.
        ASSERT(!isDeletedValue(value));
        String valueToBeDestroyed = WTFMove(value);
        constructDeletedValue(value);
    }

    // The hash() functions on StringHash and ASCIICaseInsensitiveHash do not support
    // null strings. get(), contains(), and add() on HashMap<String,..., StringHash>
    // cause a null-pointer dereference when passed null strings.

    // FIXME: We should really figure out a way to put the computeHash function that's
    // currently a member function of StringImpl into this file so we can be a little
    // closer to having all the nearly-identical hash functions in one place.

    struct StringHash {
        static unsigned hash(const StringImpl* key) { return key->hash(); }
        static inline bool equal(const StringImpl* a, const StringImpl* b)
        {
            return WTF::equal(*a, *b);
        }

        static unsigned hash(const RefPtr<StringImpl>& key) { return key->hash(); }
        static unsigned hash(const PackedPtr<StringImpl>& key) { return key->hash(); }
        static unsigned hash(const CompactPtr<StringImpl>& key) { return key->hash(); }
        static bool equal(const RefPtr<StringImpl>& a, const RefPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }
        static bool equal(const RefPtr<StringImpl>& a, const StringImpl* b)
        {
            return equal(a.get(), b);
        }
        static bool equal(const StringImpl* a, const RefPtr<StringImpl>& b)
        {
            return equal(a, b.get());
        }

        static bool equal(const PackedPtr<StringImpl>& a, const PackedPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }
        static bool equal(const PackedPtr<StringImpl>& a, const StringImpl* b)
        {
            return equal(a.get(), b);
        }
        static bool equal(const StringImpl* a, const PackedPtr<StringImpl>& b)
        {
            return equal(a, b.get());
        }

        static bool equal(const CompactPtr<StringImpl>& a, const CompactPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }
        static bool equal(const CompactPtr<StringImpl>& a, const StringImpl* b)
        {
            return equal(a.get(), b);
        }
        static bool equal(const StringImpl* a, const CompactPtr<StringImpl>& b)
        {
            return equal(a, b.get());
        }

        static unsigned hash(const String& key) { return key.impl()->hash(); }
        static bool equal(const String& a, const String& b)
        {
            return equal(a.impl(), b.impl());
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = false;
        static constexpr bool hasHashInValue = true;
    };

    struct ASCIICaseInsensitiveHash {
        struct FoldCase {
            template<typename T>
            static inline UChar convert(T character)
            {
                return toASCIILower(character);
            }
        };

        template<typename CharacterType>
        static unsigned hash(std::span<const CharacterType> characters)
        {
            return StringHasher::computeHashAndMaskTop8Bits<CharacterType, FoldCase>(characters);
        }

        static unsigned hash(const StringImpl& string)
        {
            if (string.is8Bit())
                return hash(string.span8());
            return hash(string.span16());
        }
        static unsigned hash(const StringImpl* string)
        {
            ASSERT(string);
            return hash(*string);
        }
        
        static inline bool equal(const StringImpl& a, const StringImpl& b)
        {
            return equalIgnoringASCIICase(a, b);
        }
        static inline bool equal(const StringImpl* a, const StringImpl* b)
        {
            ASSERT(a);
            ASSERT(b);
            return equal(*a, *b);
        }

        static unsigned hash(const RefPtr<StringImpl>& key) 
        {
            return hash(key.get());
        }

        static bool equal(const RefPtr<StringImpl>& a, const RefPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }

        static unsigned hash(const PackedPtr<StringImpl>& key) 
        {
            return hash(key.get());
        }

        static bool equal(const PackedPtr<StringImpl>& a, const PackedPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }

        static unsigned hash(const CompactPtr<StringImpl>& key)
        {
            return hash(key.get());
        }

        static bool equal(const CompactPtr<StringImpl>& a, const CompactPtr<StringImpl>& b)
        {
            return equal(a.get(), b.get());
        }

        static unsigned hash(const String& key)
        {
            return hash(key.impl());
        }
        static unsigned hash(const AtomString& key)
        {
            return hash(key.impl());
        }
        static bool equal(const String& a, const String& b)
        {
            return equal(a.impl(), b.impl());
        }
        static bool equal(const AtomString& a, const AtomString& b)
        {
            // FIXME: Is the "a == b" here a helpful optimization?
            // It makes all cases where the strings are not identical slightly slower.
            return a == b || equal(a.impl(), b.impl());
        }

        static constexpr bool safeToCompareToEmptyOrDeleted = false;
    };

    // This hash can be used in cases where the key is a hash of a string, but we don't
    // want to store the string. It's not really specific to string hashing, but all our
    // current uses of it are for strings.
    struct AlreadyHashed : IntHash<unsigned> {
        static unsigned hash(unsigned key) { return key; }

        // To use a hash value as a key for a hash table, we need to eliminate the
        // "deleted" value, which is negative one. That could be done by changing
        // the string hash function to never generate negative one, but this works
        // and is still relatively efficient.
        static unsigned avoidDeletedValue(unsigned hash)
        {
            ASSERT(hash);
            unsigned newHash = hash | (!(hash + 1) << 31);
            ASSERT(newHash);
            ASSERT(newHash != 0xFFFFFFFF);
            return newHash;
        }
    };

    struct StringViewHashTranslator {
        static unsigned hash(StringView key)
        {
            return key.hash();
        }

        static bool equal(const String& a, StringView b)
        {
            return a == b;
        }

        static void translate(String& location, StringView view, unsigned hash)
        {
            location = view.toString();
            location.impl()->setHash(hash);
        }
    };

    // FIXME: Find a way to incorporate this functionality into ASCIICaseInsensitiveHash and allow
    // a HashMap whose keys are type String to perform operations when given a key of type StringView.
    struct ASCIICaseInsensitiveStringViewHashTranslator {
        static unsigned hash(StringView key)
        {
            if (key.is8Bit())
                return ASCIICaseInsensitiveHash::hash(key.span8());
            return ASCIICaseInsensitiveHash::hash(key.span16());
        }

        static bool equal(const String& a, StringView b)
        {
            return equalIgnoringASCIICaseCommon(a, b);
        }

        static void translate(String& location, StringView view, unsigned)
        {
            location = view.toString();
        }
    };

    struct HashTranslatorASCIILiteral {
        static unsigned hash(ASCIILiteral literal)
        {
            return StringHasher::computeHashAndMaskTop8Bits(literal.span8());
        }

        static bool equal(const String& a, ASCIILiteral b)
        {
            return a == b;
        }

        static void translate(String& location, ASCIILiteral literal, unsigned hash)
        {
            location = literal;
            location.impl()->setHash(hash);
        }
    };

    struct HashTranslatorASCIILiteralCaseInsensitive {
        static unsigned hash(ASCIILiteral key)
        {
            return ASCIICaseInsensitiveHash::hash(key.span8());
        }

        static bool equal(const String& a, ASCIILiteral b)
        {
            return equalIgnoringASCIICase(a, b);
        }
    };

    template<> struct DefaultHash<StringImpl*> : StringHash { };
    template<> struct DefaultHash<RefPtr<StringImpl>> : StringHash { };
    template<> struct DefaultHash<PackedPtr<StringImpl>> : StringHash { };
    template<> struct DefaultHash<CompactPtr<StringImpl>> : StringHash { };
    template<> struct DefaultHash<String> : StringHash { };
}

using WTF::ASCIICaseInsensitiveHash;
using WTF::ASCIICaseInsensitiveStringViewHashTranslator;
using WTF::AlreadyHashed;
using WTF::HashTranslatorASCIILiteral;
using WTF::HashTranslatorASCIILiteralCaseInsensitive;
using WTF::StringHash;
using WTF::StringViewHashTranslator;
