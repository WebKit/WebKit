/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import <UIKit/UIKit.h>
#import <stdlib.h>
#import <wtf/RetainPtr.h>

static bool loadComplete = false;
static bool loadFailed = false;

@interface RenderInContextWebViewDelegate : NSObject <UIWebViewDelegate>
@end

@implementation RenderInContextWebViewDelegate

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webViewDidFinishLoad:(UIWebView *)webView
IGNORE_WARNINGS_END
{
    loadComplete = true;
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webView:(UIWebView *)webView didFailLoadWithError:(NSError *)error
IGNORE_WARNINGS_END
{
    loadComplete = true;
    loadFailed = true;
}

@end

namespace TestWebKitAPI {

static NSInteger getPixelIndex(NSInteger x, NSInteger y, NSInteger width)
{
    return (y * width + x) * 4;
}

TEST(WebKitLegacy, RenderInContextSnapshot)
{
    const NSInteger width = 800;
    const NSInteger height = 600;
    
    RetainPtr<UIWindow> uiWindow = adoptNS([[UIWindow alloc] initWithFrame:NSMakeRect(0, 0, width, height)]);
    RetainPtr<UIWebView> uiWebView = adoptNS([[UIWebView alloc] initWithFrame:NSMakeRect(0, 0, width, height)]);
    [uiWindow addSubview:uiWebView.get()];
    
    RetainPtr<RenderInContextWebViewDelegate> uiDelegate = adoptNS([[RenderInContextWebViewDelegate alloc] init]);
    uiWebView.get().delegate = uiDelegate.get();
    
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"large-red-square-image" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"];
    NSLog(@"Loading %@", url);
    [uiWebView loadRequest:[NSURLRequest requestWithURL:url]];
    
    Util::run(&loadComplete);
    
    EXPECT_TRUE(loadComplete);
    EXPECT_FALSE(loadFailed);
    
    unsigned char* pixelBuffer = static_cast<unsigned char*>(calloc(width * height, 4));
    
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> context = CGBitmapContextCreate(pixelBuffer, width, height, 8, 4 * width, colorSpace.get(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    
    // Flip the context
    CGContextScaleCTM(context.get(), 1, -1);
    CGContextTranslateCTM(context.get(), 0, -height);
    
    [uiWebView setFrame:CGRectMake(0, 0, 801, 601)]; // This resize stops Core Animation from using cached layer backing store
    [[uiWebView layer] renderInContext: context.get()];
    
    // There should be a red square at 0,0 - 100x100
    NSInteger pixelIndex = getPixelIndex(1, 1, width);
    unsigned char value = pixelBuffer[pixelIndex];
    EXPECT_EQ(255, value);
    EXPECT_EQ(0, pixelBuffer[pixelIndex + 1]);
    EXPECT_EQ(0, pixelBuffer[pixelIndex + 2]);
    
    // Right-bottom corner of the div (0, 0, 100, 100)
    pixelIndex = getPixelIndex(98, 98, width);
    EXPECT_EQ(255, pixelBuffer[pixelIndex]);
    EXPECT_EQ(0, pixelBuffer[pixelIndex + 1]);
    EXPECT_EQ(0, pixelBuffer[pixelIndex + 2]);
    
    // Outside the div (0, 0, 100, 100)
    pixelIndex = getPixelIndex(101, 101, width);
    EXPECT_EQ(255, pixelBuffer[pixelIndex]);
    EXPECT_EQ(255, pixelBuffer[pixelIndex + 1]);
    EXPECT_EQ(255, pixelBuffer[pixelIndex + 2]);
    
    free(pixelBuffer);
}

}

#endif

