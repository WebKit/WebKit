/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "VideoProcessingSoftLink.h"

#if ENABLE_VCP_ENCODER

#define CompressionSessionRef VCPCompressionSessionRef

#define CompressionSessionSetProperty webrtc::VCPCompressionSessionSetProperty
#define CompressionSessionGetPixelBufferPool webrtc::VCPCompressionSessionGetPixelBufferPool
#define CompressionSessionEncodeFrame webrtc::VCPCompressionSessionEncodeFrame
#define CompressionSessionCreate webrtc::VCPCompressionSessionCreate
#define kCodecTypeH264 kVCPCodecType4CC_H264
#define CompressionSessionInvalidate webrtc::VCPCompressionSessionInvalidate

#elif ENABLE_VCP_VTB_ENCODER

bool isSupportingH264RTVC();

#define CompressionSessionRef VCPCompressionSessionRef

#define CompressionSessionSetProperty(compressionSession, ...) (isSupportingH264RTVC() ? VTSessionSetProperty(reinterpret_cast<VTCompressionSessionRef>(compressionSession), __VA_ARGS__) : webrtc::VCPCompressionSessionSetProperty(compressionSession, __VA_ARGS__))
#define CompressionSessionGetPixelBufferPool(compressionSession, ...) (isSupportingH264RTVC() ? VTCompressionSessionGetPixelBufferPool(reinterpret_cast<VTCompressionSessionRef>(compressionSession), __VA_ARGS__) : webrtc::VCPCompressionSessionSetProperty(compressionSession, __VA_ARGS__))
#define CompressionSessionEncodeFrame(compressionSession, ...) (isSupportingH264RTVC() ? VTCompressionSessionEncodeFrame(reinterpret_cast<VTCompressionSessionRef>(compressionSession), __VA_ARGS__) : webrtc::VCPCompressionSessionEncodeFrame(compressionSession, __VA_ARGS__))
#define CompressionSessionCreate(a, b, c, d, e, f, g, h, i, compressionSession) (isSupportingH264RTVC() ? VTCompressionSessionCreate(a, b, c, d, e, f, g, h, i, reinterpret_cast<VTCompressionSessionRef*>(compressionSession)) : webrtc::VCPCompressionSessionCreate(a, b, c, d, e, f, g, h, i, compressionSession))
#define kCodecTypeH264 (isSupportingH264RTVC() ? kCMVideoCodecType_H264 : kVCPCodecType4CC_H264)
#define CompressionSessionInvalidate(compressionSession) (isSupportingH264RTVC() ? VTCompressionSessionInvalidate(reinterpret_cast<VTCompressionSessionRef>(compressionSession)) : webrtc::VCPCompressionSessionInvalidate(compressionSession))

#else

#define CompressionSessionRef VTCompressionSessionRef

#define CompressionSessionSetProperty VTSessionSetProperty
#define CompressionSessionGetPixelBufferPool VTCompressionSessionGetPixelBufferPool
#define CompressionSessionEncodeFrame VTCompressionSessionEncodeFrame
#define CompressionSessionCreate VTCompressionSessionCreate
#define kCodecTypeH264 kCMVideoCodecType_H264
#define CompressionSessionInvalidate VTCompressionSessionInvalidate

#endif
