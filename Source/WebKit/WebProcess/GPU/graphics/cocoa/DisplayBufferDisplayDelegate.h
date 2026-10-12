/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "IPCSemaphore.h"
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <WebCore/GraphicsLayerEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformCALayerDelegatedContents.h>
#include <wtf/Lock.h>
#include <wtf/MachSendRight.h>

namespace WebKit {

class DisplayBufferFence final : public WebCore::PlatformCALayerDelegatedContentsFence {
public:
    static Ref<DisplayBufferFence> create(IPC::Semaphore&& finishedFenceSemaphore)
    {
        return adoptRef(*new DisplayBufferFence(WTF::move(finishedFenceSemaphore)));
    }

    bool waitFor(Seconds timeout) final
    {
        Locker locker { m_lock };
        if (m_signaled)
            return true;
        m_signaled = m_semaphore.waitFor(timeout);
        return m_signaled;
    }

    void forceSignal()
    {
        Locker locker { m_lock };
        if (m_signaled)
            return;
        m_signaled = true;
        m_semaphore.signal();
    }

private:
    DisplayBufferFence(IPC::Semaphore&& finishedFenceSemaphore)
        : m_semaphore(WTF::move(finishedFenceSemaphore))
    {
    }

    Lock m_lock;
    bool m_signaled WTF_GUARDED_BY_LOCK(m_lock) { false };
    IPC::Semaphore m_semaphore;
};

class DisplayBufferDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque, WebCore::GraphicsLayerCompositingCoordinatesOrientation orientation)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque, orientation));
    }

    // WebCore::GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(WebCore::PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
    }

    void display(WebCore::PlatformCALayer& layer) final
    {
        if (!m_displayBuffer) {
            layer.clearContents();
            return;
        }
        if (m_contentsFormat)
            layer.setContentsFormat(*m_contentsFormat);
        layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, m_finishedFence });
    }

    // Set once, before the first display.
    void setContentsFormat(WebCore::ContentsFormat contentsFormat)
    {
        ASSERT(!m_contentsFormat);
        m_contentsFormat = contentsFormat;
    }

    WebCore::GraphicsLayerCompositingCoordinatesOrientation orientation() const final
    {
        return m_orientation;
    }

    void setDisplayBuffer(MachSendRight&& displayBuffer, RefPtr<DisplayBufferFence> finishedFence)
    {
        if (!displayBuffer) {
            m_finishedFence = nullptr;
            m_displayBuffer = { };
            return;
        }
        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;
        m_finishedFence = WTF::move(finishedFence);
        m_displayBuffer = WTF::move(displayBuffer);
    }

private:
    DisplayBufferDisplayDelegate(bool isOpaque, WebCore::GraphicsLayerCompositingCoordinatesOrientation orientation)
        : m_isOpaque(isOpaque)
        , m_orientation(orientation)
    {
    }

    MachSendRight m_displayBuffer;
    RefPtr<DisplayBufferFence> m_finishedFence;
    std::optional<WebCore::ContentsFormat> m_contentsFormat;
    const bool m_isOpaque;
    const WebCore::GraphicsLayerCompositingCoordinatesOrientation m_orientation;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
