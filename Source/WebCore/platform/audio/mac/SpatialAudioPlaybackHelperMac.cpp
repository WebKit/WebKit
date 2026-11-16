/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SpatialAudioPlaybackHelper.h"

#if PLATFORM(MAC)

#include "PlatformMediaConfiguration.h"
#include <pal/spi/cocoa/AudioToolboxSPI.h>
#include <span>

#include <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

bool SpatialAudioPlaybackHelper::supportsSpatialAudioPlaybackForConfiguration(const PlatformMediaConfiguration& configuration)
{
    ASSERT(configuration.audio);
    if (!configuration.audio)
        return false;

    if (!PAL::canLoad_AudioToolbox_AudioGetDeviceSpatialPreferencesForContentType())
        return false;

    SpatialAudioPreferences spatialAudioPreferences { };
    auto contentType = configuration.video ? kAudioSpatialContentType_Audiovisual : kAudioSpatialContentType_AudioOnly;

    if (noErr != PAL::AudioGetDeviceSpatialPreferencesForContentType(nullptr, static_cast<SpatialContentTypeID>(contentType), &spatialAudioPreferences))
        return false;

    if (!spatialAudioPreferences.spatialAudioSourceCount)
        return false;

    auto channelCount = configuration.audio->channels.toDouble();

    for (auto& source : std::span { spatialAudioPreferences.spatialAudioSources }.first(spatialAudioPreferences.spatialAudioSourceCount)) {
        if (source == kSpatialAudioSource_Multichannel && channelCount > 2)
            return true;
        if (source == kSpatialAudioSource_MonoOrStereo && channelCount >= 1)
            return true;
    }

    return false;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
