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

#import "InstanceMethodSwizzler.h"
#import <pal/spi/cocoa/VisionKitCoreSPI.h>
#import <wtf/RetainPtr.h>

@class VKImageAnalysis;
@class VKQuad;
@class VKWKTextInfo;
@class VKWKLineInfo;

namespace TestWebKitAPI {

RetainPtr<VKImageAnalysis> createImageAnalysisWithSimpleFixedResults();

RetainPtr<VKQuad> createQuad(CGPoint topLeft, CGPoint topRight, CGPoint bottomLeft, CGPoint bottomRight);
RetainPtr<VKWKTextInfo> createTextInfo(NSString *text, VKQuad *);
RetainPtr<VKWKLineInfo> createLineInfo(NSString *text, VKQuad *, NSArray<VKWKTextInfo *> *);
RetainPtr<VKImageAnalysis> createImageAnalysis(NSArray<VKWKLineInfo *> *);
RetainPtr<VKImageAnalyzerRequest> createRequest(CGImageRef, VKImageOrientation, VKAnalysisTypes);

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

class RemoveBackgroundSwizzler {
public:
    RemoveBackgroundSwizzler(CGImageRef, CGRect);
    ~RemoveBackgroundSwizzler() = default;

private:
    InstanceMethodSwizzler m_removeBackgroundRequestSwizzler;
};

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

} // namespace TestWebKitAPI

#endif // HAVE(VK_IMAGE_ANALYSIS)
