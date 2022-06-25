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

#import "WebCoreTestSupport.h"
#import <WebKit/WKBundleNodeHandlePrivate.h>
#import <WebKit/WKDOMDocument.h>
#import <WebKit/WKDOMElement.h>
#import <WebKit/WKDOMNodePrivate.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInNodeHandlePrivate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>

@interface DisableSpellcheckPlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation DisableSpellcheckPlugIn

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    browserContextController.loadDelegate = self;
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didClearWindowObjectForFrame:(WKWebProcessPlugInFrame *)frame inScriptWorld:(WKWebProcessPlugInScriptWorld *)scriptWorld
{
    WebCoreTestSupport::injectInternalsObject([frame jsContextForWorld:scriptWorld].JSGlobalContextRef);
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didFinishLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    auto document = [controller mainFrameDocument];
    auto inputElement1 = [document createElement:@"input"];
    [inputElement1 setAttribute:@"id" value:@"disabled"];
    [document.body appendChild:inputElement1];

    auto inputElement2 = [document createElement:@"input"];
    [inputElement2 setAttribute:@"id" value:@"enabled"];
    [document.body appendChild:inputElement2];

    auto inputElementHandle1 = [inputElement1 _copyBundleNodeHandleRef];
    WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled(inputElementHandle1, false);

    auto inputElementHandle2 = [inputElement2 _copyBundleNodeHandleRef];
    WKBundleNodeHandleSetHTMLInputElementSpellcheckEnabled(inputElementHandle2, true);
}

@end
