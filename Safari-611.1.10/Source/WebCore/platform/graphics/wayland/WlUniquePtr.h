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
#include <wayland-client-protocol.h>
#include <wayland-server.h>

namespace WebCore {

template<typename T>
struct WlPtrDeleter {
};

template<typename T>
using WlUniquePtr = std::unique_ptr<T, WlPtrDeleter<T>>;

// wl_display is omitted because there are two different destruction functions,
// wl_display_disconnect() for the client process API and wl_display_destroy()
// for the server process API. WebKit uses both, so specializing a deleter here
// would be a footgun.
#define FOR_EACH_WAYLAND_DELETER(macro) \
    macro(struct wl_global, wl_global_destroy) \
    macro(struct wl_surface, wl_surface_destroy) \
    macro(struct wl_compositor, wl_compositor_destroy) \
    macro(struct wl_registry, wl_registry_destroy) \
    macro(struct wl_proxy, wl_proxy_destroy)

#define DEFINE_WAYLAND_DELETER(typeName, deleterFunc) \
    template<> struct WlPtrDeleter<typeName> \
    { \
        void operator() (typeName* ptr) const { deleterFunc(ptr); } \
    };

FOR_EACH_WAYLAND_DELETER(DEFINE_WAYLAND_DELETER)
#undef FOR_EACH_WAYLAND_DELETER
#undef DEFINE_WAYLAND_DELETER

} // namespace WebCore

#endif // PLATFORM(WAYLAND)
