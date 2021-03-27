/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(AVFOUNDATION)

#include <wtf/SoftLinking.h>

typedef UInt32 AudioConverterPropertyID;
typedef UInt32 AudioFormatPropertyID;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef struct OpaqueAudioConverter* AudioConverterRef;

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AudioToolbox)
SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, AudioToolboxCore)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterGetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32* ioPropertyDataSize, void* outPropertyData), (inAudioConverter, inPropertyID, ioPropertyDataSize, outPropertyData))
#define AudioConverterGetProperty softLink_AudioToolbox_AudioConverterGetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterNew, OSStatus, (const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter), (inSourceFormat, inDestinationFormat, outAudioConverter))
#define AudioConverterNew softLink_AudioToolbox_AudioConverterNew
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioConverterSetProperty, OSStatus, (AudioConverterRef inAudioConverter, AudioConverterPropertyID inPropertyID, UInt32 inPropertyDataSize, const void* inPropertyData), (inAudioConverter, inPropertyID, inPropertyDataSize, inPropertyData))
#define AudioConverterSetProperty softLink_AudioToolbox_AudioConverterSetProperty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, AudioToolbox, AudioFormatGetProperty, OSStatus, (AudioFormatPropertyID inPropertyID, UInt32 inSpecifierSize, const void* inSpecifier, UInt32* ioPropertyDataSize, void* outPropertyData), (inPropertyID, inSpecifierSize, inSpecifier, ioPropertyDataSize, outPropertyData))
#define AudioFormatGetProperty softLink_AudioToolbox_AudioFormatGetProperty
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, AudioToolboxCore, AudioComponentFetchServerRegistrations, OSStatus, (CFDataRef* outBundleRegistrations), (outBundleRegistrations))
#define AudioComponentFetchServerRegistrations softLinkAudioToolboxCoreAudioComponentFetchServerRegistrations
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, AudioToolboxCore, AudioComponentApplyServerRegistrations, OSStatus, (CFDataRef inBundleRegistrations), (inBundleRegistrations))
#define AudioComponentApplyServerRegistrations softLinkAudioToolboxCoreAudioComponentApplyServerRegistrations

#endif // USE(AVFOUNDATION)
