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

#if ENABLE(WEBMETAL)

#import "Logging.h"
#import "WebMetalLayer.h"
#import <JavaScriptCore/ArrayBuffer.h>
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

GPUDevice::GPUDevice()
    : m_metal { adoptNS(MTLCreateSystemDefaultDevice()) }
{

    if (!m_metal) {
        LOG(WebMetal, "GPUDevice::GPUDevice -- could not create the device. This usually means Metal is not available on this hardware.");
        return;
    }

    LOG(WebMetal, "GPUDevice::GPUDevice Metal device is %p", m_metal.get());

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    m_layer = adoptNS([[WebMetalLayer alloc] initWithGPUDevice:this]);
    [m_layer setOpaque:0];
    [m_layer setName:@"WebMetal Layer"];

    // FIXME: WebMetal - Should this be in the WebMetalLayer initializer?
    [m_layer setDevice:m_metal.get()];
    [m_layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
    [m_layer setFramebufferOnly:YES];

    END_BLOCK_OBJC_EXCEPTIONS
}

void GPUDevice::disconnect()
{
    m_layer.get().context = nullptr;
    // FIXME: Should we also clear out the device property, which has the MTLDevice?
}

void GPUDevice::reshape(int width, int height) const
{
    [m_layer setBounds:CGRectMake(0, 0, width, height)];
    [m_layer setDrawableSize:CGSizeMake(width, height)];

    // FIXME: WebMetal - Lots of reshape stuff should go here. See GC3D.
}

CALayer *GPUDevice::platformLayer() const
{
    return m_layer.get();
}

bool GPUDevice::operator!() const
{
    return !m_metal;
}

} // namespace WebCore

#endif
