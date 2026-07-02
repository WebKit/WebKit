/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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
#include <WebCore/IntSize.h>
#include <openxr/openxr.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

class OpenXRGraphicsBinding;

class OpenXRSwapchain {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRSwapchain);
    WTF_MAKE_NONCOPYABLE(OpenXRSwapchain);
public:
    enum class HasAlpha { No, Yes };
    static std::unique_ptr<OpenXRSwapchain> create(XrSession, const XrSwapchainCreateInfo&, HasAlpha, const OpenXRGraphicsBinding&);
    ~OpenXRSwapchain();

    std::optional<uint64_t> acquireImage();
    void releaseImage();
    XrSwapchain swapchain() const { return m_swapchain; }
    int32_t width() const { return m_createInfo.width; }
    int32_t height() const { return m_createInfo.height; }
    WebCore::IntSize size() const { return WebCore::IntSize(width(), height()); }
    int64_t format() const { return m_createInfo.format; }
    uint64_t acquiredTexture() const { return m_acquiredTexture; }
    HasAlpha hasAlpha() const { return m_hasAlpha; }
    size_t imageCount() const { return m_imageHandles.size(); }

private:
    OpenXRSwapchain(XrSwapchain, const XrSwapchainCreateInfo&, Vector<uint64_t>&&, HasAlpha);

    XrSwapchain m_swapchain;
    XrSwapchainCreateInfo m_createInfo;
    Vector<uint64_t> m_imageHandles;
    uint64_t m_acquiredTexture { 0 };
    HasAlpha m_hasAlpha;
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
