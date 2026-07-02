/*
 * Copyright (C) 2026 Igalia S.L.
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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRUtils.h"
#include <memory>
#include <optional>
#include <wtf/Vector.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebKit {

class OpenXRSwapchain;

// Encapsulates the graphics library specific parts of the OpenXR backend behind an API-agnostic interface.
// Initialization is split because the constraints differ per API: EGL creates its display before
// xrCreateInstance, whereas a Vulkan binding must create its instance/device only after the OpenXR
// instance and system exist. So requiredInstanceExtensions()/initializeDisplay() run before/around
// instance creation and initializeForSession() once the instance and system are known.
class OpenXRGraphicsBinding {
public:
    enum class TextureType {
        Texture2D,
        Cubemap,
    };

    virtual ~OpenXRGraphicsBinding() = default;

    virtual Vector<ASCIILiteral> requiredInstanceExtensions() const = 0;
    virtual bool initializeDisplay(bool isForTesting) = 0;
    virtual bool initializeForSession(XrInstance, XrSystemId) = 0;

    // XrGraphicsBinding* to chain into XrSessionCreateInfo::next.
    virtual const void* sessionGraphicsBinding() const = 0;

    virtual int64_t selectColorFormat(const Vector<int64_t>& supportedFormats, bool alpha) const = 0;
    virtual Vector<uint64_t> enumerateSwapchainImages(XrSwapchain) const = 0;

    virtual std::optional<PlatformXR::FrameData::ExternalTexture> exportTexture(uint64_t image, const OpenXRSwapchain&, TextureType, uint32_t width, uint32_t height) = 0;

    virtual void commitFrame(uint64_t keyImage, const OpenXRSwapchain&, TextureType, const Vector<uint64_t>& images) = 0;
    virtual void waitFrameFence(WTF::UnixFileDescriptor&&) = 0;

    virtual void releaseSessionGraphics() = 0;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
