/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
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

#include "config.h"
#include "UString.h"

#include "JSGlobalObjectFunctions.h"
#include "Collector.h"
#include "dtoa.h"
#include "Identifier.h"
#include "Operations.h"
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <limits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/UTF8.h>
#include <wtf/StringExtras.h>

#if HAVE(STRING_H)
#include <string.h>
#endif
#if HAVE(STRINGS_H)
#include <strings.h>
#endif

using namespace WTF;
using namespace WTF::Unicode;
using namespace std;

// This can be tuned differently per platform by putting platform #ifs right here.
// If you don't define this macro at all, then copyChars will just call directly
// to memcpy.
#define USTRING_COPY_CHARS_INLINE_CUTOFF 20

namespace JSC {
 
extern const double NaN;
extern const double Inf;

// This number must be at least 2 to avoid sharing empty, null as well as 1 character strings from SmallStrings.
static const int minLengthToShare = 10;

static inline size_t overflowIndicator() { return std::numeric_limits<size_t>::max(); }
static inline size_t maxUChars() { return std::numeric_limits<size_t>::max() / sizeof(UChar); }

static inline PossiblyNull<UChar*> allocChars(size_t length)
{
    ASSERT(length);
    if (length > maxUChars())
        return 0;
    return tryFastMalloc(sizeof(UChar) * length);
}

static inline PossiblyNull<UChar*> reallocChars(UChar* buffer, size_t length)
{
    ASSERT(length);
    if (length > maxUChars())
        return 0;
    return tryFastRealloc(buffer, sizeof(UChar) * length);
}

static inline void copyChars(UChar* destination, const UChar* source, unsigned numCharacters)
{
#ifdef USTRING_COPY_CHARS_INLINE_CUTOFF
    if (numCharacters <= USTRING_COPY_CHARS_INLINE_CUTOFF) {
        for (unsigned i = 0; i < numCharacters; ++i)
            destination[i] = source[i];
        return;
    }
#endif
    memcpy(destination, source, numCharacters * sizeof(UChar));
}

COMPILE_ASSERT(sizeof(UChar) == 2, uchar_is_2_bytes);

CString::CString(const char* c)
    : m_length(strlen(c))
    , m_data(new char[m_length + 1])
{
    memcpy(m_data, c, m_length + 1);
}

CString::CString(const char* c, size_t length)
    : m_length(length)
    , m_data(new char[length + 1])
{
    memcpy(m_data, c, m_length);
    m_data[m_length] = 0;
}

CString::CString(const CString& b)
{
    m_length = b.m_length;
    if (b.m_data) {
        m_data = new char[m_length + 1];
        memcpy(m_data, b.m_data, m_length + 1);
    } else
        m_data = 0;
}

CString::~CString()
{
    delete [] m_data;
}

CString CString::adopt(char* c, size_t length)
{
    CString s;
    s.m_data = c;
    s.m_length = length;
    return s;
}

CString& CString::append(const CString& t)
{
    char* n;
    n = new char[m_length + t.m_length + 1];
    if (m_length)
        memcpy(n, m_data, m_length);
    if (t.m_length)
        memcpy(n + m_length, t.m_data, t.m_length);
    m_length += t.m_length;
    n[m_length] = 0;

    delete [] m_data;
    m_data = n;

    return *this;
}

CString& CString::operator=(const char* c)
{
    if (m_data)
        delete [] m_data;
    m_length = strlen(c);
    m_data = new char[m_length + 1];
    memcpy(m_data, c, m_length + 1);

    return *this;
}

CString& CString::operator=(const CString& str)
{
    if (this == &str)
        return *this;

    if (m_data)
        delete [] m_data;
    m_length = str.m_length;
    if (str.m_data) {
        m_data = new char[m_length + 1];
        memcpy(m_data, str.m_data, m_length + 1);
    } else
        m_data = 0;

    return *this;
}

bool operator==(const CString& c1, const CString& c2)
{
    size_t len = c1.size();
    return len == c2.size() && (len == 0 || memcmp(c1.c_str(), c2.c_str(), len) == 0);
}

// These static strings are immutable, except for rc, whose initial value is chosen to 
// reduce the possibility of it becoming zero due to ref/deref not being thread-safe.
static UChar sharedEmptyChar;
UString::BaseString* UString::Rep::nullBaseString;
UString::BaseString* UString::Rep::emptyBaseString;
UString* UString::nullUString;

static void initializeStaticBaseString(UString::BaseString& base)
{
    base.rc = INT_MAX / 2;
    base.m_identifierTableAndFlags.setFlag(UString::Rep::StaticFlag);
    base.checkConsistency();
}

void initializeUString()
{
    UString::Rep::nullBaseString = new UString::BaseString(0, 0);
    initializeStaticBaseString(*UString::Rep::nullBaseString);

    UString::Rep::emptyBaseString = new UString::BaseString(&sharedEmptyChar, 0);
    initializeStaticBaseString(*UString::Rep::emptyBaseString);

    UString::nullUString = new UString;
}

static char* statBuffer = 0; // Only used for debugging via UString::ascii().

PassRefPtr<UString::Rep> UString::Rep::createCopying(const UChar* d, int l)
{
    UChar* copyD = static_cast<UChar*>(fastMalloc(l * sizeof(UChar)));
    copyChars(copyD, d, l);
    return create(copyD, l);
}

PassRefPtr<UString::Rep> UString::Rep::createFromUTF8(const char* string)
{
    if (!string)
        return &UString::Rep::null();

    size_t length = strlen(string);
    Vector<UChar, 1024> buffer(length);
    UChar* p = buffer.data();
    if (conversionOK != convertUTF8ToUTF16(&string, string + length, &p, p + length))
        return &UString::Rep::null();

    return UString::Rep::createCopying(buffer.data(), p - buffer.data());
}

PassRefPtr<UString::Rep> UString::Rep::create(UChar* string, int length, PassRefPtr<UString::SharedUChar> sharedBuffer)
{
    PassRefPtr<UString::Rep> rep = create(string, length);
    rep->baseString()->setSharedBuffer(sharedBuffer);
    rep->checkConsistency();
    return rep;
}

UString::SharedUChar* UString::Rep::sharedBuffer()
{
    UString::BaseString* base = baseString();
    if (len < minLengthToShare)
        return 0;

    return base->sharedBuffer();
}

void UString::Rep::destroy()
{
    checkConsistency();

    // Static null and empty strings can never be destroyed, but we cannot rely on 
    // reference counting, because ref/deref are not thread-safe.
    if (!isStatic()) {
        if (identifierTable())
            Identifier::remove(this);

        UString::BaseString* base = baseString();
        if (base == this) {
            if (m_sharedBuffer)
                m_sharedBuffer->deref();
            else
                fastFree(base->buf);
        } else
            base->deref();

        delete this;
    }
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UString::Rep::computeHash(const UChar* s, int len)
{
    unsigned l = len;
    uint32_t hash = PHI;
    uint32_t tmp;

    int rem = l & 1;
    l >>= 1;

    // Main loop
    for (; l > 0; l--) {
        hash += s[0];
        tmp = (s[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }

    // Handle end case
    if (rem) {
        hash += s[0];
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

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned UString::Rep::computeHash(const char* s, int l)
{
    // This hash is designed to work on 16-bit chunks at a time. But since the normal case
    // (above) is to hash UTF-16 characters, we just treat the 8-bit chars as if they
    // were 16-bit chunks, which should give matching results

    uint32_t hash = PHI;
    uint32_t tmp;

    size_t rem = l & 1;
    l >>= 1;

    // Main loop
    for (; l > 0; l--) {
        hash += static_cast<unsigned char>(s[0]);
        tmp = (static_cast<unsigned char>(s[1]) << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        s += 2;
        hash += hash >> 11;
    }

    // Handle end case
    if (rem) {
        hash += static_cast<unsigned char>(s[0]);
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

#ifndef NDEBUG
void UString::Rep::checkConsistency() const
{
    const UString::BaseString* base = baseString();

    // There is no recursion for base strings.
    ASSERT(base == base->baseString());

    if (isStatic()) {
        // There are only two static strings: null and empty.
        ASSERT(!len);

        // Static strings cannot get in identifier tables, because they are globally shared.
        ASSERT(!identifierTable());
    }

    // The string fits in buffer.
    ASSERT(base->usedPreCapacity <= base->preCapacity);
    ASSERT(base->usedCapacity <= base->capacity);
    ASSERT(-offset <= base->usedPreCapacity);
    ASSERT(offset + len <= base->usedCapacity);
}
#endif

UString::SharedUChar* UString::BaseString::sharedBuffer()
{
    if (!m_sharedBuffer)
        setSharedBuffer(SharedUChar::create(new OwnFastMallocPtr<UChar>(buf)));
    return m_sharedBuffer;
}

void UString::BaseString::setSharedBuffer(PassRefPtr<UString::SharedUChar> sharedBuffer)
{
    // The manual steps below are because m_sharedBuffer can't be a RefPtr. m_sharedBuffer
    // is in a union with another variable to avoid making BaseString any larger.
    if (m_sharedBuffer)
        m_sharedBuffer->deref();
    m_sharedBuffer = sharedBuffer.releaseRef();
}

bool UString::BaseString::slowIsBufferReadOnly()
{
    // The buffer may not be modified as soon as the underlying data has been shared with another class.
    if (m_sharedBuffer->isShared())
        return true;

    // At this point, we know it that the underlying buffer isn't shared outside of this base class,
    // so get rid of m_sharedBuffer.
    OwnPtr<OwnFastMallocPtr<UChar> > mallocPtr(m_sharedBuffer->release());
    UChar* unsharedBuf = const_cast<UChar*>(mallocPtr->release());
    setSharedBuffer(0);
    preCapacity += (buf - unsharedBuf);
    buf = unsharedBuf;
    return false;
}

// Put these early so they can be inlined.
static inline size_t expandedSize(size_t capacitySize, size_t precapacitySize)
{
    // Combine capacitySize & precapacitySize to produce a single size to allocate,
    // check that doing so does not result in overflow.
    size_t size = capacitySize + precapacitySize;
    if (size < capacitySize)
        return overflowIndicator();

    // Small Strings (up to 4 pages):
    // Expand the allocation size to 112.5% of the amount requested.  This is largely sicking
    // to our previous policy, however 112.5% is cheaper to calculate.
    if (size < 0x4000) {
        size_t expandedSize = ((size + (size >> 3)) | 15) + 1;
        // Given the limited range within which we calculate the expansion in this
        // fashion the above calculation should never overflow.
        ASSERT(expandedSize >= size);
        ASSERT(expandedSize < maxUChars());
        return expandedSize;
    }

    // Medium Strings (up to 128 pages):
    // For pages covering multiple pages over-allocation is less of a concern - any unused
    // space will not be paged in if it is not used, so this is purely a VM overhead.  For
    // these strings allocate 2x the requested size.
    if (size < 0x80000) {
        size_t expandedSize = ((size + size) | 0xfff) + 1;
        // Given the limited range within which we calculate the expansion in this
        // fashion the above calculation should never overflow.
        ASSERT(expandedSize >= size);
        ASSERT(expandedSize < maxUChars());
        return expandedSize;
    }

    // Large Strings (to infinity and beyond!):
    // Revert to our 112.5% policy - probably best to limit the amount of unused VM we allow
    // any individual string be responsible for.
    size_t expandedSize = ((size + (size >> 3)) | 0xfff) + 1;

    // Check for overflow - any result that is at least as large as requested (but
    // still below the limit) is okay.
    if ((expandedSize >= size) && (expandedSize < maxUChars()))
        return expandedSize;
    return overflowIndicator();
}

static inline bool expandCapacity(UString::Rep* rep, int requiredLength)
{
    rep->checkConsistency();
    ASSERT(!rep->baseString()->isBufferReadOnly());

    UString::BaseString* base = rep->baseString();

    if (requiredLength > base->capacity) {
        size_t newCapacity = expandedSize(requiredLength, base->preCapacity);
        UChar* oldBuf = base->buf;
        if (!reallocChars(base->buf, newCapacity).getValue(base->buf)) {
            base->buf = oldBuf;
            return false;
        }
        base->capacity = newCapacity - base->preCapacity;
    }
    if (requiredLength > base->usedCapacity)
        base->usedCapacity = requiredLength;

    rep->checkConsistency();
    return true;
}

bool UString::Rep::reserveCapacity(int capacity)
{
    // If this is an empty string there is no point 'growing' it - just allocate a new one.
    // If the BaseString is shared with another string that is using more capacity than this
    // string is, then growing the buffer won't help.
    // If the BaseString's buffer is readonly, then it isn't allowed to grow.
    UString::BaseString* base = baseString();
    if (!base->buf || !base->capacity || (offset + len) != base->usedCapacity || base->isBufferReadOnly())
        return false;
    
    // If there is already sufficient capacity, no need to grow!
    if (capacity <= base->capacity)
        return true;

    checkConsistency();

    size_t newCapacity = expandedSize(capacity, base->preCapacity);
    UChar* oldBuf = base->buf;
    if (!reallocChars(base->buf, newCapacity).getValue(base->buf)) {
        base->buf = oldBuf;
        return false;
    }
    base->capacity = newCapacity - base->preCapacity;

    checkConsistency();
    return true;
}

void UString::expandCapacity(int requiredLength)
{
    if (!JSC::expandCapacity(m_rep.get(), requiredLength))
        makeNull();
}

void UString::expandPreCapacity(int requiredPreCap)
{
    m_rep->checkConsistency();
    ASSERT(!m_rep->baseString()->isBufferReadOnly());

    BaseString* base = m_rep->baseString();

    if (requiredPreCap > base->preCapacity) {
        size_t newCapacity = expandedSize(requiredPreCap, base->capacity);
        int delta = newCapacity - base->capacity - base->preCapacity;

        UChar* newBuf;
        if (!allocChars(newCapacity).getValue(newBuf)) {
            makeNull();
            return;
        }
        copyChars(newBuf + delta, base->buf, base->capacity + base->preCapacity);
        fastFree(base->buf);
        base->buf = newBuf;

        base->preCapacity = newCapacity - base->capacity;
    }
    if (requiredPreCap > base->usedPreCapacity)
        base->usedPreCapacity = requiredPreCap;

    m_rep->checkConsistency();
}

static PassRefPtr<UString::Rep> createRep(const char* c)
{
    if (!c)
        return &UString::Rep::null();

    if (!c[0])
        return &UString::Rep::empty();

    size_t length = strlen(c);
    UChar* d;
    if (!allocChars(length).getValue(d))
        return &UString::Rep::null();
    else {
        for (size_t i = 0; i < length; i++)
            d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend
        return UString::Rep::create(d, static_cast<int>(length));
    }

}

UString::UString(const char* c)
    : m_rep(createRep(c))
{
}

UString::UString(const UChar* c, int length)
{
    if (length == 0) 
        m_rep = &Rep::empty();
    else
        m_rep = Rep::createCopying(c, length);
}

UString::UString(UChar* c, int length, bool copy)
{
    if (length == 0)
        m_rep = &Rep::empty();
    else if (copy)
        m_rep = Rep::createCopying(c, length);
    else
        m_rep = Rep::create(c, length);
}

UString::UString(const Vector<UChar>& buffer)
{
    if (!buffer.size())
        m_rep = &Rep::empty();
    else
        m_rep = Rep::createCopying(buffer.data(), buffer.size());
}

static ALWAYS_INLINE int newCapacityWithOverflowCheck(const int currentCapacity, const int extendLength, const bool plusOne = false)
{
    ASSERT_WITH_MESSAGE(extendLength >= 0, "extendedLength = %d", extendLength);

    const int plusLength = plusOne ? 1 : 0;
    if (currentCapacity > std::numeric_limits<int>::max() - extendLength - plusLength)
        CRASH();

    return currentCapacity + extendLength + plusLength;
}

static ALWAYS_INLINE PassRefPtr<UString::Rep> concatenate(PassRefPtr<UString::Rep> r, const UChar* tData, int tSize)
{
    RefPtr<UString::Rep> rep = r;

    rep->checkConsistency();

    int thisSize = rep->size();
    int thisOffset = rep->offset;
    int length = thisSize + tSize;
    UString::BaseString* base = rep->baseString();

    // possible cases:
    if (tSize == 0) {
        // t is empty
    } else if (thisSize == 0) {
        // this is empty
        rep = UString::Rep::createCopying(tData, tSize);
    } else if (rep == base && !base->isShared()) {
        // this is direct and has refcount of 1 (so we can just alter it directly)
        if (!expandCapacity(rep.get(), newCapacityWithOverflowCheck(thisOffset, length)))
            rep = &UString::Rep::null();
        if (rep->data()) {
            copyChars(rep->data() + thisSize, tData, tSize);
            rep->len = length;
            rep->_hash = 0;
        }
    } else if (thisOffset + thisSize == base->usedCapacity && thisSize >= minShareSize && !base->isBufferReadOnly()) {
        // this reaches the end of the buffer - extend it if it's long enough to append to
        if (!expandCapacity(rep.get(), newCapacityWithOverflowCheck(thisOffset, length)))
            rep = &UString::Rep::null();
        if (rep->data()) {
            copyChars(rep->data() + thisSize, tData, tSize);
            rep = UString::Rep::create(rep, 0, length);
        }
    } else {
        // This is shared in some way that prevents us from modifying base, so we must make a whole new string.
        size_t newCapacity = expandedSize(length, 0);
        UChar* d;
        if (!allocChars(newCapacity).getValue(d))
            rep = &UString::Rep::null();
        else {
            copyChars(d, rep->data(), thisSize);
            copyChars(d + thisSize, tData, tSize);
            rep = UString::Rep::create(d, length);
            rep->baseString()->capacity = newCapacity;
        }
    }

    rep->checkConsistency();

    return rep.release();
}

static ALWAYS_INLINE PassRefPtr<UString::Rep> concatenate(PassRefPtr<UString::Rep> r, const char* t)
{
    RefPtr<UString::Rep> rep = r;

    rep->checkConsistency();

    int thisSize = rep->size();
    int thisOffset = rep->offset;
    int tSize = static_cast<int>(strlen(t));
    int length = thisSize + tSize;
    UString::BaseString* base = rep->baseString();

    // possible cases:
    if (thisSize == 0) {
        // this is empty
        rep = createRep(t);
    } else if (tSize == 0) {
        // t is empty, we'll just return *this below.
    } else if (rep == base && !base->isShared()) {
        // this is direct and has refcount of 1 (so we can just alter it directly)
        expandCapacity(rep.get(), newCapacityWithOverflowCheck(thisOffset, length));
        UChar* d = rep->data();
        if (d) {
            for (int i = 0; i < tSize; ++i)
                d[thisSize + i] = static_cast<unsigned char>(t[i]); // use unsigned char to zero-extend instead of sign-extend
            rep->len = length;
            rep->_hash = 0;
        }
    } else if (thisOffset + thisSize == base->usedCapacity && thisSize >= minShareSize && !base->isBufferReadOnly()) {
        // this string reaches the end of the buffer - extend it
        expandCapacity(rep.get(), newCapacityWithOverflowCheck(thisOffset, length));
        UChar* d = rep->data();
        if (d) {
            for (int i = 0; i < tSize; ++i)
                d[thisSize + i] = static_cast<unsigned char>(t[i]); // use unsigned char to zero-extend instead of sign-extend
            rep = UString::Rep::create(rep, 0, length);
        }
    } else {
        // This is shared in some way that prevents us from modifying base, so we must make a whole new string.
        size_t newCapacity = expandedSize(length, 0);
        UChar* d;
        if (!allocChars(newCapacity).getValue(d))
            rep = &UString::Rep::null();
        else {
            copyChars(d, rep->data(), thisSize);
            for (int i = 0; i < tSize; ++i)
                d[thisSize + i] = static_cast<unsigned char>(t[i]); // use unsigned char to zero-extend instead of sign-extend
            rep = UString::Rep::create(d, length);
            rep->baseString()->capacity = newCapacity;
        }
    }

    rep->checkConsistency();

    return rep.release();
}

PassRefPtr<UString::Rep> concatenate(UString::Rep* a, UString::Rep* b)
{
    a->checkConsistency();
    b->checkConsistency();

    int aSize = a->size();
    int bSize = b->size();
    int aOffset = a->offset;

    // possible cases:

    UString::BaseString* aBase = a->baseString();
    if (bSize == 1 && aOffset + aSize == aBase->usedCapacity && aOffset + aSize < aBase->capacity && !aBase->isBufferReadOnly()) {
        // b is a single character (common fast case)
        ++aBase->usedCapacity;
        a->data()[aSize] = b->data()[0];
        return UString::Rep::create(a, 0, aSize + 1);
    }

    // a is empty
    if (aSize == 0)
        return b;
    // b is empty
    if (bSize == 0)
        return a;

    int bOffset = b->offset;
    int length = aSize + bSize;

    UString::BaseString* bBase = b->baseString();
    if (aOffset + aSize == aBase->usedCapacity && aSize >= minShareSize && 4 * aSize >= bSize
        && (-bOffset != bBase->usedPreCapacity || aSize >= bSize) && !aBase->isBufferReadOnly()) {
        // - a reaches the end of its buffer so it qualifies for shared append
        // - also, it's at least a quarter the length of b - appending to a much shorter
        //   string does more harm than good
        // - however, if b qualifies for prepend and is longer than a, we'd rather prepend
        
        UString x(a);
        x.expandCapacity(newCapacityWithOverflowCheck(aOffset, length));
        if (!a->data() || !x.data())
            return 0;
        copyChars(a->data() + aSize, b->data(), bSize);
        PassRefPtr<UString::Rep> result = UString::Rep::create(a, 0, length);

        a->checkConsistency();
        b->checkConsistency();
        result->checkConsistency();

        return result;
    }

    if (-bOffset == bBase->usedPreCapacity && bSize >= minShareSize && 4 * bSize >= aSize && !bBase->isBufferReadOnly()) {
        // - b reaches the beginning of its buffer so it qualifies for shared prepend
        // - also, it's at least a quarter the length of a - prepending to a much shorter
        //   string does more harm than good
        UString y(b);
        y.expandPreCapacity(-bOffset + aSize);
        if (!b->data() || !y.data())
            return 0;
        copyChars(b->data() - aSize, a->data(), aSize);
        PassRefPtr<UString::Rep> result = UString::Rep::create(b, -aSize, length);

        a->checkConsistency();
        b->checkConsistency();
        result->checkConsistency();

        return result;
    }

    // a does not qualify for append, and b does not qualify for prepend, gotta make a whole new string
    size_t newCapacity = expandedSize(length, 0);
    UChar* d;
    if (!allocChars(newCapacity).getValue(d))
        return 0;
    copyChars(d, a->data(), aSize);
    copyChars(d + aSize, b->data(), bSize);
    PassRefPtr<UString::Rep> result = UString::Rep::create(d, length);
    result->baseString()->capacity = newCapacity;

    a->checkConsistency();
    b->checkConsistency();
    result->checkConsistency();

    return result;
}

PassRefPtr<UString::Rep> concatenate(UString::Rep* rep, int i)
{
    UChar buf[1 + sizeof(i) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;
  
    if (i == 0)
        *--p = '0';
    else if (i == INT_MIN) {
        char minBuf[1 + sizeof(i) * 3];
        sprintf(minBuf, "%d", INT_MIN);
        return concatenate(rep, minBuf);
    } else {
        bool negative = false;
        if (i < 0) {
            negative = true;
            i = -i;
        }
        while (i) {
            *--p = static_cast<unsigned short>((i % 10) + '0');
            i /= 10;
        }
        if (negative)
            *--p = '-';
    }

    return concatenate(rep, p, static_cast<int>(end - p));

}

PassRefPtr<UString::Rep> concatenate(UString::Rep* rep, double d)
{
    // avoid ever printing -NaN, in JS conceptually there is only one NaN value
    if (isnan(d))
        return concatenate(rep, "NaN");

    if (d == 0.0) // stringify -0 as 0
        d = 0.0;

    char buf[80];
    int decimalPoint;
    int sign;

    char result[80];
    WTF::dtoa(result, d, 0, &decimalPoint, &sign, NULL);
    int length = static_cast<int>(strlen(result));
  
    int i = 0;
    if (sign)
        buf[i++] = '-';
  
    if (decimalPoint <= 0 && decimalPoint > -6) {
        buf[i++] = '0';
        buf[i++] = '.';
        for (int j = decimalPoint; j < 0; j++)
            buf[i++] = '0';
        strcpy(buf + i, result);
    } else if (decimalPoint <= 21 && decimalPoint > 0) {
        if (length <= decimalPoint) {
            strcpy(buf + i, result);
            i += length;
            for (int j = 0; j < decimalPoint - length; j++)
                buf[i++] = '0';
            buf[i] = '\0';
        } else {
            strncpy(buf + i, result, decimalPoint);
            i += decimalPoint;
            buf[i++] = '.';
            strcpy(buf + i, result + decimalPoint);
        }
    } else if (result[0] < '0' || result[0] > '9')
        strcpy(buf + i, result);
    else {
        buf[i++] = result[0];
        if (length > 1) {
            buf[i++] = '.';
            strcpy(buf + i, result + 1);
            i += length - 1;
        }
        
        buf[i++] = 'e';
        buf[i++] = (decimalPoint >= 0) ? '+' : '-';
        // decimalPoint can't be more than 3 digits decimal given the
        // nature of float representation
        int exponential = decimalPoint - 1;
        if (exponential < 0)
            exponential = -exponential;
        if (exponential >= 100)
            buf[i++] = static_cast<char>('0' + exponential / 100);
        if (exponential >= 10)
            buf[i++] = static_cast<char>('0' + (exponential % 100) / 10);
        buf[i++] = static_cast<char>('0' + exponential % 10);
        buf[i++] = '\0';
    }
    
    return concatenate(rep, buf);
}

UString UString::from(int i)
{
    UChar buf[1 + sizeof(i) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;
  
    if (i == 0)
        *--p = '0';
    else if (i == INT_MIN) {
        char minBuf[1 + sizeof(i) * 3];
        sprintf(minBuf, "%d", INT_MIN);
        return UString(minBuf);
    } else {
        bool negative = false;
        if (i < 0) {
            negative = true;
            i = -i;
        }
        while (i) {
            *--p = static_cast<unsigned short>((i % 10) + '0');
            i /= 10;
        }
        if (negative)
            *--p = '-';
    }

    return UString(p, static_cast<int>(end - p));
}

UString UString::from(long long i)
{
    UChar buf[1 + sizeof(i) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;

    if (i == 0)
        *--p = '0';
    else if (i == std::numeric_limits<long long>::min()) {
        char minBuf[1 + sizeof(i) * 3];
#if PLATFORM(WIN_OS)
        snprintf(minBuf, sizeof(minBuf) - 1, "%I64d", std::numeric_limits<long long>::min());
#else
        snprintf(minBuf, sizeof(minBuf) - 1, "%lld", std::numeric_limits<long long>::min());
#endif
        return UString(minBuf);
    } else {
        bool negative = false;
        if (i < 0) {
            negative = true;
            i = -i;
        }
        while (i) {
            *--p = static_cast<unsigned short>((i % 10) + '0');
            i /= 10;
        }
        if (negative)
            *--p = '-';
    }

    return UString(p, static_cast<int>(end - p));
}

UString UString::from(unsigned int u)
{
    UChar buf[sizeof(u) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;
    
    if (u == 0)
        *--p = '0';
    else {
        while (u) {
            *--p = static_cast<unsigned short>((u % 10) + '0');
            u /= 10;
        }
    }
    
    return UString(p, static_cast<int>(end - p));
}

UString UString::from(long l)
{
    UChar buf[1 + sizeof(l) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;

    if (l == 0)
        *--p = '0';
    else if (l == LONG_MIN) {
        char minBuf[1 + sizeof(l) * 3];
        sprintf(minBuf, "%ld", LONG_MIN);
        return UString(minBuf);
    } else {
        bool negative = false;
        if (l < 0) {
            negative = true;
            l = -l;
        }
        while (l) {
            *--p = static_cast<unsigned short>((l % 10) + '0');
            l /= 10;
        }
        if (negative)
            *--p = '-';
    }

    return UString(p, static_cast<int>(end - p));
}

UString UString::from(double d)
{
    // avoid ever printing -NaN, in JS conceptually there is only one NaN value
    if (isnan(d))
        return "NaN";
    if (!d)
        return "0"; // -0 -> "0"

    char buf[80];
    int decimalPoint;
    int sign;
    
    char result[80];
    WTF::dtoa(result, d, 0, &decimalPoint, &sign, NULL);
    int length = static_cast<int>(strlen(result));
  
    int i = 0;
    if (sign)
        buf[i++] = '-';
  
    if (decimalPoint <= 0 && decimalPoint > -6) {
        buf[i++] = '0';
        buf[i++] = '.';
        for (int j = decimalPoint; j < 0; j++)
            buf[i++] = '0';
        strcpy(buf + i, result);
    } else if (decimalPoint <= 21 && decimalPoint > 0) {
        if (length <= decimalPoint) {
            strcpy(buf + i, result);
            i += length;
            for (int j = 0; j < decimalPoint - length; j++)
                buf[i++] = '0';
            buf[i] = '\0';
        } else {
            strncpy(buf + i, result, decimalPoint);
            i += decimalPoint;
            buf[i++] = '.';
            strcpy(buf + i, result + decimalPoint);
        }
    } else if (result[0] < '0' || result[0] > '9')
        strcpy(buf + i, result);
    else {
        buf[i++] = result[0];
        if (length > 1) {
            buf[i++] = '.';
            strcpy(buf + i, result + 1);
            i += length - 1;
        }
        
        buf[i++] = 'e';
        buf[i++] = (decimalPoint >= 0) ? '+' : '-';
        // decimalPoint can't be more than 3 digits decimal given the
        // nature of float representation
        int exponential = decimalPoint - 1;
        if (exponential < 0)
            exponential = -exponential;
        if (exponential >= 100)
            buf[i++] = static_cast<char>('0' + exponential / 100);
        if (exponential >= 10)
            buf[i++] = static_cast<char>('0' + (exponential % 100) / 10);
        buf[i++] = static_cast<char>('0' + exponential % 10);
        buf[i++] = '\0';
    }
    
    return UString(buf);
}

UString UString::spliceSubstringsWithSeparators(const Range* substringRanges, int rangeCount, const UString* separators, int separatorCount) const
{
    m_rep->checkConsistency();

    if (rangeCount == 1 && separatorCount == 0) {
        int thisSize = size();
        int position = substringRanges[0].position;
        int length = substringRanges[0].length;
        if (position <= 0 && length >= thisSize)
            return *this;
        return UString::Rep::create(m_rep, max(0, position), min(thisSize, length));
    }

    int totalLength = 0;
    for (int i = 0; i < rangeCount; i++)
        totalLength += substringRanges[i].length;
    for (int i = 0; i < separatorCount; i++)
        totalLength += separators[i].size();

    if (totalLength == 0)
        return "";

    UChar* buffer;
    if (!allocChars(totalLength).getValue(buffer))
        return null();

    int maxCount = max(rangeCount, separatorCount);
    int bufferPos = 0;
    for (int i = 0; i < maxCount; i++) {
        if (i < rangeCount) {
            copyChars(buffer + bufferPos, data() + substringRanges[i].position, substringRanges[i].length);
            bufferPos += substringRanges[i].length;
        }
        if (i < separatorCount) {
            copyChars(buffer + bufferPos, separators[i].data(), separators[i].size());
            bufferPos += separators[i].size();
        }
    }

    return UString::Rep::create(buffer, totalLength);
}

UString UString::replaceRange(int rangeStart, int rangeLength, const UString& replacement) const
{
    m_rep->checkConsistency();

    int replacementLength = replacement.size();
    int totalLength = size() - rangeLength + replacementLength;
    if (totalLength == 0)
        return "";

    UChar* buffer;
    if (!allocChars(totalLength).getValue(buffer))
        return null();

    copyChars(buffer, data(), rangeStart);
    copyChars(buffer + rangeStart, replacement.data(), replacementLength);
    int rangeEnd = rangeStart + rangeLength;
    copyChars(buffer + rangeStart + replacementLength, data() + rangeEnd, size() - rangeEnd);

    return UString::Rep::create(buffer, totalLength);
}


UString& UString::append(const UString &t)
{
    m_rep->checkConsistency();
    t.rep()->checkConsistency();

    int thisSize = size();
    int thisOffset = m_rep->offset;
    int tSize = t.size();
    int length = thisSize + tSize;
    BaseString* base = m_rep->baseString();

    // possible cases:
    if (thisSize == 0) {
        // this is empty
        *this = t;
    } else if (tSize == 0) {
        // t is empty
    } else if (m_rep == base && !base->isShared()) {
        // this is direct and has refcount of 1 (so we can just alter it directly)
        expandCapacity(newCapacityWithOverflowCheck(thisOffset, length));
        if (data()) {
            copyChars(m_rep->data() + thisSize, t.data(), tSize);
            m_rep->len = length;
            m_rep->_hash = 0;
        }
    } else if (thisOffset + thisSize == base->usedCapacity && thisSize >= minShareSize && !base->isBufferReadOnly()) {
        // this reaches the end of the buffer - extend it if it's long enough to append to
        expandCapacity(newCapacityWithOverflowCheck(thisOffset, length));
        if (data()) {
            copyChars(m_rep->data() + thisSize, t.data(), tSize);
            m_rep = Rep::create(m_rep, 0, length);
        }
    } else {
        // This is shared in some way that prevents us from modifying base, so we must make a whole new string.
        size_t newCapacity = expandedSize(length, 0);
        UChar* d;
        if (!allocChars(newCapacity).getValue(d))
            makeNull();
        else {
            copyChars(d, data(), thisSize);
            copyChars(d + thisSize, t.data(), tSize);
            m_rep = Rep::create(d, length);
            m_rep->baseString()->capacity = newCapacity;
        }
    }

    m_rep->checkConsistency();
    t.rep()->checkConsistency();

    return *this;
}

UString& UString::append(const UChar* tData, int tSize)
{
    m_rep = concatenate(m_rep.release(), tData, tSize);
    return *this;
}

UString& UString::append(const char* t)
{
    m_rep = concatenate(m_rep.release(), t);
    return *this;
}

UString& UString::append(UChar c)
{
    m_rep->checkConsistency();

    int thisOffset = m_rep->offset;
    int length = size();
    BaseString* base = m_rep->baseString();

    // possible cases:
    if (length == 0) {
        // this is empty - must make a new m_rep because we don't want to pollute the shared empty one 
        size_t newCapacity = expandedSize(1, 0);
        UChar* d;
        if (!allocChars(newCapacity).getValue(d))
            makeNull();
        else {
            d[0] = c;
            m_rep = Rep::create(d, 1);
            m_rep->baseString()->capacity = newCapacity;
        }
    } else if (m_rep == base && !base->isShared()) {
        // this is direct and has refcount of 1 (so we can just alter it directly)
        expandCapacity(newCapacityWithOverflowCheck(thisOffset, length, true));
        UChar* d = m_rep->data();
        if (d) {
            d[length] = c;
            m_rep->len = length + 1;
            m_rep->_hash = 0;
        }
    } else if (thisOffset + length == base->usedCapacity && length >= minShareSize && !base->isBufferReadOnly()) {
        // this reaches the end of the string - extend it and share
        expandCapacity(newCapacityWithOverflowCheck(thisOffset, length, true));
        UChar* d = m_rep->data();
        if (d) {
            d[length] = c;
            m_rep = Rep::create(m_rep, 0, length + 1);
        }
    } else {
        // This is shared in some way that prevents us from modifying base, so we must make a whole new string.
        size_t newCapacity = expandedSize(length + 1, 0);
        UChar* d;
        if (!allocChars(newCapacity).getValue(d))
            makeNull();
        else {
            copyChars(d, data(), length);
            d[length] = c;
            m_rep = Rep::create(d, length + 1);
            m_rep->baseString()->capacity = newCapacity;
        }
    }

    m_rep->checkConsistency();

    return *this;
}

bool UString::getCString(CStringBuffer& buffer) const
{
    int length = size();
    int neededSize = length + 1;
    buffer.resize(neededSize);
    char* buf = buffer.data();

    UChar ored = 0;
    const UChar* p = data();
    char* q = buf;
    const UChar* limit = p + length;
    while (p != limit) {
        UChar c = p[0];
        ored |= c;
        *q = static_cast<char>(c);
        ++p;
        ++q;
    }
    *q = '\0';

    return !(ored & 0xFF00);
}

char* UString::ascii() const
{
    int length = size();
    int neededSize = length + 1;
    delete[] statBuffer;
    statBuffer = new char[neededSize];

    const UChar* p = data();
    char* q = statBuffer;
    const UChar* limit = p + length;
    while (p != limit) {
        *q = static_cast<char>(p[0]);
        ++p;
        ++q;
    }
    *q = '\0';

    return statBuffer;
}

UString& UString::operator=(const char* c)
{
    if (!c) {
        m_rep = &Rep::null();
        return *this;
    }

    if (!c[0]) {
        m_rep = &Rep::empty();
        return *this;
    }

    int l = static_cast<int>(strlen(c));
    UChar* d;
    BaseString* base = m_rep->baseString();
    if (!base->isShared() && l <= base->capacity && m_rep == base && m_rep->offset == 0 && base->preCapacity == 0) {
        d = base->buf;
        m_rep->_hash = 0;
        m_rep->len = l;
    } else {
        if (!allocChars(l).getValue(d)) {
            makeNull();
            return *this;
        }
        m_rep = Rep::create(d, l);
    }
    for (int i = 0; i < l; i++)
        d[i] = static_cast<unsigned char>(c[i]); // use unsigned char to zero-extend instead of sign-extend

    return *this;
}

bool UString::is8Bit() const
{
    const UChar* u = data();
    const UChar* limit = u + size();
    while (u < limit) {
        if (u[0] > 0xFF)
            return false;
        ++u;
    }

    return true;
}

UChar UString::operator[](int pos) const
{
    if (pos >= size())
        return '\0';
    return data()[pos];
}

double UString::toDouble(bool tolerateTrailingJunk, bool tolerateEmptyString) const
{
    if (size() == 1) {
        UChar c = data()[0];
        if (isASCIIDigit(c))
            return c - '0';
        if (isASCIISpace(c) && tolerateEmptyString)
            return 0;
        return NaN;
    }

    // FIXME: If tolerateTrailingJunk is true, then we want to tolerate non-8-bit junk
    // after the number, so this is too strict a check.
    CStringBuffer s;
    if (!getCString(s))
        return NaN;
    const char* c = s.data();

    // skip leading white space
    while (isASCIISpace(*c))
        c++;

    // empty string ?
    if (*c == '\0')
        return tolerateEmptyString ? 0.0 : NaN;

    double d;

    // hex number ?
    if (*c == '0' && (*(c + 1) == 'x' || *(c + 1) == 'X')) {
        const char* firstDigitPosition = c + 2;
        c++;
        d = 0.0;
        while (*(++c)) {
            if (*c >= '0' && *c <= '9')
                d = d * 16.0 + *c - '0';
            else if ((*c >= 'A' && *c <= 'F') || (*c >= 'a' && *c <= 'f'))
                d = d * 16.0 + (*c & 0xdf) - 'A' + 10.0;
            else
                break;
        }

        if (d >= mantissaOverflowLowerBound)
            d = parseIntOverflow(firstDigitPosition, c - firstDigitPosition, 16);
    } else {
        // regular number ?
        char* end;
        d = WTF::strtod(c, &end);
        if ((d != 0.0 || end != c) && d != Inf && d != -Inf) {
            c = end;
        } else {
            double sign = 1.0;

            if (*c == '+')
                c++;
            else if (*c == '-') {
                sign = -1.0;
                c++;
            }

            // We used strtod() to do the conversion. However, strtod() handles
            // infinite values slightly differently than JavaScript in that it
            // converts the string "inf" with any capitalization to infinity,
            // whereas the ECMA spec requires that it be converted to NaN.

            if (c[0] == 'I' && c[1] == 'n' && c[2] == 'f' && c[3] == 'i' && c[4] == 'n' && c[5] == 'i' && c[6] == 't' && c[7] == 'y') {
                d = sign * Inf;
                c += 8;
            } else if ((d == Inf || d == -Inf) && *c != 'I' && *c != 'i')
                c = end;
            else
                return NaN;
        }
    }

    // allow trailing white space
    while (isASCIISpace(*c))
        c++;
    // don't allow anything after - unless tolerant=true
    if (!tolerateTrailingJunk && *c != '\0')
        d = NaN;

    return d;
}

double UString::toDouble(bool tolerateTrailingJunk) const
{
    return toDouble(tolerateTrailingJunk, true);
}

double UString::toDouble() const
{
    return toDouble(false, true);
}

uint32_t UString::toUInt32(bool* ok) const
{
    double d = toDouble();
    bool b = true;

    if (d != static_cast<uint32_t>(d)) {
        b = false;
        d = 0;
    }

    if (ok)
        *ok = b;

    return static_cast<uint32_t>(d);
}

uint32_t UString::toUInt32(bool* ok, bool tolerateEmptyString) const
{
    double d = toDouble(false, tolerateEmptyString);
    bool b = true;

    if (d != static_cast<uint32_t>(d)) {
        b = false;
        d = 0;
    }

    if (ok)
        *ok = b;

    return static_cast<uint32_t>(d);
}

uint32_t UString::toStrictUInt32(bool* ok) const
{
    if (ok)
        *ok = false;

    // Empty string is not OK.
    int len = m_rep->len;
    if (len == 0)
        return 0;
    const UChar* p = m_rep->data();
    unsigned short c = p[0];

    // If the first digit is 0, only 0 itself is OK.
    if (c == '0') {
        if (len == 1 && ok)
            *ok = true;
        return 0;
    }

    // Convert to UInt32, checking for overflow.
    uint32_t i = 0;
    while (1) {
        // Process character, turning it into a digit.
        if (c < '0' || c > '9')
            return 0;
        const unsigned d = c - '0';

        // Multiply by 10, checking for overflow out of 32 bits.
        if (i > 0xFFFFFFFFU / 10)
            return 0;
        i *= 10;

        // Add in the digit, checking for overflow out of 32 bits.
        const unsigned max = 0xFFFFFFFFU - d;
        if (i > max)
            return 0;
        i += d;

        // Handle end of string.
        if (--len == 0) {
            if (ok)
                *ok = true;
            return i;
        }

        // Get next character.
        c = *(++p);
    }
}

int UString::find(const UString& f, int pos) const
{
    int fsz = f.size();

    if (pos < 0)
        pos = 0;

    if (fsz == 1) {
        UChar ch = f[0];
        const UChar* end = data() + size();
        for (const UChar* c = data() + pos; c < end; c++) {
            if (*c == ch)
                return static_cast<int>(c - data());
        }
        return -1;
    }

    int sz = size();
    if (sz < fsz)
        return -1;
    if (fsz == 0)
        return pos;
    const UChar* end = data() + sz - fsz;
    int fsizeminusone = (fsz - 1) * sizeof(UChar);
    const UChar* fdata = f.data();
    unsigned short fchar = fdata[0];
    ++fdata;
    for (const UChar* c = data() + pos; c <= end; c++) {
        if (c[0] == fchar && !memcmp(c + 1, fdata, fsizeminusone))
            return static_cast<int>(c - data());
    }

    return -1;
}

int UString::find(UChar ch, int pos) const
{
    if (pos < 0)
        pos = 0;
    const UChar* end = data() + size();
    for (const UChar* c = data() + pos; c < end; c++) {
        if (*c == ch)
            return static_cast<int>(c - data());
    }
    
    return -1;
}

int UString::rfind(const UString& f, int pos) const
{
    int sz = size();
    int fsz = f.size();
    if (sz < fsz)
        return -1;
    if (pos < 0)
        pos = 0;
    if (pos > sz - fsz)
        pos = sz - fsz;
    if (fsz == 0)
        return pos;
    int fsizeminusone = (fsz - 1) * sizeof(UChar);
    const UChar* fdata = f.data();
    for (const UChar* c = data() + pos; c >= data(); c--) {
        if (*c == *fdata && !memcmp(c + 1, fdata + 1, fsizeminusone))
            return static_cast<int>(c - data());
    }

    return -1;
}

int UString::rfind(UChar ch, int pos) const
{
    if (isEmpty())
        return -1;
    if (pos + 1 >= size())
        pos = size() - 1;
    for (const UChar* c = data() + pos; c >= data(); c--) {
        if (*c == ch)
            return static_cast<int>(c - data());
    }

    return -1;
}

UString UString::substr(int pos, int len) const
{
    int s = size();

    if (pos < 0)
        pos = 0;
    else if (pos >= s)
        pos = s;
    if (len < 0)
        len = s;
    if (pos + len >= s)
        len = s - pos;

    if (pos == 0 && len == s)
        return *this;

    return UString(Rep::create(m_rep, pos, len));
}

bool operator==(const UString& s1, const char *s2)
{
    if (s2 == 0)
        return s1.isEmpty();

    const UChar* u = s1.data();
    const UChar* uend = u + s1.size();
    while (u != uend && *s2) {
        if (u[0] != (unsigned char)*s2)
            return false;
        s2++;
        u++;
    }

    return u == uend && *s2 == 0;
}

bool operator<(const UString& s1, const UString& s2)
{
    const int l1 = s1.size();
    const int l2 = s2.size();
    const int lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.data();
    const UChar* c2 = s2.data();
    int l = 0;
    while (l < lmin && *c1 == *c2) {
        c1++;
        c2++;
        l++;
    }
    if (l < lmin)
        return (c1[0] < c2[0]);

    return (l1 < l2);
}

bool operator>(const UString& s1, const UString& s2)
{
    const int l1 = s1.size();
    const int l2 = s2.size();
    const int lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.data();
    const UChar* c2 = s2.data();
    int l = 0;
    while (l < lmin && *c1 == *c2) {
        c1++;
        c2++;
        l++;
    }
    if (l < lmin)
        return (c1[0] > c2[0]);

    return (l1 > l2);
}

int compare(const UString& s1, const UString& s2)
{
    const int l1 = s1.size();
    const int l2 = s2.size();
    const int lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.data();
    const UChar* c2 = s2.data();
    int l = 0;
    while (l < lmin && *c1 == *c2) {
        c1++;
        c2++;
        l++;
    }

    if (l < lmin)
        return (c1[0] > c2[0]) ? 1 : -1;

    if (l1 == l2)
        return 0;

    return (l1 > l2) ? 1 : -1;
}

bool equal(const UString::Rep* r, const UString::Rep* b)
{
    int length = r->len;
    if (length != b->len)
        return false;
    const UChar* d = r->data();
    const UChar* s = b->data();
    for (int i = 0; i != length; ++i) {
        if (d[i] != s[i])
            return false;
    }
    return true;
}

CString UString::UTF8String(bool strict) const
{
    // Allocate a buffer big enough to hold all the characters.
    const int length = size();
    Vector<char, 1024> buffer(length * 3);

    // Convert to runs of 8-bit characters.
    char* p = buffer.data();
    const UChar* d = reinterpret_cast<const UChar*>(&data()[0]);
    ConversionResult result = convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size(), strict);
    if (result != conversionOK)
        return CString();

    return CString(buffer.data(), p - buffer.data());
}

// For use in error handling code paths -- having this not be inlined helps avoid PIC branches to fetch the global on Mac OS X.
NEVER_INLINE void UString::makeNull()
{
    m_rep = &Rep::null();
}

// For use in error handling code paths -- having this not be inlined helps avoid PIC branches to fetch the global on Mac OS X.
NEVER_INLINE UString::Rep* UString::nullRep()
{
    return &Rep::null();
}

} // namespace JSC
