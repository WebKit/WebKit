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

// The null string is immutable, except for refCount.
UString* UString::s_nullUString;

COMPILE_ASSERT(sizeof(UString) == sizeof(void*), UString_should_stay_small);

void initializeUString()
{
    // StringImpl::empty() does not construct its static string in a threadsafe fashion,
    // so ensure it has been initialized from here.
    StringImpl::empty();

    UString::s_nullUString = new UString;
}

UString::UString(const char* c)
    : m_rep(Rep::create(c))
{
}

UString::UString(const char* c, unsigned length)
    : m_rep(Rep::create(c, length))
{
}

UString::UString(const UChar* c, unsigned length)
    : m_rep(Rep::create(c, length))
{
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

UString UString::from(long long i)
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

UString UString::from(unsigned u)
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

UString UString::from(long l)
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

UString UString::from(double d)
{
    DtoaBuffer buffer;
    unsigned length;
    doubleToStringInJavaScriptFormat(d, buffer, &length);
    return UString(buffer, length);
}

char* UString::ascii() const
{
    static char* asciiBuffer = 0;

    unsigned length = size();
    unsigned neededSize = length + 1;
    delete[] asciiBuffer;
    asciiBuffer = new char[neededSize];

    const UChar* p = data();
    char* q = asciiBuffer;
    const UChar* limit = p + length;
    while (p != limit) {
        *q = static_cast<char>(p[0]);
        ++p;
        ++q;
    }
    *q = '\0';

    return asciiBuffer;
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

UChar UString::operator[](unsigned pos) const
{
    if (pos >= size())
        return '\0';
    return data()[pos];
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
    unsigned size = this->size();

    if (size == 1) {
        UChar c = data()[0];
        if (isASCIIDigit(c))
            return c - '0';
        if (isStrWhiteSpace(c) && tolerateEmptyString)
            return 0;
        return NaN;
    }

    // FIXME: If tolerateTrailingJunk is true, then we want to tolerate junk 
    // after the number, even if it contains invalid UTF-16 sequences. So we
    // shouldn't use the UTF8String function, which returns null when it
    // encounters invalid UTF-16. Further, we have no need to convert the
    // non-ASCII characters to UTF-8, so the UTF8String does quite a bit of
    // unnecessary work.

    // FIXME: The space skipping code below skips only ASCII spaces, but callers
    // need to skip all StrWhiteSpace. The isStrWhiteSpace function does the
    // right thing but requires UChar, not char, for its argument.

    const UChar* data = this->data();
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
    unsigned len = m_rep->length();
    if (len == 0)
        return 0;
    const UChar* p = m_rep->characters();
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

unsigned UString::find(const UString& f, unsigned pos) const
{
    unsigned fsz = f.size();

    if (fsz == 1) {
        UChar ch = f[0];
        const UChar* end = data() + size();
        for (const UChar* c = data() + pos; c < end; c++) {
            if (*c == ch)
                return static_cast<unsigned>(c - data());
        }
        return NotFound;
    }

    unsigned sz = size();
    if (sz < fsz)
        return NotFound;
    if (fsz == 0)
        return pos;
    const UChar* end = data() + sz - fsz;
    unsigned fsizeminusone = (fsz - 1) * sizeof(UChar);
    const UChar* fdata = f.data();
    unsigned short fchar = fdata[0];
    ++fdata;
    for (const UChar* c = data() + pos; c <= end; c++) {
        if (c[0] == fchar && !memcmp(c + 1, fdata, fsizeminusone))
            return static_cast<unsigned>(c - data());
    }

    return NotFound;
}

unsigned UString::find(UChar ch, unsigned pos) const
{
    const UChar* end = data() + size();
    for (const UChar* c = data() + pos; c < end; c++) {
        if (*c == ch)
            return static_cast<unsigned>(c - data());
    }

    return NotFound;
}

unsigned UString::rfind(const UString& f, unsigned pos) const
{
    unsigned sz = size();
    unsigned fsz = f.size();
    if (sz < fsz)
        return NotFound;
    if (pos > sz - fsz)
        pos = sz - fsz;
    if (fsz == 0)
        return pos;
    unsigned fsizeminusone = (fsz - 1) * sizeof(UChar);
    const UChar* fdata = f.data();
    for (const UChar* c = data() + pos; c >= data(); c--) {
        if (*c == *fdata && !memcmp(c + 1, fdata + 1, fsizeminusone))
            return static_cast<unsigned>(c - data());
    }

    return NotFound;
}

unsigned UString::rfind(UChar ch, unsigned pos) const
{
    if (isEmpty())
        return NotFound;
    if (pos + 1 >= size())
        pos = size() - 1;
    for (const UChar* c = data() + pos; c >= data(); c--) {
        if (*c == ch)
            return static_cast<unsigned>(c - data());
    }

    return NotFound;
}

UString UString::substr(unsigned pos, unsigned len) const
{
    unsigned s = size();

    if (pos >= s)
        pos = s;
    unsigned limit = s - pos;
    if (len > limit)
        len = limit;

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
    const unsigned l1 = s1.size();
    const unsigned l2 = s2.size();
    const unsigned lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.data();
    const UChar* c2 = s2.data();
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
    const unsigned l1 = s1.size();
    const unsigned l2 = s2.size();
    const unsigned lmin = l1 < l2 ? l1 : l2;
    const UChar* c1 = s1.data();
    const UChar* c2 = s2.data();
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

CString UString::UTF8String(bool strict) const
{
    // Allocate a buffer big enough to hold all the characters.
    const unsigned length = size();
    Vector<char, 1024> buffer(length * 3);

    // Convert to runs of 8-bit characters.
    char* p = buffer.data();
    const UChar* d = reinterpret_cast<const UChar*>(&data()[0]);
    ConversionResult result = convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size(), strict);
    if (result != conversionOK)
        return CString();

    return CString(buffer.data(), p - buffer.data());
}

} // namespace JSC
