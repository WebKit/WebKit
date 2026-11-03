/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CAPIObjectState.h"

#import "CAPIInstance.h"
#import "WebGPUCppBridge.h"
#import "WebGPUCppConversions.h"
#import <wtf/CompletionHandler.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>

namespace WebGPU::CAPI {

// The conversions of WebGPUCppConversions.h are in namespace WebGPU::Metal, but they convert
// between the C API and the C++ API only.

// The tables are keyed by the address of the object. The entry stores a weak pointer to the object
// too, so that an entry is never taken for the one of another object at the same address. Nothing
// is called on the objects, and no state that can hold their last reference is destroyed, while the
// lock is held: their destructors and callbacks use the tables.

struct AdapterState {
    ThreadSafeWeakPtr<WebGPU::Adapter> adapter;
    ThreadSafeWeakPtr<Instance> instance;
    // The references that the C API handed out.
    uint64_t referenceCount { 0 };
};

struct DeviceState {
    ThreadSafeWeakPtr<WebGPU::Device> device;
    ThreadSafeWeakPtr<Instance> instance;
    // The references that the C API handed out.
    uint64_t referenceCount { 0 };
    // False for a device that the C API was given rather than created, which it does not destroy.
    bool ownsDevice { true };
    RefPtr<DeviceLostEvent> lostEvent;
};

static Lock tablesLock;

static HashMap<const WebGPU::Adapter*, AdapterState>& adapters() WTF_REQUIRES_LOCK(tablesLock)
{
    static NeverDestroyed<HashMap<const WebGPU::Adapter*, AdapterState>> adapters;
    return adapters;
}

static HashMap<const WebGPU::Device*, DeviceState>& devices() WTF_REQUIRES_LOCK(tablesLock)
{
    static NeverDestroyed<HashMap<const WebGPU::Device*, DeviceState>> devices;
    return devices;
}

// The caller has a reference to the object, so the weak pointer never holds its last reference.
static AdapterState* findEntry(const WebGPU::Adapter& adapter) WTF_REQUIRES_LOCK(tablesLock)
{
    auto iterator = adapters().find(&adapter);
    if (iterator == adapters().end() || iterator->value.adapter.get().get() != &adapter)
        return nullptr;
    return &iterator->value;
}

static DeviceState* findEntry(const WebGPU::Device& device) WTF_REQUIRES_LOCK(tablesLock)
{
    auto iterator = devices().find(&device);
    if (iterator == devices().end() || iterator->value.device.get().get() != &device)
        return nullptr;
    return &iterator->value;
}

void didHandOutAdapter(WebGPU::Adapter& adapter, const Instance* instance)
{
    std::optional<AdapterState> replacedState;
    Locker locker(tablesLock);
    if (auto* state = findEntry(adapter)) {
        ++state->referenceCount;
        return;
    }
    replacedState = adapters().take(&adapter);
    adapters().add(&adapter, AdapterState { adapter, instance, 1 });
}

RefPtr<Instance> findInstance(const WebGPU::Adapter& adapter)
{
    ThreadSafeWeakPtr<Instance> instance;
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(adapter))
            instance = state->instance;
    }
    return instance.get();
}

void addReference(WebGPU::Adapter& adapter)
{
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(adapter))
            ++state->referenceCount;
    }
    adapter.ref();
}

void releaseReference(WebGPU::Adapter& adapter)
{
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(adapter); state && !--state->referenceCount)
            adapters().remove(&adapter);
    }
    adapter.deref();
}

Ref<DeviceLostEvent> DeviceLostEvent::create(const WGPUDeviceLostCallbackInfo& callbackInfo, const Instance* instance)
{
    RefPtr<EventManager> events;
    if (instance)
        events = &instance->events();
    Ref event = adoptRef(*new DeviceLostEvent(callbackInfo, WTF::move(events)));
    if (RefPtr events = event->m_events; events && callbackInfo.mode != WGPUCallbackMode_AllowSpontaneous) {
        events->addCancellation(event->m_future, [event] {
            event->complete(WGPUDeviceLostReason_CallbackCancelled, cancelledMessage());
        });
    }
    return event;
}

