/*
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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
#include "WPEQtUnderlayRenderNode.h"

#include "WPEQtView.h"
#include "WPEViewQtQuick.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <epoxy/egl.h>

#include <wtf/glib/GUniquePtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

static WTF::UnixFileDescriptor wpeQtUnderlayCreateReleaseFence(QOpenGLFunctions* gl)
{
    auto display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY || !epoxy_has_egl_extension(display, "EGL_ANDROID_native_fence_sync"))
        return { };

    auto usesEGL15 = epoxy_egl_version(display) >= 15;
    if (!usesEGL15 && !epoxy_has_egl_extension(display, "EGL_KHR_fence_sync"))
        return { };

    auto sync = usesEGL15
        ? eglCreateSync(display, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr)
        : eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (sync == EGL_NO_SYNC_KHR)
        return { };

    gl->glFlush();

    auto fd = eglDupNativeFenceFDANDROID(display, sync);
    usesEGL15 ? eglDestroySync(display, sync) : eglDestroySyncKHR(display, sync);

    if (fd == -1)
        return { };

    return WTF::UnixFileDescriptor { fd, WTF::UnixFileDescriptor::Adopt };
}

WPEQtUnderlayRenderNode::~WPEQtUnderlayRenderNode()
{
    releaseResources();
}

void WPEQtUnderlayRenderNode::setView(WPEQtView* qtView, WPEViewQtQuick* wpeView)
{
    if (m_wpeView.get() != wpeView) {
        releaseResources();
        m_wpeView = wpeView;
    }
    m_qtView = qtView;
}

void WPEQtUnderlayRenderNode::releaseResources()
{
    if (m_frameNeedsAck && m_wpeView)
        wpe_view_qtquick_rollback_frame(m_wpeView.get());

    m_blitter.invalidate();
    m_buffer = nullptr;
    m_releaseFence = { };
    m_qtView = nullptr;
    m_frameNeedsAck = false;
    m_frameReadyForAck = false;
}

void WPEQtUnderlayRenderNode::advanceFrame()
{
    if (!m_wpeView)
        return;

    if (m_frameNeedsAck && !m_frameReadyForAck) {
        m_buffer = nullptr;
        wpe_view_qtquick_rollback_frame(m_wpeView.get());
        m_frameNeedsAck = false;
    }

    if (!m_blitter.initialize())
        return;

    if (m_frameReadyForAck) {
        if (m_releaseFence)
            wpe_view_qtquick_set_frame_release_fence(m_wpeView.get(), m_releaseFence.release());
        if (m_qtView)
            m_qtView->triggerDidUpdateScene();

        m_frameReadyForAck = false;
        m_frameNeedsAck = false;
    }

    EGLImage image = EGL_NO_IMAGE_KHR;
    gboolean didPromote = FALSE;
    GUniqueOutPtr<GError> error;
    GRefPtr<WPEBuffer> buffer = adoptGRef(wpe_view_qtquick_acquire_frame(m_wpeView.get(), &image, &didPromote, &error.outPtr()));
    if (!buffer)
        return;

    if (!m_blitter.importEGLImage(image)) {
        if (didPromote)
            wpe_view_qtquick_rollback_frame(m_wpeView.get());
        return;
    }

    m_buffer = WTF::move(buffer);
    m_frameNeedsAck = didPromote;
}

void WPEQtUnderlayRenderNode::render(const RenderState* state)
{
    if (!m_buffer || !state || !m_blitter.isInitialized())
        return;

    auto* context = QOpenGLContext::currentContext();
    if (!context)
        return;

    auto* gl = context->functions();
    if (!gl)
        return;

    const auto scissorRect = state->scissorEnabled() ? std::optional<QRect>(state->scissorRect()) : std::nullopt;
    const auto stencilValue = state->stencilEnabled() ? std::optional<int>(state->stencilValue()) : std::nullopt;

    QMatrix4x4 itemToClip = *state->projectionMatrix() * (matrix() ? *matrix() : QMatrix4x4());
    itemToClip.translate(m_rect.x(), m_rect.y());
    itemToClip.scale(m_rect.width(), m_rect.height());
    if (!m_blitter.draw(itemToClip, float(inheritedOpacity()), scissorRect, stencilValue))
        return;

    m_releaseFence = wpeQtUnderlayCreateReleaseFence(gl);
    if (!m_releaseFence)
        gl->glFinish();
    m_frameReadyForAck = true;
    // The frame is acknowledged from the next synchronization, which only runs
    // when the item is updated. Schedule that update here, otherwise WPE waits
    // for an acknowledgement that never arrives and stops producing frames.
    if (m_qtView)
        m_qtView->triggerUpdateScene();
}
