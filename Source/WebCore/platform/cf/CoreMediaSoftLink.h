/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef CoreMediaSoftLink_h
#define CoreMediaSoftLink_h

#if USE(AVFOUNDATION)
// FIXME: Should be USE(COREMEDIA), but this isn't currently defined on Windows.

#include "CoreMediaSPI.h"
#include "SoftLinking.h"

SOFT_LINK_FRAMEWORK_FOR_HEADER(WebCore, CoreMedia)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMBlockBufferCopyDataBytes, OSStatus, (CMBlockBufferRef theSourceBuffer, size_t offsetToData, size_t dataLength, void* destination), (theSourceBuffer, offsetToData, dataLength, destination))
#define CMBlockBufferCopyDataBytes softLink_CoreMedia_CMBlockBufferCopyDataBytes
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMBlockBufferGetDataLength, size_t, (CMBlockBufferRef theBuffer), (theBuffer))
#define CMBlockBufferGetDataLength softLink_CoreMedia_CMBlockBufferGetDataLength
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMFormatDescriptionGetExtensions, CFDictionaryRef, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetExtensions softLink_CoreMedia_CMFormatDescriptionGetExtensions
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetDataBuffer, CMBlockBufferRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDataBuffer softLink_CoreMedia_CMSampleBufferGetDataBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetFormatDescription, CMFormatDescriptionRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetFormatDescription softLink_CoreMedia_CMSampleBufferGetFormatDescription
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetSampleTimingInfo, OSStatus, (CMSampleBufferRef sbuf, CMItemIndex sampleIndex, CMSampleTimingInfo* timingInfoOut), (sbuf, sampleIndex, timingInfoOut))
#define CMSampleBufferGetSampleTimingInfo softLink_CoreMedia_CMSampleBufferGetSampleTimingInfo
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeCompare, int32_t, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeCompare softLink_CoreMedia_CMTimeCompare
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeGetSeconds, Float64, (CMTime time), (time))
#define CMTimeGetSeconds softLink_CoreMedia_CMTimeGetSeconds
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeMake, CMTime, (int64_t value, int32_t timescale), (value, timescale))
#define CMTimeMake softLink_CoreMedia_CMTimeMake
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeMakeWithSeconds, CMTime, (Float64 seconds, int32_t preferredTimeScale), (seconds, preferredTimeScale))
#define CMTimeMakeWithSeconds softLink_CoreMedia_CMTimeMakeWithSeconds
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeRangeGetEnd, CMTime, (CMTimeRange range), (range))
#define CMTimeRangeGetEnd softLink_CoreMedia_CMTimeRangeGetEnd

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, CFStringRef)
#define kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms get_CoreMedia_kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAlignmentType_End, CFStringRef)
#define kCMTextMarkupAlignmentType_End get_CoreMedia_kCMTextMarkupAlignmentType_End()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAlignmentType_Middle, CFStringRef)
#define kCMTextMarkupAlignmentType_Middle get_CoreMedia_kCMTextMarkupAlignmentType_Middle()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAlignmentType_Start, CFStringRef)
#define kCMTextMarkupAlignmentType_Start get_CoreMedia_kCMTextMarkupAlignmentType_Start()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_Alignment, CFStringRef)
#define kCMTextMarkupAttribute_Alignment get_CoreMedia_kCMTextMarkupAttribute_Alignment()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_BackgroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_BackgroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_BackgroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, CFStringRef)
#define kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight get_CoreMedia_kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_BoldStyle, CFStringRef)
#define kCMTextMarkupAttribute_BoldStyle get_CoreMedia_kCMTextMarkupAttribute_BoldStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_CharacterBackgroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_CharacterBackgroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_CharacterBackgroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_FontFamilyName, CFStringRef)
#define kCMTextMarkupAttribute_FontFamilyName get_CoreMedia_kCMTextMarkupAttribute_FontFamilyName()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_ForegroundColorARGB, CFStringRef)
#define kCMTextMarkupAttribute_ForegroundColorARGB get_CoreMedia_kCMTextMarkupAttribute_ForegroundColorARGB()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_ItalicStyle, CFStringRef)
#define kCMTextMarkupAttribute_ItalicStyle get_CoreMedia_kCMTextMarkupAttribute_ItalicStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, CFStringRef)
#define kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection get_CoreMedia_kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_RelativeFontSize, CFStringRef)
#define kCMTextMarkupAttribute_RelativeFontSize get_CoreMedia_kCMTextMarkupAttribute_RelativeFontSize()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, CFStringRef)
#define kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection get_CoreMedia_kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_UnderlineStyle, CFStringRef)
#define kCMTextMarkupAttribute_UnderlineStyle get_CoreMedia_kCMTextMarkupAttribute_UnderlineStyle()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_VerticalLayout, CFStringRef)
#define kCMTextMarkupAttribute_VerticalLayout get_CoreMedia_kCMTextMarkupAttribute_VerticalLayout()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextMarkupAttribute_WritingDirectionSizePercentage, CFStringRef)
#define kCMTextMarkupAttribute_WritingDirectionSizePercentage get_CoreMedia_kCMTextMarkupAttribute_WritingDirectionSizePercentage()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextVerticalLayout_LeftToRight, CFStringRef)
#define kCMTextVerticalLayout_LeftToRight get_CoreMedia_kCMTextVerticalLayout_LeftToRight()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTextVerticalLayout_RightToLeft, CFStringRef)
#define kCMTextVerticalLayout_RightToLeft get_CoreMedia_kCMTextVerticalLayout_RightToLeft()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTimeInvalid, CMTime)
#define kCMTimeInvalid get_CoreMedia_kCMTimeInvalid()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTimeZero, CMTime)
#define kCMTimeZero get_CoreMedia_kCMTimeZero()

