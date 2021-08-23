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

#include "ViewBackend.h"
#include <cairo.h>
#include <glib.h>

namespace WPEToolingBackends {

class HeadlessViewBackend final : public ViewBackend {
public:
    HeadlessViewBackend(uint32_t width, uint32_t height);
    virtual ~HeadlessViewBackend();

    struct wpe_view_backend* backend() const override;

    cairo_surface_t* snapshot();

private:
    void updateSnapshot(struct wpe_fdo_shm_exported_buffer*);
    void vsync();

#if WPE_CHECK_VERSION(1, 11, 1)
    static bool onDOMFullScreenRequest(void* data, bool fullscreen);
    void dispatchFullscreenEvent();
#endif


    struct wpe_view_backend_exportable_fdo* m_exportable { nullptr };

    cairo_surface_t* m_snapshot { nullptr };

    struct {
        GSource* source { nullptr };
        bool pending { false };
    } m_update;

#if WPE_CHECK_VERSION(1, 11, 1)
    bool m_is_fullscreen { false };
#endif
};

} // namespace WPEToolingBackends
