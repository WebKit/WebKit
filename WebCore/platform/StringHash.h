/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef StringHash_h
#define StringHash_h

#include "AtomicStringImpl.h"
#include "PlatformString.h"
#include <wtf/HashTraits.h>
#include <wtf/unicode/Unicode.h>

namespace WTF {

    template<typename T> struct StrHash;

    template<> struct StrHash<WebCore::StringImpl*> {
        static unsigned hash(const WebCore::StringImpl* key) { return key->hash(); }
        static bool equal(const WebCore::StringImpl* a, const WebCore::StringImpl* b)
        {
            if (a == b)
                return true;
            if (!a || !b)
                return false;

            unsigned aLength = a->length();
            unsigned bLength = b->length();
            if (aLength != bLength)
                return false;

            const uint32_t* aChars = reinterpret_cast<const uint32_t*>(a->characters());
            const uint32_t* bChars = reinterpret_cast<const uint32_t*>(b->characters());

            unsigned halfLength = aLength >> 1;
            for (unsigned i = 0; i != halfLength; ++i)
                if (*aChars++ != *bChars++)
                    return false;

            if (aLength & 1 && *reinterpret_cast<const uint16_t*>(aChars) != *reinterpret_cast<const uint16_t*>(bChars))
                return false;

            return true;
        }
    };
    
    template<> struct StrHash<WebCore::AtomicStringImpl*> : public StrHash<WebCore::StringImpl*> { };

    template<> struct StrHash<RefPtr<WebCore::StringImpl> > {
        static unsigned hash(const RefPtr<WebCore::StringImpl>& key) { return key->hash(); }
        static bool equal(const RefPtr<WebCore::StringImpl>& a, const RefPtr<WebCore::StringImpl>& b)
        {
            return StrHash<WebCore::StringImpl*>::equal(a.get(), b.get());
        }
    };

    template<> struct StrHash<WebCore::String> {
        static unsigned hash(const WebCore::String& key) { return key.impl()->hash(); }
        static bool equal(const WebCore::String& a, const WebCore::String& b)
        {
            return StrHash<WebCore::StringImpl*>::equal(a.impl(), b.impl());
        }
    };

    template<typename T> struct CaseInsensitiveHash;

