/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wayland-client.h>
#include <wtf/FastMalloc.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>

typedef struct _WPEViewWayland WPEViewWayland;

namespace WPE {

class WaylandOutput {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WaylandOutput(struct wl_output*);
    ~WaylandOutput();

    struct wl_output* output() const { return m_output; }
    double scale() const { return m_scale; }

    void addScaleObserver(WPEViewWayland*, Function<void(WPEViewWayland*)>&&);
    void removeScaleObserver(WPEViewWayland*);

private:
    static const struct wl_output_listener s_listener;

    struct wl_output* m_output { nullptr };
    double m_scale { 1. };
    HashMap<WPEViewWayland*, Function<void(WPEViewWayland*)>> m_scaleObservers;
};

} // namespace WPE
