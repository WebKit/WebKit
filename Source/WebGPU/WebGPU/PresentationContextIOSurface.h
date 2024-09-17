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
#import <wtf/MachSendRight.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Vector.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebGPU {

class Device;
class Instance;

class PresentationContextIOSurface : public PresentationContext {
    WTF_MAKE_TZONE_ALLOCATED(PresentationContextIOSurface);
public:
    static Ref<PresentationContextIOSurface> create(const WGPUSurfaceDescriptor&, const Instance&);

    virtual ~PresentationContextIOSurface();

    void configure(Device&, const WGPUSwapChainDescriptor&) override;
    void unconfigure() override;

    void present() override;
    Texture* getCurrentTexture() override; // FIXME: This should return a Texture&.
    TextureView* getCurrentTextureView() override; // FIXME: This should return a TextureView&.

    bool isPresentationContextIOSurface() const override { return true; }

    bool isValid() override { return true; }
private:
    PresentationContextIOSurface(const WGPUSurfaceDescriptor&, const Instance&);

    void renderBuffersWereRecreated(NSArray<IOSurface *> *renderBuffers);
    void onSubmittedWorkScheduled(Function<void()>&&);
    RetainPtr<CGImageRef> getTextureAsNativeImage(uint32_t bufferIndex, bool& isIOSurfaceSupportedFormat) final;

    NSArray<IOSurface *> *m_ioSurfaces { nil };
    struct RenderBuffer {
        Ref<Texture> texture;
        RefPtr<Texture> luminanceClampTexture;
    };
    Vector<RenderBuffer> m_renderBuffers;
    RefPtr<Device> m_device;
    RefPtr<Texture> m_invalidTexture;
    size_t m_currentIndex { 0 };
    id<MTLFunction> m_luminanceClampFunction;
    id<MTLComputePipelineState> m_computePipelineState;
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    std::optional<const MachSendRight> m_webProcessID;
#endif
    WGPUColorSpace m_colorSpace { WGPUColorSpace::SRGB };
    WGPUToneMappingMode m_toneMappingMode { WGPUToneMappingMode_Standard };
    WGPUCompositeAlphaMode m_alphaMode { WGPUCompositeAlphaMode_Premultiplied };
};

} // namespace WebGPU

SPECIALIZE_TYPE_TRAITS_WEBGPU_PRESENTATION_CONTEXT(PresentationContextIOSurface, isPresentationContextIOSurface());
