/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "PresentationContext.h"
#import <wtf/Vector.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebGPU {

class PresentationContextIOSurface : public PresentationContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PresentationContextIOSurface> create(const WGPUSurfaceDescriptor&);

    virtual ~PresentationContextIOSurface();

    void configure(Device&, const WGPUSwapChainDescriptor&) override;
    void unconfigure() override;

    void present() override;
    Texture* getCurrentTexture() override; // FIXME: This should return a Texture&.
    TextureView* getCurrentTextureView() override; // FIXME: This should return a TextureView&.

    // FIXME: Delete these.
    IOSurface *displayBuffer() const;
    IOSurface *drawingBuffer() const;

    bool isPresentationContextIOSurface() const override { return true; }

private:
    PresentationContextIOSurface(const WGPUSurfaceDescriptor&);

    void renderBuffersWereRecreated(NSArray<IOSurface *> *renderBuffers);

    NSArray<IOSurface *> *m_ioSurfaces { nil };
    struct RenderBuffer {
        Ref<Texture> texture;
        Ref<TextureView> textureView;
    };
    Vector<RenderBuffer> m_renderBuffers;
    size_t m_currentIndex { 0 };
};

} // namespace WebGPU

SPECIALIZE_TYPE_TRAITS_WEBGPU_PRESENTATION_CONTEXT(PresentationContextIOSurface, isPresentationContextIOSurface());
