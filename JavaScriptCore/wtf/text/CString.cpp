/*
 * Copyright (C) 2003, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "CString.h"

using std::min;

namespace WTF {

CString::CString(const char* str)
{
    init(str, strlen(str));
}

CString::CString(const char* str, unsigned length)
{
    init(str, length);
}

void CString::init(const char* str, unsigned length)
{
    if (!str)
        return;
    
    m_buffer = CStringBuffer::create(length + 1);
    memcpy(m_buffer->mutableData(), str, length); 
    m_buffer->mutableData()[length] = '\0';
}

char* CString::mutableData()
{
    copyBufferIfNeeded();
    if (!m_buffer)
        return 0;
    return m_buffer->mutableData();
}
    
CString CString::newUninitialized(size_t length, char*& characterBuffer)
{
    CString result;
    result.m_buffer = CStringBuffer::create(length + 1);
    char* bytes = result.m_buffer->mutableData();
    bytes[length] = '\0';
    characterBuffer = bytes;
    return result;
}

void CString::copyBufferIfNeeded()
{
    if (!m_buffer || m_buffer->hasOneRef())
        return;
        
    int len = m_buffer->length();
    RefPtr<CStringBuffer> m_temp = m_buffer;
    m_buffer = CStringBuffer::create(len);
    memcpy(m_buffer->mutableData(), m_temp->data(), len);
}

bool operator==(const CString& a, const CString& b)
{
    if (a.isNull() != b.isNull())
        return false;
    if (a.length() != b.length())
        return false;
    return !strncmp(a.data(), b.data(), min(a.length(), b.length()));
}

} // namespace WTF
