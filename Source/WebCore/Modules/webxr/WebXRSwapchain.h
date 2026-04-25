/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include "GraphicsContextGL.h"

namespace WebCore {

class WebXRSwapchain {
public:
    virtual ~WebXRSwapchain() = default;
    virtual PlatformGLObject currentTexture() = 0;

    enum class SwapchainTargetFlags : uint8_t {
        Color = 1 << 0,
        Depth = 1 << 1,
        Stencil = 1 << 2,
    };
    using SwapchainTargets = OptionSet<SwapchainTargetFlags>;
protected:
    WebXRSwapchain(SwapchainTargets targets, bool clearOnAccess)
        : m_targetFlags(targets)
        , m_clearOnAccess(clearOnAccess)
    { };

    SwapchainTargets m_targetFlags;
    bool m_clearOnAccess { false };
};

} // namespace WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::WebXRSwapchain::SwapchainTargetFlags> {
    using values = EnumValues<
        WebCore::WebXRSwapchain::SwapchainTargetFlags,
        WebCore::WebXRSwapchain::SwapchainTargetFlags::Color,
        WebCore::WebXRSwapchain::SwapchainTargetFlags::Depth,
        WebCore::WebXRSwapchain::SwapchainTargetFlags::Stencil
    >;
};

} // namespace WTF

#endif // ENABLE(WEBXR_LAYERS)
