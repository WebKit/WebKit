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

#pragma once

#if USE(AVFOUNDATION)
// FIXME: Should be USE(COREMEDIA), but this isn't currently defined on Windows.

#include <pal/spi/cf/CoreMediaSPI.h>
#include <wtf/SoftLinking.h>

#if PLATFORM(WATCHOS)
#define SOFTLINK_AVKIT_FRAMEWORK() SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(AVKit)
#else
#define SOFTLINK_AVKIT_FRAMEWORK() SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400)
#define CMSAMPLEBUFFERCALL_NOESCAPE CF_NOESCAPE
#else
#define CMSAMPLEBUFFERCALL_NOESCAPE
#endif

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, CoreMedia)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBlockBufferCopyDataBytes, OSStatus, (CMBlockBufferRef theSourceBuffer, size_t offsetToData, size_t dataLength, void* destination), (theSourceBuffer, offsetToData, dataLength, destination))
#define CMBlockBufferCopyDataBytes softLink_CoreMedia_CMBlockBufferCopyDataBytes
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBlockBufferGetDataLength, size_t, (CMBlockBufferRef theBuffer), (theBuffer))
#define CMBlockBufferGetDataLength softLink_CoreMedia_CMBlockBufferGetDataLength
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMFormatDescriptionGetExtensions, CFDictionaryRef, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetExtensions softLink_CoreMedia_CMFormatDescriptionGetExtensions
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetTypeID, CFTypeID, (void), ())
#define CMSampleBufferGetTypeID softLink_CoreMedia_CMSampleBufferGetTypeID
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetDataBuffer, CMBlockBufferRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDataBuffer softLink_CoreMedia_CMSampleBufferGetDataBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetFormatDescription softLink_CoreMedia_CMSampleBufferGetFormatDescription
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetSampleTimingInfo, OSStatus, (CMSampleBufferRef sbuf, CMItemIndex sampleIndex, CMSampleTimingInfo* timingInfoOut), (sbuf, sampleIndex, timingInfoOut))
#define CMSampleBufferGetSampleTimingInfo softLink_CoreMedia_CMSampleBufferGetSampleTimingInfo
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferDataIsReady, Boolean, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferDataIsReady softLink_CoreMedia_CMSampleBufferDataIsReady
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeConvertScale, CMTime, (CMTime time, int32_t newTimescale, CMTimeRoundingMethod method), (time, newTimescale, method))
#define CMTimeConvertScale softLink_CoreMedia_CMTimeConvertScale
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeAdd, CMTime, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeAdd softLink_CoreMedia_CMTimeAdd
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeCompare softLink_CoreMedia_CMTimeCompare
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
#define CMTimeGetSeconds softLink_CoreMedia_CMTimeGetSeconds
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
#define CMTimeMake softLink_CoreMedia_CMTimeMake
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
#define CMTimeMakeWithSeconds softLink_CoreMedia_CMTimeMakeWithSeconds
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeSubtract, CMTime, (CMTime minuend, CMTime subtrahend), (minuend, subtrahend))
#define CMTimeSubtract softLink_CoreMedia_CMTimeSubtract
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))
#define CMTimeRangeGetEnd softLink_CoreMedia_CMTimeRangeGetEnd
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeRangeMake, CMTimeRange, (CMTime start, CMTime duration), (start, duration))
#define CMTimeRangeMake softLink_CoreMedia_CMTimeRangeMake
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeRangeEqual, Boolean, (CMTimeRange range1, CMTimeRange range2), (range1, range2))
#define CMTimeRangeEqual softLink_CoreMedia_CMTimeRangeEqual

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, CFStringRef)
#define kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_HEADER(PAL, CoreMedia, kCMFormatDescriptionExtension_ProtectedContentOriginalFormat, CFStringRef)
#define kCMFormatDescriptionExtension_ProtectedContentOriginalFormat get_CoreMedia_kCMFormatDescriptionExtension_ProtectedContentOriginalFormat()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAlignmentType_End, CFStringRef)
#define kCMTextMarkupAlignmentType_End get_CoreMedia_kCMTextMarkupAlignmentType_End()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAlignmentType_Middle, CFStringRef)
#define kCMTextMarkupAlignmentType_Middle get_CoreMedia_kCMTextMarkupAlignmentType_Middle()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAlignmentType_Start, CFStringRef)
#define kCMTextMarkupAlignmentType_Start get_CoreMedia_kCMTextMarkupAlignmentType_Start()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_Alignment, CFStringRef)
#define kCMTextMarkupAttribute_Alignment get_CoreMedia_kCMTextMarkupAttribute_Alignment()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_BackgroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_BackgroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_BackgroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, CFStringRef)
#define kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight get_CoreMedia_kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_BoldStyle, CFStringRef)
#define kCMTextMarkupAttribute_BoldStyle get_CoreMedia_kCMTextMarkupAttribute_BoldStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_CharacterBackgroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_CharacterBackgroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_CharacterBackgroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_FontFamilyName, CFStringRef)
#define kCMTextMarkupAttribute_FontFamilyName get_CoreMedia_kCMTextMarkupAttribute_FontFamilyName()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_ForegroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_ForegroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_ForegroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_ItalicStyle, CFStringRef)
#define kCMTextMarkupAttribute_ItalicStyle get_CoreMedia_kCMTextMarkupAttribute_ItalicStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, CFStringRef)
#define kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection get_CoreMedia_kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_RelativeFontSize, CFStringRef)
#define kCMTextMarkupAttribute_RelativeFontSize get_CoreMedia_kCMTextMarkupAttribute_RelativeFontSize()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, CFStringRef)
#define kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection get_CoreMedia_kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_UnderlineStyle, CFStringRef)
#define kCMTextMarkupAttribute_UnderlineStyle get_CoreMedia_kCMTextMarkupAttribute_UnderlineStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_VerticalLayout, CFStringRef)
#define kCMTextMarkupAttribute_VerticalLayout get_CoreMedia_kCMTextMarkupAttribute_VerticalLayout()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextMarkupAttribute_WritingDirectionSizePercentage, CFStringRef)
#define kCMTextMarkupAttribute_WritingDirectionSizePercentage get_CoreMedia_kCMTextMarkupAttribute_WritingDirectionSizePercentage()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextVerticalLayout_LeftToRight, CFStringRef)
#define kCMTextVerticalLayout_LeftToRight get_CoreMedia_kCMTextVerticalLayout_LeftToRight()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTextVerticalLayout_RightToLeft, CFStringRef)
#define kCMTextVerticalLayout_RightToLeft get_CoreMedia_kCMTextVerticalLayout_RightToLeft()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimeInvalid, CMTime)
#define kCMTimeInvalid get_CoreMedia_kCMTimeInvalid()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimeZero, CMTime)
#define kCMTimeZero PAL::get_CoreMedia_kCMTimeZero()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimePositiveInfinity, CMTime)
#define kCMTimePositiveInfinity get_CoreMedia_kCMTimePositiveInfinity()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimeRangeInvalid, CMTimeRange);
#define kCMTimeRangeInvalid get_CoreMedia_kCMTimeRangeInvalid()

