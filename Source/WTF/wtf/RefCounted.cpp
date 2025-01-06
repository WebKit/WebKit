/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include <wtf/RefCounted.h>
#include <wtf/StackShot.h>

#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

bool RefCountedBase::areThreadingChecksEnabledGlobally { false };

static_assert(sizeof(RefCountedBase) == sizeof(ThreadSafeRefCountedBase));

class RefLogStackShot : public StackShot {
    static const size_t s_size = 18;
    static const size_t s_skip = 6;
public:
    RefLogStackShot(const void* ptr)
        : StackShot(s_size)
        , m_ptr(ptr)
    {
    }

    const void* ptr() { return m_ptr; }

    void print()
    {
        if (size() < s_skip)
            return;
        WTFPrintBacktrace(span().subspan(s_skip));
    }

private:
    const void* m_ptr;
};

class RefLogSingleton {
public:
    static void append(const void* ptr)
    {
        RefLogStackShot* stackShot = new RefLogStackShot(ptr);

        size_t index = s_end.fetch_add(1, std::memory_order_acquire) & s_sizeMask;
        if (RefLogStackShot* old = s_buffer[index].exchange(nullptr, std::memory_order_acquire))
            delete old;

        // Other threads may have raced ahead and filled the log. If so, our stack shot is oldest, so we drop it.
        RefLogStackShot* expected = nullptr;
        if (!s_buffer[index].compare_exchange_strong(expected, stackShot, std::memory_order_release))
            delete stackShot;
    }

    template<typename Callback>
    static void forEachLIFO(const Callback& callback)
    {
        size_t last = s_end.load(std::memory_order_acquire) - 1;
        for (size_t i = 0; i < s_size; ++i) {
            size_t index = (last - i) & s_sizeMask;
            RefLogStackShot* stackShot = s_buffer[index].load(std::memory_order_acquire);
            if (!stackShot)
                continue;
            callback(stackShot);
        }
    }

private:
    static constexpr size_t s_size = 512; // Keep the log short to decrease the odds of logging a previous alias at the same address.
    static constexpr size_t s_sizeMask = s_size - 1;

    static std::atomic<size_t> s_end;
    static std::array<std::atomic<RefLogStackShot*>, s_size> s_buffer;
};

std::atomic<size_t> RefLogSingleton::s_end;
std::array<std::atomic<RefLogStackShot*>, RefLogSingleton::s_size> RefLogSingleton::s_buffer;

void RefCountedBase::logRefDuringDestruction(const void* ptr)
{
    RefLogSingleton::append(ptr);
}

void RefCountedBase::printRefDuringDestructionLogAndCrash(const void* ptr)
{
    WTFLogAlways("Error: Dangling RefPtr: %p", ptr);
    WTFLogAlways("This means that a ref() during destruction was not balanced by a deref() before destruction ended.");
    WTFLogAlways("Below are the most recent ref()'s during destruction for this address.");

    RefLogSingleton::forEachLIFO([&] (RefLogStackShot* stackShot) {
        if (stackShot->ptr() != ptr)
            return;

        WTFLogAlways(" ");

        stackShot->print();
    });

    CRASH_WITH_SECURITY_IMPLICATION();
}

}
