/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "DeprecatedCString.h"

namespace WebCore {

CString::CString(const char* str)
{
    init(str, strlen(str));
}

CString::CString(const char* str, unsigned length)
{
    init(str, length);
}

CString::CString(const DeprecatedCString& str)
{
    init(str.data(), str.length());
}

void CString::init(const char* str, unsigned length)
{
    if (!str)
        return;
    
    m_buffer = new CStringBuffer(length + 1);
    memcpy(m_buffer->data(), str, length); 
    m_buffer->data()[length] = '\0';
}

char* CString::mutableData()
{
    copyBufferIfNeeded();
    if (!m_buffer)
        return 0;
    return m_buffer->data();
}
    
unsigned CString::length() const
{
    return m_buffer ? m_buffer->length() - 1 : 0;
}

DeprecatedCString CString::deprecatedCString() const
{
    return DeprecatedCString(data(), length() + 1);
}
    
CString CString::newUninitialized(size_t length, char*& characterBuffer)
{
    CString result;
    result.m_buffer = new CStringBuffer(length + 1);
    char* bytes = result.m_buffer->data();
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
    m_buffer = new CStringBuffer(len);
    memcpy(m_buffer->data(), m_temp->data(), len);
}

int CString::find(const char* substring, int index) const
{
    const char* str = data();
 
    if(str && str[0] && substring && index >=0) { // don't search empty strings
        // advance until we get to index
        int pos = 0;
        while(pos < index)
            if(str[pos++] == 0)
                return -1;                  // index is beyond end of str
        
        // now search from index onward
        while(str[index] != 0) {
            char a, b;
            
            // compare until we reach the end or a mismatch
            pos = 0;

            while((a = substring[pos]) && (b = str[index]) && a == b)
                pos++, index++;
            
            // reached the end of our compare string without a mismatch?
            if(substring[pos] == 0)
                return index - pos;
            
            index ++;
        }
    }
    
    return -1;
}

}
