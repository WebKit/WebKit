/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef StringPrintStream_h
#define StringPrintStream_h

#include <wtf/PrintStream.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF {

class StringPrintStream : public PrintStream {
public:
    WTF_EXPORT_PRIVATE StringPrintStream();
    WTF_EXPORT_PRIVATE virtual ~StringPrintStream();
    
    virtual void vprintf(const char* format, va_list) override WTF_ATTRIBUTE_PRINTF(2, 0);
    
    WTF_EXPORT_PRIVATE CString toCString();
    WTF_EXPORT_PRIVATE String toString();
    WTF_EXPORT_PRIVATE void reset();
    
private:
    void increaseSize(size_t);
    
    char* m_buffer;
    size_t m_next;
    size_t m_size;
    char m_inlineBuffer[128];
};

// Stringify any type T that has a WTF::printInternal(PrintStream&, const T&)
template<typename T>
CString toCString(const T& value)
{
    StringPrintStream stream;
    stream.print(value);
    return stream.toCString();
}
template<typename T1, typename T2>
CString toCString(const T1& value1, const T2& value2)
{
    StringPrintStream stream;
    stream.print(value1, value2);
    return stream.toCString();
}
template<typename T1, typename T2, typename T3>
CString toCString(const T1& value1, const T2& value2, const T3& value3)
{
    StringPrintStream stream;
    stream.print(value1, value2, value3);
    return stream.toCString();
}

template<typename T1, typename T2, typename T3, typename T4>
CString toCString(const T1& value1, const T2& value2, const T3& value3, const T4& value4)
{
    StringPrintStream stream;
    stream.print(value1, value2, value3, value4);
    return stream.toCString();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5>
CString toCString(const T1& value1, const T2& value2, const T3& value3, const T4& value4, const T5& value5)
{
    StringPrintStream stream;
    stream.print(value1, value2, value3, value4, value5);
    return stream.toCString();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
CString toCString(const T1& value1, const T2& value2, const T3& value3, const T4& value4, const T5& value5, const T6& value6)
{
    StringPrintStream stream;
    stream.print(value1, value2, value3, value4, value5, value6);
    return stream.toCString();
}

template<typename T>
String toString(const T& value)
{
    StringPrintStream stream;
    stream.print(value);
    return stream.toString();
}

} // namespace WTF

using WTF::StringPrintStream;
using WTF::toCString;
using WTF::toString;

#endif // StringPrintStream_h

