/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// FIXME: We intentionally use ifndef guards instead of `#pragma once` here to avoid build failures.
// WTF headers are copied to build directory. Based on how we include headers, WTF headers may be
// included from either source directory or build directory. Compilers are confused with their
// two different paths and include the same file twice.
// https://bugs.webkit.org/show_bug.cgi?id=174986
#ifndef ThreadHolder_h
#define ThreadHolder_h

#include <wtf/FastTLS.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSpecific.h>

namespace WTF {

class Thread;
void initializeThreading();

// Holds Thread in the thread-specific storage. The destructor of this holder reliably destroy Thread.
// For pthread, it employs pthreads-specific 2-pass destruction to reliably remove Thread.
// For Windows, we use thread_local to defer thread holder destruction. It assumes regular ThreadSpecific
// types don't use multiple-pass destruction.
class ThreadHolder {
    WTF_MAKE_NONCOPYABLE(ThreadHolder);
public:
    friend class Thread;
    friend void WTF::initializeThreading();
    ~ThreadHolder();

    static ThreadHolder& current();

    Thread& thread() { return m_thread.get(); }

#if OS(WINDOWS)
    static RefPtr<Thread> get(ThreadIdentifier);
#endif

private:
    ThreadHolder(Ref<Thread>&& thread)
        : m_thread(WTFMove(thread))
        , m_isDestroyedOnce(false)
    {
    }

    // Creates and puts an instance of ThreadHolder into thread-specific storage.
    static ThreadHolder& initialize(Ref<Thread>&&);
    WTF_EXPORT_PRIVATE static ThreadHolder& initializeCurrent();

#if !HAVE(FAST_TLS)
    // One time initialization for this class as a whole.
    // This method must be called before initialize() and it is not thread-safe.
    WTF_EXPORT_PRIVATE static void initializeKey();
#endif

    // Returns 0 if thread-specific storage was not initialized.
    static ThreadHolder* currentMayBeNull();

#if OS(WINDOWS)
    WTF_EXPORT_PRIVATE static ThreadHolder* currentDying();
#endif

    // This thread-specific destructor is called 2 times when thread terminates:
    // - first, when all the other thread-specific destructors are called, it simply remembers it was 'destroyed once'
    // and (1) re-sets itself into the thread-specific slot or (2) constructs thread local value to call it again later.
    // - second, after all thread-specific destructors were invoked, it gets called again - this time, we remove the
    // Thread from the threadMap, completing the cleanup.
    static void THREAD_SPECIFIC_CALL destruct(void* data);

    Ref<Thread> m_thread;
    bool m_isDestroyedOnce;
#if !HAVE(FAST_TLS)
    static WTF_EXPORTDATA ThreadSpecificKey m_key;
#endif
};

inline ThreadHolder* ThreadHolder::currentMayBeNull()
{
#if !HAVE(FAST_TLS)
    ASSERT(m_key != InvalidThreadSpecificKey);
    return static_cast<ThreadHolder*>(threadSpecificGet(m_key));
#else
    return static_cast<ThreadHolder*>(_pthread_getspecific_direct(WTF_THREAD_DATA_KEY));
#endif
}

inline ThreadHolder& ThreadHolder::current()
{
    // WRT WebCore:
    //    ThreadHolder is used on main thread before it could possibly be used
    //    on secondary ones, so there is no need for synchronization here.
    // WRT JavaScriptCore:
    //    ThreadHolder::initializeKey() is initially called from initializeThreading(), ensuring
    //    this is initially called in a pthread_once locked context.
#if !HAVE(FAST_TLS)
    if (UNLIKELY(ThreadHolder::m_key == InvalidThreadSpecificKey))
        WTF::initializeThreading();
#endif
    if (auto* holder = currentMayBeNull())
        return *holder;
#if OS(WINDOWS)
    if (auto* holder = currentDying())
        return *holder;
#endif
    return initializeCurrent();
}

} // namespace WTF
#endif // ThreadHolder_h
