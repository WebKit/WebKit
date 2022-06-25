/*
 * Copyright (C) 2010-2022 Apple Inc. All Rights Reserved.
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
 *
 */

#pragma once

#include <algorithm>
#include <wtf/StackPointer.h>
#include <wtf/ThreadingPrimitives.h>

namespace WTF {

class StackBounds {
    WTF_MAKE_FAST_ALLOCATED;
public:

    // This 64k number was picked because a sampling of stack usage differences
    // between consecutive entries into one of the Interpreter::execute...()
    // functions was seen to be as high as 27k. Hence, 64k is chosen as a
    // conservative availability value that is not too large but comfortably
    // exceeds 27k with some buffer for error.
#if !ASAN_ENABLED
    static constexpr size_t DefaultReservedZone = 64 * 1024;
#else
    // ASAN inflates stack frames a lot. A factor of 3 was empirically found to
    // be the ratio of inflation of stack usage between 2 consecutive stack
    // recursion checkpoints. So, we'll also multiply the reserved zone size
    // accordingly to accommodate this.
    static constexpr size_t DefaultReservedZone = 64 * 1024 * 3;
#endif

    static constexpr StackBounds emptyBounds() { return StackBounds(); }

#if HAVE(STACK_BOUNDS_FOR_NEW_THREAD)
    // This function is only effective for newly created threads. In some platform, it returns a bogus value for the main thread.
    static StackBounds newThreadStackBounds(PlatformThreadHandle);
#endif
    static StackBounds currentThreadStackBounds()
    {
        auto result = currentThreadStackBoundsInternal();
        result.checkConsistency();
        return result;
    }

    void* origin() const
    {
        ASSERT(m_origin);
        return m_origin;
    }

    void* end() const
    {
        ASSERT(m_bound);
        return m_bound;
    }
    
    size_t size() const
    {
        return static_cast<char*>(m_origin) - static_cast<char*>(m_bound);
    }

    bool isEmpty() const { return !m_origin; }

    bool contains(void* p) const
    {
        if (isEmpty())
            return false;
        return (m_origin >= p) && (p > m_bound);
    }

    void* recursionLimit(size_t minReservedZone = DefaultReservedZone) const
    {
        checkConsistency();
        return static_cast<char*>(m_bound) + minReservedZone;
    }

    void* recursionLimit(char* startOfUserStack, size_t maxUserStack, size_t reservedZoneSize) const
    {
        checkConsistency();
        if (maxUserStack < reservedZoneSize)
            reservedZoneSize = maxUserStack;
        size_t maxUserStackWithReservedZone = maxUserStack - reservedZoneSize;

        char* endOfStackWithReservedZone = reinterpret_cast<char*>(m_bound) + reservedZoneSize;
        if (startOfUserStack < endOfStackWithReservedZone)
            return endOfStackWithReservedZone;
        size_t availableUserStack = startOfUserStack - endOfStackWithReservedZone;
        if (maxUserStackWithReservedZone > availableUserStack)
            maxUserStackWithReservedZone = availableUserStack;
        return startOfUserStack - maxUserStackWithReservedZone;
    }

    StackBounds withSoftOrigin(void* origin) const
    {
        ASSERT(contains(origin));
        return StackBounds(origin, m_bound);
    }

private:
    StackBounds(void* origin, void* end)
        : m_origin(origin)
        , m_bound(end)
    {
        ASSERT(isGrowingDownwards());
    }

    constexpr StackBounds()
        : m_origin(nullptr)
        , m_bound(nullptr)
    {
    }

    inline bool isGrowingDownwards() const
    {
        ASSERT(m_origin && m_bound);
        return m_bound <= m_origin;
    }

    WTF_EXPORT_PRIVATE static StackBounds currentThreadStackBoundsInternal();

    void checkConsistency() const
    {
#if ASSERT_ENABLED
        void* currentPosition = currentStackPointer();
        ASSERT(m_origin != m_bound);
        ASSERT(currentPosition < m_origin && currentPosition > m_bound);
#endif
    }

    void* m_origin;
    void* m_bound;

    friend class StackStats;
};

} // namespace WTF

using WTF::StackBounds;
