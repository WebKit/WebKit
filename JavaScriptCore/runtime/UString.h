/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2009 Google Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef UString_h
#define UString_h

#include "Collector.h"
#include <stdint.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/CrossThreadRefCounted.h>
#include <wtf/OwnFastMallocPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/PtrAndFlags.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace JSC {

    using WTF::PlacementNewAdoptType;
    using WTF::PlacementNewAdopt;

    class IdentifierTable;
  
    class CString {
    public:
        CString()
            : m_length(0)
            , m_data(0)
        {
        }

        CString(const char*);
        CString(const char*, size_t);
        CString(const CString&);

        ~CString();

        static CString adopt(char*, size_t); // buffer should be allocated with new[].

        CString& append(const CString&);
        CString& operator=(const char* c);
        CString& operator=(const CString&);
        CString& operator+=(const CString& c) { return append(c); }

        size_t size() const { return m_length; }
        const char* c_str() const { return m_data; }

    private:
        size_t m_length;
        char* m_data;
    };

    typedef Vector<char, 32> CStringBuffer;

    class UString {
        friend class JIT;

    public:
        typedef CrossThreadRefCounted<OwnFastMallocPtr<UChar> > SharedUChar;
        struct BaseString;
        struct Rep : Noncopyable {
            friend class JIT;

            static PassRefPtr<Rep> create(UChar* buffer, int length)
            {
                return adoptRef(new BaseString(buffer, length));
            }

            static PassRefPtr<Rep> createEmptyBuffer(size_t size)
            {
                // Guard against integer overflow
                if (size < (std::numeric_limits<size_t>::max() / sizeof(UChar))) {
                    if (void * buf = tryFastMalloc(size * sizeof(UChar)))
                        return adoptRef(new BaseString(static_cast<UChar*>(buf), 0, size));
                }
                return adoptRef(new BaseString(0, 0, 0));
            }

            static PassRefPtr<Rep> createCopying(const UChar*, int);
            static PassRefPtr<Rep> create(PassRefPtr<Rep> base, int offset, int length);

            // Constructs a string from a UTF-8 string, using strict conversion (see comments in UTF8.h).
            // Returns UString::Rep::null for null input or conversion failure.
            static PassRefPtr<Rep> createFromUTF8(const char*);

            // Uses SharedUChar to have joint ownership over the UChar*.
            static PassRefPtr<Rep> create(UChar*, int, PassRefPtr<SharedUChar>);

            SharedUChar* sharedBuffer();
            void destroy();

            bool baseIsSelf() const { return m_identifierTableAndFlags.isFlagSet(BaseStringFlag); }
            UChar* data() const;
            int size() const { return len; }

            unsigned hash() const { if (_hash == 0) _hash = computeHash(data(), len); return _hash; }
            unsigned computedHash() const { ASSERT(_hash); return _hash; } // fast path for Identifiers

            static unsigned computeHash(const UChar*, int length);
            static unsigned computeHash(const char*, int length);
            static unsigned computeHash(const char* s) { return computeHash(s, strlen(s)); }

            IdentifierTable* identifierTable() const { return m_identifierTableAndFlags.get(); }
            void setIdentifierTable(IdentifierTable* table) { ASSERT(!isStatic()); m_identifierTableAndFlags.set(table); }

            bool isStatic() const { return m_identifierTableAndFlags.isFlagSet(StaticFlag); }
            void setStatic(bool);
            void setBaseString(PassRefPtr<BaseString>);
            BaseString* baseString();
            const BaseString* baseString() const;

            Rep* ref() { ++rc; return this; }
            ALWAYS_INLINE void deref() { if (--rc == 0) destroy(); }

            void checkConsistency() const;
            enum UStringFlags {
                StaticFlag,
                BaseStringFlag
            };

            // unshared data
            int offset;
            int len;
            int rc; // For null and empty static strings, this field does not reflect a correct count, because ref/deref are not thread-safe. A special case in destroy() guarantees that these do not get deleted.
            mutable unsigned _hash;
            PtrAndFlags<IdentifierTable, UStringFlags> m_identifierTableAndFlags;

            static BaseString& null() { return *nullBaseString; }
            static BaseString& empty() { return *emptyBaseString; }

            bool reserveCapacity(int capacity);

        protected:
            // Constructor for use by BaseString subclass; they use the union with m_baseString for another purpose.
            Rep(int length)
                : offset(0)
                , len(length)
                , rc(1)
                , _hash(0)
                , m_baseString(0)
            {
            }

            Rep(PassRefPtr<BaseString> base, int offsetInBase, int length)
                : offset(offsetInBase)
                , len(length)
                , rc(1)
                , _hash(0)
                , m_baseString(base.releaseRef())
            {
                checkConsistency();
            }

            union {
                // If !baseIsSelf()
                BaseString* m_baseString;
                // If baseIsSelf()
                SharedUChar* m_sharedBuffer;
            };

        private:
            // For SmallStringStorage which allocates an array and does initialization manually.
            Rep() { }

            friend class SmallStringsStorage;
            friend void initializeUString();
            JS_EXPORTDATA static BaseString* nullBaseString;
            JS_EXPORTDATA static BaseString* emptyBaseString;
        };


        struct BaseString : public Rep {
            bool isShared() { return rc != 1 || isBufferReadOnly(); }
            void setSharedBuffer(PassRefPtr<SharedUChar>);

            bool isBufferReadOnly()
            {
                if (!m_sharedBuffer)
                    return false;
                return slowIsBufferReadOnly();
            }

            // potentially shared data.
            UChar* buf;
            int preCapacity;
            int usedPreCapacity;
            int capacity;
            int usedCapacity;

            size_t reportedCost;

        private:
            BaseString(UChar* buffer, int length, int additionalCapacity = 0)
                : Rep(length)
                , buf(buffer)
                , preCapacity(0)
                , usedPreCapacity(0)
                , capacity(length + additionalCapacity)
                , usedCapacity(length)
                , reportedCost(0)
            {
                m_identifierTableAndFlags.setFlag(BaseStringFlag);
                checkConsistency();
            }

            SharedUChar* sharedBuffer();
            bool slowIsBufferReadOnly();

            friend struct Rep;
            friend class SmallStringsStorage;
            friend void initializeUString();
        };

    public:
        UString();
        UString(const char*);
        UString(const UChar*, int length);
        UString(UChar*, int length, bool copy);

        UString(const UString& s)
            : m_rep(s.m_rep)
        {
        }

        UString(const Vector<UChar>& buffer);

        ~UString()
        {
        }

        // Special constructor for cases where we overwrite an object in place.
        UString(PlacementNewAdoptType)
            : m_rep(PlacementNewAdopt)
        {
        }

        static UString from(int);
        static UString from(unsigned int);
        static UString from(long);
        static UString from(double);

        struct Range {
        public:
            Range(int pos, int len)
                : position(pos)
                , length(len)
            {
            }

            Range()
            {
            }

            int position;
            int length;
        };

        UString spliceSubstringsWithSeparators(const Range* substringRanges, int rangeCount, const UString* separators, int separatorCount) const;

        UString replaceRange(int rangeStart, int RangeEnd, const UString& replacement) const;

        UString& append(const UString&);
        UString& append(const char*);
        UString& append(UChar);
        UString& append(char c) { return append(static_cast<UChar>(static_cast<unsigned char>(c))); }
        UString& append(const UChar*, int size);
        UString& appendNumeric(int);
        UString& appendNumeric(double);

        bool getCString(CStringBuffer&) const;

        // NOTE: This method should only be used for *debugging* purposes as it
        // is neither Unicode safe nor free from side effects nor thread-safe.
        char* ascii() const;

        /**
         * Convert the string to UTF-8, assuming it is UTF-16 encoded.
         * In non-strict mode, this function is tolerant of badly formed UTF-16, it
         * can create UTF-8 strings that are invalid because they have characters in
         * the range U+D800-U+DDFF, U+FFFE, or U+FFFF, but the UTF-8 string is
         * guaranteed to be otherwise valid.
         * In strict mode, error is returned as null CString.
         */
        CString UTF8String(bool strict = false) const;

        UString& operator=(const char*c);

        UString& operator+=(const UString& s) { return append(s); }
        UString& operator+=(const char* s) { return append(s); }

        const UChar* data() const { return m_rep->data(); }

        bool isNull() const { return (m_rep == &Rep::null()); }
        bool isEmpty() const { return (!m_rep->len); }

        bool is8Bit() const;

        int size() const { return m_rep->size(); }

        UChar operator[](int pos) const;

        double toDouble(bool tolerateTrailingJunk, bool tolerateEmptyString) const;
        double toDouble(bool tolerateTrailingJunk) const;
        double toDouble() const;

        uint32_t toUInt32(bool* ok = 0) const;
        uint32_t toUInt32(bool* ok, bool tolerateEmptyString) const;
        uint32_t toStrictUInt32(bool* ok = 0) const;

        unsigned toArrayIndex(bool* ok = 0) const;

        int find(const UString& f, int pos = 0) const;
        int find(UChar, int pos = 0) const;
        int rfind(const UString& f, int pos) const;
        int rfind(UChar, int pos) const;

        UString substr(int pos = 0, int len = -1) const;

        static const UString& null() { return *nullUString; }

        Rep* rep() const { return m_rep.get(); }
        static Rep* nullRep();

        UString(PassRefPtr<Rep> r)
            : m_rep(r)
        {
            ASSERT(m_rep);
        }

        size_t cost() const;

        // Attempt to grow this string such that it can grow to a total length of 'capacity'
        // without reallocation.  This may fail a number of reasons - if the BasicString is
        // shared and another string is using part of the capacity beyond our end point, if
        // the realloc fails, or if this string is empty and has no storage.
        //
        // This method returns a boolean indicating success.
        bool reserveCapacity(int capacity)
        {
            return m_rep->reserveCapacity(capacity);
        }

    private:
        void expandCapacity(int requiredLength);
        void expandPreCapacity(int requiredPreCap);
        void makeNull();

        RefPtr<Rep> m_rep;
        static UString* nullUString;

        friend void initializeUString();
        friend bool operator==(const UString&, const UString&);
        friend PassRefPtr<Rep> concatenate(Rep*, Rep*); // returns 0 if out of memory
    };
    PassRefPtr<UString::Rep> concatenate(UString::Rep*, UString::Rep*);
    PassRefPtr<UString::Rep> concatenate(UString::Rep*, int);
    PassRefPtr<UString::Rep> concatenate(UString::Rep*, double);

    inline bool operator==(const UString& s1, const UString& s2)
    {
        int size = s1.size();
        switch (size) {
        case 0:
            return !s2.size();
        case 1:
            return s2.size() == 1 && s1.data()[0] == s2.data()[0];
        case 2: {
            if (s2.size() != 2)
                return false;
            const UChar* d1 = s1.data();
            const UChar* d2 = s2.data();
            return (d1[0] == d2[0]) & (d1[1] == d2[1]);
        }
        default:
            return s2.size() == size && memcmp(s1.data(), s2.data(), size * sizeof(UChar)) == 0;
        }
    }


    inline bool operator!=(const UString& s1, const UString& s2)
    {
        return !JSC::operator==(s1, s2);
    }

    bool operator<(const UString& s1, const UString& s2);
    bool operator>(const UString& s1, const UString& s2);

    bool operator==(const UString& s1, const char* s2);

    inline bool operator!=(const UString& s1, const char* s2)
    {
        return !JSC::operator==(s1, s2);
    }

    inline bool operator==(const char *s1, const UString& s2)
    {
        return operator==(s2, s1);
    }

    inline bool operator!=(const char *s1, const UString& s2)
    {
        return !JSC::operator==(s1, s2);
    }

    bool operator==(const CString&, const CString&);

    inline UString operator+(const UString& s1, const UString& s2)
    {
        RefPtr<UString::Rep> result = concatenate(s1.rep(), s2.rep());
        return UString(result ? result.release() : UString::nullRep());
    }

    int compare(const UString&, const UString&);

    bool equal(const UString::Rep*, const UString::Rep*);

    inline PassRefPtr<UString::Rep> UString::Rep::create(PassRefPtr<UString::Rep> rep, int offset, int length)
    {
        ASSERT(rep);
        rep->checkConsistency();

        int repOffset = rep->offset;

        PassRefPtr<BaseString> base = rep->baseString();

        ASSERT(-(offset + repOffset) <= base->usedPreCapacity);
        ASSERT(offset + repOffset + length <= base->usedCapacity);

        // Steal the single reference this Rep was created with.
        return adoptRef(new Rep(base, repOffset + offset, length));
    }

    inline UChar* UString::Rep::data() const
    {
        const BaseString* base = baseString();
        return base->buf + base->preCapacity + offset;
    }

    inline void UString::Rep::setStatic(bool v)
    {
        ASSERT(!identifierTable());
        if (v)
            m_identifierTableAndFlags.setFlag(StaticFlag);
        else
            m_identifierTableAndFlags.clearFlag(StaticFlag);
    }

    inline void UString::Rep::setBaseString(PassRefPtr<BaseString> base)
    {
        ASSERT(base != this);
        ASSERT(!baseIsSelf());
        m_baseString = base.releaseRef();
    }

    inline UString::BaseString* UString::Rep::baseString()
    {
        return !baseIsSelf() ? m_baseString : reinterpret_cast<BaseString*>(this) ;
    }

    inline const UString::BaseString* UString::Rep::baseString() const
    {
        return const_cast<Rep*>(this)->baseString();
    }

