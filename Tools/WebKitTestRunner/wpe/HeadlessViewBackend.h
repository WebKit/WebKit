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

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cairo.h>
#include <glib.h>
#include <unordered_map>
#include <wpe-mesa/view-backend-exportable-dma-buf.h>

class HeadlessViewBackend {
public:
    HeadlessViewBackend();
    ~HeadlessViewBackend();

    struct wpe_view_backend* backend() const;

    cairo_surface_t* createSnapshot();

private:
    bool makeCurrent();
    void performUpdate();

    static struct wpe_mesa_view_backend_exportable_dma_buf_client s_exportableClient;

    struct {
        EGLDisplay display;
        EGLConfig config;
        EGLContext context;

        PFNEGLCREATEIMAGEKHRPROC createImage;
        PFNEGLDESTROYIMAGEKHRPROC destroyImage;
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2DOES;
    } m_egl;

    struct wpe_mesa_view_backend_exportable_dma_buf* m_exportable;

    std::unordered_map<uint32_t, int32_t> m_exportMap;
    std::pair<uint32_t, std::tuple<EGLImageKHR, uint32_t, uint32_t>> m_pendingImage { };
    std::pair<uint32_t, std::tuple<EGLImageKHR, uint32_t, uint32_t>> m_lockedImage { };

    GSource* m_updateSource;
    gint64 m_frameRate { G_USEC_PER_SEC / 60 };
};
