/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "SoftLinking.h"

// Soft-link against AVFoundationCF functions and variables required by MediaPlayerPrivateAVFoundationCF.cpp.

#ifdef DEBUG_ALL
// FIXME: <rdar://problem/9898937> AVFoundationCF doesn't currently deliver a debug library.
SOFT_LINK_LIBRARY(AVFoundationCF)
#else
SOFT_LINK_LIBRARY(AVFoundationCF)
#endif

// Functions

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetCancelLoading, void, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetCancelLoading softLink_AVCFAssetCancelLoading

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetCopyAssetTracks, CFArrayRef, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetCopyAssetTracks softLink_AVCFAssetCopyAssetTracks

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetCopyTracksWithMediaCharacteristic, CFArrayRef, __cdecl, (AVCFAssetRef asset, CFStringRef mediaCharacteristic), (asset, mediaCharacteristic))
#define AVCFAssetCopyTracksWithMediaCharacteristic softLink_AVCFAssetCopyTracksWithMediaCharacteristic

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetCopyTracksWithMediaType, CFArrayRef, __cdecl, (AVCFAssetRef asset, CFStringRef mediaType), (asset, mediaType))
#define AVCFAssetCopyTracksWithMediaType softLink_AVCFAssetCopyTracksWithMediaType

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetGetDuration, CMTime, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetGetDuration softLink_AVCFAssetGetDuration

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetGetNaturalSize, CGSize, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetGetNaturalSize softLink_AVCFAssetGetNaturalSize

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetGetPreferredTransform, CGAffineTransform, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetGetPreferredTransform softLink_AVCFAssetGetPreferredTransform

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetGetStatusOfValueForProperty, AVCFPropertyValueStatus, __cdecl, (AVCFAssetRef asset, CFStringRef property, CFErrorRef *errorOut), (asset, property, errorOut))
#define AVCFAssetGetStatusOfValueForProperty softLink_AVCFAssetGetStatusOfValueForProperty

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorCopyCGImageAtTime, CGImageRef, __cdecl, (AVCFAssetImageGeneratorRef generator, CMTime requestedTime, CMTime *actualTimeOut, CFErrorRef *errorOut), (generator, requestedTime, actualTimeOut, errorOut))
#define AVCFAssetImageGeneratorCopyCGImageAtTime softLink_AVCFAssetImageGeneratorCopyCGImageAtTime

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorCreateWithAsset, AVCFAssetImageGeneratorRef, __cdecl, (CFAllocatorRef allocator, AVCFAssetRef asset), (allocator, asset))
#define AVCFAssetImageGeneratorCreateWithAsset softLink_AVCFAssetImageGeneratorCreateWithAsset

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorSetApertureMode, void, __cdecl, (AVCFAssetImageGeneratorRef generator, CFStringRef mode), (generator, mode))
#define AVCFAssetImageGeneratorSetApertureMode softLink_AVCFAssetImageGeneratorSetApertureMode

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorSetAppliesPreferredTrackTransform, void, __cdecl, (AVCFAssetImageGeneratorRef generator, Boolean appliesTransfrom), (generator, appliesTransfrom))
#define AVCFAssetImageGeneratorSetAppliesPreferredTrackTransform softLink_AVCFAssetImageGeneratorSetAppliesPreferredTrackTransform

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorSetMaximumSize, void, __cdecl, (AVCFAssetImageGeneratorRef generator, CGSize maxSize), (generator, maxSize))
#define AVCFAssetImageGeneratorSetMaximumSize softLink_AVCFAssetImageGeneratorSetMaximumSize

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorSetRequestedTimeToleranceAfter, void, __cdecl, (AVCFAssetImageGeneratorRef generator, CMTime toleranceAfter), (generator, toleranceAfter))
#define AVCFAssetImageGeneratorSetRequestedTimeToleranceAfter softLink_AVCFAssetImageGeneratorSetRequestedTimeToleranceAfter

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorSetRequestedTimeToleranceBefore, void, __cdecl, (AVCFAssetImageGeneratorRef generator, CMTime toleranceBefore), (generator, toleranceBefore))
#define AVCFAssetImageGeneratorSetRequestedTimeToleranceBefore softLink_AVCFAssetImageGeneratorSetRequestedTimeToleranceBefore

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetIsPlayable, Boolean, __cdecl, (AVCFAssetRef asset), (asset))
#define AVCFAssetIsPlayable softLink_AVCFAssetIsPlayable

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetLoadValuesAsynchronouslyForProperties, void, __cdecl, (AVCFAssetRef asset, CFArrayRef properties, AVCFAssetLoadValuesCompletionCallback callback, void *clientContext), (asset, properties, callback, clientContext))
#define AVCFAssetLoadValuesAsynchronouslyForProperties softLink_AVCFAssetLoadValuesAsynchronouslyForProperties

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetTrackGetMediaType, CFStringRef, __cdecl, (AVCFAssetTrackRef assetTrack), (assetTrack))
#define AVCFAssetTrackGetMediaType softLink_AVCFAssetTrackGetMediaType

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetTrackGetNaturalSize, CGSize, __cdecl, (AVCFAssetTrackRef assetTrack), (assetTrack))
#define AVCFAssetTrackGetNaturalSize softLink_AVCFAssetTrackGetNaturalSize

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetTrackGetPreferredTransform, CGAffineTransform, __cdecl, (AVCFAssetTrackRef assetTrack), (assetTrack))
#define AVCFAssetTrackGetPreferredTransform softLink_AVCFAssetTrackGetPreferredTransform

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFAssetTrackGetTotalSampleDataLength, int64_t, __cdecl, (AVCFAssetTrackRef assetTrack), (assetTrack))
#define AVCFAssetTrackGetTotalSampleDataLength softLink_AVCFAssetTrackGetTotalSampleDataLength

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerCreatePeriodicTimeObserverForInterval, AVCFPlayerObserverRef, __cdecl, (AVCFPlayerRef player, CMTime interval, dispatch_queue_t queue, AVCFPlayerPeriodicTimeObserverCallback callback, void *clientContext), (player, interval, queue, callback, clientContext))
#define AVCFPlayerCreatePeriodicTimeObserverForInterval softLink_AVCFPlayerCreatePeriodicTimeObserverForInterval

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemCreateWithAsset, AVCFPlayerItemRef, __cdecl, (CFAllocatorRef allocator, AVCFAssetRef asset, dispatch_queue_t notificationQueue), (allocator, asset, notificationQueue))
#define AVCFPlayerItemCreateWithAsset softLink_AVCFPlayerItemCreateWithAsset

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerCreateWithPlayerItemAndOptions, AVCFPlayerRef, __cdecl, (CFAllocatorRef allocator, AVCFPlayerItemRef playerItem, CFDictionaryRef options, dispatch_queue_t notificationQueue), (allocator, playerItem, options, notificationQueue))
#define AVCFPlayerCreateWithPlayerItemAndOptions softLink_AVCFPlayerCreateWithPlayerItemAndOptions

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemCopyLoadedTimeRanges, CFArrayRef, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemCopyLoadedTimeRanges softLink_AVCFPlayerItemCopyLoadedTimeRanges

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemCopySeekableTimeRanges, CFArrayRef, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemCopySeekableTimeRanges softLink_AVCFPlayerItemCopySeekableTimeRanges

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemCopyTracks, CFArrayRef, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemCopyTracks softLink_AVCFPlayerItemCopyTracks

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemGetAsset, AVCFAssetRef, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemGetAsset softLink_AVCFPlayerItemGetAsset

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemGetCurrentTime, CMTime, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemGetCurrentTime softLink_AVCFPlayerItemGetCurrentTime

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemGetStatus, AVCFPlayerItemStatus, __cdecl, (AVCFPlayerItemRef playerItem, CFErrorRef *errorOut), (playerItem, errorOut))
#define AVCFPlayerItemGetStatus softLink_AVCFPlayerItemGetStatus

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackBufferEmpty, Boolean, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemIsPlaybackBufferEmpty softLink_AVCFPlayerItemIsPlaybackBufferEmpty

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackBufferFull, Boolean, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemIsPlaybackBufferFull softLink_AVCFPlayerItemIsPlaybackBufferFull

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackLikelyToKeepUp, Boolean, __cdecl, (AVCFPlayerItemRef playerItem), (playerItem))
#define AVCFPlayerItemIsPlaybackLikelyToKeepUp softLink_AVCFPlayerItemIsPlaybackLikelyToKeepUp

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemSeekToTimeWithToleranceAndCompletionCallback, AVCFAssetRef, __cdecl, (AVCFPlayerItemRef playerItem, CMTime time, CMTime toleranceBefore, CMTime toleranceAfter, AVCFPlayerItemSeekCompletionCallback completionCallback, void *context), (playerItem, time, toleranceBefore, toleranceAfter, completionCallback, context))
#define AVCFPlayerItemSeekToTimeWithToleranceAndCompletionCallback softLink_AVCFPlayerItemSeekToTimeWithToleranceAndCompletionCallback

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemTrackCopyAssetTrack, AVCFAssetTrackRef, __cdecl, (AVCFPlayerItemTrackRef track), (track))
#define AVCFPlayerItemTrackCopyAssetTrack softLink_AVCFPlayerItemTrackCopyAssetTrack

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemTrackIsEnabled, Boolean, __cdecl, (AVCFPlayerItemTrackRef track), (track))
#define AVCFPlayerItemTrackIsEnabled softLink_AVCFPlayerItemTrackIsEnabled

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerGetRate, Float32, __cdecl, (AVCFPlayerRef player), (player))
#define AVCFPlayerGetRate softLink_AVCFPlayerGetRate

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerLayerCopyCACFLayer, CACFLayerRef, __cdecl, (AVCFPlayerLayerRef playerLayer), (playerLayer))
#define AVCFPlayerLayerCopyCACFLayer softLink_AVCFPlayerLayerCopyCACFLayer

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerLayerCreateWithAVCFPlayer, AVCFPlayerLayerRef, __cdecl, (CFAllocatorRef allocator, AVCFPlayerRef player, dispatch_queue_t notificationQueue), (allocator, player, notificationQueue))
#define AVCFPlayerLayerCreateWithAVCFPlayer softLink_AVCFPlayerLayerCreateWithAVCFPlayer

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerLayerIsReadyForDisplay, Boolean, __cdecl, (AVCFPlayerLayerRef playerLayer), (playerLayer))
#define AVCFPlayerLayerIsReadyForDisplay softLink_AVCFPlayerLayerIsReadyForDisplay

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerLayerSetPlayer, void, __cdecl, (AVCFPlayerLayerRef playerLayer, AVCFPlayerRef player), (playerLayer, player))
#define AVCFPlayerLayerSetPlayer softLink_AVCFPlayerLayerSetPlayer

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerRemoveObserver, OSStatus, __cdecl, (AVCFPlayerRef player, AVCFPlayerObserverRef observer), (player, observer))
#define AVCFPlayerRemoveObserver softLink_AVCFPlayerRemoveObserver

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerSetClosedCaptionDisplayEnabled, void, __cdecl, (AVCFPlayerRef player, Boolean enabled), (player, enabled))
#define AVCFPlayerSetClosedCaptionDisplayEnabled softLink_AVCFPlayerSetClosedCaptionDisplayEnabled

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerSetRate, void, __cdecl, (AVCFPlayerRef player, Float32 rate), (player, rate))
#define AVCFPlayerSetRate softLink_AVCFPlayerSetRate

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFPlayerSetVolume, void, __cdecl, (AVCFPlayerRef player, Float32 volume), (player, volume))
#define AVCFPlayerSetVolume softLink_AVCFPlayerSetVolume

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFURLAssetCopyAudiovisualMIMETypes, CFArrayRef, __cdecl, (), ())
#define AVCFURLAssetCopyAudiovisualMIMETypes softLink_AVCFURLAssetCopyAudiovisualMIMETypes