#ifdef NDEBUG
    inline void UString::Rep::checkConsistency() const
    {
    }
#endif

    inline UString::UString()
        : m_rep(&Rep::null())
    {
    }

    // Rule from ECMA 15.2 about what an array index is.
    // Must exactly match string form of an unsigned integer, and be less than 2^32 - 1.
    inline unsigned UString::toArrayIndex(bool* ok) const
    {
        unsigned i = toStrictUInt32(ok);
        if (ok && i >= 0xFFFFFFFFU)
            *ok = false;
        return i;
    }

    // We'd rather not do shared substring append for small strings, since
    // this runs too much risk of a tiny initial string holding down a
    // huge buffer.
    // FIXME: this should be size_t but that would cause warnings until we
    // fix UString sizes to be size_t instead of int
    static const int minShareSize = Heap::minExtraCostSize / sizeof(UChar);

    inline size_t UString::cost() const
    {
        BaseString* base = m_rep->baseString();
        size_t capacity = (base->capacity + base->preCapacity) * sizeof(UChar);
        size_t reportedCost = base->reportedCost;
        ASSERT(capacity >= reportedCost);

        size_t capacityDelta = capacity - reportedCost;

        if (capacityDelta < static_cast<size_t>(minShareSize))
            return 0;

        base->reportedCost = capacity;

        return capacityDelta;
    }

    struct IdentifierRepHash : PtrHash<RefPtr<JSC::UString::Rep> > {
        static unsigned hash(const RefPtr<JSC::UString::Rep>& key) { return key->computedHash(); }
        static unsigned hash(JSC::UString::Rep* key) { return key->computedHash(); }
    };

    void initializeUString();
} // namespace JSC

