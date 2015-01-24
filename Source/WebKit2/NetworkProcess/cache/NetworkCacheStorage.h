/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheStorage_h
#define NetworkCacheStorage_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheKey.h"
#include <WebCore/ResourceResponse.h>
#include <wtf/BloomFilter.h>
#include <wtf/Deque.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/OSObjectPtr.h>
#endif

namespace WebCore {
class SharedBuffer;
}

namespace IPC {
class ArgumentEncoder;
class ArgumentDecoder;
}

namespace WebKit {

class ShareableResource;

class NetworkCacheStorage {
    WTF_MAKE_NONCOPYABLE(NetworkCacheStorage);
public:
    static std::unique_ptr<NetworkCacheStorage> open(const String& cachePath);

    class Data {
    public:
        Data();
        Data(const uint8_t*, size_t);
#if PLATFORM(COCOA)
        explicit Data(OSObjectPtr<dispatch_data_t>);
#endif
        bool isNull() const;

        const uint8_t* data() const;
        size_t size() const { return m_size; }

#if PLATFORM(COCOA)
        dispatch_data_t dispatchData() const { return m_dispatchData.get(); }
#endif
    private:
#if PLATFORM(COCOA)
        mutable OSObjectPtr<dispatch_data_t> m_dispatchData;
#endif
        mutable const uint8_t* m_data;
        size_t m_size;
    };

    struct Entry {
        std::chrono::milliseconds timeStamp;
        Data header;
        Data body;
    };
    // This may call completion handler synchronously on failure.
    void retrieve(const NetworkCacheKey&, unsigned priority, std::function<bool (std::unique_ptr<Entry>)>);
    void store(const NetworkCacheKey&, const Entry&, std::function<void (bool success)>);

    void setMaximumSize(size_t);
    void clear();

    static const unsigned version = 1;

private:
    NetworkCacheStorage(const String& directoryPath);

    void initialize();
    void shrinkIfNeeded();

    void removeEntry(const NetworkCacheKey&);

    struct RetrieveOperation {
        NetworkCacheKey key;
        std::function<bool (std::unique_ptr<Entry>)> completionHandler;
    };
    void dispatchRetrieveOperation(const RetrieveOperation&);
    void dispatchPendingRetrieveOperations();

    const String m_directoryPath;

    size_t m_maximumSize { std::numeric_limits<size_t>::max() };

    BloomFilter<20> m_keyFilter;
    std::atomic<size_t> m_approximateEntryCount { 0 };
    std::atomic<bool> m_shrinkInProgress { false };

    Vector<Deque<RetrieveOperation>> m_pendingRetrieveOperationsByPriority;
    unsigned m_activeRetrieveOperationCount { 0 };

#if PLATFORM(COCOA)
    mutable OSObjectPtr<dispatch_queue_t> m_ioQueue;
    mutable OSObjectPtr<dispatch_queue_t> m_backgroundIOQueue;
#endif
};

}
#endif
#endif
