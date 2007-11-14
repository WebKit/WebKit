/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#ifndef SharedBuffer_h
#define SharedBuffer_h

#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class NSData;
#else
class NSData;
#endif

#endif

namespace WebCore {

class SharedBuffer : public RefCounted<SharedBuffer> {
public:
    SharedBuffer();
    SharedBuffer(const char*, int);
    SharedBuffer(const unsigned char*, int);

    static PassRefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath);
    
#if PLATFORM(MAC)
    NSData *createNSData();
    CFDataRef createCFData();
    static PassRefPtr<SharedBuffer> wrapNSData(NSData *data);
#endif
        
    const char* data() const;
    unsigned size() const;
    const Vector<char> &buffer() { return m_buffer; }

    bool isEmpty() const { return size() == 0; }

    void append(const char*, int);
    void clear();
    const char* platformData() const;
    unsigned platformDataSize() const;

    PassRefPtr<SharedBuffer> copy() const;
    
private:
    void clearPlatformData();
    void maybeTransferPlatformData();
    bool hasPlatformData() const;
    
    Vector<char> m_buffer;
#if PLATFORM(MAC)
    SharedBuffer(NSData *nsdata);
    RetainPtr<NSData> m_nsData;
#endif
};
    
}

#endif
