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

#if ENABLE(IMAGE_ANALYSIS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/spi/cocoa/VisionKitCoreSPI.h>

namespace TestWebKitAPI {

#if PLATFORM(IOS_FAMILY)

static CGPoint swizzledLocationInView(id, SEL, UIView *)
{
    return CGPointMake(100, 100);
}

static bool gDidProcessRequest = false;
static void swizzledProcessRequest(id, SEL, VKImageAnalyzerRequest *, void (^)(double progress), void (^completion)(VKImageAnalysis *analysis, NSError *error))
{
    gDidProcessRequest = true;
    completion(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:1 userInfo:nil]);
}

TEST(ImageAnalysisTests, DoNotAnalyzeImagesInEditableContent)
{
    InstanceMethodSwizzler gestureLocationSwizzler { UIGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequest) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"image"];

    [webView _imageAnalysisGestureRecognizer].state = UIGestureRecognizerStateBegan;
    [webView waitForNextPresentationUpdate];
    EXPECT_FALSE(gDidProcessRequest);
}

TEST(ImageAnalysisTests, HandleImageAnalyzerError)
{
    InstanceMethodSwizzler gestureLocationSwizzler { UIGestureRecognizer.class, @selector(locationInView:), reinterpret_cast<IMP>(swizzledLocationInView) };
    InstanceMethodSwizzler imageAnalysisRequestSwizzler { PAL::getVKImageAnalyzerClass(), @selector(processRequest:progressHandler:completionHandler:), reinterpret_cast<IMP>(swizzledProcessRequest) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400)]);
    [webView synchronouslyLoadTestPageNamed:@"image"];

    [webView _imageAnalysisGestureRecognizer].state = UIGestureRecognizerStateBegan;
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(gDidProcessRequest);
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace TestWebKitAPI

#endif // ENABLE(IMAGE_ANALYSIS)
