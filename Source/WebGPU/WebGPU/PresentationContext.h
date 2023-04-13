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

#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/RetainPtr.h>
#import <wtf/TypeCasts.h>

struct WGPUSurfaceImpl {
};

struct WGPUSwapChainImpl {
};

namespace WebGPU {

class Adapter;
class Device;
class Texture;
class TextureView;

class PresentationContext : public WGPUSurfaceImpl, public WGPUSwapChainImpl, public RefCounted<PresentationContext> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<PresentationContext> create(const WGPUSurfaceDescriptor&);
    static Ref<PresentationContext> createInvalid()
    {
        return adoptRef(*new PresentationContext());
    }

    virtual ~PresentationContext();

    WGPUTextureFormat getPreferredFormat(const Adapter&);

    virtual void configure(Device&, const WGPUSwapChainDescriptor&);
    virtual void unconfigure();

    virtual void present();
    virtual Texture* getCurrentTexture(); // FIXME: This should return a Texture&.
    virtual TextureView* getCurrentTextureView(); // FIXME: This should return a TextureView&.

    virtual bool isPresentationContextIOSurface() const { return false; }
    virtual bool isPresentationContextCoreAnimation() const { return false; }

protected:
    PresentationContext();
};

} // namespace WebGPU

#define SPECIALIZE_TYPE_TRAITS_WEBGPU_PRESENTATION_CONTEXT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::ToValueTypeName) \
    static bool isType(const WebGPU::PresentationContext& presentationContext) { return presentationContext.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
