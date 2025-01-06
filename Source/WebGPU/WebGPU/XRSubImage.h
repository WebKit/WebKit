/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#import <utility>
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/WeakPtr.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

struct WGPUXRSubImageImpl {
};

namespace WebGPU {

class CommandEncoder;
class Device;
class Texture;

class XRSubImage : public RefCountedAndCanMakeWeakPtr<XRSubImage>, public WGPUXRSubImageImpl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XRSubImage> create(Device& device)
    {
        return adoptRef(*new XRSubImage(true, device));
    }
    static Ref<XRSubImage> createInvalid(Device& device)
    {
        return adoptRef(*new XRSubImage(device));
    }

    ~XRSubImage();

    void setLabel(String&&);

    bool isValid() const;
    void update(id<MTLTexture> colorTexture, id<MTLTexture> depthTexture, size_t currentTextureIndex, const std::pair<id<MTLSharedEvent>, uint64_t>&);
    Texture* colorTexture();
    Texture* depthTexture();

private:
    XRSubImage(bool, Device&);
    XRSubImage(Device&);

    HashMap<uint64_t, RefPtr<Texture>, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_colorTextures;
    HashMap<uint64_t, RefPtr<Texture>, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_depthTextures;
    uint64_t m_currentTextureIndex { 0 };

    ThreadSafeWeakPtr<Device> m_device;
};

} // namespace WebGPU
