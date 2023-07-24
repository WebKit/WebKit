/* Copyright (c) the JPEG XL Project Authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define JXL_BOOL int

typedef enum {
    JXL_TYPE_UINT8 = 2,
} JxlDataType;

typedef enum {
    JXL_NATIVE_ENDIAN = 0,
} JxlEndianness;

typedef enum {
    JXL_DEC_SUCCESS = 0,
    JXL_DEC_ERROR = 1,
    JXL_DEC_NEED_MORE_INPUT = 2,
    JXL_DEC_BASIC_INFO = 0x40,
    JXL_DEC_COLOR_ENCODING = 0x100,
    JXL_DEC_FRAME = 0x400,
    JXL_DEC_FULL_IMAGE = 0x1000,
} JxlDecoderStatus;

typedef enum {
    JXL_SIG_NOT_ENOUGH_BYTES = 0,
    JXL_SIG_INVALID = 1,
} JxlSignature;

typedef enum {
    JXL_ORIENT_ROTATE_90_CCW = 8,
} JxlOrientation;

typedef enum {
    JXL_BLEND_MUL = 4,
} JxlBlendMode;

typedef struct {
    uint32_t num_channels;
    JxlDataType data_type;
    JxlEndianness endianness;
    size_t align;
} JxlPixelFormat;

typedef struct {
    uint32_t xsize;
    uint32_t ysize;
} JxlPreviewHeader;

typedef struct {
    uint32_t tps_numerator;
    uint32_t tps_denominator;
    uint32_t num_loops;
    JXL_BOOL have_timecodes;
} JxlAnimationHeader;

typedef struct {
    JXL_BOOL have_container;
    uint32_t xsize;
    uint32_t ysize;
    uint32_t bits_per_sample;
    uint32_t exponent_bits_per_sample;
    float intensity_target;
    float min_nits;
    JXL_BOOL relative_to_max_display;
    float linear_below;
    JXL_BOOL uses_original_profile;
    JXL_BOOL have_preview;
    JXL_BOOL have_animation;
    JxlOrientation orientation;
    uint32_t num_color_channels;
    uint32_t num_extra_channels;
    uint32_t alpha_bits;
    uint32_t alpha_exponent_bits;
    JXL_BOOL alpha_premultiplied;
    JxlPreviewHeader preview;
    JxlAnimationHeader animation;
    uint32_t intrinsic_xsize;
    uint32_t intrinsic_ysize;
    uint8_t padding[100];
} JxlBasicInfo;

typedef struct {
    JxlBlendMode blendmode;
    uint32_t source;
    uint32_t alpha;
    JXL_BOOL clamp;
} JxlBlendInfo;

typedef struct {
    JXL_BOOL have_crop;
    int32_t crop_x0;
    int32_t crop_y0;
    uint32_t xsize;
    uint32_t ysize;
    JxlBlendInfo blend_info;
    uint32_t save_as_reference;
} JxlLayerInfo;

typedef struct {
    uint32_t duration;
    uint32_t timecode;
    uint32_t name_length;
    JXL_BOOL is_last;
    JxlLayerInfo layer_info;
} JxlFrameHeader;

typedef enum {
    JXL_COLOR_PROFILE_TARGET_DATA = 1,
} JxlColorProfileTarget;

typedef struct JxlDecoderStruct JxlDecoder;

typedef void* (*jpegxl_alloc_func)(void* opaque, size_t size);
typedef void (*jpegxl_free_func)(void* opaque, void* address);

typedef struct JxlMemoryManagerStruct {
    void* opaque;
    jpegxl_alloc_func alloc;
    jpegxl_free_func free;
} JxlMemoryManager;

typedef void (*JxlImageOutCallback)(void* opaque, size_t x, size_t y, size_t num_pixels, const void* pixels);

JxlDecoder* JxlDecoderCreate(const JxlMemoryManager*);
JxlDecoderStatus JxlDecoderSubscribeEvents(JxlDecoder*, int events_wanted);
void JxlDecoderRewind(JxlDecoder*);
void JxlDecoderSkipFrames(JxlDecoder*, size_t amount);
JxlDecoderStatus JxlDecoderSetInput(JxlDecoder*, const uint8_t* data, size_t);
size_t JxlDecoderReleaseInput(JxlDecoder*);
JxlDecoderStatus JxlDecoderProcessInput(JxlDecoder*);
JxlDecoderStatus JxlDecoderGetBasicInfo(const JxlDecoder*, JxlBasicInfo*);
JxlDecoderStatus JxlDecoderGetFrameHeader(const JxlDecoder*, JxlFrameHeader*);
JxlDecoderStatus JxlDecoderSetImageOutCallback(JxlDecoder*, const JxlPixelFormat*, JxlImageOutCallback, void* opaque);
void JxlDecoderDestroy(JxlDecoder*);
JxlSignature JxlSignatureCheck(const uint8_t* buf, size_t len);
JxlDecoderStatus JxlDecoderGetICCProfileSize(const JxlDecoder*, const JxlPixelFormat*, JxlColorProfileTarget, size_t*);
JxlDecoderStatus JxlDecoderGetColorAsICCProfile(const JxlDecoder*, const JxlPixelFormat*, JxlColorProfileTarget, uint8_t* icc_profile, size_t);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

