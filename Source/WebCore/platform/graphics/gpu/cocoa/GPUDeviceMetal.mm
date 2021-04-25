/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "GPUDevice.h"

#if ENABLE(WEBGPU)

#import "GPURequestAdapterOptions.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

static bool isAcceptableDevice(id <MTLDevice> device)
{
    UNUSED_PARAM(device);
    return true;
}

void GPUDevice::prewarm()
{
#if PLATFORM(MAC)
    // Call MTLCopyAllDevices() on a background thread to avoid hanging the main thread.
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        auto devices = adoptNS(MTLCopyAllDevices());
    });
#endif
}

RefPtr<GPUDevice> GPUDevice::tryCreate(const Optional<GPURequestAdapterOptions>& options)
{
    RetainPtr<MTLDevice> devicePtr;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
#if PLATFORM(MAC)
    if (!options || !options->powerPreference || options->powerPreference == GPUPowerPreference::LowPower) {
        auto devices = adoptNS(MTLCopyAllDevices());
        
        for (id <MTLDevice> device : devices.get()) {
            if (!isAcceptableDevice(device))
                continue;
            if (device.lowPower) {
                devicePtr = device;
                break;
            }
        }
    }
#else
    UNUSED_PARAM(options);
#endif // PLATFORM(MAC)
    if (!devicePtr)
        devicePtr = adoptNS(MTLCreateSystemDefaultDevice());

    if (!isAcceptableDevice(devicePtr.get()))
        devicePtr.clear();

    END_BLOCK_OBJC_EXCEPTIONS

    if (!devicePtr) {
        LOG(WebGPU, "GPUDevice::GPUDevice(): Unable to create GPUDevice!");
        return nullptr;
    }

    LOG(WebGPU, "GPUDevice::GPUDevice(): MTLDevice is %p", devicePtr.get());
    return adoptRef(new GPUDevice(WTFMove(devicePtr)));
}

GPUDevice::GPUDevice(RetainPtr<MTLDevice>&& device)
    : m_platformDevice(WTFMove(device))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
