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
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if PLATFORM(CF)
#include <wtf/RetainPtr.h>
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSData;
#else
class NSData;
#endif

#endif

namespace WebCore {
    
class PurgeableBuffer;

class SharedBuffer : public RefCounted<SharedBuffer> {
public:
    static PassRefPtr<SharedBuffer> create() { return adoptRef(new SharedBuffer); }
    static PassRefPtr<SharedBuffer> create(const char* c, int i) { return adoptRef(new SharedBuffer(c, i)); }
    static PassRefPtr<SharedBuffer> create(const unsigned char* c, int i) { return adoptRef(new SharedBuffer(c, i)); }

    static PassRefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath);

    static PassRefPtr<SharedBuffer> adoptVector(Vector<char>& vector);
    
    // The buffer must be in non-purgeable state before adopted to a SharedBuffer. 
    // It will stay that way until released.
    static PassRefPtr<SharedBuffer> adoptPurgeableBuffer(PurgeableBuffer* buffer);
    
    ~SharedBuffer();
    
#if PLATFORM(MAC)
    NSData *createNSData();
    static PassRefPtr<SharedBuffer> wrapNSData(NSData *data);
#endif
#if PLATFORM(CF)
    CFDataRef createCFData();
    static PassRefPtr<SharedBuffer> wrapCFData(CFDataRef);
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
    
    bool hasPurgeableBuffer() const { return m_purgeableBuffer.get(); }

    // Ensure this buffer has no other clients before calling this.
    PurgeableBuffer* releasePurgeableBuffer();
    
private:
    SharedBuffer();
    SharedBuffer(const char*, int);
    SharedBuffer(const unsigned char*, int);
    
    void clearPlatformData();
    void maybeTransferPlatformData();
    bool hasPlatformData() const;
    
    Vector<char> m_buffer;
    OwnPtr<PurgeableBuffer> m_purgeableBuffer;
#if PLATFORM(CF)
    SharedBuffer(CFDataRef);
    RetainPtr<CFDataRef> m_cfData;
#endif
};
    
}

#endif
