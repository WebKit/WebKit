/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef MediaSourceSettings_h
#define MediaSourceSettings_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceCapabilities.h"
#include "ScriptWrappable.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaSourceSettings : public RefCounted<MediaSourceSettings>, public ScriptWrappable {
public:
    static Ref<MediaSourceSettings> create(const RealtimeMediaSourceSettings&);

    bool supportsWidth() const { return m_sourceSettings.supportsWidth(); }
    unsigned long width() const { return m_sourceSettings.width(); }

    bool supportsHeight() const { return m_sourceSettings.supportsHeight(); }
    unsigned long height() const { return m_sourceSettings.height(); }

    bool supportsAspectRatio() const { return m_sourceSettings.supportsAspectRatio(); }
    float aspectRatio() const { return m_sourceSettings.aspectRatio(); }

    bool supportsFrameRate() const { return m_sourceSettings.supportsFrameRate(); }
    float frameRate() const { return m_sourceSettings.frameRate(); }

    bool supportsFacingMode() const { return m_sourceSettings.supportsFacingMode(); }
    const AtomicString& facingMode() const;

    bool supportsVolume() const { return m_sourceSettings.supportsVolume(); }
    double volume() const { return m_sourceSettings.volume(); }

    bool supportsSampleRate() const { return m_sourceSettings.supportsSampleRate(); }
    unsigned long sampleRate() const { return m_sourceSettings.sampleRate(); }

    bool supportsSampleSize() const { return m_sourceSettings.supportsSampleSize(); }
    unsigned long sampleSize() const { return m_sourceSettings.sampleSize(); }

    bool supportsEchoCancellation() const { return m_sourceSettings.supportsEchoCancellation(); }
    bool echoCancellation() const { return m_sourceSettings.echoCancellation(); }

    bool supportsDeviceId() const { return m_sourceSettings.supportsDeviceId(); }
    const AtomicString& deviceId() const { return m_sourceSettings.deviceId(); }

    bool supportsGroupId() const { return m_sourceSettings.supportsGroupId(); }
    const AtomicString& groupId() const { return m_sourceSettings.groupId(); }

private:
    explicit MediaSourceSettings(const RealtimeMediaSourceSettings&);

    RealtimeMediaSourceSettings m_sourceSettings;
};

} // namespace WebCore

#endif // MediaSourceSettings_h

#endif
