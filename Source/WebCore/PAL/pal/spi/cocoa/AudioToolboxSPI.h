/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK)

#import <AudioToolbox/AudioComponentPriv.h>
#import <AudioToolbox/AudioFormatPriv.h>
#import <AudioToolbox/Spatialization.h>

#else

static constexpr OSType kAudioFormatProperty_AvailableDecodeChannelLayoutTags = 'adcl';
static constexpr OSType kAudioFormatProperty_VorbisModeInfo = 'vnfo';

struct AudioFormatVorbisModeInfo {
    UInt32 mShortBlockSize;
    UInt32 mLongBlockSize;
    UInt32 mModeCount;
    UInt64 mModeFlags;
};
typedef struct AudioFormatVorbisModeInfo AudioFormatVorbisModeInfo;

enum SpatialAudioSourceID : UInt32;
static constexpr UInt32 kSpatialAudioSource_Multichannel = 'mlti';
static constexpr UInt32 kSpatialAudioSource_MonoOrStereo = 'most';
static constexpr UInt32 kSpatialAudioSource_BinauralForHeadphones = 'binh';
static constexpr UInt32 kSpatialAudioSource_Unknown = '?src';

enum SpatialContentTypeID : UInt32;
static constexpr UInt32 kAudioSpatialContentType_Audiovisual = 'moov';
static constexpr UInt32 kAudioSpatialContentType_AudioOnly = 'soun';

struct SpatialAudioPreferences {
    Boolean prefersHeadTrackedSpatialization;
    Boolean prefersLossyAudioSources;
    Boolean alwaysSpatialize;
    Boolean pad[5];
    UInt32 maxSpatializableChannels;
    UInt32 spatialAudioSourceCount;
    SpatialAudioSourceID spatialAudioSources[3];
};

#endif
