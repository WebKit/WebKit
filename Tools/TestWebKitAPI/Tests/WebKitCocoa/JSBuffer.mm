/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "TestWKWebView.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKJSScriptingBuffer.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKJSBuffer.h>

TEST(JSBuffer, Data)
{
    static const char constantString[] = "Hello world!";

    RetainPtr oddLength = adoptNS([[WKJSScriptingBuffer alloc] initWithData:[NSData dataWithBytes:"abc" length:3]]);
    RetainPtr evenLength = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytes:"abcd" length:4]]);
    RetainPtr invalidSurrogatePair = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytes:"\x3d\xd8\x27\x00\xff\xff\x00\x00" length:8]]);
    RetainPtr readOnlyBuffer = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytesNoCopy:(void *)constantString length:sizeof(constantString)-1 freeWhenDone:NO]]);
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get().userContentController addBuffer:oddLength.get() name:@"oddLength" contentWorld:WKContentWorld.pageWorld];
    [configuration.get().userContentController _addBuffer:evenLength.get() contentWorld:WKContentWorld.pageWorld name:@"evenLength"];
    [configuration.get().userContentController _addBuffer:invalidSurrogatePair.get() contentWorld:WKContentWorld.pageWorld name:@"invalidSurrogatePair"];
    [configuration.get().userContentController _addBuffer:readOnlyBuffer.get() contentWorld:WKContentWorld.pageWorld name:@"readOnlyBuffer"];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.webkit.buffers.oddLength.asLatin1String()"], "abc");
    EXPECT_WK_STREQ([webView objectByCallingAsyncFunction:@"try { return window.webkit.buffers.oddLength.asUTF16String() } catch(e) { return 'threw ' + e; }" withArguments:nil], "threw RangeError: Bad value");

    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.webkit.buffers.evenLength.asLatin1String()"], "abcd");
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.webkit.buffers.evenLength.asUTF16String()"], @"\u6261\u6463");

    NSArray *actual = [webView objectByCallingAsyncFunction:@"let s = window.webkit.buffers.invalidSurrogatePair.asUTF16String(); return [s.charCodeAt(0), s.charCodeAt(1), s.charCodeAt(2), s.charCodeAt(3), s.length]" withArguments:nil];
    NSArray *expected = @[
        @55357,
        @39,
        @65535,
        @0,
        @4
    ];
    EXPECT_TRUE([actual isEqual:expected]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.webkit.buffers.invalidSurrogatePair.asLatin1String()"] isEqualToString:@"\u003D\u00D8\u0027\0\u00FF\u00FF\0\0"]);

    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.webkit.buffers.readOnlyBuffer.asLatin1String()"] isEqualToString:@"Hello world!"]);
}

TEST(JSBuffer, IDLExposed)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitBuffer"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitBufferNamespace"] boolValue]);
}

TEST(JSBuffer, EvaluateScript)
{
    const uint8_t script[] = "didPass = true; 'PAS' + 'S'";
    RetainPtr sourceBuffer = adoptNS([[WKJSScriptingBuffer alloc] initWithData:[NSData dataWithBytes:script length:std::size(script) - 1]]);
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);

    RetainPtr contentWorldConfiguration = adoptNS([_WKContentWorldConfiguration new]);
    contentWorldConfiguration.get().name = @"nonMainWorld";
    RetainPtr world = [WKContentWorld _worldWithConfiguration:contentWorldConfiguration.get()];

    [configuration.get().userContentController addBuffer:sourceBuffer.get() name:@"sourceCode" contentWorld:world.get()];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.didPass"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.didPass" inFrame:nil inContentWorld:world.get()] boolValue]);
    EXPECT_WK_STREQ([webView objectByEvaluatingJavaScript:@"window.webkit.evaluateScript(window.webkit.buffers.sourceCode.asLatin1String())" inFrame:nil inContentWorld:world.get()], "PASS");
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"window.didPass"] boolValue]);
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"window.didPass" inFrame:nil inContentWorld:world.get()] boolValue]);
}
