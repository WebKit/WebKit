/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef CaptureDeviceManager_h
#define CaptureDeviceManager_h

#if ENABLE(MEDIA_STREAM)

#include "CaptureDeviceInfo.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include "RealtimeMediaSource.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDeviceManager {
public:
    virtual Vector<CaptureDeviceInfo>& captureDeviceList() = 0;
    virtual void refreshCaptureDeviceList() { }
    virtual TrackSourceInfoVector getSourcesInfo(const String&);
    virtual Vector<RefPtr<RealtimeMediaSource>> bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type, MediaConstraints&);
    virtual RefPtr<RealtimeMediaSource> sourceWithUID(const String&, RealtimeMediaSource::Type, MediaConstraints*);

    virtual bool verifyConstraintsForMediaType(RealtimeMediaSource::Type, const MediaConstraints&, const CaptureSessionInfo*, String&);

protected:
    virtual ~CaptureDeviceManager();
    virtual RealtimeMediaSource* createMediaSourceForCaptureDeviceWithConstraints(const CaptureDeviceInfo&, MediaConstraints*) = 0;

    virtual CaptureSessionInfo defaultCaptureSession() const { return CaptureSessionInfo(); }
    virtual bool sessionSupportsConstraint(const CaptureSessionInfo*, RealtimeMediaSource::Type, const MediaConstraint&);
    virtual bool isSupportedFrameRate(const MediaConstraint&) const;

    bool captureDeviceFromDeviceID(const String& captureDeviceID, CaptureDeviceInfo& source);
    CaptureDeviceInfo* bestDeviceForFacingMode(RealtimeMediaSourceSettings::VideoFacingMode);
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif /* CaptureDeviceManager_h */
