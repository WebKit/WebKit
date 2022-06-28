/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(VK_IMAGE_ANALYSIS)

#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, VisionKitCore)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKImageAnalyzer)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKImageAnalyzerRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCImageAnalyzer)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCImageAnalyzerRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCImageAnalysis)
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCImageAnalysisInteraction)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCImageAnalysisOverlayView)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, VisionKitCore, vk_cgImageRemoveBackground, void, (CGImageRef image, BOOL crop, VKCGImageRemoveBackgroundCompletion completion), (image, crop, completion))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, VisionKitCore, vk_cgImageRemoveBackgroundWithDownsizing, void, (CGImageRef image, BOOL canDownsize, BOOL cropToFit, void(^completion)(CGImageRef, NSError *)), (image, canDownsize, cropToFit, completion))
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCRemoveBackgroundRequestHandler)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCRemoveBackgroundRequest)
SOFT_LINK_CLASS_FOR_HEADER(PAL, VKCRemoveBackgroundResult)
#endif

#endif // HAVE(VK_IMAGE_ANALYSIS)
