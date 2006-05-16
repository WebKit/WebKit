/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#ifndef FONT_FAMILY_H
#define FONT_FAMILY_H

#include "AtomicString.h"

#if __APPLE__
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif

namespace WebCore {

class FontFamily {
public:
    FontFamily();
    ~FontFamily();
    
    FontFamily(const FontFamily &);    
    FontFamily &operator=(const FontFamily &);
        
    void setFamily(const AtomicString &);
    const AtomicString& family() const { return m_family; }
    bool familyIsEmpty() const { return m_family.isEmpty(); }

    FontFamily *next() { return m_next; }
    const FontFamily *next() const { return m_next; }

    void appendFamily(FontFamily *family) 
    {
        if (family)
            family->ref();
        if (m_next) 
            m_next->deref(); 
        m_next = family; 
    }
    
    bool operator==(const FontFamily &) const;
    bool operator!=(const FontFamily &x) const { return !(*this == x); }
    
    void ref() { m_refCnt++; }
    void deref() { m_refCnt--; if (m_refCnt == 0) delete this; }
    
private:
    AtomicString m_family;
    FontFamily* m_next;
    int m_refCnt;
};

}

#endif
