/*
 * Copyright (C) 2016 Igalia S.L.
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

#if PLATFORM(WAYLAND)

#include "AcceleratedSurface.h"
#include "WebPage.h"
#include <WebCore/WlUniquePtr.h>
#include <wayland-egl.h>

namespace WebKit {

class AcceleratedSurfaceWayland final : public AcceleratedSurface {
    WTF_MAKE_NONCOPYABLE(AcceleratedSurfaceWayland); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<AcceleratedSurfaceWayland> create(WebPage&, Client&);
    ~AcceleratedSurfaceWayland() = default;

    uint64_t window() const override { return reinterpret_cast<uint64_t>(m_window); }
    uint64_t surfaceID() const override { return m_webPage.pageID(); }
    void clientResize(const WebCore::IntSize&) override;
    bool shouldPaintMirrored() const override { return true; }

    void initialize() override;
    void finalize() override;
    void didRenderFrame() override;

private:
    AcceleratedSurfaceWayland(WebPage&, Client&);

    WebCore::WlUniquePtr<struct wl_surface> m_surface;
    struct wl_egl_window* m_window { nullptr };
};

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
