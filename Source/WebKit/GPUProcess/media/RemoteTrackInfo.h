/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/MediaSample.h>

namespace WebKit {

struct RemoteAudioInfo {
    RemoteAudioInfo() = default;
    explicit RemoteAudioInfo(const WebCore::AudioInfo& info)
        : codecName(info.codecName)
        , codecString(info.codecString)
        , trackID(info.trackID)
        , rate(info.rate)
        , channels(info.channels)
        , framesPerPacket(info.framesPerPacket)
        , bitDepth(info.bitDepth)
        , cookieData(info.cookieData)
    {
    }
    // Used by IPC generator
    RemoteAudioInfo(WebCore::FourCC codecName, const String& codecString, WebCore::TrackID trackID, uint32_t rate, uint32_t channels, uint32_t framesPerPacket, uint8_t bitDepth, RefPtr<WebCore::SharedBuffer> cookieData)
        : codecName(codecName)
        , codecString(codecString)
        , trackID(trackID)
        , rate(rate)
        , channels(channels)
        , framesPerPacket(framesPerPacket)
        , bitDepth(bitDepth)
        , cookieData(WTFMove(cookieData))
    {
    }

    Ref<WebCore::AudioInfo> toAudioInfo() const
    {
        Ref audioInfo = WebCore::AudioInfo::create();
        audioInfo->codecName = codecName;
        audioInfo->codecString = codecString;
        audioInfo->trackID = trackID;
        audioInfo->rate = rate;
        audioInfo->channels = channels;
        audioInfo->framesPerPacket = framesPerPacket;
        audioInfo->bitDepth = bitDepth;
        audioInfo->cookieData = cookieData;
        return audioInfo;
    }

    WebCore::FourCC codecName;
    String codecString;
    WebCore::TrackID trackID { 0 };
    uint32_t rate { 0 };
    uint32_t channels { 0 };
    uint32_t framesPerPacket { 0 };
    uint8_t bitDepth { 16 };
    RefPtr<WebCore::SharedBuffer> cookieData;
};

struct RemoteVideoInfo {
    RemoteVideoInfo() = default;
    explicit RemoteVideoInfo(const WebCore::VideoInfo& info)
        : codecName(info.codecName)
        , codecString(info.codecString)
        , trackID(info.trackID)
        , size(info.size)
        , displaySize(info.displaySize)
        , bitDepth(info.bitDepth)
        , colorSpace(info.colorSpace)
        , atomData(info.atomData)
    {
    }
    // Used by IPC generator
    RemoteVideoInfo(WebCore::FourCC codecName, const String& codecString, WebCore::TrackID trackID, WebCore::FloatSize size, WebCore::FloatSize displaySize, uint8_t bitDepth, WebCore::PlatformVideoColorSpace colorSpace, RefPtr<WebCore::SharedBuffer> atomData)
        : codecName(codecName)
        , codecString(codecString)
        , trackID(trackID)
        , size(size)
        , displaySize(displaySize)
        , bitDepth(bitDepth)
        , colorSpace(colorSpace)
        , atomData(WTFMove(atomData))
    {
    }

    Ref<WebCore::VideoInfo> toVideoInfo() const
    {
        Ref videoInfo = WebCore::VideoInfo::create();
        videoInfo->codecName = codecName;
        videoInfo->codecString = codecString;
        videoInfo->trackID = trackID;
        videoInfo->size = size;
        videoInfo->displaySize = displaySize;
        videoInfo->bitDepth = bitDepth;
        videoInfo->colorSpace = colorSpace;
        videoInfo->atomData = atomData;
        return videoInfo;
    }

    WebCore::FourCC codecName;
    String codecString;
    WebCore::TrackID trackID { 0 };
    WebCore::FloatSize size;
    WebCore::FloatSize displaySize;
    uint8_t bitDepth { 8 };
    WebCore::PlatformVideoColorSpace colorSpace;
    RefPtr<WebCore::SharedBuffer> atomData;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