#if PLATFORM(COCOA)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMFormatDescriptionGetMediaSubType, FourCharCode, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetMediaSubType softLink_CoreMedia_CMFormatDescriptionGetMediaSubType
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMFormatDescriptionGetMediaType, CMMediaType, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetMediaType softLink_CoreMedia_CMFormatDescriptionGetMediaType
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), ())
#define CMNotificationCenterGetDefaultLocalCenter softLink_CoreMedia_CMNotificationCenterGetDefaultLocalCenter
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags))
#define CMNotificationCenterAddListener softLink_CoreMedia_CMNotificationCenterAddListener
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object))
#define CMNotificationCenterRemoveListener softLink_CoreMedia_CMNotificationCenterRemoveListener
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCreate, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, numSampleTimingEntries, sampleTimingArray, numSampleSizeEntries, sampleSizeArray, sBufOut))
#define CMSampleBufferCreate softLink_CoreMedia_CMSampleBufferCreate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCreateCopy, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CMSampleBufferRef *sbufCopyOut), (allocator, sbuf, sbufCopyOut))
#define CMSampleBufferCreateCopy softLink_CoreMedia_CMSampleBufferCreateCopy
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCreateCopyWithNewTiming, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef originalSBuf, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMSampleBufferRef *sBufCopyOut), (allocator, originalSBuf, numSampleTimingEntries, sampleTimingArray, sBufCopyOut))
#define CMSampleBufferCreateCopyWithNewTiming softLink_CoreMedia_CMSampleBufferCreateCopyWithNewTiming
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCreateReadyWithImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming, CMSampleBufferRef *sBufOut), (allocator, imageBuffer, formatDescription, sampleTiming, sBufOut))
#define CMSampleBufferCreateReadyWithImageBuffer softLink_CoreMedia_CMSampleBufferCreateReadyWithImageBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCreateForImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void* makeDataReadyRefcon, CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo* sampleTiming, CMSampleBufferRef* sampleBufferOut), (allocator, imageBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, sampleTiming, sampleBufferOut))
#define CMSampleBufferCreateForImageBuffer softLink_CoreMedia_CMSampleBufferCreateForImageBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetDecodeTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDecodeTimeStamp softLink_CoreMedia_CMSampleBufferGetDecodeTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDuration softLink_CoreMedia_CMSampleBufferGetDuration
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetImageBuffer, CVImageBufferRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetImageBuffer softLink_CoreMedia_CMSampleBufferGetImageBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetPresentationTimeStamp softLink_CoreMedia_CMSampleBufferGetPresentationTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetOutputDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetOutputDuration softLink_CoreMedia_CMSampleBufferGetOutputDuration
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetOutputPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetOutputPresentationTimeStamp softLink_CoreMedia_CMSampleBufferGetOutputPresentationTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetSampleAttachmentsArray, CFArrayRef, (CMSampleBufferRef sbuf, Boolean createIfNecessary), (sbuf, createIfNecessary))
#define CMSampleBufferGetSampleAttachmentsArray softLink_CoreMedia_CMSampleBufferGetSampleAttachmentsArray
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetSampleTimingInfoArray, OSStatus, (CMSampleBufferRef sbuf, CMItemCount timingArrayEntries, CMSampleTimingInfo *timingArrayOut, CMItemCount *timingArrayEntriesNeededOut), (sbuf, timingArrayEntries, timingArrayOut, timingArrayEntriesNeededOut))
#define CMSampleBufferGetSampleTimingInfoArray softLink_CoreMedia_CMSampleBufferGetSampleTimingInfoArray
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetTotalSampleSize, size_t, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetTotalSampleSize softLink_CoreMedia_CMSampleBufferGetTotalSampleSize
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferSetDataBuffer, OSStatus, (CMSampleBufferRef sbuf, CMBlockBufferRef buffer), (sbuf, buffer))
#define CMSampleBufferSetDataBuffer softLink_CoreMedia_CMSampleBufferSetDataBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMGetAttachment, CFTypeRef, (CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode* attachmentModeOut), (target, key, attachmentModeOut))
#define CMGetAttachment softLink_CoreMedia_CMGetAttachment
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSetAttachment, void, (CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode), (target, key, value, attachmentMode))
#define CMSetAttachment softLink_CoreMedia_CMSetAttachment
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMRemoveAttachment, void, (CMAttachmentBearerRef target, CFStringRef key), (target, key))
#define CMRemoveAttachment softLink_CoreMedia_CMRemoveAttachment
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseCreateWithMasterClock, OSStatus, (CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut), (allocator, masterClock, timebaseOut))
#define CMTimebaseCreateWithMasterClock softLink_CoreMedia_CMTimebaseCreateWithMasterClock
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetTime softLink_CoreMedia_CMTimebaseGetTime
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseGetRate, Float64, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetRate softLink_CoreMedia_CMTimebaseGetRate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetRate, OSStatus, (CMTimebaseRef timebase, Float64 rate), (timebase, rate))
#define CMTimebaseSetRate softLink_CoreMedia_CMTimebaseSetRate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetTime, OSStatus, (CMTimebaseRef timebase, CMTime time), (timebase, time))
#define CMTimebaseSetTime softLink_CoreMedia_CMTimebaseSetTime
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseGetEffectiveRate, Float64, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetEffectiveRate softLink_CoreMedia_CMTimebaseGetEffectiveRate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseAddTimerDispatchSource, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource))
#define CMTimebaseAddTimerDispatchSource softLink_CoreMedia_CMTimebaseAddTimerDispatchSource
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseRemoveTimerDispatchSource, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource))
#define CMTimebaseRemoveTimerDispatchSource softLink_CoreMedia_CMTimebaseRemoveTimerDispatchSource
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetTimerDispatchSourceNextFireTime, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource, CMTime fireTime, uint32_t flags), (timebase, timerSource, fireTime, flags))
#define CMTimebaseSetTimerDispatchSourceNextFireTime softLink_CoreMedia_CMTimebaseSetTimerDispatchSourceNextFireTime
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimebaseSetTimerDispatchSourceToFireImmediately, OSStatus, (CMTimebaseRef timebase, dispatch_source_t timerSource), (timebase, timerSource))
#define CMTimebaseSetTimerDispatchSourceToFireImmediately softLink_CoreMedia_CMTimebaseSetTimerDispatchSourceToFireImmediately
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))
#define CMTimeCopyAsDictionary softLink_CoreMedia_CMTimeCopyAsDictionary
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMVideoFormatDescriptionCreateForImageBuffer, OSStatus, (CFAllocatorRef allocator, CVImageBufferRef imageBuffer, CMVideoFormatDescriptionRef *outDesc), (allocator, imageBuffer, outDesc))
#define CMVideoFormatDescriptionCreateForImageBuffer softLink_CoreMedia_CMVideoFormatDescriptionCreateForImageBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMVideoFormatDescriptionGetDimensions, CMVideoDimensions, (CMVideoFormatDescriptionRef videoDesc), (videoDesc))
#define CMVideoFormatDescriptionGetDimensions softLink_CoreMedia_CMVideoFormatDescriptionGetDimensions
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMVideoFormatDescriptionGetPresentationDimensions, CGSize, (CMVideoFormatDescriptionRef videoDesc, Boolean usePixelAspectRatio, Boolean useCleanAperture), (videoDesc, usePixelAspectRatio, useCleanAperture))
#define CMVideoFormatDescriptionGetPresentationDimensions softLink_CoreMedia_CMVideoFormatDescriptionGetPresentationDimensions
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueCreate, OSStatus, (CFAllocatorRef allocator, CMItemCount capacity, const CMBufferCallbacks* callbacks, CMBufferQueueRef* queueOut), (allocator, capacity, callbacks, queueOut))
#define CMBufferQueueCreate softLink_CoreMedia_CMBufferQueueCreate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueReset, OSStatus, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueReset softLink_CoreMedia_CMBufferQueueReset
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueGetHead, CMBufferRef, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueGetHead softLink_CoreMedia_CMBufferQueueGetHead
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueDequeueAndRetain, CMBufferRef, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueDequeueAndRetain softLink_CoreMedia_CMBufferQueueDequeueAndRetain
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueEnqueue, OSStatus, (CMBufferQueueRef queue, CMBufferRef buffer), (queue, buffer))
#define CMBufferQueueEnqueue softLink_CoreMedia_CMBufferQueueEnqueue
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueIsEmpty, Boolean, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueIsEmpty softLink_CoreMedia_CMBufferQueueIsEmpty
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueGetBufferCount, CMItemCount, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueGetBufferCount softLink_CoreMedia_CMBufferQueueGetBufferCount
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueGetFirstPresentationTimeStamp, CMTime, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueGetFirstPresentationTimeStamp softLink_CoreMedia_CMBufferQueueGetFirstPresentationTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueGetEndPresentationTimeStamp, CMTime, (CMBufferQueueRef queue), (queue))
#define CMBufferQueueGetEndPresentationTimeStamp softLink_CoreMedia_CMBufferQueueGetEndPresentationTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBufferQueueInstallTriggerWithIntegerThreshold, OSStatus, (CMBufferQueueRef queue, CMBufferQueueTriggerCallback triggerCallback, void* triggerRefcon, CMBufferQueueTriggerCondition triggerCondition, CMItemCount triggerThreshold, CMBufferQueueTriggerToken* triggerTokenOut), (queue, triggerCallback, triggerRefcon, triggerCondition, triggerThreshold, triggerTokenOut))
#define CMBufferQueueInstallTriggerWithIntegerThreshold softLink_CoreMedia_CMBufferQueueInstallTriggerWithIntegerThreshold

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleAttachmentKey_DoNotDisplay, CFStringRef)
#define kCMSampleAttachmentKey_DoNotDisplay get_CoreMedia_kCMSampleAttachmentKey_DoNotDisplay()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleAttachmentKey_NotSync, CFStringRef)
#define kCMSampleAttachmentKey_NotSync get_CoreMedia_kCMSampleAttachmentKey_NotSync()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleAttachmentKey_DisplayImmediately, CFStringRef)
#define kCMSampleAttachmentKey_DisplayImmediately get_CoreMedia_kCMSampleAttachmentKey_DisplayImmediately()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleAttachmentKey_IsDependedOnByOthers, CFStringRef)
#define kCMSampleAttachmentKey_IsDependedOnByOthers get_CoreMedia_kCMSampleAttachmentKey_IsDependedOnByOthers()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately, CFStringRef)
#define kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately get_CoreMedia_kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_DrainAfterDecoding, CFStringRef)
#define kCMSampleBufferAttachmentKey_DrainAfterDecoding get_CoreMedia_kCMSampleBufferAttachmentKey_DrainAfterDecoding()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_EmptyMedia, CFStringRef)
#define kCMSampleBufferAttachmentKey_EmptyMedia get_CoreMedia_kCMSampleBufferAttachmentKey_EmptyMedia()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed, CFStringRef)
#define kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed get_CoreMedia_kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding, CFStringRef)
#define kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding get_CoreMedia_kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_SampleReferenceByteOffset, CFStringRef)
#define kCMSampleBufferAttachmentKey_SampleReferenceByteOffset get_CoreMedia_kCMSampleBufferAttachmentKey_SampleReferenceByteOffset()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferAttachmentKey_SampleReferenceURL, CFStringRef)
#define kCMSampleBufferAttachmentKey_SampleReferenceURL get_CoreMedia_kCMSampleBufferAttachmentKey_SampleReferenceURL()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimebaseNotification_EffectiveRateChanged, CFStringRef)
#define kCMTimebaseNotification_EffectiveRateChanged get_CoreMedia_kCMTimebaseNotification_EffectiveRateChanged()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimebaseNotification_TimeJumped, CFStringRef)
#define kCMTimebaseNotification_TimeJumped get_CoreMedia_kCMTimebaseNotification_TimeJumped()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMSampleBufferConsumerNotification_BufferConsumed, CFStringRef)
#define kCMSampleBufferConsumerNotification_BufferConsumed get_CoreMedia_kCMSampleBufferConsumerNotification_BufferConsumed()
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioFormatDescriptionGetStreamBasicDescription, const AudioStreamBasicDescription *, (CMAudioFormatDescriptionRef desc), (desc))
#define CMAudioFormatDescriptionGetStreamBasicDescription softLink_CoreMedia_CMAudioFormatDescriptionGetStreamBasicDescription
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMBlockBufferCreateWithMemoryBlock, OSStatus, (CFAllocatorRef structureAllocator, void* memoryBlock, size_t blockLength, CFAllocatorRef blockAllocator, const CMBlockBufferCustomBlockSource* customBlockSource, size_t offsetToData, size_t dataLength, CMBlockBufferFlags flags, CMBlockBufferRef* blockBufferOut), (structureAllocator, memoryBlock, blockLength, blockAllocator, customBlockSource, offsetToData, dataLength, flags, blockBufferOut))
#define CMBlockBufferCreateWithMemoryBlock softLink_CoreMedia_CMBlockBufferCreateWithMemoryBlock
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer, OSStatus, (CMSampleBufferRef sbuf, size_t *bufferListSizeNeededOut, AudioBufferList *bufferListOut, size_t bufferListSize, CFAllocatorRef bbufStructAllocator, CFAllocatorRef bbufMemoryAllocator, uint32_t flags, CMBlockBufferRef *blockBufferOut), (sbuf, bufferListSizeNeededOut, bufferListOut, bufferListSize, bbufStructAllocator, bbufMemoryAllocator, flags, blockBufferOut))
#define CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer softLink_CoreMedia_CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetNumSamples, CMItemCount, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetNumSamples softLink_CoreMedia_CMSampleBufferGetNumSamples
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCopySampleBufferForRange, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CFRange sampleRange, CMSampleBufferRef* sBufOut), (allocator, sbuf, sampleRange, sBufOut))
#define CMSampleBufferCopySampleBufferForRange softLink_CoreMedia_CMSampleBufferCopySampleBufferForRange
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferGetSampleSizeArray, OSStatus, (CMSampleBufferRef sbuf, CMItemCount sizeArrayEntries, size_t* sizeArrayOut, CMItemCount* sizeArrayEntriesNeededOut), (sbuf, sizeArrayEntries, sizeArrayOut, sizeArrayEntriesNeededOut))
#define CMSampleBufferGetSampleSizeArray softLink_CoreMedia_CMSampleBufferGetSampleSizeArray

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioSampleBufferCreateWithPacketDescriptions, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMTime sbufPTS, const AudioStreamPacketDescription *packetDescriptions, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, sbufPTS, packetDescriptions, sBufOut))
#define CMAudioSampleBufferCreateWithPacketDescriptions softLink_CoreMedia_CMAudioSampleBufferCreateWithPacketDescriptions
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferSetDataBufferFromAudioBufferList, OSStatus, (CMSampleBufferRef sbuf, CFAllocatorRef bbufStructAllocator, CFAllocatorRef bbufMemoryAllocator, uint32_t flags, const AudioBufferList *bufferList), (sbuf, bbufStructAllocator, bbufMemoryAllocator, flags, bufferList))
#define CMSampleBufferSetDataBufferFromAudioBufferList softLink_CoreMedia_CMSampleBufferSetDataBufferFromAudioBufferList
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferSetDataReady, OSStatus, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferSetDataReady softLink_CoreMedia_CMSampleBufferSetDataReady
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioFormatDescriptionCreate, OSStatus, (CFAllocatorRef allocator, const AudioStreamBasicDescription* asbd, size_t layoutSize, const AudioChannelLayout* layout, size_t magicCookieSize, const void* magicCookie, CFDictionaryRef extensions, CMAudioFormatDescriptionRef* outDesc), (allocator, asbd, layoutSize, layout, magicCookieSize, magicCookie, extensions, outDesc))
#define CMAudioFormatDescriptionCreate softLink_CoreMedia_CMAudioFormatDescriptionCreate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMClockGetHostTimeClock, CMClockRef, (void), ())
#define CMClockGetHostTimeClock  softLink_CoreMedia_CMClockGetHostTimeClock
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMClockGetTime, CMTime, (CMClockRef clock), (clock))
#define CMClockGetTime  softLink_CoreMedia_CMClockGetTime

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCallForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (* CMSAMPLEBUFFERCALL_NOESCAPE callback)(CMSampleBufferRef sampleBuffer, CMItemCount index, void *refcon), void *refcon), (sbuf, callback, refcon))
#define CMSampleBufferCallForEachSample softLink_CoreMedia_CMSampleBufferCallForEachSample
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMSampleBufferCallBlockForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (^ CMSAMPLEBUFFERCALL_NOESCAPE handler)(CMSampleBufferRef, CMItemCount)), (sbuf, handler))
#define CMSampleBufferCallBlockForEachSample softLink_CoreMedia_CMSampleBufferCallBlockForEachSample

