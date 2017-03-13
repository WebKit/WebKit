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
#include "GPUDevice.h"

#if ENABLE(WEBGPU)

#include "GPUBuffer.h"
#include "GPUCommandQueue.h"
#include "GPUDrawable.h"
#include "GPULibrary.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include "Logging.h"

namespace WebCore {

RefPtr<GPUDevice> GPUDevice::create()
{
    RefPtr<GPUDevice> device = adoptRef(new GPUDevice());

#if PLATFORM(COCOA)
    if (!device->platformDevice()) {
        LOG(WebGPU, "GPUDevice::create() was unable to create the low-level device");
        return nullptr;
    }
#endif

    LOG(WebGPU, "GPUDevice::create() device is %p", device.get());
    return device;
}

GPUDevice::~GPUDevice()
{
    LOG(WebGPU, "GPUDevice::~GPUDevice()");
}

RefPtr<GPUCommandQueue> GPUDevice::createCommandQueue()
{
    return GPUCommandQueue::create(this);
}

RefPtr<GPULibrary> GPUDevice::createLibrary(const String& sourceCode)
{
    return GPULibrary::create(this, sourceCode);
}

RefPtr<GPUBuffer> GPUDevice::createBufferFromData(ArrayBufferView* data)
{
    return GPUBuffer::create(this, data);
}

RefPtr<GPUTexture> GPUDevice::createTexture(GPUTextureDescriptor* descriptor)
{
    return GPUTexture::create(this, descriptor);
}

RefPtr<GPUDrawable> GPUDevice::getFramebuffer()
{
    return GPUDrawable::create(this);
}

#if !PLATFORM(COCOA)

GPUDevice::GPUDevice()
{

}

void GPUDevice::reshape(int, int)
{
}

#endif

} // namespace WebCore

#endif
