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

#ifndef MediaTrackSupportedConstraints_h
#define MediaTrackSupportedConstraints_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSupportedConstraints.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaTrackSupportedConstraints : public RefCounted<MediaTrackSupportedConstraints>, public ScriptWrappable {
public:
    static Ref<MediaTrackSupportedConstraints> create(const RealtimeMediaSourceSupportedConstraints& supportedConstraints)
    {
        return adoptRef(*new MediaTrackSupportedConstraints(supportedConstraints));
    }

    bool supportsWidth() const { return m_supportedConstraints.supportsWidth(); }
    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    bool supportsAspectRatio() const { return m_supportedConstraints.supportsAspectRatio(); }
    bool supportsFrameRate() const { return m_supportedConstraints.supportsFrameRate(); }
    bool supportsFacingMode() const { return m_supportedConstraints.supportsFacingMode(); }
    bool supportsVolume() const { return m_supportedConstraints.supportsVolume(); }
    bool supportsSampleRate() const { return m_supportedConstraints.supportsSampleRate(); }
    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }

private:
    explicit MediaTrackSupportedConstraints(const RealtimeMediaSourceSupportedConstraints& supportedConstraints)
        : m_supportedConstraints(supportedConstraints)
    {
    }

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
};

} // namespace WebCore

#endif // MediaTrackSupportedConstraints_h

#endif
