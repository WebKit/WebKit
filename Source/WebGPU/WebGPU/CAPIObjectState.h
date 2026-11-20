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

#pragma once

#import <WebGPU/WebGPU.h>
#import <WebGPU/WebGPUCpp.h>
#import <atomic>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/text/WTFString.h>

// The state of the C API for the adapters and devices that it hands out. Their handles are the
// objects of the C++ API (WebGPUCppBridge.h), so the state is in process-wide tables keyed by the
// object. An entry exists while the C API has references to its object, and those references keep
// the object alive. An object without an entry is one that the C API did not hand out, or whose
// last reference of the C API was released.

namespace WebGPU::CAPI {

class EventManager;
class Instance;

// The C API hands out a new reference to the adapter, of the instance: it adds an entry, or counts
// the reference in the entry that the adapter has.
void didHandOutAdapter(WebGPU::Adapter&, const Instance*);
// Null when the adapter has no entry, or the instance is gone.
RefPtr<Instance> findInstance(const WebGPU::Adapter&);
// wgpuAdapterAddRef() and wgpuAdapterRelease().
void addReference(WebGPU::Adapter&);
void releaseReference(WebGPU::Adapter&);

// The device-lost event of the C API, for WGPUDeviceDescriptor.deviceLostCallbackInfo. As in webgpu.h,
// its callback is called once: when the device is lost, which destroying the device and releasing its
// last reference also do, or when the device cannot be created. A WGPUCallbackMode_AllowSpontaneous
// callback is called then, on that thread. The other ones are called in wgpuInstanceProcessEvents()
// and wgpuInstanceWaitAny(), as the other callbacks of the C API, including the
// WGPUCallbackMode_WaitAnyOnly ones.
//
// As Dawn does when its instance is destroyed, destroying the instance calls the callbacks that have
// not been called yet with WGPUDeviceLostReason_CallbackCancelled, except the
// WGPUCallbackMode_AllowSpontaneous ones, which are still called when the device is lost. Unlike in
// Dawn, adapters and devices do not keep the instance alive, so, as webgpu.h specifies, that is when
// the last reference to the instance is released, also when the device is still alive.
class DeviceLostEvent final : public ThreadSafeRefCounted<DeviceLostEvent> {
public:
    static Ref<DeviceLostEvent> create(const WGPUDeviceLostCallbackInfo&, const Instance*);

    // Only an event whose callback was never to be called is destroyed before it completes.
    ~DeviceLostEvent();

    WGPUFuture future() const { return { m_future }; }
    // True once the device is lost, also before the callback is called.
    bool isLost() const { return m_isLost; }
    void setDevice(WebGPU::Device&);
    void lose(WGPUDeviceLostReason, String&& message);

private:
    DeviceLostEvent(const WGPUDeviceLostCallbackInfo&, RefPtr<EventManager>&&);

    static String cancelledMessage();
    void complete(WGPUDeviceLostReason, const String& message);

    const WGPUDeviceLostCallbackInfo m_callbackInfo;
    // The event does not keep the instance alive, so that releasing the instance cancels it.
    const ThreadSafeWeakPtr<EventManager> m_events;
    const uint64_t m_future;
    ThreadSafeWeakPtr<WebGPU::Device> m_device;
    std::atomic<bool> m_isLost { false };
    std::atomic<bool> m_isCompleted { false };
};

// The C API created the device, of the instance, and owns its first reference. The device-lost
// event is called when the device is lost.
void didCreateDevice(WebGPU::Device&, const Instance*, DeviceLostEvent&);
// Null when the device has no entry, or the instance is gone.
RefPtr<Instance> findInstance(const WebGPU::Device&);
// wgpuDeviceAddRef() and wgpuDeviceRelease(). As in webgpu.h, releasing the last reference destroys
// a device that the C API owns, also when its objects still reference it.
void addReference(WebGPU::Device&);
void releaseReference(WebGPU::Device&);
// The future of wgpuDeviceGetLostFuture(). WGPU_FUTURE_INIT when the device has no entry.
WGPUFuture lostFuture(const WebGPU::Device&);
// True when the device is lost, or the C API no longer has it.
bool isLost(const WebGPU::Device&);

} // namespace WebGPU::CAPI
