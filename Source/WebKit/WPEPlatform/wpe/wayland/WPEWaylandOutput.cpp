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

#include "config.h"
#include "WPEWaylandOutput.h"

namespace WPE {

WaylandOutput::WaylandOutput(struct wl_output* output)
    : m_output(output)
{
    wl_output_add_listener(m_output, &s_listener, this);
}

WaylandOutput::~WaylandOutput()
{
    wl_output_destroy(m_output);
}

const struct wl_output_listener WaylandOutput::s_listener = {
    // geometry
    [](void*, struct wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t)
    {
    },
    // mode
    [](void*, struct wl_output*, uint32_t, int32_t, int32_t, int32_t)
    {
    },
    // done
    [](void*, struct wl_output*)
    {
    },
    // scale
    [](void* data, struct wl_output*, int32_t factor)
    {
        auto& output = *static_cast<WaylandOutput*>(data);
        if (output.m_scale == factor)
            return;

        output.m_scale = factor;
        for (const auto& it : output.m_scaleObservers)
            it.value(it.key);

    },
#ifdef WL_OUTPUT_NAME_SINCE_VERSION
    // name
    [](void*, struct wl_output*, const char*)
    {
    },
#endif
#ifdef WL_OUTPUT_DESCRIPTION_SINCE_VERSION
    // description
    [](void*, struct wl_output*, const char*)
    {
    },
#endif
};

void WaylandOutput::addScaleObserver(WPEViewWayland* view, Function<void(WPEViewWayland*)>&& observer)
{
    m_scaleObservers.set(view, WTFMove(observer));
}

void WaylandOutput::removeScaleObserver(WPEViewWayland* view)
{
    m_scaleObservers.remove(view);
}

} // namespace WPE
