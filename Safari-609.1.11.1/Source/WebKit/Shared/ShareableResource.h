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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShareableResource_h
#define ShareableResource_h

#if ENABLE(SHAREABLE_RESOURCE)

#include "SharedMemory.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class SharedBuffer;
}

namespace WebKit {
    
class ShareableResource : public RefCounted<ShareableResource> {
public:

    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();
        Handle(Handle&&) = default;
        Handle& operator=(Handle&&) = default;

        bool isNull() const { return m_handle.isNull(); }
        unsigned size() const { return m_size; }

        void encode(IPC::Encoder&) const;
        static bool decode(IPC::Decoder&, Handle&);

        RefPtr<WebCore::SharedBuffer> tryWrapInSharedBuffer() const;

    private:
        friend class ShareableResource;

        mutable SharedMemory::Handle m_handle;
        unsigned m_offset;
        unsigned m_size;
    };

    // Create a shareable resource that uses malloced memory.
    static Ref<ShareableResource> create(Ref<SharedMemory>&&, unsigned offset, unsigned size);

    // Create a shareable resource from a handle.
    static RefPtr<ShareableResource> map(const Handle&);

    // Create a handle.
    bool createHandle(Handle&);

    ~ShareableResource();

    const char* data() const;
    unsigned size() const;
    
private:
    ShareableResource(Ref<SharedMemory>&&, unsigned offset, unsigned size);
    RefPtr<WebCore::SharedBuffer> wrapInSharedBuffer();

    Ref<SharedMemory> m_sharedMemory;

    unsigned m_offset;
    unsigned m_size;    
};

} // namespace WebKit

#endif // ENABLE(SHAREABLE_RESOURCE)

#endif // ShareableResource_h
