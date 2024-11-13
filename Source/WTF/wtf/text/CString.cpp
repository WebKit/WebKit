/*
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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
#include <wtf/text/CString.h>

#include <string.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringCommon.h>
#include <wtf/text/SuperFastHash.h>

namespace WTF {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CStringBuffer);

Ref<CStringBuffer> CStringBuffer::createUninitialized(size_t length)
{
    // The +1 is for the terminating null character.
    size_t size = Checked<size_t>(sizeof(CStringBuffer)) + length + 1U;
    auto* stringBuffer = static_cast<CStringBuffer*>(CStringBufferMalloc::malloc(size));

    Ref buffer = adoptRef(*new (NotNull, stringBuffer) CStringBuffer(length));
    buffer->mutableSpanIncludingNullCharacter()[length] = '\0';
    return buffer;
}

CString::CString(const char* string)
{
    if (!string)
        return;

    init(WTF::span(string));
}

CString::CString(std::span<const char> string)
{
    if (!string.data()) {
        ASSERT(string.empty());
        return;
    }

    init(string);
}

void CString::init(std::span<const char> string)
{
    ASSERT(string.data());

    m_buffer = CStringBuffer::createUninitialized(string.size());
    memcpySpan(m_buffer->mutableSpan(), string);
}

char* CString::mutableData()
{
    copyBufferIfNeeded();
    if (!m_buffer)
        return nullptr;
    return m_buffer->mutableData();
}

CString CString::newUninitialized(size_t length, std::span<char>& characterBuffer)
{
    CString result;
    result.m_buffer = CStringBuffer::createUninitialized(length);
    characterBuffer = result.m_buffer->mutableSpan();
    return result;
}

void CString::copyBufferIfNeeded()
{
    if (!m_buffer || m_buffer->hasOneRef())
        return;

    RefPtr<CStringBuffer> buffer = WTFMove(m_buffer);
    size_t length = buffer->length();
    m_buffer = CStringBuffer::createUninitialized(length);
    memcpy(m_buffer->mutableData(), buffer->data(), length + 1);
}

bool CString::isSafeToSendToAnotherThread() const
{
    return !m_buffer || m_buffer->hasOneRef();
}

void CString::grow(size_t newLength)
{
    ASSERT(newLength > length());

    auto newBuffer = CStringBuffer::createUninitialized(newLength);
    memcpy(newBuffer->mutableData(), m_buffer->data(), length() + 1);
    m_buffer = WTFMove(newBuffer);
}

bool operator==(const CString& a, const CString& b)
{
    if (a.isNull() != b.isNull())
        return false;
    if (a.length() != b.length())
        return false;
    return equal(a.span().data(), b.span());
}

bool operator==(const CString& a, const char* b)
{
    if (a.isNull() != !b)
        return false;
    if (!b)
        return true;
    return !strcmp(a.data(), b);
}

unsigned CString::hash() const
{
    if (isNull())
        return 0;
    SuperFastHash hasher;
    for (auto character : span())
        hasher.addCharacter(character);
    return hasher.hash();
}

bool operator<(const CString& a, const CString& b)
{
    if (a.isNull())
        return !b.isNull();
    if (b.isNull())
        return false;
    return strcmp(a.data(), b.data()) < 0;
}

bool CStringHash::equal(const CString& a, const CString& b)
{
    if (a.isHashTableDeletedValue())
        return b.isHashTableDeletedValue();
    if (b.isHashTableDeletedValue())
        return false;
    return a == b;
}

} // namespace WTF
