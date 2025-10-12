/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if USE(MEDIATOOLBOX)

#include <pal/spi/cocoa/MediaToolboxSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, MediaToolbox)

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, FigPhotoDecompressionSetHardwareCutoff, void, (int, size_t numPixelsCutoff), (format, numPixelsCutoff))

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, FigVideoTargetCreateWithVideoReceiverEndpointID, OSStatus, (CFAllocatorRef allocator, xpc_object_t videoReceiverXPCEndpointID, CFDictionaryRef creationOptions, CF_RETURNS_RETAINED FigVideoTargetRef* videoTargetOut), (allocator, videoReceiverXPCEndpointID, creationOptions, videoTargetOut))
#define FigVideoTargetCreateWithVideoReceiverEndpointID PAL::softLink_MediaToolbox_FigVideoTargetCreateWithVideoReceiverEndpointID

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MTShouldPlayHDRVideo, Boolean, (CFArrayRef displayList), (displayList))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MTOverrideShouldPlayHDRVideo, void, (Boolean override, Boolean playHDRVideo), (override, playHDRVideo))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MT_GetShouldPlayHDRVideoNotificationSingleton, CFTypeRef, (void), ())
#define MT_GetShouldPlayHDRVideoNotificationSingleton PAL::softLinkMediaToolboxMT_GetShouldPlayHDRVideoNotificationSingleton

SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, kMTSupportNotification_ShouldPlayHDRVideoChanged, CFStringRef)
#define kMTSupportNotification_ShouldPlayHDRVideoChanged PAL::get_MediaToolbox_kMTSupportNotification_ShouldPlayHDRVideoChangedSingleton()

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTAudioProcessingTapGetStorage, void*, (MTAudioProcessingTapRef tap), (tap))
#define MTAudioProcessingTapGetStorage softLink_MediaToolbox_MTAudioProcessingTapGetStorage
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, MediaToolbox, MTAudioProcessingTapGetSourceAudio, OSStatus, (MTAudioProcessingTapRef tap, CMItemCount numberFrames, AudioBufferList *bufferListInOut, MTAudioProcessingTapFlags *flagsOut, CMTimeRange *timeRangeOut, CMItemCount *numberFramesOut), (tap, numberFrames, bufferListInOut, flagsOut, timeRangeOut, numberFramesOut))
#define MTAudioProcessingTapGetSourceAudio softLink_MediaToolbox_MTAudioProcessingTapGetSourceAudio

// FIXME: CoreMedia doesn't specify CF_RETURNS_RETAINED. See rdar://148149858.
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, MediaToolbox, MTAudioProcessingTapCreate, OSStatus, (CFAllocatorRef allocator, const MTAudioProcessingTapCallbacks* callbacks, MTAudioProcessingTapCreationFlags flags, CF_RETURNS_RETAINED MTAudioProcessingTapRef* tapOut), (allocator, callbacks, flags, tapOut))
#define MTAudioProcessingTapCreate softLink_MediaToolbox_MTAudioProcessingTapCreate

#endif // USE(MEDIATOOLBOX)
