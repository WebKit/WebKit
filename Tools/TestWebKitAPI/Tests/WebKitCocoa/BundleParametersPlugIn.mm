/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextController.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <wtf/RetainPtr.h>

#if WK_HAVE_C_SPI
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePageLoaderClient.h>
#import <WebKit/WKObjCTypeWrapperRef.h>
#import <WebKit/WKType.h>
#endif

static NSString * const testParameter1 = @"TestParameter1";
static NSString * const testParameter2 = @"TestParameter2";

@interface BundleParametersPlugIn : NSObject <WKWebProcessPlugIn>
@end

@implementation BundleParametersPlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<WKWebProcessPlugInController> _plugInController;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    ASSERT(!_browserContextController);
    ASSERT(!_plugInController);
    _browserContextController = browserContextController;
    _plugInController = plugInController;
    [plugInController.parameters addObserver:self forKeyPath:testParameter1 options:NSKeyValueObservingOptionInitial context:NULL];
    [plugInController.parameters addObserver:self forKeyPath:testParameter2 options:NSKeyValueObservingOptionInitial context:NULL];
    [plugInController.parameters addObserver:self forKeyPath:TestWebKitAPI::Util::TestPlugInClassNameParameter options:NSKeyValueObservingOptionInitial context:NULL];

#if WK_HAVE_C_SPI
    WKBundlePageLoaderClientV6 loaderClient = { };
    loaderClient.base.version = 6;
    loaderClient.willLoadDataRequest = [] (WKBundlePageRef page, WKURLRequestRef, WKDataRef, WKStringRef, WKStringRef, WKURLRef, WKTypeRef userData, const void*) {
        id data = WKObjCTypeWrapperGetObject((WKObjCTypeWrapperRef)userData);
        RELEASE_ASSERT([data isKindOfClass:NSData.class]);
    };
    WKBundlePageSetPageLoaderClient(static_cast<WKBundlePageRef>(browserContextController), &loaderClient.base);
#endif
}

- (void)dealloc
{
    [[_plugInController parameters] removeObserver:self forKeyPath:testParameter1];
    [[_plugInController parameters] removeObserver:self forKeyPath:testParameter2];
    [[_plugInController parameters] removeObserver:self forKeyPath:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    JSContext *jsContext = [[_browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]];
    [jsContext setObject:[object valueForKeyPath:keyPath] forKeyedSubscript:keyPath];
}

@end
