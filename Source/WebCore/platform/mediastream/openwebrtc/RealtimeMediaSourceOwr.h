/*
 * Copyright (C) 2015 Igalia S.L
 * Copyright (C) 2015 Metrological
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RealtimeMediaSourceOwr_h
#define RealtimeMediaSourceOwr_h

#if ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)

#include "GRefPtrGStreamer.h"
#include "RealtimeMediaSource.h"
#include <owr/owr_media_source.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RealtimeMediaSourceCapabilities;

class RealtimeMediaSourceOwr final : public RealtimeMediaSource {
public:
RealtimeMediaSourceOwr(OwrMediaSource* mediaSource, const String& id, RealtimeMediaSource::Type type, const String& name)
    : RealtimeMediaSource(id, type, name)
    , m_mediaSource(mediaSource)
    {
    }

    virtual ~RealtimeMediaSourceOwr() { }

    virtual RefPtr<RealtimeMediaSourceCapabilities> capabilities() { return m_capabilities; }
    virtual const RealtimeMediaSourceSettings& settings() { return m_currentSettings; }

    OwrMediaSource* mediaSource() const { return m_mediaSource; }

private:
    RefPtr<RealtimeMediaSourceCapabilities> m_capabilities;
    RealtimeMediaSourceSettings m_currentSettings;
    OwrMediaSource* m_mediaSource;
};

typedef HashMap<String, RefPtr<RealtimeMediaSourceOwr>> RealtimeMediaSourceOwrMap;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)

#endif // RealtimeMediaSourceOwr_h
