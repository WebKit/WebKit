/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice;
class CaptureDeviceManager;
class RealtimeMediaSource;

struct CaptureSourceOrError;
struct MediaConstraints;

class WEBCORE_EXPORT SingleSourceFactory {
public:
    virtual ~SingleSourceFactory() = default;

    virtual void setActiveSource(RealtimeMediaSource&);
    virtual void unsetActiveSource(RealtimeMediaSource&);

    virtual RealtimeMediaSource* activeSource() { return m_activeSource; }

private:
    RealtimeMediaSource* m_activeSource { nullptr };
};

class AudioCaptureFactory
#if PLATFORM(IOS_FAMILY)
    : public SingleSourceFactory
#endif
{
public:
    virtual ~AudioCaptureFactory() = default;
    virtual CaptureSourceOrError createAudioCaptureSource(const CaptureDevice&, String&&, const MediaConstraints*) = 0;
    virtual CaptureDeviceManager& audioCaptureDeviceManager() = 0;

protected:
    AudioCaptureFactory() = default;
};

class VideoCaptureFactory
#if PLATFORM(IOS_FAMILY)
    : public SingleSourceFactory
#endif
{
public:
    virtual ~VideoCaptureFactory() = default;
    virtual CaptureSourceOrError createVideoCaptureSource(const CaptureDevice&, String&&, const MediaConstraints*) = 0;
    virtual CaptureDeviceManager& videoCaptureDeviceManager() = 0;

protected:
    VideoCaptureFactory() = default;
};

class DisplayCaptureFactory
#if PLATFORM(IOS_FAMILY)
    : public SingleSourceFactory
#endif
{
public:
    virtual ~DisplayCaptureFactory() = default;
    virtual CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice&, const MediaConstraints*) = 0;
    virtual CaptureDeviceManager& displayCaptureDeviceManager() = 0;

protected:
    DisplayCaptureFactory() = default;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
