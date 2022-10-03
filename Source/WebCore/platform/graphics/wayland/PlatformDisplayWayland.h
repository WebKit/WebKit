/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "PlatformDisplay.h"
#include "WlUniquePtr.h"
#include <wayland-client.h>

namespace WebCore {

class PlatformDisplayWayland : public PlatformDisplay {
public:
    static std::unique_ptr<PlatformDisplay> create();
#if PLATFORM(GTK)
    static std::unique_ptr<PlatformDisplay> create(GdkDisplay*);
#endif

    virtual ~PlatformDisplayWayland();

    struct wl_display* native() const { return m_display; }

    WlUniquePtr<struct wl_surface> createSurface() const;

private:
    static const struct wl_registry_listener s_registryListener;

    Type type() const final { return PlatformDisplay::Type::Wayland; }

    void registryGlobal(const char* interface, uint32_t name);

protected:
    explicit PlatformDisplayWayland(struct wl_display*);
#if PLATFORM(GTK)
    explicit PlatformDisplayWayland(GdkDisplay*);

    void sharedDisplayDidClose() override;
#endif

    void initialize();

private:
    struct wl_display* m_display;
    WlUniquePtr<struct wl_registry> m_registry;
    WlUniquePtr<struct wl_compositor> m_compositor;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_DISPLAY(PlatformDisplayWayland, Wayland)

#endif // PLATFORM(WAYLAND)
