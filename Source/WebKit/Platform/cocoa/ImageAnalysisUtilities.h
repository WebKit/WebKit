/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#if ENABLE(IMAGE_ANALYSIS)

#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/CompletionHandler.h>
#import <wtf/RetainPtr.h>

OBJC_CLASS NSData;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
using CocoaImageAnalysis = VKCImageAnalysis;
using CocoaImageAnalyzer = VKCImageAnalyzer;
using CocoaImageAnalyzerRequest = VKCImageAnalyzerRequest;
#else
using CocoaImageAnalysis = VKImageAnalysis;
using CocoaImageAnalyzer = VKImageAnalyzer;
using CocoaImageAnalyzerRequest = VKImageAnalyzerRequest;
#endif

namespace WebCore {
struct TextRecognitionResult;
}

namespace WebKit {

bool isLiveTextAvailableAndEnabled();
bool textRecognitionEnhancementsSystemFeatureEnabled();
bool imageAnalysisQueueSystemFeatureEnabled();
bool isImageAnalysisMarkupSystemFeatureEnabled();
bool canStartImageAnalysis(NSString *);

WebCore::TextRecognitionResult makeTextRecognitionResult(CocoaImageAnalysis *);

RetainPtr<CocoaImageAnalyzer> createImageAnalyzer();
RetainPtr<CocoaImageAnalyzerRequest> createImageAnalyzerRequest(CGImageRef, VKAnalysisTypes);

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
void requestImageAnalysisWithIdentifiers(CocoaImageAnalyzer *, NSURL *, const String& source, const String& target, CGImageRef, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&);
void requestImageAnalysisMarkup(CGImageRef, CompletionHandler<void(CGImageRef, CGRect)>&&);

std::pair<RetainPtr<NSData>, RetainPtr<CFStringRef>> imageDataForCroppedImageResult(CGImageRef, const String& sourceMIMEType);

#if PLATFORM(IOS_FAMILY)
using PlatformImageAnalysisObject = VKCImageAnalysisInteraction;
#else
using PlatformImageAnalysisObject = VKCImageAnalysisOverlayView;
#endif
void setUpAdditionalImageAnalysisBehaviors(PlatformImageAnalysisObject *);
#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

}

#endif // ENABLE(IMAGE_ANALYSIS)
