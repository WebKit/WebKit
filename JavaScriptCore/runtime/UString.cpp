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
#include <limits.h>
#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Vector.h>
#include <wtf/unicode/UTF8.h>

#if HAVE(STRINGS_H)
#include <strings.h>
#endif

using namespace WTF;
using namespace WTF::Unicode;
using namespace std;

namespace JSC {

extern const double NaN;
extern const double Inf;

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
UString::UString(const char* characters, unsigned length)
    : m_impl(characters ? StringImpl::create(characters, length) : 0)
{
}

// Construct a string with latin1 data, from a null-terminated source.
UString::UString(const char* characters)
    : m_impl(characters ? StringImpl::create(characters) : 0)
{
}

UString UString::number(int i)
{
    UChar buf[1 + sizeof(i) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;

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
    UChar buf[1 + sizeof(i) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;

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

    return UString(p, static_cast<unsigned>(end - p));
}

UString UString::number(long l)
{
    UChar buf[1 + sizeof(l) * 3];
    UChar* end = buf + sizeof(buf) / sizeof(UChar);
    UChar* p = end;

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
    DtoaBuffer buffer;
    unsigned length;
    doubleToStringInJavaScriptFormat(d, buffer, &length);
    return UString(buffer, length);
}

static inline bool isInfinity(double number)
{
    return number == Inf || number == -Inf;
}

static bool isInfinity(const UChar* data, const UChar* end)
{
    return data + 7 < end
        && data[0] == 'I'
        && data[1] == 'n'
        && data[2] == 'f'
        && data[3] == 'i'
        && data[4] == 'n'
        && data[5] == 'i'
        && data[6] == 't'
        && data[7] == 'y';
}

double UString::toDouble(bool tolerateTrailingJunk, bool tolerateEmptyString) const
{
    unsigned size = this->length();

    if (size == 1) {
        UChar c = characters()[0];
        if (isASCIIDigit(c))
            return c - '0';
        if (isStrWhiteSpace(c) && tolerateEmptyString)
            return 0;
        return NaN;
    }

    const UChar* data = this->characters();
    const UChar* end = data + size;

    // Skip leading white space.
    for (; data < end; ++data) {
        if (!isStrWhiteSpace(*data))
            break;
    }

    // Empty string.
    if (data == end)
        return tolerateEmptyString ? 0.0 : NaN;

    double number;

    if (data[0] == '0' && data + 2 < end && (data[1] | 0x20) == 'x' && isASCIIHexDigit(data[2])) {
        // Hex number.
        data += 2;
        const UChar* firstDigitPosition = data;
        number = 0;
        while (true) {
            number = number * 16 + toASCIIHexValue(*data);
            ++data;
            if (data == end)
                break;
            if (!isASCIIHexDigit(*data))
                break;
        }
        if (number >= mantissaOverflowLowerBound)
            number = parseIntOverflow(firstDigitPosition, data - firstDigitPosition, 16);
    } else {
        // Decimal number.

        // Put into a null-terminated byte buffer.
        Vector<char, 32> byteBuffer;
        for (const UChar* characters = data; characters < end; ++characters) {
            UChar character = *characters;
            byteBuffer.append(isASCII(character) ? character : 0);
        }
        byteBuffer.append(0);

        char* byteBufferEnd;
        number = WTF::strtod(byteBuffer.data(), &byteBufferEnd);
        const UChar* pastNumber = data + (byteBufferEnd - byteBuffer.data());

        if ((number || pastNumber != data) && !isInfinity(number))
            data = pastNumber;
        else {
            // We used strtod() to do the conversion. However, strtod() handles
            // infinite values slightly differently than JavaScript in that it
            // converts the string "inf" with any capitalization to infinity,
            // whereas the ECMA spec requires that it be converted to NaN.

            double signedInfinity = Inf;
            if (data < end) {
                if (*data == '+')
                    data++;
                else if (*data == '-') {
                    signedInfinity = -Inf;
                    data++;
                }
            }
            if (isInfinity(data, end)) {
                number = signedInfinity;
                data += 8;
            } else if (isInfinity(number) && data < end && (*data | 0x20) != 'i')
                data = pastNumber;
            else
                return NaN;
        }
    }

    // Look for trailing junk.
    if (!tolerateTrailingJunk) {
        // Allow trailing white space.
        for (; data < end; ++data) {
            if (!isStrWhiteSpace(*data))
                break;
        }
        if (data != end)
            return NaN;
    }

    return number;
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
    unsigned len = m_impl->length();
    if (len == 0)
        return 0;
    const UChar* p = m_impl->characters();
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

UString UString::substr(unsigned pos, unsigned len) const
{
    // FIXME: We used to check against a limit of Heap::minExtraCost / sizeof(UChar).

    unsigned s = length();

    if (pos >= s)
        pos = s;
    unsigned limit = s - pos;
    if (len > limit)
        len = limit;

    if (pos == 0 && len == s)
        return *this;

    return UString(StringImpl::create(m_impl, pos, len));
}

bool operator==(const UString& s1, const char *s2)
{
    if (s2 == 0)
        return s1.isEmpty();

    const UChar* u = s1.characters();
    const UChar* uend = u + s1.length();
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
    const UChar* characters = this->characters();

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
    const UChar* characters = this->characters();

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
    Vector<char, 1024> bufferVector(length * 3);

    char* buffer = bufferVector.data();
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

    return CString(bufferVector.data(), buffer - bufferVector.data());
}

} // namespace JSC
