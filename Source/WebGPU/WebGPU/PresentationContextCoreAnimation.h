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
#import <QuartzCore/CAMetalLayer.h>
#import <optional>
#import <wtf/OptionSet.h>
#import <wtf/Ref.h>
#import <wtf/RefPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/Vector.h>

namespace WebGPU::Metal {

class Device;
class Texture;

// A presentation context that presents into a CAMetalLayer, for the surfaces of the C API that
// are made from one (WGPUSurfaceSourceMetalLayer).
class PresentationContextCoreAnimation final : public PresentationContext {
    WTF_MAKE_TZONE_ALLOCATED(PresentationContextCoreAnimation);
public:
    static Ref<PresentationContextCoreAnimation> create(CAMetalLayer *layer)
    {
        return adoptRef(*new PresentationContextCoreAnimation(layer));
    }

    virtual ~PresentationContextCoreAnimation();

    void configure(const WebGPU::CanvasConfiguration&) final;
    void unconfigure() final;

    void present(uint32_t) final;
    Texture* currentTexture(uint32_t) final;

    bool isPresentationContextCoreAnimation() const final { return true; }
    bool isValid() const final { return !!m_configuration; }

private:
    explicit PresentationContextCoreAnimation(CAMetalLayer *);

    struct Configuration {
        Ref<Device> device;
        WebGPU::TextureFormat format;
        OptionSet<WebGPU::TextureUsage> usage;
        Vector<WebGPU::TextureFormat> viewFormats;
        uint32_t width { 0 };
        uint32_t height { 0 };
    };

    // The drawable of the frame, and its texture, until the frame is presented.
    struct Frame {
        id<CAMetalDrawable> drawable;
        Ref<Texture> texture;
    };
    Frame* currentFrame();

    CAMetalLayer *m_layer { nil };
    std::optional<Configuration> m_configuration;
    std::optional<Frame> m_currentFrame;
};

} // namespace WebGPU::Metal

SPECIALIZE_TYPE_TRAITS_WEBGPU_PRESENTATION_CONTEXT(PresentationContextCoreAnimation, isPresentationContextCoreAnimation());
