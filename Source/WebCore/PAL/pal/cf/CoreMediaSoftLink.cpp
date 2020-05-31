/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "config.h"

#if USE(AVFOUNDATION)
// FIXME: Should be USE(COREMEDIA), but this isn't currently defined on Windows.

#include <pal/spi/cf/CoreMediaSPI.h>
#include <wtf/SoftLinking.h>

#if PLATFORM(COCOA)
#include <CoreVideo/CoreVideo.h>
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400)
#define CMSAMPLEBUFFERCALL_NOESCAPE CF_NOESCAPE
#else
#define CMSAMPLEBUFFERCALL_NOESCAPE
#endif


SOFT_LINK_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBlockBufferCopyDataBytes, OSStatus, (CMBlockBufferRef theSourceBuffer, size_t offsetToData, size_t dataLength, void* destination), (theSourceBuffer, offsetToData, dataLength, destination), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBlockBufferGetDataLength, size_t, (CMBlockBufferRef theBuffer), (theBuffer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMFormatDescriptionGetExtensions, CFDictionaryRef, (CMFormatDescriptionRef desc), (desc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetTypeID, CFTypeID, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetDataBuffer, CMBlockBufferRef, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetSampleTimingInfo, OSStatus, (CMSampleBufferRef sbuf, CMItemIndex sampleIndex, CMSampleTimingInfo* timingInfoOut), (sbuf, sampleIndex, timingInfoOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferDataIsReady, Boolean, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeAdd, CMTime, (CMTime time1, CMTime time2), (time1, time2), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeSubtract, CMTime, (CMTime minuend, CMTime subtrahend), (minuend, subtrahend), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeRangeMake, CMTimeRange, (CMTime start, CMTime duration), (start, duration), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeRangeEqual, Boolean, (CMTimeRange range1, CMTimeRange range2), (range1, range2), PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAlignmentType_End, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAlignmentType_Middle, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAlignmentType_Start, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_Alignment, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_BackgroundColorARGB, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_BoldStyle, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_CharacterBackgroundColorARGB, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_FontFamilyName, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_ForegroundColorARGB, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_ItalicStyle, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_RelativeFontSize, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_UnderlineStyle, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_VerticalLayout, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextMarkupAttribute_WritingDirectionSizePercentage, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextVerticalLayout_LeftToRight, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTextVerticalLayout_RightToLeft, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimeInvalid, CMTime, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimeZero, CMTime, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimePositiveInfinity, CMTime, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, CoreMedia, kCMTimeRangeInvalid, CMTimeRange);

#if PLATFORM(COCOA)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMFormatDescriptionGetMediaSubType, FourCharCode, (CMFormatDescriptionRef desc), (desc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMFormatDescriptionGetMediaType, CMMediaType, (CMFormatDescriptionRef desc), (desc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCreate, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, numSampleTimingEntries, sampleTimingArray, numSampleSizeEntries, sampleSizeArray, sBufOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCreateCopy, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CMSampleBufferRef *sbufCopyOut), (allocator, sbuf, sbufCopyOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCreateCopyWithNewTiming, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef originalSBuf, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMSampleBufferRef *sBufCopyOut), (allocator, originalSBuf, numSampleTimingEntries, sampleTimingArray, sBufCopyOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCreateReadyWithImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo* sampleTiming, CMSampleBufferRef* sBufOut), (allocator, imageBuffer, formatDescription, sampleTiming, sBufOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCreateForImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void* makeDataReadyRefcon, CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo* sampleTiming, CMSampleBufferRef* sampleBufferOut), (allocator, imageBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, sampleTiming, sampleBufferOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetDecodeTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetImageBuffer, CVImageBufferRef, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetOutputDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetOutputPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetSampleAttachmentsArray, CFArrayRef, (CMSampleBufferRef sbuf, Boolean createIfNecessary), (sbuf, createIfNecessary), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetSampleTimingInfoArray, OSStatus, (CMSampleBufferRef sbuf, CMItemCount timingArrayEntries, CMSampleTimingInfo *timingArrayOut, CMItemCount *timingArrayEntriesNeededOut), (sbuf, timingArrayEntries, timingArrayOut, timingArrayEntriesNeededOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeConvertScale, CMTime, (CMTime time, int32_t newTimescale, CMTimeRoundingMethod method), (time, newTimescale, method), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetTotalSampleSize, size_t, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferSetDataBuffer, OSStatus, (CMSampleBufferRef sbuf, CMBlockBufferRef buffer), (sbuf, buffer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMGetAttachment, CFTypeRef, (CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode* attachmentModeOut), (target, key, attachmentModeOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSetAttachment, void, (CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode), (target, key, value, attachmentMode), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMRemoveAttachment, void, (CMAttachmentBearerRef target, CFStringRef key), (target, key), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseCreateWithMasterClock, OSStatus, (CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut), (allocator, masterClock, timebaseOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseGetRate, Float64, (CMTimebaseRef timebase), (timebase), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseSetRate, OSStatus, (CMTimebaseRef timebase, Float64 rate), (timebase, rate), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseSetTime, OSStatus, (CMTimebaseRef timebase, CMTime time), (timebase, time), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseGetEffectiveRate, Float64, (CMTimebaseRef timebase), (timebase), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseAddTimerDispatchSource, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseRemoveTimerDispatchSource, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseSetTimerDispatchSourceNextFireTime, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource, CMTime fireTime, uint32_t flags), (timebase, timerSource, fireTime, flags), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimebaseSetTimerDispatchSourceToFireImmediately, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMVideoFormatDescriptionCreateForImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, CMVideoFormatDescriptionRef* outDesc), (allocator, imageBuffer, outDesc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMVideoFormatDescriptionGetDimensions, CMVideoDimensions, (CMVideoFormatDescriptionRef videoDesc), (videoDesc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMVideoFormatDescriptionGetPresentationDimensions, CGSize, (CMVideoFormatDescriptionRef videoDesc, Boolean usePixelAspectRatio, Boolean useCleanAperture), (videoDesc, usePixelAspectRatio, useCleanAperture), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueReset, OSStatus, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueCreate, OSStatus, (CFAllocatorRef allocator, CMItemCount capacity, const CMBufferCallbacks* callbacks, CMBufferQueueRef* queueOut), (allocator, capacity, callbacks, queueOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueGetHead, CMBufferRef, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueDequeueAndRetain, CMBufferRef, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueEnqueue, OSStatus, (CMBufferQueueRef queue, CMBufferRef buffer), (queue, buffer), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueIsEmpty, Boolean, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueGetBufferCount, CMItemCount, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueGetFirstPresentationTimeStamp, CMTime, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueGetEndPresentationTimeStamp, CMTime, (CMBufferQueueRef queue), (queue), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBufferQueueInstallTriggerWithIntegerThreshold, OSStatus, (CMBufferQueueRef queue, CMBufferQueueTriggerCallback triggerCallback, void* triggerRefcon, CMBufferQueueTriggerCondition triggerCondition, CMItemCount triggerThreshold, CMBufferQueueTriggerToken* triggerTokenOut), (queue, triggerCallback, triggerRefcon, triggerCondition, triggerThreshold, triggerTokenOut), PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleAttachmentKey_DoNotDisplay, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleAttachmentKey_NotSync, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_DrainAfterDecoding, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_EmptyMedia, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_SampleReferenceByteOffset, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferAttachmentKey_SampleReferenceURL, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleAttachmentKey_DisplayImmediately, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleAttachmentKey_IsDependedOnByOthers, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMSampleBufferConsumerNotification_BufferConsumed, CFStringRef, PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimebaseNotification_EffectiveRateChanged, CFStringRef, PAL_EXPORT)
SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimebaseNotification_TimeJumped, CFStringRef, PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMAudioFormatDescriptionGetStreamBasicDescription, const AudioStreamBasicDescription *, (CMAudioFormatDescriptionRef desc), (desc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMBlockBufferCreateWithMemoryBlock, OSStatus, (CFAllocatorRef structureAllocator, void* memoryBlock, size_t blockLength, CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource* customBlockSource, size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags, CMBlockBufferRef* blockBufferOut), (structureAllocator, memoryBlock, blockLength, blockAllocator, customBlockSource, offsetToData, dataLength, flags, blockBufferOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer, OSStatus, (CMSampleBufferRef sbuf, size_t *bufferListSizeNeededOut, AudioBufferList *bufferListOut, size_t bufferListSize, CFAllocatorRef bbufStructAllocator, CFAllocatorRef bbufMemoryAllocator, uint32_t flags, CMBlockBufferRef *blockBufferOut), (sbuf, bufferListSizeNeededOut, bufferListOut, bufferListSize, bbufStructAllocator, bbufMemoryAllocator, flags, blockBufferOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetNumSamples, CMItemCount, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCopySampleBufferForRange, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CFRange sampleRange, CMSampleBufferRef* sBufOut), (allocator, sbuf, sampleRange, sBufOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferGetSampleSizeArray, OSStatus, (CMSampleBufferRef sbuf, CMItemCount sizeArrayEntries, size_t* sizeArrayOut, CMItemCount* sizeArrayEntriesNeededOut), (sbuf, sizeArrayEntries, sizeArrayOut, sizeArrayEntriesNeededOut), PAL_EXPORT)

SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMAudioSampleBufferCreateWithPacketDescriptions, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMTime sbufPTS, const AudioStreamPacketDescription *packetDescriptions, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, sbufPTS, packetDescriptions, sBufOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferSetDataBufferFromAudioBufferList, OSStatus, (CMSampleBufferRef sbuf, CFAllocatorRef bbufStructAllocator, CFAllocatorRef bbufMemoryAllocator, uint32_t flags, const AudioBufferList *bufferList), (sbuf, bbufStructAllocator, bbufMemoryAllocator, flags, bufferList), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferSetDataReady, OSStatus, (CMSampleBufferRef sbuf), (sbuf), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMAudioFormatDescriptionCreate, OSStatus, (CFAllocatorRef allocator, const AudioStreamBasicDescription* asbd, size_t layoutSize, const AudioChannelLayout* layout, size_t magicCookieSize, const void* magicCookie, CFDictionaryRef extensions, CMAudioFormatDescriptionRef* outDesc), (allocator, asbd, layoutSize, layout, magicCookieSize, magicCookie, extensions, outDesc), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMClockGetHostTimeClock, CMClockRef, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMClockGetTime, CMTime, (CMClockRef clock), (clock), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCallForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (* CMSAMPLEBUFFERCALL_NOESCAPE callback)( CMSampleBufferRef sampleBuffer, CMItemCount index, void *refcon), void *refcon), (sbuf, callback, refcon), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMSampleBufferCallBlockForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (^ CMSAMPLEBUFFERCALL_NOESCAPE handler)(CMSampleBufferRef, CMItemCount)), (sbuf, handler), PAL_EXPORT)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMFormatDescriptionExtension_ProtectedContentOriginalFormat, CFStringRef, PAL_EXPORT)

#endif // PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMAudioClockCreate, OSStatus, (CFAllocatorRef allocator, CMClockRef *clockOut), (allocator, clockOut), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeMaximum, CMTime, (CMTime time1, CMTime time2), (time1, time2), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeMinimum, CMTime, (CMTime time1, CMTime time2), (time1, time2), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeRangeContainsTime, Boolean, (CMTimeRange range, CMTime time), (range, time), PAL_EXPORT)

SOFT_LINK_CONSTANT_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, kCMTimeIndefinite, CMTime, PAL_EXPORT)
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMAudioDeviceClockCreate, OSStatus, (CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut), (allocator, deviceUID, clockOut), PAL_EXPORT)
#endif // PLATFORM(MAC)

#if PLATFORM(WIN)
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, CoreMedia, CMTimeMakeFromDictionary, CMTime, (CFDictionaryRef dict), (dict), PAL_EXPORT)
#endif // PLATFORM(WIN)

#endif // USE(AVFOUNDATION)
