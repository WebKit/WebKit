/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockRealtimeMediaSource_h
#define MockRealtimeMediaSource_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSource.h"

namespace WebCore {

class FloatRect;
class GraphicsContext;
class TrackSourceInfo;

class MockRealtimeMediaSource : public RealtimeMediaSource {
public:
    virtual ~MockRealtimeMediaSource() { }

    static const AtomicString& mockAudioSourcePersistentID();
    static const AtomicString& mockAudioSourceName();

    static const AtomicString& mockVideoSourcePersistentID();
    static const AtomicString& mockVideoSourceName();

    static RefPtr<TrackSourceInfo> trackSourceWithUID(const String&, MediaConstraints*);

protected:
    MockRealtimeMediaSource(const String& id, Type, const String& name);

    virtual void updateSettings(RealtimeMediaSourceSettings&) = 0;
    virtual void initializeCapabilities(RealtimeMediaSourceCapabilities&) = 0;
    virtual void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) = 0;

    void startProducingData() override { m_isProducingData = true; }
    void stopProducingData() override { m_isProducingData = false; }

    RefPtr<RealtimeMediaSourceCapabilities> capabilities() override;
    const RealtimeMediaSourceSettings& settings() override;

    MediaConstraints& constraints() { return *m_constraints.get(); }
    RealtimeMediaSourceSupportedConstraints& supportedConstraints();

private:

    bool isProducingData() const override { return m_isProducingData; }

    RealtimeMediaSourceSettings m_currentSettings;
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    RefPtr<RealtimeMediaSourceCapabilities> m_capabilities;
    RefPtr<MediaConstraints> m_constraints;
    bool m_isProducingData { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MockRealtimeMediaSource_h
