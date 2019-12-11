/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/MathExtras.h>
#include <wtf/Vector.h>

namespace WTF {

// This class implements a basic priority queue. The class is backed as a binary heap, like std::priority_queue.
// PriorityQueue has a couple of advantages over std::priority_queue:
//
// 1) The backing vector is fastMalloced.
// 2) You can iterate the elements.
// 3) It has in-place decrease/increaseKey methods, although they are still O(n) rather than O(log(n)).

template<typename T, bool (*isHigherPriority)(const T&, const T&) = &isLessThan<T>, size_t inlineCapacity = 0>
class PriorityQueue final {
    WTF_MAKE_FAST_ALLOCATED;
    using BufferType = Vector<T, inlineCapacity>;
    using const_iterator = typename BufferType::const_iterator;
public:
    size_t size() const { return m_buffer.size(); }
    bool isEmpty() const { return m_buffer.isEmpty(); }

    void enqueue(T element)
    {
        size_t location = m_buffer.size();
        m_buffer.append(std::forward<T>(element));
        siftUp(location);
    }

    const T& peek() const { return m_buffer[0]; }
    T dequeue()
    {
        std::swap(m_buffer[0], m_buffer.last());
        T result = m_buffer.takeLast();
        siftDown(0);
        return result;
    }

    template<typename Functor>
    void decreaseKey(const Functor& desiredElement)
    {
        for (size_t i = 0; i < m_buffer.size(); ++i) {
            if (desiredElement(m_buffer[i])) {
                siftDown(i);
                return;
            }
        }
        ASSERT(isValidHeap());
    }

    template<typename Functor>
    void increaseKey(const Functor& desiredElement)
    {
        for (size_t i = 0; i < m_buffer.size(); ++i) {
            if (desiredElement(m_buffer[i])) {
                siftUp(i);
                return;
            }
        }
        ASSERT(isValidHeap());
    }

    const_iterator begin() const { return m_buffer.begin(); };
    const_iterator end() const { return m_buffer.end(); };

    bool isValidHeap() const
    {
        for (size_t i = 0; i < m_buffer.size(); ++i) {
            if (leftChildOf(i) < m_buffer.size() && !isHigherPriority(m_buffer[i], m_buffer[leftChildOf(i)]))
                return false;
            if (rightChildOf(i) < m_buffer.size() && !isHigherPriority(m_buffer[i], m_buffer[rightChildOf(i)]))
                return false;
        }
        return true;
    }

protected:
    static inline size_t parentOf(size_t location) { ASSERT(location); return (location - 1) / 2; }
    static constexpr size_t leftChildOf(size_t location) { return location * 2 + 1; }
    static constexpr size_t rightChildOf(size_t location) { return leftChildOf(location) + 1; }

    void siftUp(size_t location)
    {
        while (location) {
            auto parent = parentOf(location);
            if (isHigherPriority(m_buffer[parent], m_buffer[location]))
                return;

            std::swap(m_buffer[parent], m_buffer[location]);
            location = parent;
        }
    }

    void siftDown(size_t location)
    {
        while (leftChildOf(location) < m_buffer.size()) {
            size_t higherPriorityChild;
            if (LIKELY(rightChildOf(location) < m_buffer.size()))
                higherPriorityChild = isHigherPriority(m_buffer[leftChildOf(location)], m_buffer[rightChildOf(location)]) ? leftChildOf(location) : rightChildOf(location);
            else
                higherPriorityChild = leftChildOf(location);

            if (isHigherPriority(m_buffer[location], m_buffer[higherPriorityChild]))
                return;

            std::swap(m_buffer[location], m_buffer[higherPriorityChild]);
            location = higherPriorityChild;
        }
    }

    Vector<T, inlineCapacity> m_buffer;
};

} // namespace WTF

using WTF::PriorityQueue;
