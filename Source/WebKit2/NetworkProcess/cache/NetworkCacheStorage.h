/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include <wtf/BloomFilter.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedBuffer;
}

namespace IPC {
class ArgumentEncoder;
class ArgumentDecoder;
}

namespace WebKit {

#if PLATFORM(COCOA)
template <typename T> class DispatchPtr;
template <typename T> DispatchPtr<T> adoptDispatch(T dispatchObject);

// FIXME: Use OSObjectPtr instead when it works with dispatch_data_t on all platforms.
template<typename T> class DispatchPtr {
public:
    DispatchPtr()
        : m_ptr(nullptr)
    {
    }
    DispatchPtr(T ptr)
        : m_ptr(ptr)
    {
        if (m_ptr)
            dispatch_retain(m_ptr);
    }
    DispatchPtr(const DispatchPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            dispatch_retain(m_ptr);
    }
    ~DispatchPtr()
    {
        if (m_ptr)
            dispatch_release(m_ptr);
    }

    DispatchPtr& operator=(const DispatchPtr& other)
    {
        auto copy = other;
        std::swap(m_ptr, copy.m_ptr);
        return *this;
    }

    T get() const { return m_ptr; }
    explicit operator bool() const { return m_ptr; }

    friend DispatchPtr adoptDispatch<T>(T);

private:
    struct Adopt { };
    DispatchPtr(Adopt, T data)
        : m_ptr(data)
    {
    }

    T m_ptr;
};

template <typename T> DispatchPtr<T> adoptDispatch(T dispatchObject)
{
    return DispatchPtr<T>(typename DispatchPtr<T>::Adopt { }, dispatchObject);
}
#endif

class NetworkCacheStorage {
    WTF_MAKE_NONCOPYABLE(NetworkCacheStorage);
public:
    static std::unique_ptr<NetworkCacheStorage> open(const String& cachePath);

    class Data {
    public:
        Data() { }
        Data(const uint8_t*, size_t);

        enum class Backing { Buffer, Map };
#if PLATFORM(COCOA)
        explicit Data(DispatchPtr<dispatch_data_t>, Backing = Backing::Buffer);
#endif
        bool isNull() const;

        const uint8_t* data() const;
        size_t size() const { return m_size; }
        bool isMap() const { return m_isMap; }

#if PLATFORM(COCOA)
        dispatch_data_t dispatchData() const { return m_dispatchData.get(); }
#endif
    private:
#if PLATFORM(COCOA)
        mutable DispatchPtr<dispatch_data_t> m_dispatchData;
#endif
        mutable const uint8_t* m_data { nullptr };
        size_t m_size { 0 };
        bool m_isMap { false };
    };

    struct Entry {
        std::chrono::milliseconds timeStamp;
        Data header;
        Data body;
    };
    // This may call completion handler synchronously on failure.
    typedef std::function<bool (std::unique_ptr<Entry>)> RetrieveCompletionHandler;
    void retrieve(const NetworkCacheKey&, unsigned priority, RetrieveCompletionHandler&&);

    typedef std::function<void (bool success, const Data& mappedBody)> StoreCompletionHandler;
    void store(const NetworkCacheKey&, const Entry&, StoreCompletionHandler&&);
    void update(const NetworkCacheKey&, const Entry& updateEntry, const Entry& existingEntry, StoreCompletionHandler&&);

    void setMaximumSize(size_t);
    void clear();

    static const unsigned version = 2;

    const String& directoryPath() const { return m_directoryPath; }

private:
    NetworkCacheStorage(const String& directoryPath);

    void initialize();
    void deleteOldVersions();
    void shrinkIfNeeded();

    void removeEntry(const NetworkCacheKey&);

    struct ReadOperation {
        NetworkCacheKey key;
        RetrieveCompletionHandler completionHandler;
    };
    void dispatchReadOperation(const ReadOperation&);
    void dispatchPendingReadOperations();

    struct WriteOperation {
        NetworkCacheKey key;
        Entry entry;
        Optional<Entry> existingEntry;
        StoreCompletionHandler completionHandler;
    };
    void dispatchFullWriteOperation(const WriteOperation&);
    void dispatchHeaderWriteOperation(const WriteOperation&);
    void dispatchPendingWriteOperations();

    const String m_baseDirectoryPath;
    const String m_directoryPath;

    size_t m_maximumSize { std::numeric_limits<size_t>::max() };

    BloomFilter<20> m_contentsFilter;
    std::atomic<size_t> m_approximateSize { 0 };
    std::atomic<bool> m_shrinkInProgress { false };

    static const int maximumRetrievePriority = 4;
    Deque<std::unique_ptr<const ReadOperation>> m_pendingReadOperationsByPriority[maximumRetrievePriority + 1];
    HashSet<std::unique_ptr<const ReadOperation>> m_activeReadOperations;

    Deque<std::unique_ptr<const WriteOperation>> m_pendingWriteOperations;
    HashSet<std::unique_ptr<const WriteOperation>> m_activeWriteOperations;

#if PLATFORM(COCOA)
    mutable DispatchPtr<dispatch_queue_t> m_ioQueue;
    mutable DispatchPtr<dispatch_queue_t> m_backgroundIOQueue;
#endif
};

}
#endif
#endif
