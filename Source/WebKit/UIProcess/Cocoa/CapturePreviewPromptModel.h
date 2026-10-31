/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "CapturePreviewDeviceSelection.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/HostingContext.h>
#include <optional>
#include <wtf/Function.h>

namespace WebKit {

class CapturePreviewPromptModel : public CapturePreviewDeviceSelection {
public:
    using SelectionChangedHandler = Function<void(std::optional<WebCore::CaptureDevice>&& videoDevice, std::optional<WebCore::CaptureDevice>&& audioDevice)>;

    CapturePreviewPromptModel() = default;
    CapturePreviewPromptModel(Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices)
        : CapturePreviewDeviceSelection(WTF::move(videoDevices), WTF::move(audioDevices))
    {
    }

    bool isStopped() const { return m_hasStopped; }

    bool shouldIgnoreHostingContext(const WebCore::HostingContext& hostingContext) const { return m_hasStopped && hostingContext.contextID; }

    void setSelectionChangedHandler(SelectionChangedHandler&& handler)
    {
        if (m_hasStopped)
            return;
        m_selectionChangedHandler = WTF::move(handler);
        notifySelectionChanged();
    }

    void beginPreview()
    {
        if (m_isPreviewing || m_hasStopped)
            return;
        m_isPreviewing = true;
        notifySelectionChanged();
    }

    void notifySelectionChanged()
    {
        if (!m_selectionChangedHandler || !m_isPreviewing)
            return;

        m_selectionChangedHandler(selectedDevice(CapturePreviewDeviceType::Camera), selectedDevice(CapturePreviewDeviceType::Microphone));
    }

    void stop()
    {
        m_hasStopped = true;
        m_selectionChangedHandler = nullptr;
    }

private:
    SelectionChangedHandler m_selectionChangedHandler;
    bool m_isPreviewing { false };
    bool m_hasStopped { false };
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
