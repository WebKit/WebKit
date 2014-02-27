/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#if PLATFORM(IOS)
#include "Settings.h"
#endif

#include <mutex>
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

NetworkStateNotifier& networkStateNotifier()
{
    static std::once_flag onceFlag;
    static NetworkStateNotifier* networkStateNotifier;

    std::call_once(onceFlag, []{
        networkStateNotifier = std::make_unique<NetworkStateNotifier>().release();
    });

    return *networkStateNotifier;
}

void NetworkStateNotifier::addNetworkStateChangeListener(std::function<void (bool)> listener)
{
    ASSERT(listener);
#if PLATFORM(IOS)
    if (Settings::shouldOptOutOfNetworkStateObservation())
        return;
    registerObserverIfNecessary();
#endif

    m_listeners.append(std::move(listener));
}

void NetworkStateNotifier::notifyNetworkStateChange() const
{
    for (const auto& listener : m_listeners)
        listener(m_isOnLine);
}

} // namespace WebCore
