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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "config.h"
#include "MockRealtimeAudioSource.h"

#if ENABLE(MEDIA_STREAM)
#include "Logging.h"
#include "MediaConstraints.h"
#include "NotImplemented.h"
#include "RealtimeMediaSourceSettings.h"
#include "UUID.h"

namespace WebCore {

Ref<MockRealtimeAudioSource> MockRealtimeAudioSource::create()
{
    return adoptRef(*new MockRealtimeAudioSource(MockRealtimeMediaSource::mockAudioSourceName()));
}

Ref<MockRealtimeAudioSource> MockRealtimeAudioSource::createMuted(const String& name)
{
    auto source = adoptRef(*new MockRealtimeAudioSource(name));
    source->m_muted = true;
    return source;
}

MockRealtimeAudioSource::MockRealtimeAudioSource(const String& name)
    : MockRealtimeMediaSource(createCanonicalUUIDString(), RealtimeMediaSource::Audio, name)
{
}

void MockRealtimeAudioSource::updateSettings(RealtimeMediaSourceSettings& settings)
{
    settings.setVolume(50);
}

void MockRealtimeAudioSource::initializeCapabilities(RealtimeMediaSourceCapabilities& capabilities)
{
    capabilities.setVolume(CapabilityValueOrRange(0, 1.0));
    capabilities.setEchoCancellation(RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
}

void MockRealtimeAudioSource::initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints& supportedConstraints)
{
    supportedConstraints.setSupportsVolume(true);
    supportedConstraints.setSupportsEchoCancellation(true);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
