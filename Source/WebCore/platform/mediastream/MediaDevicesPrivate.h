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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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
 *
 */

#ifndef MediaDevicesPrivate_h
#define MediaDevicesPrivate_h

#if ENABLE(MEDIA_STREAM)

#include "MediaDeviceInfo.h"
#include "MediaStreamTrackSourcesRequestClient.h"

#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaDevicesPrivate : public MediaStreamTrackSourcesRequestClient {
public:
    static Ref<MediaDevicesPrivate> create();
    
    MediaDevicesPrivate();

    ~MediaDevicesPrivate() { }

    Vector<RefPtr<MediaDeviceInfo>> availableMediaDevices(ScriptExecutionContext&);
    
    // FIXME(148041): requestOrigin inside of getMediaStreamTrackSources not used
    const String& requestOrigin() const override { return emptyString(); }
    
    void didCompleteRequest(const Vector<RefPtr<TrackSourceInfo>>&) override;
    
    Vector<RefPtr<TrackSourceInfo>> capturedDevices() const { return m_capturedDevices; }
    
private:
    Vector<RefPtr<TrackSourceInfo>> m_capturedDevices;
};

}

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaDevicesPrivate_h
