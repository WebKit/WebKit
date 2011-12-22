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
#include "Heap.h"
#include "Identifier.h"
#include "Operations.h"
#include <ctype.h>
#include <limits.h>
#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/dtoa.h>
#include <wtf/unicode/UTF8.h>

#if HAVE(STRINGS_H)
#include <strings.h>
#endif

using namespace WTF;
using namespace WTF::Unicode;
using namespace std;

namespace JSC {

COMPILE_ASSERT(sizeof(UString) == sizeof(void*), UString_should_stay_small);

// Construct a string with UTF-16 data.
UString::UString(const UChar* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(characters, length) : 0)
{
}

// Construct a string with UTF-16 data, from a null-terminated source.
UString::UString(const UChar* characters)
{
    if (!characters)
        return;

    int length = 0;
    while (characters[length] != UChar(0))
        ++length;

    m_impl = StringImpl::create(characters, length);
}

// Construct a string with latin1 data.
UString::UString(const LChar* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(characters, length) : 0)
{
}

UString::UString(const char* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(reinterpret_cast<const LChar*>(characters), length) : 0)
{
}

// Construct a string with latin1 data, from a null-terminated source.
UString::UString(const LChar* characters)
    : m_impl(characters ? StringImpl::create(characters) : 0)
{
}

UString::UString(const char* characters)
    : m_impl(characters ? StringImpl::create(reinterpret_cast<const LChar*>(characters)) : 0)
{
}

UString UString::number(int i)
{
    LChar buf[1 + sizeof(i) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    if (i == 0)
        *--p = '0';
    else if (i == INT_MIN) {
        char minBuf[1 + sizeof(i) * 3];
        snprintf(minBuf, sizeof(minBuf), "%d", INT_MIN);
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

    return UString(p, static_cast<unsigned>(end - p));
}

UString UString::number(long long i)
{
    LChar buf[1 + sizeof(i) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    if (i == 0)
        *--p = '0';
    else if (i == std::numeric_limits<long long>::min()) {
        char minBuf[1 + sizeof(i) * 3];
#if OS(WINDOWS)
        snprintf(minBuf, sizeof(minBuf), "%I64d", std::numeric_limits<long long>::min());
#else
        snprintf(minBuf, sizeof(minBuf), "%lld", std::numeric_limits<long long>::min());
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

    return UString(p, static_cast<unsigned>(end - p));
}

UString UString::number(unsigned u)
{
    LChar buf[sizeof(u) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    if (u == 0)
        *--p = '0';
    else {
        while (u) {
            *--p = static_cast<unsigned short>((u % 10) + '0');
            u /= 10;
        }
    }

    return UString(p, static_cast<unsigned>(end - p));
}

UString UString::number(long l)
{
    LChar buf[1 + sizeof(l) * 3];
    LChar* end = buf + WTF_ARRAY_LENGTH(buf);
    LChar* p = end;

    if (l == 0)
        *--p = '0';
    else if (l == LONG_MIN) {
        char minBuf[1 + sizeof(l) * 3];
        snprintf(minBuf, sizeof(minBuf), "%ld", LONG_MIN);
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

    return UString(p, end - p);
}

UString UString::number(double d)
{
    NumberToStringBuffer buffer;
    return UString(numberToString(d, buffer));
}

UString UString::substringSharingImpl(unsigned offset, unsigned length) const
{
    // FIXME: We used to check against a limit of Heap::minExtraCost / sizeof(UChar).

    unsigned stringLength = this->length();
    offset = min(offset, stringLength);
    length = min(length, stringLength - offset);

    if (!offset && length == stringLength)
        return *this;
    return UString(StringImpl::create(m_impl, offset, length));
}

bool operator==(const UString& s1, const char *s2)
{
    if (s1.isEmpty())
        return !s2;

    return equal(s1.impl(), s2);
}

// This method assumes that all simple checks have been performed by
// the inlined operator==() in the header file.
bool equalSlowCase(const UString& s1, const UString& s2)
{
    StringImpl* rep1 = s1.impl();
    StringImpl* rep2 = s2.impl();
    unsigned size1 = rep1->length();

    // At this point we know 
    //   (a) that the strings are the same length and
    //   (b) that they are greater than zero length.
    bool s1Is8Bit = rep1->is8Bit();
    bool s2Is8Bit = rep2->is8Bit();
    
    if (s1Is8Bit) {
        const LChar* d1 = rep1->characters8();
        if (s2Is8Bit) {
            const LChar* d2 = rep2->characters8();
            
            if (d1 == d2) // Check to see if the data pointers are the same.
                return true;
            
            // Do quick checks for sizes 1 and 2.
            switch (size1) {
            case 1:
                return d1[0] == d2[0];
            case 2:
                return (d1[0] == d2[0]) & (d1[1] == d2[1]);
            default:
                return (!memcmp(d1, d2, size1 * sizeof(LChar)));
            }
        }
        
        const UChar* d2 = rep2->characters16();
        
        for (unsigned i = 0; i < size1; i++) {
            if (d1[i] != d2[i])
                return false;
        }
        return true;
    }
    
    if (s2Is8Bit) {
        const UChar* d1 = rep1->characters16();
        const LChar* d2 = rep2->characters8();
        
        for (unsigned i = 0; i < size1; i++) {
            if (d1[i] != d2[i])
                return false;
        }
        return true;
        
    }
    
    const UChar* d1 = rep1->characters16();
    const UChar* d2 = rep2->characters16();
    
    if (d1 == d2) // Check to see if the data pointers are the same.
        return true;
    
    // Do quick checks for sizes 1 and 2.
    switch (size1) {
    case 1:
        return d1[0] == d2[0];
    case 2:
        return (d1[0] == d2[0]) & (d1[1] == d2[1]);
    default:
        return (!memcmp(d1, d2, size1 * sizeof(UChar)));
    }
}

bool operator<(const UString& s1, const UString& s2)
{
    const unsigned l1 = s1.length();
    const unsigned l2 = s2.length();
    const unsigned lmin = l1 < l2 ? l1 : l2;
    if (s1.is8Bit() && s2.is8Bit()) {
        const LChar* c1 = s1.characters8();
        const LChar* c2 = s2.characters8();
        unsigned length = 0;
        while (length < lmin && *c1 == *c2) {
            c1++;
            c2++;
            length++;
        }
        if (length < lmin)
            return (c1[0] < c2[0]);

        return (l1 < l2);        
    }
    const UChar* c1 = s1.characters();
    const UChar* c2 = s2.characters();
    unsigned length = 0;
    while (length < lmin && *c1 == *c2) {
        c1++;
        c2++;
        length++;
    }
    if (length < lmin)
        return (c1[0] < c2[0]);

    return (l1 < l2);
}

bool operator>(const UString& s1, const UString& s2)
{
    const unsigned l1 = s1.length();
    const unsigned l2 = s2.length();
    const unsigned lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.characters();
    const UChar* c2 = s2.characters();
    unsigned l = 0;
    while (l < lmin && *c1 == *c2) {
        c1++;
        c2++;
        l++;
    }
    if (l < lmin)
        return (c1[0] > c2[0]);

    return (l1 > l2);
}

CString UString::ascii() const
{
    // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
    // preserved, characters outside of this range are converted to '?'.

    unsigned length = this->length();

    if (this->is8Bit()) {
        const LChar* characters = this->characters8();
        
        char* characterBuffer;
        CString result = CString::newUninitialized(length, characterBuffer);
        
        for (unsigned i = 0; i < length; ++i) {
            LChar ch = characters[i];
            characterBuffer[i] = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
        }
        
        return result;        
    }

    const UChar* characters = this->characters16();

    char* characterBuffer;
    CString result = CString::newUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        characterBuffer[i] = ch && (ch < 0x20 || ch >= 0x7f) ? '?' : ch;
    }

    return result;
}

CString UString::latin1() const
{
    // Basic Latin1 (ISO) encoding - Unicode characters 0..255 are
    // preserved, characters outside of this range are converted to '?'.

    unsigned length = this->length();
    const UChar* characters = this->characters();

    char* characterBuffer;
    CString result = CString::newUninitialized(length, characterBuffer);

    for (unsigned i = 0; i < length; ++i) {
        UChar ch = characters[i];
        characterBuffer[i] = ch > 0xff ? '?' : ch;
    }

    return result;
}

// Helper to write a three-byte UTF-8 code point to the buffer, caller must check room is available.
static inline void putUTF8Triple(char*& buffer, UChar ch)
{
    ASSERT(ch >= 0x0800);
    *buffer++ = static_cast<char>(((ch >> 12) & 0x0F) | 0xE0);
    *buffer++ = static_cast<char>(((ch >> 6) & 0x3F) | 0x80);
    *buffer++ = static_cast<char>((ch & 0x3F) | 0x80);
}

CString UString::utf8(bool strict) const
{
    unsigned length = this->length();

    if (!length)
        return CString("", 0);

    // Allocate a buffer big enough to hold all the characters
    // (an individual UTF-16 UChar can only expand to 3 UTF-8 bytes).
    // Optimization ideas, if we find this function is hot:
    //  * We could speculatively create a CStringBuffer to contain 'length' 
    //    characters, and resize if necessary (i.e. if the buffer contains
    //    non-ascii characters). (Alternatively, scan the buffer first for
    //    ascii characters, so we know this will be sufficient).
    //  * We could allocate a CStringBuffer with an appropriate size to
    //    have a good chance of being able to write the string into the
    //    buffer without reallocing (say, 1.5 x length).
    if (length > numeric_limits<unsigned>::max() / 3)
        return CString();

    Vector<char, 1024> bufferVector(length * 3);
    char* buffer = bufferVector.data();

    if (is8Bit()) {
        const LChar* characters = this->characters8();

        ConversionResult result = convertLatin1ToUTF8(&characters, characters + length, &buffer, buffer + bufferVector.size());
        ASSERT_UNUSED(result, result != targetExhausted); // (length * 3) should be sufficient for any conversion
    } else {
        const UChar* characters = this->characters16();

        ConversionResult result = convertUTF16ToUTF8(&characters, characters + length, &buffer, buffer + bufferVector.size(), strict);
        ASSERT(result != targetExhausted); // (length * 3) should be sufficient for any conversion

        // Only produced from strict conversion.
        if (result == sourceIllegal)
            return CString();

        // Check for an unconverted high surrogate.
        if (result == sourceExhausted) {
            if (strict)
                return CString();
            // This should be one unpaired high surrogate. Treat it the same
            // was as an unpaired high surrogate would have been handled in
            // the middle of a string with non-strict conversion - which is
            // to say, simply encode it to UTF-8.
            ASSERT((characters + 1) == (this->characters() + length));
            ASSERT((*characters >= 0xD800) && (*characters <= 0xDBFF));
            // There should be room left, since one UChar hasn't been converted.
            ASSERT((buffer + 3) <= (buffer + bufferVector.size()));
            putUTF8Triple(buffer, *characters);
        }
    }

    return CString(bufferVector.data(), buffer - bufferVector.data());
}

} // namespace JSC
