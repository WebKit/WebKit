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

#include "BAssert.h"
#include <atomic>

// A fast replacement for std::mutex for use in static storage, where global
// constructors and exit-time destructors are prohibited.

namespace bmalloc {

class StaticMutex {
public:
    void lock();
    bool try_lock();
    void unlock();

private:
    friend class Mutex;

    // Static storage will zero-initialize us automatically, but Mutex needs an
    // API for explicit initialization.
    void init();

    void lockSlowCase();

    std::atomic_flag m_flag;
};

inline void StaticMutex::init()
{
    m_flag.clear();
}

inline bool StaticMutex::try_lock()
{
    return !m_flag.test_and_set(std::memory_order_acquire);
}

inline void StaticMutex::lock()
{
    if (!try_lock())
        lockSlowCase();
}

inline void StaticMutex::unlock()
{
    m_flag.clear(std::memory_order_release);
}

} // namespace bmalloc

#endif // StaticMutex_h
