/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SpatialAudioExperienceHelper.h"

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)

#import <pal/spi/cocoa/AudioToolboxCoreSPI.h>

namespace WebCore {

static CASoundStageSize toCASoundStageSize(MediaPlayerSoundStageSize size)
{
    static_assert(static_cast<CASoundStageSize>(MediaPlayerSoundStageSize::Auto) == CASoundStageSizeAutomatic);
    static_assert(static_cast<CASoundStageSize>(MediaPlayerSoundStageSize::Small) == CASoundStageSizeSmall);
    static_assert(static_cast<CASoundStageSize>(MediaPlayerSoundStageSize::Medium) == CASoundStageSizeMedium);
    static_assert(static_cast<CASoundStageSize>(MediaPlayerSoundStageSize::Large) == CASoundStageSizeLarge);
    return static_cast<CASoundStageSize>(size);
}

RetainPtr<CASpatialAudioExperience> createSpatialAudioExperienceWithOptions(const SpatialAudioExperienceOptions& options)
{
#if HAVE(SPATIAL_TRACKING_LABEL)
    if (options.isVisible && options.hasTarget) {
        // Automatic anchoring does not yet work with remote video targets, so rely on
        // the spatial tracking label, and use that label to create a CAAudioTether. This
        // tether informs the audio rendering subsystem about the connection between the
        // visual layer and the audio being generated.
        RetainPtr uuid = adoptNS([[NSUUID alloc] initWithUUIDString:options.spatialTrackingLabel]);
        RetainPtr tether = adoptNS([[CAAudioTether alloc] initWithType:CAAudioTetherTypeLayer identifier:uuid.get() pid:0]);
        RetainPtr anchoringStrategy = adoptNS([[CAAudioTetherAnchoringStrategy alloc] initWithAudioTether:tether.get()]);
        return adoptNS([[CAHeadTrackedSpatialAudio alloc] initWithSoundStageSize:toCASoundStageSize(options.soundStageSize) anchoringStrategy:anchoringStrategy.get()]);
    }
#endif

    if (options.isVisible && options.hasLayer) {
        RetainPtr anchoring = adoptNS([[CAAutomaticAnchoringStrategy alloc] init]);
        return adoptNS([[CAHeadTrackedSpatialAudio alloc] initWithSoundStageSize:toCASoundStageSize(options.soundStageSize) anchoringStrategy:anchoring.get()]);
    }

    // Either not visible, or with no layer or target:
    if (options.sceneIdentifier.isEmpty())
        return nil;

    RetainPtr anchoring = adoptNS([[CASceneAnchoringStrategy alloc] initWithSceneIdentifier:options.sceneIdentifier]);
    return adoptNS([[CAHeadTrackedSpatialAudio alloc] initWithSoundStageSize:toCASoundStageSize(options.soundStageSize) anchoringStrategy:anchoring.get()]);
}

}

#endif
