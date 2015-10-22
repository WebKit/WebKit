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

#ifndef TextStream_h
#define TextStream_h

#include <wtf/Forward.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class LayoutUnit;

class TextStream {
public:
    struct FormatNumberRespectingIntegers {
        FormatNumberRespectingIntegers(double number) : value(number) { }
        double value;
    };
    
    enum class LineMode { SingleLine, MultipleLine };
    TextStream(LineMode lineMode = LineMode::MultipleLine)
        : m_multiLineMode(lineMode == LineMode::MultipleLine)
    {
    }

    WEBCORE_EXPORT TextStream& operator<<(bool);
    WEBCORE_EXPORT TextStream& operator<<(int);
    WEBCORE_EXPORT TextStream& operator<<(unsigned);
    WEBCORE_EXPORT TextStream& operator<<(long);
    WEBCORE_EXPORT TextStream& operator<<(unsigned long);
    WEBCORE_EXPORT TextStream& operator<<(long long);
    WEBCORE_EXPORT TextStream& operator<<(LayoutUnit);

    WEBCORE_EXPORT TextStream& operator<<(unsigned long long);
    WEBCORE_EXPORT TextStream& operator<<(float);
    WEBCORE_EXPORT TextStream& operator<<(double);
    WEBCORE_EXPORT TextStream& operator<<(const char*);
    WEBCORE_EXPORT TextStream& operator<<(const void*);
    WEBCORE_EXPORT TextStream& operator<<(const String&);
    WEBCORE_EXPORT TextStream& operator<<(const FormatNumberRespectingIntegers&);

    template<typename T>
    void dumpProperty(const String& name, const T& value)
    {
        TextStream& ts = *this;
        ts.startGroup();
        ts << name << " " << value;
        ts.endGroup();
    }

    WEBCORE_EXPORT String release();
    
    WEBCORE_EXPORT void startGroup();
    WEBCORE_EXPORT void endGroup();
    WEBCORE_EXPORT void nextLine(); // Output newline and indent.

    WEBCORE_EXPORT void increaseIndent() { ++m_indent; }
    WEBCORE_EXPORT void decreaseIndent() { --m_indent; ASSERT(m_indent >= 0); }

    WEBCORE_EXPORT void writeIndent();

    class GroupScope {
    public:
        GroupScope(TextStream& ts)
            : m_stream(ts)
        {
            m_stream.startGroup();
        }
        ~GroupScope()
        {
            m_stream.endGroup();
        }

    private:
        TextStream& m_stream;
    };

private:
    StringBuilder m_text;
    int m_indent { 0 };
    bool m_multiLineMode { true };
};

template<typename Item>
TextStream& operator<<(TextStream& ts, const Vector<Item>& vector)
{
    ts << "[";

    unsigned size = vector.size();
    for (unsigned i = 0; i < size; ++i) {
        ts << vector[i];
        if (i < size - 1)
            ts << ", ";
    }

    return ts << "]";
}

void writeIndent(TextStream&, int indent);

}

#endif
