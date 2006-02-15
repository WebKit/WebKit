/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include "FontFamily.h"
#include "StringImpl.h"

namespace WebCore {

FontFamily::FontFamily()
    : m_next(0)
    , m_refCnt(0)
    , m_CFFamily(0)
{
}

FontFamily::FontFamily(const FontFamily& other)
    : m_family(other.m_family)
    , m_next(other.m_next)
    , m_refCnt(0)
#if __APPLE__
    , m_CFFamily(other.m_CFFamily)
#endif
{
    if (m_next)
        m_next->ref();
}

FontFamily::~FontFamily()
{ 
    if (m_next)
        m_next->deref();
}
   
FontFamily& FontFamily::operator=(const FontFamily& other)
{
    if (other.m_next)
        other.m_next->ref();
    if (m_next)
        m_next->deref();
    m_family = other.m_family;
    m_next = other.m_next;
#if __APPLE__
    m_CFFamily = other.m_CFFamily;
#endif
    return *this;
}

void FontFamily::setFamily(const AtomicString &family)
{
    m_family = family;
#if __APPLE__
    m_CFFamily = 0;
#endif
}

bool FontFamily::operator==(const FontFamily &compareFontFamily) const
{
    if ((!m_next && compareFontFamily.m_next) || 
        (m_next && !compareFontFamily.m_next) ||
        ((m_next && compareFontFamily.m_next) && (*m_next != *(compareFontFamily.m_next))))
        return false;
    
    return m_family == compareFontFamily.m_family;
}

}
