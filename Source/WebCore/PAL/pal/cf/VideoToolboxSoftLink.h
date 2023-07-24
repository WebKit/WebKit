/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include <VideoToolbox/VTCompressionSession.h>

#include <pal/spi/cf/VideoToolboxSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, VideoToolbox)

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_ExpectedFrameRate, CFStringRef)
#define kVTCompressionPropertyKey_ExpectedFrameRate get_VideoToolbox_kVTCompressionPropertyKey_ExpectedFrameRate()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_MaxKeyFrameInterval, CFStringRef)
#define kVTCompressionPropertyKey_MaxKeyFrameInterval get_VideoToolbox_kVTCompressionPropertyKey_MaxKeyFrameInterval()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, CFStringRef)
#define kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration get_VideoToolbox_kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_RealTime, CFStringRef)
#define kVTCompressionPropertyKey_RealTime get_VideoToolbox_kVTCompressionPropertyKey_RealTime()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_AverageBitRate, CFStringRef)
#define kVTCompressionPropertyKey_AverageBitRate get_VideoToolbox_kVTCompressionPropertyKey_AverageBitRate()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTCompressionPropertyKey_ProfileLevel, CFStringRef)
#define kVTCompressionPropertyKey_ProfileLevel get_VideoToolbox_kVTCompressionPropertyKey_ProfileLevel()

SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, CFStringRef)
#define kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder get_VideoToolbox_kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTProfileLevel_H264_Baseline_AutoLevel, CFStringRef)
#define kVTProfileLevel_H264_Baseline_AutoLevel get_VideoToolbox_kVTProfileLevel_H264_Baseline_AutoLevel()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTProfileLevel_H264_High_AutoLevel, CFStringRef)
#define kVTProfileLevel_H264_High_AutoLevel get_VideoToolbox_kVTProfileLevel_H264_High_AutoLevel()
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, VideoToolbox, kVTProfileLevel_H264_Main_AutoLevel, CFStringRef)
#define kVTProfileLevel_H264_Main_AutoLevel get_VideoToolbox_kVTProfileLevel_H264_Main_AutoLevel()


SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTCompressionSessionCreate, OSStatus, (CFAllocatorRef allocator, int32_t width, int32_t height, CMVideoCodecType codecType, CFDictionaryRef encoderSpecification, CFDictionaryRef sourceImageBufferAttributes, CFAllocatorRef compressedDataAllocator, VTCompressionOutputCallback outputCallback, void* outputCallbackRefCon, VTCompressionSessionRef* compressionSessionOut), (allocator, width, height, codecType, encoderSpecification, sourceImageBufferAttributes, compressedDataAllocator, outputCallback, outputCallbackRefCon, compressionSessionOut))
#define VTCompressionSessionCreate softLink_VideoToolbox_VTCompressionSessionCreate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTCompressionSessionCompleteFrames, OSStatus, (VTCompressionSessionRef session, CMTime completeUntilPresentationTimeStamp), (session, completeUntilPresentationTimeStamp))
#define VTCompressionSessionCompleteFrames softLink_VideoToolbox_VTCompressionSessionCompleteFrames
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTCompressionSessionEncodeFrame, OSStatus, (VTCompressionSessionRef session, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime duration, CFDictionaryRef frameProperties, void* sourceFrameRefcon, VTEncodeInfoFlags* infoFlagsOut), (session, imageBuffer, presentationTimeStamp, duration, frameProperties, sourceFrameRefcon, infoFlagsOut))
#define VTCompressionSessionEncodeFrame softLink_VideoToolbox_VTCompressionSessionEncodeFrame
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTCompressionSessionPrepareToEncodeFrames, OSStatus, (VTCompressionSessionRef session), (session))
#define VTCompressionSessionPrepareToEncodeFrames softLink_VideoToolbox_VTCompressionSessionPrepareToEncodeFrames
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTCompressionSessionInvalidate, void, (VTCompressionSessionRef session), (session))
#define VTCompressionSessionInvalidate softLink_VideoToolbox_VTCompressionSessionInvalidate
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, VideoToolbox, VTGetDefaultColorAttributesWithHints, OSStatus, (OSType codecTypeHint, CFStringRef colorSpaceNameHint, size_t widthHint, size_t heightHint, CFStringRef* colorPrimariesOut, CFStringRef* transferFunctionOut, CFStringRef* yCbCrMatrixOut), (codecTypeHint, colorSpaceNameHint, widthHint, heightHint, colorPrimariesOut, transferFunctionOut, yCbCrMatrixOut))
#define VTGetDefaultColorAttributesWithHints softLink_VideoToolbox_VTGetDefaultColorAttributesWithHints

SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, VideoToolbox, VTRestrictVideoDecoders, OSStatus, (VTVideoDecoderRestrictions restrictionFlags, const CMVideoCodecType* allowedCodecTypeList, CMItemCount allowedCodecTypeCount), (restrictionFlags, allowedCodecTypeList, allowedCodecTypeCount));

#endif // USE(AVFOUNDATION)
