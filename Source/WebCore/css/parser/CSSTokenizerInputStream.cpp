// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"
#include "CSSTokenizerInputStream.h"

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSTokenizerInputStream);

CSSTokenizerInputStream::CSSTokenizerInputStream(const String& input)
    : m_offset(0)
    , m_stringLength(input.length())
    , m_string(input.impl())
{
}

void CSSTokenizerInputStream::advanceUntilNonWhitespace()
{
    // Using ASCII whitespace here rather than CSS space since we don't do preprocessing
    if (m_string->is8Bit()) {
        const LChar* characters = m_string->characters8();
        while (m_offset < m_stringLength && isASCIIWhitespace(characters[m_offset]))
            ++m_offset;
    } else {
        const UChar* characters = m_string->characters16();
        while (m_offset < m_stringLength && isASCIIWhitespace(characters[m_offset]))
            ++m_offset;
    }
}

double CSSTokenizerInputStream::getDouble(unsigned start, unsigned end) const
{
    ASSERT(start <= end && ((m_offset + end) <= m_stringLength));
    bool isResultOK = false;
    double result = 0.0;
    if (start < end) {
        if (m_string->is8Bit())
            result = charactersToDouble(m_string->characters8() + m_offset + start, end - start, &isResultOK);
        else
            result = charactersToDouble(m_string->characters16() + m_offset + start, end - start, &isResultOK);
    }
    // FIXME: It looks like callers ensure we have a valid number
    return isResultOK ? result : 0.0;
}

} // namespace WebCore
