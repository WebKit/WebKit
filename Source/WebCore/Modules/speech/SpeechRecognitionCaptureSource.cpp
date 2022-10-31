/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "SpeechRecognitionCaptureSource.h"

#if ENABLE(MEDIA_STREAM)
#include "CaptureDeviceManager.h"
#include "RealtimeMediaSourceCenter.h"
#include "SpeechRecognitionUpdate.h"
#endif

namespace WebCore {

void SpeechRecognitionCaptureSource::mute()
{
#if ENABLE(MEDIA_STREAM)
    m_impl->mute();
#endif
}

#if ENABLE(MEDIA_STREAM)

std::optional<CaptureDevice> SpeechRecognitionCaptureSource::findCaptureDevice()
{
    std::optional<CaptureDevice> captureDevice;
    auto devices = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().audioCaptureDeviceManager().captureDevices();
    for (auto device : devices) {
        if (!device.enabled())
            continue;

        if (!captureDevice)
            captureDevice = device;

        if (device.isDefault()) {
            captureDevice = device;
            break;
        }
    }
    return captureDevice;
}

CaptureSourceOrError SpeechRecognitionCaptureSource::createRealtimeMediaSource(const CaptureDevice& captureDevice, PageIdentifier pageIdentifier)
{
    return RealtimeMediaSourceCenter::singleton().audioCaptureFactory().createAudioCaptureSource(captureDevice, { "SpeechID"_s, "SpeechID"_s }, { }, pageIdentifier);
}

SpeechRecognitionCaptureSource::SpeechRecognitionCaptureSource(SpeechRecognitionConnectionClientIdentifier clientIdentifier, DataCallback&& dataCallback, StateUpdateCallback&& stateUpdateCallback, Ref<RealtimeMediaSource>&& source)
    : m_impl(makeUnique<SpeechRecognitionCaptureSourceImpl>(clientIdentifier, WTFMove(dataCallback), WTFMove(stateUpdateCallback), WTFMove(source)))
{
}

#endif

} // namespace WebCore
