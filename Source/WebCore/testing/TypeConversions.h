/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TypeConversions_h
#define TypeConversions_h

#include <wtf/FastMalloc.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class TypeConversions : public RefCounted<TypeConversions> {
public:
    static PassRefPtr<TypeConversions> create() { return adoptRef(new TypeConversions()); }

    long testLong() { return m_testLong; }
    void setTestLong(long value) { m_testLong = value; }
    unsigned long testUnsignedLong() { return m_testUnsignedLong; }
    void setTestUnsignedLong(unsigned long value) { m_testUnsignedLong = value; }

    long long testLongLong() { return m_testLongLong; }
    void setTestLongLong(long long value) { m_testLongLong = value; }
    unsigned long long testUnsignedLongLong() { return m_testUnsignedLongLong; }
    void setTestUnsignedLongLong(unsigned long long value) { m_testUnsignedLongLong = value; }

private:
    TypeConversions()
    {
    }

    long m_testLong;
    unsigned long m_testUnsignedLong;
    long long m_testLongLong;
    unsigned long long m_testUnsignedLongLong;
};

} // namespace WebCore

#endif
