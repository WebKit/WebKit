/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "config.h"

#if ((USE(SYSTEM_PREVIEW) && HAVE(ARKIT_QUICK_LOOK_PREVIEW_ITEM)) || ((PLATFORM(IOS) || PLATFORM(VISION)) && USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/ARKitSoftLinkAdditions.mm>)))

#import <simd/simd.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebKit, ARKit);

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, ARKit, ARQuickLookPreviewItem);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, ARKit, ARSession);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, ARKit, ARWorldTrackingConfiguration);

SOFT_LINK_FUNCTION_FOR_SOURCE(WebKit, ARKit, ARMatrixMakeLookAt, simd_float4x4, (simd_float3 origin, simd_float3 direction), (origin, direction));

#if (PLATFORM(IOS) || PLATFORM(VISION)) && USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/ARKitSoftLinkAdditions.mm>)
#import <WebKitAdditions/ARKitSoftLinkAdditions.mm>
#endif

#endif
