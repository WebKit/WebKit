/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef BackingStore_h
#define BackingStore_h

#include "SharedMemory.h"
#include <WebCore/IntSize.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
    class GraphicsContext;
    class IntRect;
}

namespace WebKit {
    
class BackingStore : public RefCounted<BackingStore> {
public:
    // Create a backing store that uses malloced memory.
    static PassRefPtr<BackingStore> create(const WebCore::IntSize&);

    // Create a backing store whose backing memory can be shared with another process.
    static PassRefPtr<BackingStore> createSharable(const WebCore::IntSize&);

    // Create a backing store from a shared memory handle.
    static PassRefPtr<BackingStore> create(const WebCore::IntSize&, const SharedMemory::Handle&);

    // Create a shared memory handle.
    bool createHandle(SharedMemory::Handle&);

    ~BackingStore();

    const WebCore::IntSize& size() const { return m_size; }
    bool resize(const WebCore::IntSize& size);

    // Create a graphics context that can be used to paint into the backing store.
    PassOwnPtr<WebCore::GraphicsContext> createGraphicsContext();

    // Paint the backing store into the given context.
    void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);

private:
    BackingStore(const WebCore::IntSize&, void*);
    BackingStore(const WebCore::IntSize&, PassRefPtr<SharedMemory>);

    bool isBackedBySharedMemory() const { return m_sharedMemory; }
    void* data() const;

    WebCore::IntSize m_size;

    // If the backing store is backed by shared memory, this points to the shared memory object.
    RefPtr<SharedMemory> m_sharedMemory;

    // If the backing store is backed by fastMalloced memory, this points to the data.
    void* m_data;
};

} // namespace WebKit

#endif // BackingStore_h
