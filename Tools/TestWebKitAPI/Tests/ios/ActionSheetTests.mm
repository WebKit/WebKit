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

#include "config.h"

#if PLATFORM(IOS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

@interface ActionSheetObserver : NSObject<WKUIDelegatePrivate>
@property (nonatomic) BOOL presentedActionSheet;
@end

@implementation ActionSheetObserver

- (BOOL)waitForActionSheetAfterBlock:(dispatch_block_t)block
{
    _presentedActionSheet = NO;
    block();
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (_presentedActionSheet)
            break;
    }
    return _presentedActionSheet;
}

- (NSArray *)_webView:(ActionSheetObserver *)webView actionsForElement:(_WKActivatedElementInfo *)element defaultActions:(NSArray<_WKElementAction *> *)defaultActions
{
    _presentedActionSheet = YES;
    return defaultActions;
}

@end

namespace TestWebKitAPI {

class IPadUserInterfaceSwizzler {
public:
    IPadUserInterfaceSwizzler()
        : m_swizzler([UIDevice class], @selector(userInterfaceIdiom), reinterpret_cast<IMP>(padUserInterfaceIdiom))
    {
    }
private:
    static UIUserInterfaceIdiom padUserInterfaceIdiom()
    {
        return UIUserInterfaceIdiomPad;
    }
    InstanceMethodSwizzler m_swizzler;
};

TEST(ActionSheetTests, ImageMapDoesNotDestroySelection)
{
    IPadUserInterfaceSwizzler iPadUserInterface;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)]);
    auto observer = adoptNS([[ActionSheetObserver alloc] init]);
    [webView setUIDelegate:observer.get()];
    [webView synchronouslyLoadTestPageNamed:@"image-map"];
    [webView stringByEvaluatingJavaScript:@"selectTextNode(h1.childNodes[0])"];

    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
    [observer waitForActionSheetAfterBlock:^() {
        [webView _simulateLongPressActionAtLocation:CGPointMake(200, 200)];
    }];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"getSelection().toString()"]);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS)
