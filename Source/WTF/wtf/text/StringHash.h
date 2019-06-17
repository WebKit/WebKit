/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved
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

#include <wtf/HashTraits.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringHasher.h>

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
        static unsigned hash(StringImpl* key) { return key->hash(); }
        static inline bool equal(const StringImpl* a, const StringImpl* b)
        {
            return WTF::equal(*a, *b);
        }

        static unsigned hash(const RefPtr<StringImpl>& key) { return key->hash(); }
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

        static unsigned hash(const String& key) { return key.impl()->hash(); }
        static bool equal(const String& a, const String& b)
        {
            return equal(a.impl(), b.impl());
        }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct ASCIICaseInsensitiveHash {
        template<typename T>
        struct FoldCase {
            static inline UChar convert(T character)
            {
                return toASCIILower(character);
            }
        };

        static unsigned hash(const UChar* data, unsigned length)
        {
            return StringHasher::computeHashAndMaskTop8Bits<UChar, FoldCase<UChar>>(data, length);
        }

        static unsigned hash(StringImpl& string)
        {
            if (string.is8Bit())
                return hash(string.characters8(), string.length());
            return hash(string.characters16(), string.length());
        }
        static unsigned hash(StringImpl* string)
        {
            ASSERT(string);
            return hash(*string);
        }

        static unsigned hash(const LChar* data, unsigned length)
        {
            return StringHasher::computeHashAndMaskTop8Bits<LChar, FoldCase<LChar>>(data, length);
        }

        static inline unsigned hash(const char* data, unsigned length)
        {
            return hash(reinterpret_cast<const LChar*>(data), length);
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

        static const bool safeToCompareToEmptyOrDeleted = false;
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

    // FIXME: Find a way to incorporate this functionality into ASCIICaseInsensitiveHash and allow
    // a HashMap whose keys are type String to perform operations when given a key of type StringView.
    struct ASCIICaseInsensitiveStringViewHashTranslator {
        static unsigned hash(StringView key)
        {
            if (key.is8Bit())
                return ASCIICaseInsensitiveHash::hash(key.characters8(), key.length());
            return ASCIICaseInsensitiveHash::hash(key.characters16(), key.length());
        }

        static bool equal(const String& a, StringView b)
        {
            return equalIgnoringASCIICaseCommon(a, b);
        }
    };

}

using WTF::ASCIICaseInsensitiveHash;
using WTF::ASCIICaseInsensitiveStringViewHashTranslator;
using WTF::AlreadyHashed;
using WTF::StringHash;
