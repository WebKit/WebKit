/*
 * Copyright 2010, The Android Open Source Project
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "config.h"
#include "DeviceController.h"

#include "DeviceClient.h"
#include "Document.h"

namespace WebCore {

DeviceController::DeviceController(DeviceClient& client)
    : m_client(client)
    , m_timer(*this, &DeviceController::fireDeviceEvent)
{
}

void DeviceController::addDeviceEventListener(DOMWindow& window)
{
    bool wasEmpty = m_listeners.isEmpty();
    m_listeners.add(&window);

    if (hasLastData()) {
        m_lastEventListeners.add(&window);
        if (!m_timer.isActive())
            m_timer.startOneShot(0_s);
    }

    if (wasEmpty)
        m_client.startUpdating();
}

void DeviceController::removeDeviceEventListener(DOMWindow& window)
{
    m_listeners.remove(&window);
    m_lastEventListeners.remove(&window);
    if (m_listeners.isEmpty())
        m_client.stopUpdating();
}

void DeviceController::removeAllDeviceEventListeners(DOMWindow& window)
{
    m_listeners.removeAll(&window);
    m_lastEventListeners.removeAll(&window);
    if (m_listeners.isEmpty())
        m_client.stopUpdating();
}

bool DeviceController::hasDeviceEventListener(DOMWindow& window) const
{
    return m_listeners.contains(&window);
}

void DeviceController::dispatchDeviceEvent(Event& event)
{
    for (auto& listener : copyToVector(m_listeners.values())) {
        auto document = listener->document();
        if (document && !document->activeDOMObjectsAreSuspended() && !document->activeDOMObjectsAreStopped())
            listener->dispatchEvent(event);
    }
}

void DeviceController::fireDeviceEvent()
{
    ASSERT(hasLastData());

    m_timer.stop();
    auto listenerVector = copyToVector(m_lastEventListeners.values());
    m_lastEventListeners.clear();
    for (auto& listener : listenerVector) {
        auto document = listener->document();
        if (document && !document->activeDOMObjectsAreSuspended() && !document->activeDOMObjectsAreStopped()) {
            if (auto lastEvent = getLastEvent())
                listener->dispatchEvent(*lastEvent);
        }
    }
}

} // namespace WebCore