DeviceLostEvent::DeviceLostEvent(const WGPUDeviceLostCallbackInfo& callbackInfo, RefPtr<EventManager>&& events)
    : m_callbackInfo(callbackInfo)
    , m_events(events)
    , m_future(events ? events->createFuture() : 0)
{
}

DeviceLostEvent::~DeviceLostEvent()
{
    complete(WGPUDeviceLostReason_CallbackCancelled, cancelledMessage());
}

String DeviceLostEvent::cancelledMessage()
{
    return "The instance was released before the device-lost callback was called."_s;
}

void DeviceLostEvent::setDevice(WebGPU::Device& device)
{
    m_device = device;
}

void DeviceLostEvent::lose(WGPUDeviceLostReason reason, String&& message)
{
    m_isLost = true;
    if (m_callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous) {
        complete(reason, message);
        return;
    }
    RefPtr events = m_events;
    if (!events) {
        complete(WGPUDeviceLostReason_CallbackCancelled, cancelledMessage());
        return;
    }
    events->scheduleWork([protectedThis = Ref { *this }, reason, message = WTF::move(message).isolatedCopy()] {
        protectedThis->complete(reason, message);
    });
}

static bool hasEntry(const WebGPU::Device& device)
{
    Locker locker(tablesLock);
    return findEntry(device);
}

void DeviceLostEvent::complete(WGPUDeviceLostReason reason, const String& message)
{
    if (m_isCompleted.exchange(true))
        return;
    // As in webgpu.h, there is no device when it was not created, when the callback is cancelled,
    // and when its last reference has been released.
    RefPtr device = m_device;
    bool hasDevice = device && reason != WGPUDeviceLostReason_FailedCreation && reason != WGPUDeviceLostReason_CallbackCancelled && hasEntry(*device);
    WGPUDevice handle = hasDevice ? WebGPU::toAPI(*device) : nullptr;
    if (m_callbackInfo.callback) {
        auto utf8Message = message.utf8();
        m_callbackInfo.callback(&handle, reason, WebGPU::Metal::toAPI(utf8Message), m_callbackInfo.userdata1, m_callbackInfo.userdata2);
    }
    if (RefPtr events = m_events) {
        events->removeCancellation(m_future);
        events->completeFuture(m_future);
    }
}

void didCreateDevice(WebGPU::Device& device, const Instance* instance, DeviceLostEvent& lostEvent)
{
    {
        std::optional<DeviceState> replacedState;
        Locker locker(tablesLock);
        replacedState = devices().take(&device);
        devices().add(&device, DeviceState { device, instance, 1, true, &lostEvent });
    }
    lostEvent.setDevice(device);
    device.resolveDeviceLostPromise({ [lostEvent = Ref { lostEvent }](WebGPU::DeviceLostReason reason, String&& message) {
        lostEvent->lose(WebGPU::Metal::toAPI(reason), WTF::move(message));
    }, CompletionHandlerCallThread::AnyThread });
}

RefPtr<Instance> findInstance(const WebGPU::Device& device)
{
    ThreadSafeWeakPtr<Instance> instance;
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(device))
            instance = state->instance;
    }
    return instance.get();
}

void addReference(WebGPU::Device& device)
{
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(device))
            ++state->referenceCount;
    }
    device.ref();
}

void releaseReference(WebGPU::Device& device)
{
    std::optional<DeviceState> removedState;
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(device); state && !--state->referenceCount)
            removedState = devices().take(&device);
    }
    if (removedState && removedState->ownsDevice)
        device.destroy();
    removedState = std::nullopt;
    device.deref();
}

WGPUFuture lostFuture(const WebGPU::Device& device)
{
    RefPtr<DeviceLostEvent> lostEvent;
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(device))
            lostEvent = state->lostEvent;
    }
    if (!lostEvent)
        return WGPU_FUTURE_INIT;
    return lostEvent->future();
}

bool isLost(const WebGPU::Device& device)
{
    RefPtr<DeviceLostEvent> lostEvent;
    {
        Locker locker(tablesLock);
        if (auto* state = findEntry(device))
            lostEvent = state->lostEvent;
    }
    return !lostEvent || lostEvent->isLost();
}

} // namespace WebGPU::CAPI
