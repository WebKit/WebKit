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

#include "CVDisplayLink.h"

#include "CVDisplayLinkClient.h"
#include <QuartzCore/CABase.h>
#include <wtf/RefPtr.h>

namespace WKQCA {

inline CVDisplayLink::CVDisplayLink(CVDisplayLinkClient* client)
    : m_client(client)
    , m_wakeupEvent(::CreateEventW(0, FALSE, FALSE, 0))
{
    ASSERT_ARG(client, client);
}

CVDisplayLink::~CVDisplayLink()
{
    stop();
    ::CloseHandle(m_wakeupEvent);
}

Ref<CVDisplayLink> CVDisplayLink::create(CVDisplayLinkClient* client)
{
    return adoptRef(*new CVDisplayLink(client));
}

void CVDisplayLink::start()
{
    if (m_isRunning) {
        ASSERT(m_ioThread);
        return;
    }

    ASSERT(!m_ioThread);

    m_isRunning = true;

    ref(); // Adopted in runIOThread.
    m_ioThread = Thread::create("WKQCA: CVDisplayLink", [this] {
        runIOThread();
    });
}

void CVDisplayLink::stop()
{
    if (!m_isRunning) {
        ASSERT(!m_ioThread);
        return;
    }

    ASSERT(m_ioThread);

    m_isRunning = false;

    // Unpause the thread so it will exit.
    setPaused(false);

    DWORD result = m_ioThread->waitForCompletion();
    m_ioThread = nullptr;

    switch (result) {
    case WAIT_OBJECT_0:
        // The thread's HANDLE was signaled, so the thread has exited.
        break;
    case WAIT_FAILED:
        LOG_ERROR("waitForCompletion failed with error code %u", ::GetLastError());
        break;
    default:
        ASSERT_WITH_MESSAGE(false, "waitForCompletion returned unexpected result %u", result);
        break;
    }
}

void CVDisplayLink::setPaused(bool paused)
{
    if (m_isPaused == paused)
        return;

    m_isPaused = paused;

    if (m_isPaused)
        return;

    // Since we were paused until right now, the IO thread, if it has been started, is currently
    // waiting forever inside ::WaitForSingleObject. We need to wake it up so it can continue
    // running. If it hasn't been started, signaling this event is harmless.
    ::SetEvent(m_wakeupEvent);
}

void CVDisplayLink::runIOThread()
{
    RefPtr<CVDisplayLink> link = adoptRef(this);
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while (m_isRunning) {
        // When we're not paused, we want to wait for as short a time as possible. MSDN says that
        // passing a timeout of 0 causes ::WaitForSingleObject to return immediately without
        // entering the wait state at all, which wouldn't give other threads a chance to acquire any
        // mutexes our client acquires beneath displayLinkReachedCAMediaTime. So we pass a timeout
        // of 1 instead. (Passing a timeout of 0 does seem to cause this function to enter the wait
        // state at least sometimes on Windows XP, despite what MSDN says, but not on Windows 7.)
        DWORD result = ::WaitForSingleObject(m_wakeupEvent, m_isPaused ? INFINITE : 1);
        switch (result) {
        case WAIT_TIMEOUT:
            // It must be time to render.
            m_client->displayLinkReachedCAMediaTime(this, CACurrentMediaTime());
            break;
        case WAIT_OBJECT_0:
            // Someone wanted to wake us up.
            break;
        case WAIT_FAILED:
            LOG_ERROR("::WaitForSingleObject failed with error code %u", ::GetLastError());
            break;
        default:
            ASSERT_WITH_MESSAGE(false, "::WaitForSingleObject returned unexpected result %u", result);
            break;
        }
    }
}

} // namespace WKQCA
