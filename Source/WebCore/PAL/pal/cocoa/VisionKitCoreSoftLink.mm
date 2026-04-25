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

#import "config.h"

#if HAVE(VK_IMAGE_ANALYSIS)

#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE_WITH_EXPORT(PAL, VisionKitCore, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKImageAnalyzer, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKImageAnalyzerRequest, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCImageAnalyzer, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCImageAnalyzerRequest, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCImageAnalysis, PAL_EXPORT, true)
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCImageAnalysisInteraction, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCImageAnalysisOverlayView, PAL_EXPORT, true)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, VisionKitCore, vk_cgImageRemoveBackground, void, (CGImageRef image, BOOL crop, VKCGImageRemoveBackgroundCompletion completion), (image, crop, completion), PAL_EXPORT)
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE_WITH_EXPORT(PAL, VisionKitCore, vk_cgImageRemoveBackgroundWithDownsizing, void, (CGImageRef image, BOOL canDownsize, BOOL cropToFit, void(^completion)(CGImageRef, NSError *)), (image, canDownsize, cropToFit, completion), PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCRemoveBackgroundRequestHandler, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCRemoveBackgroundRequest, PAL_EXPORT, true)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT_AND_IS_OPTIONAL(PAL, VisionKitCore, VKCRemoveBackgroundResult, PAL_EXPORT, true)
#endif

#endif // HAVE(VK_IMAGE_ANALYSIS)