#if PLATFORM(COCOA)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMFormatDescriptionGetMediaSubType, FourCharCode, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetMediaSubType softLink_CoreMedia_CMFormatDescriptionGetMediaSubType
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMFormatDescriptionGetMediaType, CMMediaType, (CMFormatDescriptionRef desc), (desc))
#define CMFormatDescriptionGetMediaType softLink_CoreMedia_CMFormatDescriptionGetMediaType
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMNotificationCenterGetDefaultLocalCenter, CMNotificationCenterRef, (void), ())
#define CMNotificationCenterGetDefaultLocalCenter softLink_CoreMedia_CMNotificationCenterGetDefaultLocalCenter
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMNotificationCenterAddListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object, UInt32 flags), (center, listener, callback, notification, object, flags))
#define CMNotificationCenterAddListener softLink_CoreMedia_CMNotificationCenterAddListener
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMNotificationCenterRemoveListener, OSStatus, (CMNotificationCenterRef center, const void* listener, CMNotificationCallback callback, CFStringRef notification, const void* object), (center, listener, callback, notification, object))
#define CMNotificationCenterRemoveListener softLink_CoreMedia_CMNotificationCenterRemoveListener
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferCallForEachSample, OSStatus, (CMSampleBufferRef sbuf, OSStatus (*callback)( CMSampleBufferRef sampleBuffer, CMItemCount index, void *refcon), void *refcon), (sbuf, callback, refcon))
#define CMSampleBufferCallForEachSample softLink_CoreMedia_CMSampleBufferCallForEachSample
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferCreate, OSStatus, (CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sBufOut), (allocator, dataBuffer, dataReady, makeDataReadyCallback, makeDataReadyRefcon, formatDescription, numSamples, numSampleTimingEntries, sampleTimingArray, numSampleSizeEntries, sampleSizeArray, sBufOut))
#define CMSampleBufferCreate softLink_CoreMedia_CMSampleBufferCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferCreateCopy, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef sbuf, CMSampleBufferRef *sbufCopyOut), (allocator, sbuf, sbufCopyOut))
#define CMSampleBufferCreateCopy softLink_CoreMedia_CMSampleBufferCreateCopy
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferCreateCopyWithNewTiming, OSStatus, (CFAllocatorRef allocator, CMSampleBufferRef originalSBuf, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray, CMSampleBufferRef *sBufCopyOut), (allocator, originalSBuf, numSampleTimingEntries, sampleTimingArray, sBufCopyOut))
#define CMSampleBufferCreateCopyWithNewTiming softLink_CoreMedia_CMSampleBufferCreateCopyWithNewTiming
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetDecodeTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDecodeTimeStamp softLink_CoreMedia_CMSampleBufferGetDecodeTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetDuration, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetDuration softLink_CoreMedia_CMSampleBufferGetDuration
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetImageBuffer, CVImageBufferRef, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetImageBuffer softLink_CoreMedia_CMSampleBufferGetImageBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetPresentationTimeStamp, CMTime, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetPresentationTimeStamp softLink_CoreMedia_CMSampleBufferGetPresentationTimeStamp
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetSampleAttachmentsArray, CFArrayRef, (CMSampleBufferRef sbuf, Boolean createIfNecessary), (sbuf, createIfNecessary))
#define CMSampleBufferGetSampleAttachmentsArray softLink_CoreMedia_CMSampleBufferGetSampleAttachmentsArray
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetSampleTimingInfoArray, OSStatus, (CMSampleBufferRef sbuf, CMItemCount timingArrayEntries, CMSampleTimingInfo *timingArrayOut, CMItemCount *timingArrayEntriesNeededOut), (sbuf, timingArrayEntries, timingArrayOut, timingArrayEntriesNeededOut))
#define CMSampleBufferGetSampleTimingInfoArray softLink_CoreMedia_CMSampleBufferGetSampleTimingInfoArray
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetTotalSampleSize, size_t, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetTotalSampleSize softLink_CoreMedia_CMSampleBufferGetTotalSampleSize
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSetAttachment, void, (CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value, CMAttachmentMode attachmentMode), (target, key, value, attachmentMode))
#define CMSampleBufferGetTotalSampleSize softLink_CoreMedia_CMSampleBufferGetTotalSampleSize
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimebaseCreateWithMasterClock, OSStatus, (CFAllocatorRef allocator, CMClockRef masterClock, CMTimebaseRef *timebaseOut), (allocator, masterClock, timebaseOut))
#define CMTimebaseCreateWithMasterClock softLink_CoreMedia_CMTimebaseCreateWithMasterClock
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimebaseGetTime, CMTime, (CMTimebaseRef timebase), (timebase))
#define CMTimebaseGetTime softLink_CoreMedia_CMTimebaseGetTime
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimebaseSetRate, OSStatus, (CMTimebaseRef timebase, Float64 rate), (timebase, rate))
#define CMTimebaseSetRate softLink_CoreMedia_CMTimebaseSetRate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimebaseSetTime, OSStatus, (CMTimebaseRef timebase, CMTime time), (timebase, time))
#define CMTimebaseSetTime softLink_CoreMedia_CMTimebaseSetTime
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeCopyAsDictionary, CFDictionaryRef, (CMTime time, CFAllocatorRef allocator), (time, allocator))
#define CMTimeCopyAsDictionary softLink_CoreMedia_CMTimeCopyAsDictionary
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMVideoFormatDescriptionGetDimensions, CMVideoDimensions, (CMVideoFormatDescriptionRef videoDesc), (videoDesc))
#define CMVideoFormatDescriptionGetDimensions softLink_CoreMedia_CMVideoFormatDescriptionGetDimensions
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMVideoFormatDescriptionGetPresentationDimensions, CGSize, (CMVideoFormatDescriptionRef videoDesc, Boolean usePixelAspectRatio, Boolean useCleanAperture), (videoDesc, usePixelAspectRatio, useCleanAperture))
#define CMVideoFormatDescriptionGetPresentationDimensions softLink_CoreMedia_CMVideoFormatDescriptionGetPresentationDimensions

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleAttachmentKey_DoNotDisplay, CFStringRef)
#define kCMSampleAttachmentKey_DoNotDisplay get_CoreMedia_kCMSampleAttachmentKey_DoNotDisplay()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleAttachmentKey_NotSync, CFStringRef)
#define kCMSampleAttachmentKey_NotSync get_CoreMedia_kCMSampleAttachmentKey_NotSync()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately, CFStringRef)
#define kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately get_CoreMedia_kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleBufferAttachmentKey_DrainAfterDecoding, CFStringRef)
#define kCMSampleBufferAttachmentKey_DrainAfterDecoding get_CoreMedia_kCMSampleBufferAttachmentKey_DrainAfterDecoding()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleBufferAttachmentKey_EmptyMedia, CFStringRef)
#define kCMSampleBufferAttachmentKey_EmptyMedia get_CoreMedia_kCMSampleBufferAttachmentKey_EmptyMedia()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding, CFStringRef)
#define kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding get_CoreMedia_kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTimebaseNotification_EffectiveRateChanged, CFStringRef)
#define kCMTimebaseNotification_EffectiveRateChanged get_CoreMedia_kCMTimebaseNotification_EffectiveRateChanged()
SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTimebaseNotification_TimeJumped, CFStringRef)
#define kCMTimebaseNotification_TimeJumped get_CoreMedia_kCMTimebaseNotification_TimeJumped()
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMAudioFormatDescriptionGetStreamBasicDescription, const AudioStreamBasicDescription *, (CMAudioFormatDescriptionRef desc), (desc))
#define CMAudioFormatDescriptionGetStreamBasicDescription softLink_CoreMedia_CMAudioFormatDescriptionGetStreamBasicDescription
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer, OSStatus, (CMSampleBufferRef sbuf, size_t *bufferListSizeNeededOut, AudioBufferList *bufferListOut, size_t bufferListSize, CFAllocatorRef bbufStructAllocator, CFAllocatorRef bbufMemoryAllocator, uint32_t flags, CMBlockBufferRef *blockBufferOut), (sbuf, bufferListSizeNeededOut, bufferListOut, bufferListSize, bbufStructAllocator, bbufMemoryAllocator, flags, blockBufferOut))
#define CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer softLink_CoreMedia_CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMSampleBufferGetNumSamples, CMItemCount, (CMSampleBufferRef sbuf), (sbuf))
#define CMSampleBufferGetNumSamples softLink_CoreMedia_CMSampleBufferGetNumSamples