#endif // PLATFORM(COCOA)

#if PLATFORM(IOS_FAMILY)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioClockCreate, OSStatus, (CFAllocatorRef allocator, CMClockRef *clockOut), (allocator, clockOut))
#define CMAudioClockCreate softLink_CoreMedia_CMAudioClockCreate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMaximum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeMaximum softLink_CoreMedia_CMTimeMaximum
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMinimum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeMinimum softLink_CoreMedia_CMTimeMinimum
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeRangeContainsTime, Boolean, (CMTimeRange range, CMTime time), (range, time))
#define CMTimeRangeContainsTime softLink_CoreMedia_CMTimeRangeContainsTime

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, CoreMedia, kCMTimeIndefinite, CMTime)
#define kCMTimeIndefinite get_CoreMedia_kCMTimeIndefinite()

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMAudioDeviceClockCreate, OSStatus, (CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut), (allocator, deviceUID, clockOut))
#define CMAudioDeviceClockCreate  softLink_CoreMedia_CMAudioDeviceClockCreate

#endif // PLATFORM(MAC)

#if PLATFORM(WIN)

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, CoreMedia, CMTimeMakeFromDictionary, CMTime, (CFDictionaryRef dict), (dict))
#define CMTimeMakeFromDictionary softLink_CoreMedia_CMTimeMakeFromDictionary

#endif // PLATFORM(WIN)

#endif // USE(AVFOUNDATION)