SOFT_LINK_DLL_IMPORT(AVFoundationCF, AVCFURLAssetCreateWithURLAndOptions, AVCFURLAssetRef, __cdecl, (CFAllocatorRef allocator, CFURLRef URL, CFDictionaryRef options, dispatch_queue_t notificationQueue), (allocator, URL, options, notificationQueue))
#define AVCFURLAssetCreateWithURLAndOptions softLink_AVCFURLAssetCreateWithURLAndOptions

SOFT_LINK_DLL_IMPORT_OPTIONAL(AVFoundationCF, AVCFPlayerSetDirect3DDevice, void, __cdecl, (AVCFPlayerRef player, IDirect3DDevice9* d3dDevice))

// Variables

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetImageGeneratorApertureModeCleanAperture, const CFStringRef);
#define AVCFAssetImageGeneratorApertureModeCleanAperture get_AVCFAssetImageGeneratorApertureModeCleanAperture()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyDuration, const CFStringRef);
#define AVCFAssetPropertyDuration get_AVCFAssetPropertyDuration()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyNaturalSize, const CFStringRef);
#define AVCFAssetPropertyNaturalSize get_AVCFAssetPropertyNaturalSize()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyPlayable, const CFStringRef);
#define AVCFAssetPropertyPlayable get_AVCFAssetPropertyPlayable()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyPreferredRate, const CFStringRef);
#define AVCFAssetPropertyPreferredRate get_AVCFAssetPropertyPreferredRate()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyPreferredTransform, const CFStringRef);
#define AVCFAssetPropertyPreferredTransform get_AVCFAssetPropertyPreferredTransform()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFAssetPropertyTracks, const CFStringRef);
#define AVCFAssetPropertyTracks get_AVCFAssetPropertyTracks()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFMediaCharacteristicAudible, const CFStringRef);
#define AVCFMediaCharacteristicAudible get_AVCFMediaCharacteristicAudible()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFMediaCharacteristicVisual, const CFStringRef);
#define AVCFMediaCharacteristicVisual get_AVCFMediaCharacteristicVisual()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFMediaTypeAudio, const CFStringRef);
#define AVCFMediaTypeAudio get_AVCFMediaTypeAudio()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFMediaTypeClosedCaption, const CFStringRef);
#define AVCFMediaTypeClosedCaption get_AVCFMediaTypeClosedCaption()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFMediaTypeVideo, const CFStringRef);
#define AVCFMediaTypeVideo get_AVCFMediaTypeVideo()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemDidPlayToEndTimeNotification, const CFStringRef);
#define AVCFPlayerItemDidPlayToEndTimeNotification get_AVCFPlayerItemDidPlayToEndTimeNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification, const CFStringRef);
#define AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification get_AVCFPlayerItemIsPlaybackBufferEmptyChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackBufferFullChangedNotification, const CFStringRef);
#define AVCFPlayerItemIsPlaybackBufferFullChangedNotification get_AVCFPlayerItemIsPlaybackBufferFullChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification, const CFStringRef);
#define AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification get_AVCFPlayerItemIsPlaybackLikelyToKeepUpChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemLoadedTimeRangesChangedNotification, const CFStringRef);
#define AVCFPlayerItemLoadedTimeRangesChangedNotification get_AVCFPlayerItemLoadedTimeRangesChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemPresentationSizeChangedNotification, const CFStringRef);
#define AVCFPlayerItemPresentationSizeChangedNotification get_AVCFPlayerItemPresentationSizeChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemSeekableTimeRangesChangedNotification, const CFStringRef);
#define AVCFPlayerItemSeekableTimeRangesChangedNotification get_AVCFPlayerItemSeekableTimeRangesChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemStatusChangedNotification, const CFStringRef);
#define AVCFPlayerItemStatusChangedNotification get_AVCFPlayerItemStatusChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerItemTracksChangedNotification, const CFStringRef);
#define AVCFPlayerItemTracksChangedNotification get_AVCFPlayerItemTracksChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT(AVFoundationCF, AVCFPlayerRateChangedNotification, const CFStringRef);
#define AVCFPlayerRateChangedNotification get_AVCFPlayerRateChangedNotification()

SOFT_LINK_VARIABLE_DLL_IMPORT_OPTIONAL(AVFoundationCF, AVCFPlayerEnableHardwareAcceleratedVideoDecoderKey, const CFStringRef);
#define AVCFPlayerEnableHardwareAcceleratedVideoDecoderKey get_AVCFPlayerEnableHardwareAcceleratedVideoDecoderKey()