#endif // PLATFORM(COCOA)

#if PLATFORM(IOS)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMAudioClockCreate, OSStatus, (CFAllocatorRef allocator, CMClockRef *clockOut), (allocator, clockOut))
#define CMAudioClockCreate softLink_CoreMedia_CMAudioClockCreate
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeMaximum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeMaximum softLink_CoreMedia_CMTimeMaximum
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeMinimum, CMTime, (CMTime time1, CMTime time2), (time1, time2))
#define CMTimeMinimum softLink_CoreMedia_CMTimeMinimum
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeRangeContainsTime, Boolean, (CMTimeRange range, CMTime time), (range, time))
#define CMTimeRangeContainsTime softLink_CoreMedia_CMTimeRangeContainsTime
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeRangeMake, CMTimeRange, (CMTime start, CMTime duration), (start, duration))
#define CMTimeRangeMake softLink_CoreMedia_CMTimeRangeMake
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeSubtract, CMTime, (CMTime minuend, CMTime subtrahend), (minuend, subtrahend))
#define CMTimeSubtract softLink_CoreMedia_CMTimeSubtract

SOFT_LINK_CONSTANT_FOR_HEADER(WebCore, CoreMedia, kCMTimeIndefinite, CMTime)
#define kCMTimeIndefinite get_CoreMedia_kCMTimeIndefinite()

#endif // PLATFORM(IOS)

#if PLATFORM(MAC)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMAudioDeviceClockCreate, OSStatus, (CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut), (allocator, deviceUID, clockOut))
#define CMAudioDeviceClockCreate  softLink_CoreMedia_CMAudioDeviceClockCreate

#endif // PLATFORM(MAC)

#if PLATFORM(WIN)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeAdd, CMTime, (CMTime addend1, CMTime addend2), (addend1, addend2))
#define CMTimeAdd softLink_CoreMedia_CMTimeAdd
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, CoreMedia, CMTimeMakeFromDictionary, CMTime, (CFDictionaryRef dict), (dict))
#define CMTimeMakeFromDictionary softLink_CoreMedia_CMTimeMakeFromDictionary

#endif // PLATFORM(WIN)

#endif // USE(AVFOUNDATION)

#endif // CoreMediaSoftLink_h
