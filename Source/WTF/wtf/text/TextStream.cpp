/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <wtf/text/TextStream.h>

#include <wtf/text/WTFString.h>

namespace WTF {

static constexpr size_t printBufferSize = 100; // large enough for any integer or floating point value in string format, including trailing null character

static inline bool hasFractions(double val)
{
    static constexpr double s_epsilon = 0.0001;
    int ival = static_cast<int>(val);
    double dval = static_cast<double>(ival);
    return fabs(val - dval) > s_epsilon;
}

TextStream& TextStream::operator<<(bool b)
{
    return *this << (b ? "1" : "0");
}

TextStream& TextStream::operator<<(char c)
{
    m_text.append(c);
    return *this;
}

TextStream& TextStream::operator<<(int i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(unsigned i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(long i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(unsigned long i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(long long i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(unsigned long long i)
{
    m_text.append(i);
    return *this;
}

TextStream& TextStream::operator<<(float f)
{
    if (m_formattingFlags & Formatting::NumberRespectingIntegers)
        return *this << FormatNumberRespectingIntegers(f);

    m_text.append(FormattedNumber::fixedWidth(f, 2));
    return *this;
}

TextStream& TextStream::operator<<(double d)
{
    if (m_formattingFlags & Formatting::NumberRespectingIntegers)
        return *this << FormatNumberRespectingIntegers(d);

    m_text.append(FormattedNumber::fixedWidth(d, 2));
    return *this;
}

TextStream& TextStream::operator<<(const char* string)
{
    m_text.append(string);
    return *this;
}

TextStream& TextStream::operator<<(const void* p)
{
    char buffer[printBufferSize];
    snprintf(buffer, sizeof(buffer) - 1, "%p", p);
    return *this << buffer;
}

TextStream& TextStream::operator<<(const AtomString& string)
{
    m_text.append(string);
    return *this;
}

TextStream& TextStream::operator<<(const String& string)
{
    m_text.append(string);
    return *this;
}

TextStream& TextStream::operator<<(ASCIILiteral string)
{
    m_text.append(string);
    return *this;
}

TextStream& TextStream::operator<<(StringView string)
{
    m_text.append(string);
    return *this;
}

TextStream& TextStream::operator<<(const FormatNumberRespectingIntegers& numberToFormat)
{
    if (hasFractions(numberToFormat.value)) {
        m_text.append(FormattedNumber::fixedWidth(numberToFormat.value, 2));
        return *this;
    }

    m_text.append(static_cast<int>(numberToFormat.value));
    return *this;
}

String TextStream::release()
{
    String result = m_text.toString();
    m_text.clear();
    return result;
}

void TextStream::startGroup()
{
    TextStream& ts = *this;

    if (m_multiLineMode) {
        ts << "\n";
        ts.writeIndent();
        ts << "(";
        ts.increaseIndent();
    } else
        ts << " (";
}

void TextStream::endGroup()
{
    TextStream& ts = *this;
    ts << ")";
    if (m_multiLineMode)
        ts.decreaseIndent();
}

void TextStream::nextLine()
{
    TextStream& ts = *this;
    if (m_multiLineMode) {
        ts << "\n";
        ts.writeIndent();
    } else
        ts << " ";
}

void TextStream::writeIndent()
{
    if (m_multiLineMode)
        WTF::writeIndent(*this, m_indent);
}

void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i < indent; ++i)
        ts << "  ";
}

}
