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

class IntPoint;
class IntRect;
class FloatPoint;
class FloatSize;
class LayoutPoint;
class LayoutRect;
class LayoutUnit;

class TextStream {
public:
    struct FormatNumberRespectingIntegers {
        FormatNumberRespectingIntegers(double number) : value(number) { }
        double value;
    };

    WEBCORE_EXPORT TextStream& operator<<(bool);
    WEBCORE_EXPORT TextStream& operator<<(int);
    WEBCORE_EXPORT TextStream& operator<<(unsigned);
    TextStream& operator<<(long);
    TextStream& operator<<(unsigned long);
    TextStream& operator<<(long long);
    WEBCORE_EXPORT TextStream& operator<<(unsigned long long);
    WEBCORE_EXPORT TextStream& operator<<(float);
    WEBCORE_EXPORT TextStream& operator<<(double);
    WEBCORE_EXPORT TextStream& operator<<(const char*);
    WEBCORE_EXPORT TextStream& operator<<(const void*);
    WEBCORE_EXPORT TextStream& operator<<(const String&);
    TextStream& operator<<(const FormatNumberRespectingIntegers&);

    WEBCORE_EXPORT TextStream& operator<<(const IntPoint&);
    WEBCORE_EXPORT TextStream& operator<<(const IntRect&);
    WEBCORE_EXPORT TextStream& operator<<(const FloatPoint&);
    WEBCORE_EXPORT TextStream& operator<<(const FloatSize&);
    TextStream& operator<<(const LayoutUnit&);
    TextStream& operator<<(const LayoutPoint&);
    TextStream& operator<<(const LayoutRect&);

    template<typename Item>
    TextStream& operator<<(const Vector<Item>& vector)
    {
        *this << "[";

        unsigned size = vector.size();
        for (unsigned i = 0; i < size; ++i) {
            *this << vector[i];
            if (i < size - 1)
                *this << ", ";
        }

        return *this << "]";
    }

    WEBCORE_EXPORT String release();

private:
    StringBuilder m_text;
};

void writeIndent(TextStream&, int indent);

}

#endif
