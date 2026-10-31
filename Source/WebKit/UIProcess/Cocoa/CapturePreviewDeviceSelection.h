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

#include <WebCore/CaptureDevice.h>
#include <optional>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class CapturePreviewDeviceType : bool { Camera, Microphone };

class CapturePreviewDeviceSelection {
public:
    CapturePreviewDeviceSelection() = default;
    CapturePreviewDeviceSelection(Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices);

    bool requestNeeds(CapturePreviewDeviceType type) const { return type == CapturePreviewDeviceType::Camera ? m_requestNeedsVideo : m_requestNeedsAudio; }
    const Vector<WebCore::CaptureDevice>& devices(CapturePreviewDeviceType type) const { return type == CapturePreviewDeviceType::Camera ? m_videoDevices : m_audioDevices; }
    size_t selectedIndex(CapturePreviewDeviceType type) const { return type == CapturePreviewDeviceType::Camera ? m_selectedVideoIndex : m_selectedAudioIndex; }

    std::optional<WebCore::CaptureDevice> selectedDevice(CapturePreviewDeviceType) const;
    String selectedDeviceID(CapturePreviewDeviceType) const;
    void select(CapturePreviewDeviceType, size_t index);
    bool canStillSatisfyRequest() const;
    bool hasSameDevices(const Vector<WebCore::CaptureDevice>& videoDevices, const Vector<WebCore::CaptureDevice>& audioDevices) const;
    bool update(Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices);

private:
    Vector<WebCore::CaptureDevice> m_videoDevices;
    Vector<WebCore::CaptureDevice> m_audioDevices;
    size_t m_selectedVideoIndex { 0 };
    size_t m_selectedAudioIndex { 0 };
    bool m_requestNeedsVideo { false };
    bool m_requestNeedsAudio { false };
};

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
