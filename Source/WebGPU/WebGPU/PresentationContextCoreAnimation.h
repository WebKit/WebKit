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

#import <QuartzCore/QuartzCore.h>
#import <optional>
#import <wtf/text/WTFString.h>

namespace WebGPU {

class PresentationContextCoreAnimation : public PresentationContext {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PresentationContextCoreAnimation> create(const WGPUSurfaceDescriptor& descriptor)
    {
        return adoptRef(*new PresentationContextCoreAnimation(descriptor));
    }

    virtual ~PresentationContextCoreAnimation();

    void configure(Device&, const WGPUSwapChainDescriptor&) override;
    void unconfigure() override;

    void present() override;
    Texture* getCurrentTexture() override; // FIXME: This should return a Texture&.
    TextureView* getCurrentTextureView() override; // FIXME: This should return a TextureView&.

    bool isPresentationContextCoreAnimation() const override { return true; }

private:
    PresentationContextCoreAnimation(const WGPUSurfaceDescriptor&);

    struct Configuration {
        Configuration(uint32_t width, uint32_t height, WGPUTextureUsageFlags usage, String&& label, WGPUTextureFormat format, Device& device)
            : width(width)
            , height(height)
            , usage(usage)
            , label(WTFMove(label))
            , format(format)
            , device(device)
        {
        }

        struct FrameState {
            id<CAMetalDrawable> currentDrawable;
            RefPtr<Texture> currentTexture;
            RefPtr<TextureView> currentTextureView;
        };

        FrameState generateCurrentFrameState(CAMetalLayer *);

        std::optional<FrameState> currentFrameState;
        uint32_t width { 0 };
        uint32_t height { 0 };
        WGPUTextureUsageFlags usage { WGPUTextureUsage_None };
        String label;
        WGPUTextureFormat format { WGPUTextureFormat_Undefined };
        Ref<Device> device;
    };


    CAMetalLayer *m_layer { nil };
    std::optional<Configuration> m_configuration;
};

} // namespace WebGPU

SPECIALIZE_TYPE_TRAITS_WEBGPU_PRESENTATION_CONTEXT(PresentationContextCoreAnimation, isPresentationContextCoreAnimation());
