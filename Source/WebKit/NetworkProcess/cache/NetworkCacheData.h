/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/FunctionDispatcher.h>
#include <wtf/SHA1.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/OSObjectPtr.h>
#endif

#if USE(SOUP)
#include <WebCore/GRefPtrSoup.h>
#endif

namespace WebKit {

class SharedMemory;

namespace NetworkCache {

class Data {
public:
    Data() { }
    Data(const uint8_t*, size_t);

    ~Data() { }

    static Data empty();
    static Data adoptMap(void* map, size_t, int fd);

#if PLATFORM(COCOA)
    enum class Backing { Buffer, Map };
    Data(OSObjectPtr<dispatch_data_t>&&, Backing = Backing::Buffer);
#endif
#if USE(SOUP)
    Data(GRefPtr<SoupBuffer>&&, int fd = -1);
#endif
    bool isNull() const;
    bool isEmpty() const { return !m_size; }

    const uint8_t* data() const;
    size_t size() const { return m_size; }
    bool isMap() const { return m_isMap; }
    RefPtr<SharedMemory> tryCreateSharedMemory() const;

    Data subrange(size_t offset, size_t) const;

    bool apply(const Function<bool (const uint8_t*, size_t)>&) const;

    Data mapToFile(const char* path) const;

#if PLATFORM(COCOA)
    dispatch_data_t dispatchData() const { return m_dispatchData.get(); }
#endif

#if USE(SOUP)
    SoupBuffer* soupBuffer() const { return m_buffer.get(); }
#endif
private:
#if PLATFORM(COCOA)
    mutable OSObjectPtr<dispatch_data_t> m_dispatchData;
#endif
#if USE(SOUP)
    mutable GRefPtr<SoupBuffer> m_buffer;
    int m_fileDescriptor { -1 };
#endif
    mutable const uint8_t* m_data { nullptr };
    size_t m_size { 0 };
    bool m_isMap { false };
};

Data concatenate(const Data&, const Data&);
bool bytesEqual(const Data&, const Data&);
Data adoptAndMapFile(int fd, size_t offset, size_t);
Data mapFile(const char* path);

using Salt = std::array<uint8_t, 8>;

Optional<Salt> readOrMakeSalt(const String& path);
SHA1::Digest computeSHA1(const Data&, const Salt&);

}

}
