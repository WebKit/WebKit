/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ResourceBuffer_h
#define ResourceBuffer_h

#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#if USE(FOUNDATION)
OBJC_CLASS NSData;
#endif

namespace WebCore {

class SharedBuffer;

class ResourceBuffer : public RefCounted<ResourceBuffer> {
public:

    static PassRefPtr<ResourceBuffer> create() { return adoptRef(new ResourceBuffer); }
    static PassRefPtr<ResourceBuffer> create(const char* data, unsigned size) { return adoptRef(new ResourceBuffer(data, size)); }
    static PassRefPtr<ResourceBuffer> adoptSharedBuffer(PassRefPtr<SharedBuffer>);

    WEBCORE_EXPORT virtual ~ResourceBuffer();

    WEBCORE_EXPORT virtual const char* data() const;
    WEBCORE_EXPORT virtual unsigned size() const;
    WEBCORE_EXPORT virtual bool isEmpty() const;

    WEBCORE_EXPORT void append(const char*, unsigned);
    void append(SharedBuffer*);
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    void append(CFDataRef);
#endif
    void clear();
    
    unsigned getSomeData(const char*& data, unsigned position = 0) const;
    
    WEBCORE_EXPORT SharedBuffer* sharedBuffer() const;
#if USE(FOUNDATION)
    void tryReplaceSharedBufferContents(SharedBuffer*);
#endif
    PassRefPtr<ResourceBuffer> copy() const;

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData();
#endif
#if USE(CF)
    RetainPtr<CFDataRef> createCFData();
#endif

protected:
    WEBCORE_EXPORT ResourceBuffer();

private:
    WEBCORE_EXPORT ResourceBuffer(const char*, unsigned);
    ResourceBuffer(PassRefPtr<SharedBuffer>);

    RefPtr<SharedBuffer> m_sharedBuffer;
};

} // namespace WebCore

#endif // ResourceBuffer_h
