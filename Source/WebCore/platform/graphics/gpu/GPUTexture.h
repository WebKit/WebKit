/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGPU)

#include "GPUTextureUsage.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(METAL)
#include <wtf/RetainPtr.h>
#endif

#if USE(METAL)
OBJC_PROTOCOL(MTLTexture);
#endif

namespace WebCore {

class GPUDevice;
class GPUErrorScopes;

struct GPUTextureDescriptor;

#if USE(METAL)
using PlatformTexture = MTLTexture;
using PlatformTextureSmartPtr = RetainPtr<MTLTexture>;
#endif

class GPUTexture : public RefCounted<GPUTexture> {
public:
    static RefPtr<GPUTexture> tryCreate(const GPUDevice&, const GPUTextureDescriptor&, GPUErrorScopes&);
    static Ref<GPUTexture> create(PlatformTextureSmartPtr&&, OptionSet<GPUTextureUsage::Flags>);

    PlatformTexture *platformTexture() const { return m_platformTexture.get(); }
    bool isCopySource() const { return m_usage.contains(GPUTextureUsage::Flags::CopySource); }
    bool isCopyDestination() const { return m_usage.contains(GPUTextureUsage::Flags::CopyDestination); }
    bool isOutputAttachment() const { return m_usage.contains(GPUTextureUsage::Flags::OutputAttachment); }
    bool isReadOnly() const { return m_usage.containsAny({ GPUTextureUsage::Flags::CopySource, GPUTextureUsage::Flags::Sampled }); }
    bool isSampled() const { return m_usage.contains(GPUTextureUsage::Flags::Sampled); }
    bool isStorage() const { return m_usage.contains(GPUTextureUsage::Flags::Storage); }
    unsigned platformUsage() const { return m_platformUsage; }

    RefPtr<GPUTexture> tryCreateDefaultTextureView();
    void destroy() { m_platformTexture = nullptr; }

private:
    explicit GPUTexture(PlatformTextureSmartPtr&&, OptionSet<GPUTextureUsage::Flags>);

    PlatformTextureSmartPtr m_platformTexture;

    OptionSet<GPUTextureUsage::Flags> m_usage;
    unsigned m_platformUsage;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
