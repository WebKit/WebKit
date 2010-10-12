/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef UStringBuilder_h
#define UStringBuilder_h

#include <wtf/text/StringBuilder.h>

namespace JSC {

class UStringBuilder : public StringBuilder {
public:
    // Forward declare these methods, otherwhise append() is ambigious.
    void append(const UChar u) { StringBuilder::append(u); }
    void append(const char* str) { StringBuilder::append(str); }
    void append(const char* str, size_t len) { StringBuilder::append(str, len); }
    void append(const UChar* str, size_t len) { StringBuilder::append(str, len); }

    void append(const UString& str)
    {
        m_buffer.append(str.characters(), str.length());
    }

    UString toUString()
    {
        m_buffer.shrinkToFit();
        ASSERT(m_buffer.data() || !m_buffer.size());
        return UString::adopt(m_buffer);
    }
};

} // namespace JSC

#endif // UStringBuilder_h
