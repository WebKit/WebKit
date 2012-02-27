/*
 * Copyright 2010 Apple Inc. All rights reserved.
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
#include "DeviceMotionController.h"

#include "DeviceMotionClient.h"
#include "DeviceMotionData.h"
#include "DeviceMotionEvent.h"

namespace WebCore {

DeviceMotionController::DeviceMotionController(DeviceMotionClient* client)
    : m_client(client)
    , m_timer(this, &DeviceMotionController::timerFired)
{
    ASSERT(m_client);
    m_client->setController(this);
}

DeviceMotionController::~DeviceMotionController()
{
    m_client->deviceMotionControllerDestroyed();
}

PassOwnPtr<DeviceMotionController> DeviceMotionController::create(DeviceMotionClient* client)
{
    return adoptPtr(new DeviceMotionController(client));
}

void DeviceMotionController::timerFired(Timer<DeviceMotionController>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);
    ASSERT(m_client->currentDeviceMotion());
    m_timer.stop();
    
    RefPtr<DeviceMotionData> deviceMotionData = m_client->currentDeviceMotion();
    RefPtr<DeviceMotionEvent> event = DeviceMotionEvent::create(eventNames().devicemotionEvent, deviceMotionData.get());
 
    Vector<RefPtr<DOMWindow> > listenersVector;
    copyToVector(m_newListeners, listenersVector);
    m_newListeners.clear();
    for (size_t i = 0; i < listenersVector.size(); ++i)
        listenersVector[i]->dispatchEvent(event);
}
    
void DeviceMotionController::addListener(DOMWindow* window)
{
    // If the client already has motion data,
    // immediately trigger an asynchronous response.
    if (m_client->currentDeviceMotion()) {
        m_newListeners.add(window);
        if (!m_timer.isActive())
            m_timer.startOneShot(0);
    }
    
    bool wasEmpty = m_listeners.isEmpty();
    m_listeners.add(window);
    if (wasEmpty)
        m_client->startUpdating();
}

void DeviceMotionController::removeListener(DOMWindow* window)
{
    m_listeners.remove(window);
    m_suspendedListeners.remove(window);
    m_newListeners.remove(window);
    if (m_listeners.isEmpty())
        m_client->stopUpdating();
}

void DeviceMotionController::removeAllListeners(DOMWindow* window)
{
    // May be called with a DOMWindow that's not a listener.
    if (!m_listeners.contains(window))
        return;

    m_listeners.removeAll(window);
    m_suspendedListeners.removeAll(window);
    m_newListeners.remove(window);
    if (m_listeners.isEmpty())
        m_client->stopUpdating();
}

void DeviceMotionController::suspendEventsForAllListeners(DOMWindow* window)
{
    if (!m_listeners.contains(window))
        return;

    int count = m_listeners.count(window);
    removeAllListeners(window);
    while (count--)
        m_suspendedListeners.add(window);
}

void DeviceMotionController::resumeEventsForAllListeners(DOMWindow* window)
{
    if (!m_suspendedListeners.contains(window))
        return;

    int count = m_suspendedListeners.count(window);
    m_suspendedListeners.removeAll(window);
    while (count--)
        addListener(window);
}

void DeviceMotionController::didChangeDeviceMotion(DeviceMotionData* deviceMotionData)
{
    RefPtr<DeviceMotionEvent> event = DeviceMotionEvent::create(eventNames().devicemotionEvent, deviceMotionData);
    Vector<RefPtr<DOMWindow> > listenersVector;
    copyToVector(m_listeners, listenersVector);
    for (size_t i = 0; i < listenersVector.size(); ++i)
        listenersVector[i]->dispatchEvent(event);
}

const AtomicString& DeviceMotionController::supplementName()
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("DeviceMotionController"));
    return name;
}

bool DeviceMotionController::isActiveAt(Page* page)
{
    if (DeviceMotionController* self = DeviceMotionController::from(page))
        return self->isActive();
    return false;
}

void provideDeviceMotionTo(Page* page, DeviceMotionClient* client)
{
    DeviceMotionController::provideTo(page, DeviceMotionController::supplementName(), DeviceMotionController::create(client));
}

} // namespace WebCore
