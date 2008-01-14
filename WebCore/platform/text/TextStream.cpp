/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TextStream.h"

#include "DeprecatedString.h"
#include "Logging.h"
#include "PlatformString.h"
#include <wtf/Vector.h>

namespace WebCore {

const size_t integerOrPointerAsStringBufferSize = 100; // large enough for any integer or pointer in string format, including trailing null character
const char* const precisionFormats[7] = { "%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f", "%.6f"}; 
const int maxPrecision = 6; // must match size of precisionFormats
const int defaultPrecision = 6; // matches qt and sprintf(.., "%f", ...) behaviour

TextStream::TextStream(DeprecatedString* s)
    : m_hasByteArray(false), m_string(s), m_precision(defaultPrecision)
{
}

TextStream& TextStream::operator<<(char c)
{
    if (m_hasByteArray)
        m_byteArray.append(c);

    if (m_string)
        m_string->append(DeprecatedChar(c));
    return *this;
}

TextStream& TextStream::operator<<(short i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(unsigned short i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(int i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(unsigned i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(long i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%ld", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(unsigned long i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%lu", i);
    return *this << buffer;
}

TextStream& TextStream::operator<<(float f)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, precisionFormats[m_precision], f);
    return *this << buffer;
}

TextStream& TextStream::operator<<(double d)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, precisionFormats[m_precision], d);
    return *this << buffer;
}

TextStream& TextStream::operator<<(const char* s)
{
    if (m_hasByteArray) {
        unsigned length = strlen(s);
        unsigned oldSize = m_byteArray.size();
        m_byteArray.grow(oldSize + length);
        memcpy(m_byteArray.data() + oldSize, s, length);
    }
    if (m_string)
        m_string->append(s);
    return *this;
}

TextStream& TextStream::operator<<(const DeprecatedString& s)
{
    if (m_hasByteArray) {
        unsigned length = s.length();
        unsigned oldSize = m_byteArray.size();
        m_byteArray.grow(oldSize + length);
        memcpy(m_byteArray.data() + oldSize, s.latin1(), length);
    }
    if (m_string)
        m_string->append(s);
    return *this;
}

TextStream& TextStream::operator<<(const String& s)
{
    return (*this) << s.deprecatedString();
}

TextStream& TextStream::operator<<(void* p)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%p", p);
    return *this << buffer;
}

TextStream& TextStream::operator<<(const TextStreamManipulator& m) 
{
    return m(*this);
}

int TextStream::precision(int p) 
{
    int oldPrecision = m_precision;
    
    if (p >= 0 && p <= maxPrecision)
        m_precision = p;

    return oldPrecision;
}

TextStream &endl(TextStream& stream)
{
    return stream << '\n';
}

}