    template<> class CaseInsensitiveHash<WebCore::StringImpl*> {
    private:
        // Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
        static const unsigned PHI = 0x9e3779b9U;
    public:
        // Paul Hsieh's SuperFastHash
        // http://www.azillionmonkeys.com/qed/hash.html
        static unsigned hash(const WebCore::StringImpl* str)
        {
            unsigned l = str->length();
            const UChar* s = str->characters();
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop
            for (; l > 0; l--) {
                hash += WTF::Unicode::foldCase(s[0]);
                tmp = (WTF::Unicode::foldCase(s[1]) << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += WTF::Unicode::foldCase(s[0]);
                hash ^= hash << 11;
                hash += hash >> 17;
            }
            
            // Force "avalanching" of final 127 bits
            hash ^= hash << 3;
            hash += hash >> 5;
            hash ^= hash << 2;
            hash += hash >> 15;
            hash ^= hash << 10;
            
            // this avoids ever returning a hash code of 0, since that is used to
            // signal "hash not computed yet", using a value that is likely to be
            // effectively the same as 0 when the low bits are masked
            if (hash == 0)
                hash = 0x80000000;
            
            return hash;
        }
        
        static unsigned hash(const char* str, unsigned length)
        {
            // This hash is designed to work on 16-bit chunks at a time. But since the normal case
            // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
            // were 16-bit chunks, which will give matching results.

            unsigned l = length;
            const char* s = str;
            uint32_t hash = PHI;
            uint32_t tmp;
            
            int rem = l & 1;
            l >>= 1;
            
            // Main loop
            for (; l > 0; l--) {
                hash += WTF::Unicode::foldCase(s[0]);
                tmp = (WTF::Unicode::foldCase(s[1]) << 11) ^ hash;
                hash = (hash << 16) ^ tmp;
                s += 2;
                hash += hash >> 11;
            }
            
            // Handle end case
            if (rem) {
                hash += WTF::Unicode::foldCase(s[0]);
                hash ^= hash << 11;
                hash += hash >> 17;
            }
            
            // Force "avalanching" of final 127 bits
            hash ^= hash << 3;
            hash += hash >> 5;
            hash ^= hash << 2;
            hash += hash >> 15;
            hash ^= hash << 10;
            
            // this avoids ever returning a hash code of 0, since that is used to
            // signal "hash not computed yet", using a value that is likely to be
            // effectively the same as 0 when the low bits are masked
            if (hash == 0)
                hash = 0x80000000;
            
            return hash;
        }
        
        static bool equal(const WebCore::StringImpl* a, const WebCore::StringImpl* b)
        {
            if (a == b)
                return true;
            if (!a || !b)
                return false;
            unsigned length = a->length();
            if (length != b->length())
                return false;
            return WTF::Unicode::umemcasecmp(a->characters(), b->characters(), length) == 0;
        }
    };

    template<> struct CaseInsensitiveHash<WebCore::AtomicStringImpl*> : public CaseInsensitiveHash<WebCore::StringImpl*> { };

    template<> struct CaseInsensitiveHash<RefPtr<WebCore::StringImpl> > {
        static unsigned hash(const RefPtr<WebCore::StringImpl>& key) 
        {
            return CaseInsensitiveHash<WebCore::StringImpl*>::hash(key.get());
        }

        static bool equal(const RefPtr<WebCore::StringImpl>& a, const RefPtr<WebCore::StringImpl>& b)
        {
            return CaseInsensitiveHash<WebCore::StringImpl*>::equal(a.get(), b.get());
        }
    };

    template<> struct CaseInsensitiveHash<WebCore::String> {
        static unsigned hash(const WebCore::String& key)
        {
            return CaseInsensitiveHash<WebCore::StringImpl*>::hash(key.impl());
        }
        static bool equal(const WebCore::String& a, const WebCore::String& b)
        {
            return CaseInsensitiveHash<WebCore::StringImpl*>::equal(a.impl(), b.impl());
        }
    };

    // store WebCore::String as StringImpl*

    template<> struct HashTraits<WebCore::String> : GenericHashTraits<WebCore::String> {
        typedef HashTraits<WebCore::StringImpl*>::StorageTraits StorageTraits;
        typedef StorageTraits::TraitType StorageType;
        static const bool emptyValueIsZero = true;
        static const bool needsRef = true;
        
        typedef union { 
            WebCore::StringImpl* m_p; 
            StorageType m_s; 
        } UnionType;

        static void ref(const StorageType& s) { ref(reinterpret_cast<const UnionType*>(&s)->m_p); }
        static void deref(const StorageType& s) { deref(reinterpret_cast<const UnionType*>(&s)->m_p); }
        
        static void ref(const WebCore::StringImpl* str) { if (str) const_cast<WebCore::StringImpl*>(str)->ref(); }
        static void deref(const WebCore::StringImpl* str) { if (str) const_cast<WebCore::StringImpl*>(str)->deref(); }
    };

    // share code between StringImpl*, RefPtr<StringImpl>, and String

    template<> struct HashKeyStorageTraits<StrHash<RefPtr<WebCore::StringImpl> >, HashTraits<RefPtr<WebCore::StringImpl> > > {
        typedef StrHash<WebCore::StringImpl*> Hash;
        typedef HashTraits<WebCore::StringImpl*> Traits;
    };
    template<> struct HashKeyStorageTraits<StrHash<WebCore::String>, HashTraits<WebCore::String> > {
        typedef StrHash<WebCore::StringImpl*> Hash;
        typedef HashTraits<WebCore::StringImpl*> Traits;
    };

    template<> struct HashKeyStorageTraits<CaseInsensitiveHash<RefPtr<WebCore::StringImpl> >, HashTraits<RefPtr<WebCore::StringImpl> > > {
        typedef CaseInsensitiveHash<WebCore::StringImpl*> Hash;
        typedef HashTraits<WebCore::StringImpl*> Traits;
    };
    template<> struct HashKeyStorageTraits<CaseInsensitiveHash<WebCore::String>, HashTraits<WebCore::String> > {
        typedef CaseInsensitiveHash<WebCore::StringImpl*> Hash;
        typedef HashTraits<WebCore::StringImpl*> Traits;
    };

}

using WTF::CaseInsensitiveHash;

#endif
