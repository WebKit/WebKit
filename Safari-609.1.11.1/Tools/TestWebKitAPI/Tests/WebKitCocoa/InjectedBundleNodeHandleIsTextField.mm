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
#import <WebKit/WKDOMNodePrivate.h>
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInNodeHandlePrivate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>

@interface InjectedBundleNodeHandleIsTextField : NSObject <WKWebProcessPlugIn>
@end

@implementation InjectedBundleNodeHandleIsTextField

- (void)verifyTextFieldForHTMLInputType:(NSString *)htmlInputType document:(WKDOMDocument *)document jsContext:(JSContext *)jsContext expectedResult:(BOOL)expectedResult failedInputTypes:(NSMutableArray <NSString *> *)failedInputTypes
{
    WKDOMElement *inputElement = [document createElement:@"input"];
    [inputElement setAttribute:@"type" value:htmlInputType];
    [[document body] appendChild:inputElement];

    auto *jsValue = [jsContext evaluateScript:@"document.querySelector('input')"];
    auto* nodeHandle = [WKWebProcessPlugInNodeHandle nodeHandleWithJSValue:jsValue inContext:jsContext];
    if ([nodeHandle isTextField] != expectedResult)
        [failedInputTypes addObject:htmlInputType];

    [[document body] removeChild:inputElement];
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    WKDOMDocument *document = [browserContextController mainFrameDocument];
    auto *jsContext = [[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]];

    NSMutableArray <NSString *> *failedInputTypes = [NSMutableArray array];

    [self verifyTextFieldForHTMLInputType:@"date" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"datetime" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"datetime-local" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"email" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"month" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"number" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"password" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"search" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"tel" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"text" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"time" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"url" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"week" document:document jsContext:jsContext expectedResult:YES failedInputTypes:failedInputTypes];

    [self verifyTextFieldForHTMLInputType:@"button" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
#if ENABLE_INPUT_TYPE_COLOR
    [self verifyTextFieldForHTMLInputType:@"color" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
#endif
    [self verifyTextFieldForHTMLInputType:@"file" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"hidden" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"image" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"radio" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"range" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"reset" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];
    [self verifyTextFieldForHTMLInputType:@"submit" document:document jsContext:jsContext expectedResult:NO failedInputTypes:failedInputTypes];

    if (!failedInputTypes.count) {
        [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:@"alert('isTextField success')"];
        return;
    }

    [[[browserContextController mainFrame] jsContextForWorld:[WKWebProcessPlugInScriptWorld normalWorld]] evaluateScript:[NSString stringWithFormat:@"alert('isTextField failed for %@')", [failedInputTypes componentsJoinedByString:@", "]]];
}

@end
