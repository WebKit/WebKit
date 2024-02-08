/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#import "Device.h"
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/WeakPtr.h>

struct WGPUExternalTextureImpl {
};

namespace WebGPU {

class CommandEncoder;

class ExternalTexture : public WGPUExternalTextureImpl, public RefCounted<ExternalTexture>, public CanMakeWeakPtr<ExternalTexture> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ExternalTexture> create(CVPixelBufferRef pixelBuffer, WGPUColorSpace colorSpace, Device& device)
    {
        return adoptRef(*new ExternalTexture(pixelBuffer, colorSpace, device));
    }
    static Ref<ExternalTexture> createInvalid(Device& device)
    {
        return adoptRef(*new ExternalTexture(device));
    }

    ~ExternalTexture();

    CVPixelBufferRef pixelBuffer() const { return m_pixelBuffer.get(); }
    WGPUColorSpace colorSpace() const { return m_colorSpace; }

    void destroy();
    void undestroy();
    void setCommandEncoder(CommandEncoder&) const;
    bool isDestroyed() const;

private:
    ExternalTexture(CVPixelBufferRef, WGPUColorSpace, Device&);
    ExternalTexture(Device&);

    RetainPtr<CVPixelBufferRef> m_pixelBuffer;
    WGPUColorSpace m_colorSpace;
    const Ref<Device> m_device;
    bool m_destroyed { false };
    mutable WeakPtr<CommandEncoder> m_commandEncoder;
};

} // namespace WebGPU

