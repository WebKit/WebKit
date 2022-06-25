/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <ImageIO/ImageIOBase.h> 

#if USE(APPLE_INTERNAL_SDK)
#include <ImageIO/CGImageSourcePrivate.h>
#endif

#if !PLATFORM(WIN)
IMAGEIO_EXTERN const CFStringRef kCGImageSourceShouldPreferRGB32;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceSkipMetadata;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceSubsampleFactor;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceShouldCacheImmediately;
IMAGEIO_EXTERN const CFStringRef kCGImageSourceUseHardwareAcceleration;
#endif

WTF_EXTERN_C_BEGIN
CFStringRef CGImageSourceGetTypeWithData(CFDataRef, CFStringRef, bool*);
#if HAVE(CGIMAGESOURCE_WITH_SET_ALLOWABLE_TYPES)
OSStatus CGImageSourceSetAllowableTypes(CFArrayRef allowableTypes);
#endif
WTF_EXTERN_C_END
