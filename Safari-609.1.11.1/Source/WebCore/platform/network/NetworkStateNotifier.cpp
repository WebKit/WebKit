/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
#include "NetworkStateNotifier.h"

#include <wtf/NeverDestroyed.h>

#if USE(WEB_THREAD)
#include "WebCoreThread.h"
#endif

namespace WebCore {

static const Seconds updateStateSoonInterval { 2_s };

static bool shouldSuppressThreadSafetyCheck()
{
#if USE(WEB_THREAD)
    return WebThreadIsEnabled();
#else
    return false;
#endif
}

NetworkStateNotifier& NetworkStateNotifier::singleton()
{
    RELEASE_ASSERT(shouldSuppressThreadSafetyCheck() || isMainThread());
    static NeverDestroyed<NetworkStateNotifier> networkStateNotifier;
    return networkStateNotifier;
}

NetworkStateNotifier::NetworkStateNotifier()
    : m_updateStateTimer([] {
        singleton().updateState();
    })
{
}

bool NetworkStateNotifier::onLine()
{
    if (!m_isOnLine)
        updateState();
    return m_isOnLine.valueOr(true);
}

void NetworkStateNotifier::addListener(WTF::Function<void(bool)>&& listener)
{
    ASSERT(listener);
    if (m_listeners.isEmpty())
        startObserving();
    m_listeners.append(WTFMove(listener));
}

void NetworkStateNotifier::updateState()
{
    auto wasOnLine = m_isOnLine;
    updateStateWithoutNotifying();
    if (m_isOnLine == wasOnLine)
        return;
    for (auto& listener : m_listeners)
        listener(m_isOnLine.value());
}

void NetworkStateNotifier::updateStateSoon()
{
    m_updateStateTimer.startOneShot(updateStateSoonInterval);
}

} // namespace WebCore
