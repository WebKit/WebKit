/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LayerChangesFlusher.h"

#if USE(CA)

#include "AbstractCACFLayerTreeHost.h"
#include "StructuredExceptionHandlerSuppressor.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebCore {

LayerChangesFlusher& LayerChangesFlusher::singleton()
{
    static NeverDestroyed<LayerChangesFlusher> flusher;
    return flusher;
}

LayerChangesFlusher::LayerChangesFlusher()
    : m_hook(0)
    , m_isCallingHosts(false)
{
}

void LayerChangesFlusher::flushPendingLayerChangesSoon(AbstractCACFLayerTreeHost* host)
{
    if (!m_hostsWithChangesToFlush.add(host).isNewEntry || m_hook)
        return;

    setHook();
}

void LayerChangesFlusher::cancelPendingFlush(AbstractCACFLayerTreeHost* host)
{
    m_hostsWithChangesToFlush.remove(host);

    if (!m_hostsWithChangesToFlush.isEmpty() || !m_hook)
        return;

    // We handle removing the hook when we finish calling out to the hosts, so we shouldn't
    // mess with it while we're in the process of calling them.
    if (m_isCallingHosts)
        return;

    removeHook();
}

LRESULT LayerChangesFlusher::hookCallback(int code, WPARAM wParam, LPARAM lParam)
{
    // Suppress the exception handler Windows puts around all hook calls so we can 
    // crash for debugging purposes if an exception is hit. 
    ExceptionRegistration registrationStruct; // Note: must be stack allocated.
    StructuredExceptionHandlerSuppressor suppressor(registrationStruct);
    return singleton().hookFired(code, wParam, lParam);
}

LRESULT LayerChangesFlusher::hookFired(int code, WPARAM wParam, LPARAM lParam)
{
    ASSERT(m_hook);

    // Calling out to the hosts can cause m_hostsWithChangesToFlush to be modified, so we copy it
    // into a Vector first.
    auto hosts = copyToVector(m_hostsWithChangesToFlush);
    m_hostsWithChangesToFlush.clear();

    m_isCallingHosts = true;
    for (size_t i = 0; i < hosts.size(); ++i)
        hosts[i]->flushPendingLayerChangesNow();
    m_isCallingHosts = false;

    LRESULT result = ::CallNextHookEx(m_hook, code, wParam, lParam);

    if (m_hook && m_hostsWithChangesToFlush.isEmpty()) {
        // We won't have any work to do next time around, so just remove our hook.
        removeHook();
    }

    return result;
}

void LayerChangesFlusher::setHook()
{
    ASSERT(!m_hook);
    ASSERT(!m_isCallingHosts);

    DWORD threadID = ::GetCurrentThreadId();

    m_hook = ::SetWindowsHookExW(WH_GETMESSAGE, hookCallback, 0, threadID);
    ASSERT_WITH_MESSAGE(m_hook, "::SetWindowsHookExW failed with error %lu", ::GetLastError());

    // Post a message to the message queue to prevent ::GetMessage from blocking, which will ensure
    // our hook is called soon.
    ::PostThreadMessageW(threadID, WM_NULL, 0, 0);
}

void LayerChangesFlusher::removeHook()
{
    ASSERT(m_hook);
    ASSERT(!m_isCallingHosts);

    if (!::UnhookWindowsHookEx(m_hook))
        ASSERT_WITH_MESSAGE(false, "::UnhookWindowsHookEx failed with error %lu", ::GetLastError());

    m_hook = 0;
}

} // namespace WebCore

#endif
