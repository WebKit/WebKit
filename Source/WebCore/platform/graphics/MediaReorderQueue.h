/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "MediaSample.h"
#include <wtf/Compiler.h>
#include <wtf/Deque.h>

namespace WebCore {

template <class T, class Compare = std::less_equal<T>>
class MediaReorderQueue {
public:
    MediaReorderQueue() = default;

    using ContainerType = Deque<T>;
    ContainerType::iterator begin() LIFETIME_BOUND { return m_queue.begin(); }
    ContainerType::iterator end() LIFETIME_BOUND { return m_queue.end(); }
    ContainerType::const_iterator begin() const LIFETIME_BOUND { return m_queue.begin(); }
    ContainerType::const_iterator end() const LIFETIME_BOUND { return m_queue.end(); }
    ContainerType::reverse_iterator rbegin() LIFETIME_BOUND { return ContainerType::reverse_iterator(end()); }
    ContainerType::reverse_iterator rend() LIFETIME_BOUND { return ContainerType::reverse_iterator(begin()); }
    ContainerType::const_reverse_iterator rbegin() const LIFETIME_BOUND { return ContainerType::const_reverse_iterator(end()); }
    ContainerType::const_reverse_iterator rend() const LIFETIME_BOUND { return ContainerType::const_reverse_iterator(begin()); }

    bool isEmpty() const { return m_queue.isEmpty(); }
    size_t size() const { return m_queue.size(); }

    void append(T&& object)
    {
        m_queue.append(std::forward<T>(object));
        auto begin = this->begin();
        auto iter = std::prev(end());
        while (iter != begin) {
            auto prev = std::prev(iter);
            if (m_compare(*prev, *iter))
                return;
            std::swap(*prev, *iter);
            iter = prev;
        }
    }

    std::optional<T> takeIfAvailable(bool& moreObjectsAvailable)
    {
        if (!isAvailable()) {
            moreObjectsAvailable = false;
            return { };
        }

        T object = m_queue.takeFirst();
        moreObjectsAvailable = isAvailable();
        return object;
    }

    T first() const
    {
        ASSERT(!isEmpty());
        return m_queue.first();
    }

    T last() const
    {
        ASSERT(!isEmpty());
        return m_queue.last();
    }

    T takeFirst()
    {
        ASSERT(!isEmpty());
        return m_queue.takeFirst();
    }

    void removeFirst()
    {
        ASSERT(!isEmpty());
        m_queue.removeFirst();
    }

    void clear() { m_queue.clear(); }
    uint8_t reorderSize() const { return m_reorderSize; }
    void setReorderSize(uint8_t size) { m_reorderSize = size; }
    bool isAvailable() const { return m_queue.size() > m_reorderSize; }

private:
    ContainerType m_queue;
    uint8_t m_reorderSize { 0 };
    NO_UNIQUE_ADDRESS Compare m_compare;
};

struct MediaSampleReorderQueueComparator {
    bool operator()(const Ref<const MediaSample>& a, const Ref<const MediaSample>& b) const { return a->presentationTime() <= b->presentationTime(); }
};

using MediaSampleReorderQueue = MediaReorderQueue<Ref<const MediaSample>, MediaSampleReorderQueueComparator>;

}
