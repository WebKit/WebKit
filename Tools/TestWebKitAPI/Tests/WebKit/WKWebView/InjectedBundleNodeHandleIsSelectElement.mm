/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import <WebKit/WKDOMDocument.h>
#import <WebKit/WKDOMElement.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInNodeHandle.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>

@interface InjectedBundleNodeHandleIsSelectElement : NSObject <WKWebProcessPlugIn>
@end

@implementation InjectedBundleNodeHandleIsSelectElement

- (void)verifySelectElementForHTMLElementTag:(NSString *)htmlElementTag document:(WKDOMDocument *)document jsContext:(JSContext *)jsContext expectedResult:(BOOL)expectedResult failedElementTags:(NSMutableArray<NSString *> *)failedElementTags
{
    WKDOMElement *htmlElement = [document createElement:htmlElementTag];
    [document.body appendChild:htmlElement];

    NSString *scriptString = [NSString stringWithFormat:@"document.querySelector('%@')", htmlElementTag];
    JSValue *jsValue = [jsContext evaluateScript:scriptString];
    WKWebProcessPlugInNodeHandle * nodeHandle = [WKWebProcessPlugInNodeHandle nodeHandleWithJSValue:jsValue inContext:jsContext];
    if (nodeHandle.isSelectElement != expectedResult)
        [failedElementTags addObject:htmlElementTag];

    [document.body removeChild:htmlElement];
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    WKDOMDocument *document = [browserContextController mainFrameDocument];
    auto *jsContext = [[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]];

    NSMutableArray <NSString *> *failedElementTags = [NSMutableArray array];

    [self verifySelectElementForHTMLElementTag:@"select" document:document jsContext:jsContext expectedResult:YES failedElementTags:failedElementTags];
    [self verifySelectElementForHTMLElementTag:@"input" document:document jsContext:jsContext expectedResult:NO failedElementTags:failedElementTags];

    if (!failedElementTags.count) {
        [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:@"alert('isSelectElement success')"];
        return;
    }

    [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:[NSString stringWithFormat:@"alert('isSelectElement failed for %@')", [failedElementTags componentsJoinedByString:@", "]]];
}

@end
