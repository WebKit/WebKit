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

static QRect wpeQtUnderlayViewportFor(const QMatrix4x4& projectionModelView, const QRectF& rect, const int glViewport[4])
{
    auto toWindow = [&](qreal x, qreal y) -> QPointF {
        const QVector4D clip = projectionModelView * QVector4D(x, y, 0, 1);
        if (qFuzzyIsNull(clip.w()))
            return QPointF();
        // Convert clip coordinates to framebuffer pixels: NDC [-1, 1] -> [0, 1],
        // then scale and offset by the GL viewport (-1 to 0, 0 to 0.5 etc).
        return QPointF(glViewport[0] + (clip.x() / clip.w() * 0.5 + 0.5) * glViewport[2], glViewport[1] + (clip.y() / clip.w() * 0.5 + 0.5) * glViewport[3]);
    };
    // GL's Y axis is opposite to item coordinates, so normalize the mapped corners.
    const auto first = toWindow(rect.left(), rect.top());
    const auto second = toWindow(rect.right(), rect.bottom());
    return QRect(qRound(qMin(first.x(), second.x())), qRound(qMin(first.y(), second.y())), qRound(qAbs(second.x() - first.x())), qRound(qAbs(second.y() - first.y())));
}

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

QSGRenderNode::StateFlags WPEQtUnderlayRenderNode::changedStates() const
{
    return ViewportState;
}

void WPEQtUnderlayRenderNode::syncFrame()
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
    m_buffer = WTF::move(buffer);
    m_frameNeedsAck = didPromote;
    m_blitter.importEGLImage(image);
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

    int sceneViewport[4] = { 0, 0, 0, 0 };
    gl->glGetIntegerv(GL_VIEWPORT, sceneViewport);
    const auto viewport = wpeQtUnderlayViewportFor(*state->projectionMatrix() * (matrix() ? *matrix() : QMatrix4x4()), m_rect, sceneViewport);
    if (!m_blitter.draw(viewport.x(), viewport.y(), viewport.width(), viewport.height()))
        return;

    m_releaseFence = wpeQtUnderlayCreateReleaseFence(gl);
    if (!m_releaseFence)
        gl->glFinish();
    m_frameReadyForAck = true;
}
