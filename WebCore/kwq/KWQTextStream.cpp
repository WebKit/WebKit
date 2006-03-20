/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include "KWQTextStream.h"

#include "PlatformString.h"
#include "Logging.h"

const size_t integerOrPointerAsStringBufferSize = 100; // large enough for any integer or pointer in string format, including trailing null character
const char *precisionFormats[] = { "%.0f", "%.1f", "%.2f", "%.3f", "%.4f", "%.5f" "%.6f"}; 
const int maxPrecision = 6; // must match to precisionFormats
const int defaultPrecision = 6; // matches qt and sprintf(.., "%f", ...) behaviour

QTextStream::QTextStream(const DeprecatedByteArray& ba)
    : m_hasByteArray(true), m_byteArray(ba), m_string(0), m_precision(defaultPrecision)
{
}

QTextStream::QTextStream(DeprecatedString* s)
    : m_hasByteArray(false), m_string(s), m_precision(defaultPrecision)
{
}

QTextStream& QTextStream::operator<<(char c)
{
    if (m_hasByteArray) {
        unsigned oldSize = m_byteArray.size();
        m_byteArray.resize(oldSize + 1);
        m_byteArray[oldSize] = c;
    }
    if (m_string)
        m_string->append(QChar(c));
    return *this;
}

QTextStream& QTextStream::operator<<(short i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(unsigned short i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(int i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%d", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(unsigned i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%u", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(long i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%ld", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(unsigned long i)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%lu", i);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(float f)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, precisionFormats[m_precision], f);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(double d)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, precisionFormats[m_precision], d);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(const char* s)
{
    if (m_hasByteArray) {
        unsigned length = strlen(s);
        unsigned oldSize = m_byteArray.size();
        m_byteArray.resize(oldSize + length);
        memcpy(m_byteArray.data() + oldSize, s, length);
    }
    if (m_string)
        m_string->append(s);
    return *this;
}

QTextStream& QTextStream::operator<<(const DeprecatedCString& qcs)
{
    const char *s = qcs;
    return *this << s;
}

QTextStream& QTextStream::operator<<(const DeprecatedString& s)
{
    if (m_hasByteArray) {
        unsigned length = s.length();
        unsigned oldSize = m_byteArray.size();
        m_byteArray.resize(oldSize + length);
        memcpy(m_byteArray.data() + oldSize, s.latin1(), length);
    }
    if (m_string)
        m_string->append(s);
    return *this;
}

QTextStream& QTextStream::operator<<(const WebCore::String& s)
{
    return (*this) << s.deprecatedString();
}

QTextStream& QTextStream::operator<<(void* p)
{
    char buffer[integerOrPointerAsStringBufferSize];
    sprintf(buffer, "%p", p);
    return *this << buffer;
}

QTextStream& QTextStream::operator<<(const QTextStreamManipulator& m) 
{
    return m(*this);
}

int QTextStream::precision(int p) 
{
    int oldPrecision = m_precision;
    
    if (p >= 0 && p <= maxPrecision)
        m_precision = p;

    return oldPrecision;
}

QTextStream &endl(QTextStream& stream)
{
    return stream << '\n';
}
