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

#import "config.h"
#import "GPUDevice.h"

#import "Logging.h"
#import "WebGPULayer.h"

#import <JavaScriptCore/ArrayBuffer.h>
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

#if ENABLE(WEBGPU)

namespace WebCore {

GPUDevice::GPUDevice()
{
    m_device = MTLCreateSystemDefaultDevice();

    if (!m_device) {
        LOG(WebGPU, "GPUDevice::GPUDevice -- could not create the device. This usually means Metal is not available on this hardware.");
        return;
    }

    LOG(WebGPU, "GPUDevice::GPUDevice Metal device is %p", m_device.get());

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    m_layer = [[WebGPULayer alloc] initWithGPUDevice:this];
    [m_layer setOpaque:0];
    [m_layer setName:@"WebGPU Layer"];

    // FIXME: WebGPU - Should this be in the WebGPULayer initializer?
    [m_layer setDevice:(id<MTLDevice>)m_device.get()];
    [m_layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
    [m_layer setFramebufferOnly:YES];
    END_BLOCK_OBJC_EXCEPTIONS
}

void GPUDevice::reshape(int width, int height)
{
    if (!m_device)
        return;

    if (m_layer) {
        [m_layer setBounds:CGRectMake(0, 0, width, height)];
        [m_layer setDrawableSize:CGSizeMake(width, height)];
    }

    // FIXME: WebGPU - Lots of reshape stuff should go here. See GC3D.
}

id GPUDevice::platformDevice()
{
    return m_device.get();
}

} // namespace WebCore

#endif
