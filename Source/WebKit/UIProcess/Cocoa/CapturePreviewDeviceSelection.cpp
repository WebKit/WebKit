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

#include "config.h"
#include "CapturePreviewDeviceSelection.h"

#if ENABLE(MEDIA_STREAM)

#include <algorithm>

namespace WebKit {

static bool isSameList(const Vector<WebCore::CaptureDevice>& a, const Vector<WebCore::CaptureDevice>& b)
{
    return std::ranges::equal(a, b, [](auto& deviceA, auto& deviceB) {
        return deviceA.persistentId() == deviceB.persistentId() && deviceA.label() == deviceB.label();
    });
}

static size_t indexOfDevice(const Vector<WebCore::CaptureDevice>& devices, const String& deviceID)
{
    if (deviceID.isEmpty())
        return 0;
    size_t index = devices.findIf([&deviceID](auto& device) {
        return device.persistentId() == deviceID;
    });
    return index == notFound ? 0 : index;
}

CapturePreviewDeviceSelection::CapturePreviewDeviceSelection(Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices)
    : m_videoDevices(WTF::move(videoDevices))
    , m_audioDevices(WTF::move(audioDevices))
    , m_requestNeedsVideo(!m_videoDevices.isEmpty())
    , m_requestNeedsAudio(!m_audioDevices.isEmpty())
{
}

std::optional<WebCore::CaptureDevice> CapturePreviewDeviceSelection::selectedDevice(CapturePreviewDeviceType type) const
{
    auto& devices = this->devices(type);
    size_t index = selectedIndex(type);
    if (index >= devices.size())
        return std::nullopt;
    return devices[index];
}

String CapturePreviewDeviceSelection::selectedDeviceID(CapturePreviewDeviceType type) const
{
    auto device = selectedDevice(type);
    return device ? device->persistentId() : String { };
}

void CapturePreviewDeviceSelection::select(CapturePreviewDeviceType type, size_t index)
{
    ASSERT(index < devices(type).size());
    if (type == CapturePreviewDeviceType::Camera)
        m_selectedVideoIndex = index;
    else
        m_selectedAudioIndex = index;
}

bool CapturePreviewDeviceSelection::canStillSatisfyRequest() const
{
    return !(m_requestNeedsVideo && m_videoDevices.isEmpty()) && !(m_requestNeedsAudio && m_audioDevices.isEmpty());
}

bool CapturePreviewDeviceSelection::hasSameDevices(const Vector<WebCore::CaptureDevice>& videoDevices, const Vector<WebCore::CaptureDevice>& audioDevices) const
{
    return isSameList(m_videoDevices, videoDevices) && isSameList(m_audioDevices, audioDevices);
}

bool CapturePreviewDeviceSelection::update(Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices)
{
    String previousVideoDeviceID = selectedDeviceID(CapturePreviewDeviceType::Camera);
    String previousAudioDeviceID = selectedDeviceID(CapturePreviewDeviceType::Microphone);

    m_videoDevices = WTF::move(videoDevices);
    m_audioDevices = WTF::move(audioDevices);

    m_selectedVideoIndex = indexOfDevice(m_videoDevices, previousVideoDeviceID);
    m_selectedAudioIndex = indexOfDevice(m_audioDevices, previousAudioDeviceID);

    return selectedDeviceID(CapturePreviewDeviceType::Camera) != previousVideoDeviceID
        || selectedDeviceID(CapturePreviewDeviceType::Microphone) != previousAudioDeviceID;
}

} // namespace WebKit

#endif // ENABLE(MEDIA_STREAM)
