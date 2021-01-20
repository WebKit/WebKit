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

#import "config.h"

#if PLATFORM(MAC)

#import <WebKit/WKBundleNodeHandlePrivate.h>
#import <WebKit/WKBundlePagePrivate.h>
#import <WebKit/WKDOMDocument.h>
#import <WebKit/WKDOMElement.h>
#import <WebKit/WKDOMNodePrivate.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>

@interface AutoFillAvailable : NSObject <WKWebProcessPlugIn>
@end

@implementation AutoFillAvailable

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    WKDOMDocument *document = [browserContextController mainFrameDocument];
    WKDOMElement *inputElement = [document createElement:@"input"];
    [[document body] appendChild:inputElement];
    WKBundleNodeHandleRef node = [inputElement _copyBundleNodeHandleRef];
    WKBundleNodeHandleSetHTMLInputElementAutoFillAvailable(node, true);
    [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:@"document.getElementsByTagName(\"input\")[0].focus()"];

    void* accessibilityElement = WKAccessibilityFocusedObject([browserContextController _bundlePageRef]);
    id autofillAvailable = [(id)accessibilityElement accessibilityAttributeValue:@"AXValueAutofillAvailable"];

    NSString *script = [autofillAvailable boolValue] ?  @"alert('autofill available')" : @"alert('autofill not available')";
    [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:script];
}

@end

#endif // PLATFORM(MAC)
