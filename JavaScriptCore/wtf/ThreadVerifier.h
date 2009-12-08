/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef ThreadVerifier_h
#define ThreadVerifier_h

#ifndef NDEBUG

#include <wtf/Assertions.h>
#include <wtf/Threading.h>

namespace WTF {

class ThreadVerifier {
public:
    ThreadVerifier()
        : m_isActive(false)
        , m_isThreadVerificationDisabled(false)
        , m_isOwnedByMainThread(false)
        , m_owningThread(0)
    {
    }

    // Capture the current thread and start verifying that subsequent ref/deref happen on this thread.
    void activate()
    {
        ASSERT(m_isThreadVerificationDisabled || !m_isActive);
        m_isActive = true;
        m_isOwnedByMainThread = isMainThread();
        if (!m_isOwnedByMainThread)
            m_owningThread = currentThread();
    }

    void deactivate()
    {
        ASSERT(m_isThreadVerificationDisabled || m_isActive);
        m_isActive = false;
    }

    // Permanently disable the verification on the instance. Used for RefCounted that are intentionally used cross-thread.
    void disableThreadVerification() { m_isThreadVerificationDisabled = true; }

    bool verifyThread() const
    {
        // isMainThread() is way faster then currentThread() on many platforms, use it first.
        return m_isThreadVerificationDisabled || !m_isActive || (m_isOwnedByMainThread && isMainThread()) || (m_owningThread == currentThread());
    }

private:
    bool m_isActive;
    bool m_isThreadVerificationDisabled;
    bool m_isOwnedByMainThread;
    ThreadIdentifier m_owningThread;
};

} // namespace WTF

#endif // NDEBUG

#endif // ThreadVerifier_h

