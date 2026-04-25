/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace PlatformXR {
struct FrameData;
struct RateMapDescription;
using LayerHandle = int;
struct DeviceLayer;
}

namespace WebCore {

class XRLayerBacking : public RefCountedAndCanMakeWeakPtr<XRLayerBacking> {
    WTF_MAKE_TZONE_ALLOCATED(XRLayerBacking);
public:
    virtual uint32_t colorTextureWidth() const = 0;
    virtual uint32_t colorTextureHeight() const = 0;
    virtual uint32_t colorTextureArrayLength() const = 0;

    virtual std::optional<uint32_t> depthTextureWidth() const { return std::nullopt; }
    virtual std::optional<uint32_t> depthTextureHeight() const { return std::nullopt; }

#if PLATFORM(COCOA)
    virtual void startFrame(size_t frameIndex, MachSendRight&& colorBuffer, MachSendRight&& depthBuffer, MachSendRight&& completionSyncEvent, size_t reusableTextureIndex, PlatformXR::RateMapDescription&&) = 0;
    virtual void endFrame() = 0;
#else
    virtual void startFrame(PlatformXR::FrameData&) = 0;
    virtual void endFrame(PlatformXR::DeviceLayer&) = 0;
#endif

    virtual ~XRLayerBacking() = default;

    PlatformXR::LayerHandle handle() { return m_handle; }
    void setHandle(PlatformXR::LayerHandle handle) { m_handle = handle; }

    virtual bool allColorTexturesAreBound() const = 0;

private:
    PlatformXR::LayerHandle m_handle;
};

} // namespace WebCore
