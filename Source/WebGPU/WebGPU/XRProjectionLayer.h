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
#import <wtf/RefCounted.h>
#import <wtf/WeakPtr.h>

struct WGPUXRProjectionLayerImpl {
};

namespace WebGPU {

class CommandEncoder;
class Device;

class XRProjectionLayer : public WGPUXRProjectionLayerImpl, public RefCounted<XRProjectionLayer>, public CanMakeWeakPtr<XRProjectionLayer> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XRProjectionLayer> create(Device& device)
    {
        return adoptRef(*new XRProjectionLayer(true, device));
    }
    static Ref<XRProjectionLayer> createInvalid(Device& device)
    {
        return adoptRef(*new XRProjectionLayer(device));
    }

    ~XRProjectionLayer();

    void setLabel(String&&);

    bool isValid() const;
    void startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex);

    id<MTLTexture> colorTexture() const;
    id<MTLTexture> depthTexture() const;
    const std::pair<id<MTLSharedEvent>, uint64_t>& completionEvent() const;
    size_t reusableTextureIndex() const;

private:
    XRProjectionLayer(bool, Device&);
    XRProjectionLayer(Device&);

    NSMutableDictionary<NSNumber*, id<MTLTexture>>* m_colorTextures { nil };
    NSMutableDictionary<NSNumber*, id<MTLTexture>>* m_depthTextures { nil };
    id<MTLTexture> m_colorTexture { nil };
    id<MTLTexture> m_depthTexture { nil };
    std::pair<id<MTLSharedEvent>, uint64_t> m_sharedEvent;
    size_t m_reusableTextureIndex { 0 };

    Ref<Device> m_device;
};

} // namespace WebGPU
