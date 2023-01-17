/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#pragma once

#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>

struct WGPUSurfaceImpl {
};

struct WGPUSwapChainImpl {
};

namespace WebGPU {

class Adapter;
class Device;
class TextureView;

class PresentationContext : public WGPUSurfaceImpl, public WGPUSwapChainImpl, public RefCounted<PresentationContext> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PresentationContext> create(const WGPUSurfaceDescriptor& descriptor)
    {
        return adoptRef(*new PresentationContext(descriptor));
    }

    ~PresentationContext();

    WGPUTextureFormat getPreferredFormat(const Adapter&);

    void configure(Device&, const WGPUSwapChainDescriptor&);

    void present();
    TextureView* getCurrentTextureView(); // FIXME: This should return a TextureView&.

    RetainPtr<IOSurfaceRef> displayBuffer() const { return m_displayBuffer; }
    RetainPtr<IOSurfaceRef> drawingBuffer() const { return m_drawingBuffer; }
    RetainPtr<IOSurfaceRef> nextDrawable();

private:
    PresentationContext(const WGPUSurfaceDescriptor&);
    PresentationContext(int, int);

    RetainPtr<IOSurfaceRef> m_displayBuffer;
    RetainPtr<IOSurfaceRef> m_drawingBuffer;
};

} // namespace WebGPU
