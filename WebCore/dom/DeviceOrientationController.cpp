/*
 * Copyright 2010, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeviceOrientationController.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "DeviceOrientation.h"
#include "DeviceOrientationClient.h"
#include "DeviceOrientationEvent.h"

namespace WebCore {

DeviceOrientationController::DeviceOrientationController(Page* page, DeviceOrientationClient* client)
    : m_page(page)
    , m_client(client)
{
}

void DeviceOrientationController::addListener(DOMWindow* window)
{
    // If no client is present, signal that no orientation data is available.
    // If the client already has an orientation, call back to this new listener
    // immediately.
    if (!m_client) {
        RefPtr<DeviceOrientation> emptyOrientation = DeviceOrientation::create();
        window->dispatchEvent(DeviceOrientationEvent::create(eventNames().deviceorientationEvent, emptyOrientation.get()));
    } else if (m_client && m_client->lastOrientation())
        window->dispatchEvent(DeviceOrientationEvent::create(eventNames().deviceorientationEvent, m_client->lastOrientation()));

    // The client may call back synchronously.
    bool wasEmpty = m_listeners.isEmpty();
    m_listeners.add(window);
    if (wasEmpty && m_client)
        m_client->startUpdating();
}

void DeviceOrientationController::removeListener(DOMWindow* window)
{
    m_listeners.remove(window);
    if (m_listeners.isEmpty() && m_client)
        m_client->stopUpdating();
}

void DeviceOrientationController::removeAllListeners(DOMWindow* window)
{
    // May be called with a DOMWindow that's not a listener.
    if (!m_listeners.contains(window))
        return;

    m_listeners.removeAll(window);
    if (m_listeners.isEmpty() && m_client)
        m_client->stopUpdating();
}

void DeviceOrientationController::didChangeDeviceOrientation(DeviceOrientation* orientation)
{
    RefPtr<DeviceOrientationEvent> event = DeviceOrientationEvent::create(eventNames().deviceorientationEvent, orientation);
    Vector<DOMWindow*> listenersVector;
    copyToVector(m_listeners, listenersVector);
    for (size_t i = 0; i < listenersVector.size(); ++i)
        listenersVector[i]->dispatchEvent(event);
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
