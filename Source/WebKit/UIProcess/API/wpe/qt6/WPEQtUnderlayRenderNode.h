/*
 * Copyright (C) 2026 Savoir-faire Linux, Inc.
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

#include "WPEQtUnderlayBlitter.h"

#include <QMatrix4x4>
#include <QRectF>
#include <QSGRenderNode>

#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

class WPEQtView;
typedef struct _WPEBuffer WPEBuffer;
typedef struct _WPEViewQtQuick WPEViewQtQuick;

class WPEQtUnderlayRenderNode final : public QSGRenderNode {
public:
    WPEQtUnderlayRenderNode() = default;
    ~WPEQtUnderlayRenderNode() override;

    void setView(WPEQtView*, WPEViewQtQuick*);
    void setRect(const QRectF& rect) { m_rect = rect; }
    QRectF rect() const override { return m_rect; }
    StateFlags changedStates() const override;
    RenderingFlags flags() const override { return BoundedRectRendering; }
    void render(const RenderState*) override;
    void releaseResources() override;
    void syncFrame();

private:
    WPEQtView* m_qtView { nullptr };
    WTF::GRefPtr<WPEViewQtQuick> m_wpeView;
    WPEQtUnderlayBlitter m_blitter;
    WTF::GRefPtr<WPEBuffer> m_buffer;
    WTF::UnixFileDescriptor m_releaseFence;
    bool m_frameNeedsAck { false };
    bool m_frameReadyForAck { false };
    QRectF m_rect;
};