namespace WTF {

    template<typename T> struct DefaultHash;
    template<typename T> struct StrHash;

    template<> struct StrHash<JSC::UString::Rep*> {
        static unsigned hash(const JSC::UString::Rep* key) { return key->hash(); }
        static bool equal(const JSC::UString::Rep* a, const JSC::UString::Rep* b) { return JSC::equal(a, b); }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<> struct StrHash<RefPtr<JSC::UString::Rep> > : public StrHash<JSC::UString::Rep*> {
        using StrHash<JSC::UString::Rep*>::hash;
        static unsigned hash(const RefPtr<JSC::UString::Rep>& key) { return key->hash(); }
        using StrHash<JSC::UString::Rep*>::equal;
        static bool equal(const RefPtr<JSC::UString::Rep>& a, const RefPtr<JSC::UString::Rep>& b) { return JSC::equal(a.get(), b.get()); }
        static bool equal(const JSC::UString::Rep* a, const RefPtr<JSC::UString::Rep>& b) { return JSC::equal(a, b.get()); }
        static bool equal(const RefPtr<JSC::UString::Rep>& a, const JSC::UString::Rep* b) { return JSC::equal(a.get(), b); }

        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    template<> struct DefaultHash<JSC::UString::Rep*> {
        typedef StrHash<JSC::UString::Rep*> Hash;
    };

    template<> struct DefaultHash<RefPtr<JSC::UString::Rep> > {
        typedef StrHash<RefPtr<JSC::UString::Rep> > Hash;

    };

} // namespace WTF

#endif
