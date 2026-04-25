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

#include "CoreAudioCaptureUnit.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class CoreAudioCaptureInternalUnit final :  public CoreAudioCaptureUnit::InternalUnit {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(CoreAudioCaptureInternalUnit);
public:
    static Expected<UniqueRef<InternalUnit>, OSStatus> create(bool shouldUseVPIO);
    CoreAudioCaptureInternalUnit(CoreAudioCaptureUnit::StoredAudioUnit&&, bool shouldUseVPIO);
    ~CoreAudioCaptureInternalUnit() final;

private:
    OSStatus initialize() final;
    OSStatus uninitialize() final;
    OSStatus start() final;
    OSStatus stop() final;
    OSStatus set(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, const void*, UInt32) final;
    OSStatus get(AudioUnitPropertyID, AudioUnitScope, AudioUnitElement, void*, UInt32*) final;
    OSStatus render(AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*) final;
    OSStatus defaultInputDevice(uint32_t*) final;
    OSStatus defaultOutputDevice(uint32_t*) final;
    bool setVoiceActivityDetection(bool) final;
    bool canRenderAudio() const final { return m_shouldUseVPIO; }

    CoreAudioCaptureUnit::StoredAudioUnit m_audioUnit;
    bool m_shouldUseVPIO { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
