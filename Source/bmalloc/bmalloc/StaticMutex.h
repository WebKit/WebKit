/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#ifndef StaticMutex_h
#define StaticMutex_h

#include "Algorithm.h"
#include "BAssert.h"
#include "BExport.h"
#include <atomic>
#include <mutex>
#include <thread>

// A fast replacement for std::mutex, for use in static storage.

// Use StaticMutex in static storage, where global constructors and exit-time
// destructors are prohibited, but all memory is zero-initialized automatically.

// This uses the code from what WTF used to call WordLock. It's a fully adaptive mutex that uses
// sizeof(void*) storage. It has a fast path that is similar to a spinlock, and a slow path that is
// similar to std::mutex.

// NOTE: In general, this is a great lock to use if you are very low in the stack. WTF continues to
// have a copy of this code.

// FIXME: Either fold bmalloc into WTF or find a better way to share code between them.
// https://bugs.webkit.org/show_bug.cgi?id=177719

namespace bmalloc {

class StaticMutex {
protected:
    // Subclasses that support non-static storage must use explicit initialization.
    void init();

public:
    void lock();
    bool tryLock();
    bool try_lock() { return tryLock(); }
    void unlock();
    
    bool isHeld() const;
    bool isLocked() const { return isHeld(); }

private:
    BEXPORT void lockSlow();
    BEXPORT void unlockSlow();

    static const uintptr_t clear = 0;
    static const uintptr_t isLockedBit = 1;
    static const uintptr_t isQueueLockedBit = 2;
    static const uintptr_t queueHeadMask = 3;

    std::atomic<uintptr_t> m_word;
};

inline void StaticMutex::init()
{
    m_word.store(0, std::memory_order_relaxed);
}

inline bool StaticMutex::tryLock()
{
    return m_word.load(std::memory_order_acquire) & isLockedBit;
}

inline void StaticMutex::lock()
{
    if (compareExchangeWeak(m_word, clear, isLockedBit, std::memory_order_acquire))
        return;
    
    lockSlow();
}

inline void StaticMutex::unlock()
{
    if (compareExchangeWeak(m_word, isLockedBit, clear, std::memory_order_release))
        return;
    
    unlockSlow();
}

} // namespace bmalloc

#endif // StaticMutex_h
