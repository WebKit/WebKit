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
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/_WKJSBuffer.h>

TEST(JSBuffer, Data)
{
    RetainPtr oddLength = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytes:"abc" length:3]]);
    RetainPtr evenLength = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytes:"abcd" length:4]]);
    RetainPtr invalidSurrogatePair = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytes:"\x3d\xd8\x27\x00\xff\xff\x00\x00" length:8]]);
    RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
    [configuration.get().userContentController _addBuffer:oddLength.get() contentWorld:WKContentWorld.pageWorld name:@"oddLength"];
    [configuration.get().userContentController _addBuffer:evenLength.get() contentWorld:WKContentWorld.pageWorld name:@"evenLength"];
    [configuration.get().userContentController _addBuffer:invalidSurrogatePair.get() contentWorld:WKContentWorld.pageWorld name:@"invalidSurrogatePair"];
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
}

TEST(JSBuffer, IDLExposed)
{
    RetainPtr webView = adoptNS([TestWKWebView new]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitBuffer"] boolValue]);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!window.WebKitBufferNamespace"] boolValue]);
}

TEST(JSBuffer, EvaluateJavaScriptFromBuffer)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    {
        RetainPtr jsString8 = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytesNoCopy:const_cast<char*>("1+2+3+4") length:7 freeWhenDone:NO]]);
        RetainPtr<NSString> evalResult;
        bool callbackComplete = false;
        [webView _evaluateJavaScriptFromBuffer:jsString8.get() withEncoding:_WKJSBufferStringEncodingLatin1 withSourceURL:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:YES completionHandler:[&] (id result, NSError *error) {
            evalResult = [NSString stringWithFormat:@"%@", result];
            callbackComplete = true;
        }];
        TestWebKitAPI::Util::run(&callbackComplete);
        EXPECT_WK_STREQ(evalResult.get(), "10");
    }
    {
        RetainPtr jsString16 = adoptNS([[_WKJSBuffer alloc] initWithData:[NSData dataWithBytesNoCopy:const_cast<char16_t*>(u"22+33") length:10 freeWhenDone:NO]]);
        RetainPtr<NSString> evalResult;
        bool callbackComplete = false;
        [webView _evaluateJavaScriptFromBuffer:jsString16.get() withEncoding:_WKJSBufferStringEncodingUniChar withSourceURL:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:YES completionHandler:[&] (id result, NSError *error) {
            evalResult = [NSString stringWithFormat:@"%@", result];
            callbackComplete = true;
        }];
        TestWebKitAPI::Util::run(&callbackComplete);
        EXPECT_WK_STREQ(evalResult.get(), "55");
    }
    {
        bool hadError = false;
        bool callbackComplete = false;
        [webView _evaluateJavaScriptFromBuffer:nil withEncoding:_WKJSBufferStringEncodingUniChar withSourceURL:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:YES completionHandler:[&] (id result, NSError *error) {
            hadError = !!error;
            callbackComplete = true;
        }];
        TestWebKitAPI::Util::run(&callbackComplete);
        EXPECT_TRUE(hadError);
    }
}

TEST(JSBuffer, StringConstructor)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero]);
    {
        _WKJSBufferStringEncoding encoding = _WKJSBufferStringEncodingUniChar;
        RetainPtr jsString8 = adoptNS([[_WKJSBuffer alloc] initWithString:@"'a'+'b'" resultStringEncoding:&encoding]);
        EXPECT_EQ(encoding, _WKJSBufferStringEncodingLatin1);
        RetainPtr<NSString> evalResult;
        bool callbackComplete = false;
        [webView _evaluateJavaScriptFromBuffer:jsString8.get() withEncoding:encoding withSourceURL:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:YES completionHandler:[&] (id result, NSError *error) {
            evalResult = [NSString stringWithFormat:@"%@", result];
            callbackComplete = true;
        }];
        TestWebKitAPI::Util::run(&callbackComplete);
        EXPECT_WK_STREQ(evalResult.get(), "ab");
    }
    {
        const unichar scriptChars[] = { 0x0027, 0x2022, 0x0027 }; // '<bullet>'.
        RetainPtr<NSString> script = [NSString stringWithCharacters:scriptChars length:1];
        _WKJSBufferStringEncoding encoding = _WKJSBufferStringEncodingLatin1;
        RetainPtr jsString16 = adoptNS([[_WKJSBuffer alloc] initWithString:script.get() resultStringEncoding:&encoding]);
        EXPECT_EQ(encoding, _WKJSBufferStringEncodingUniChar);
        RetainPtr<NSString> evalResult;
        bool callbackComplete = false;
        [webView _evaluateJavaScriptFromBuffer:jsString16.get() withEncoding:encoding withSourceURL:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld withUserGesture:YES completionHandler:[&] (id result, NSError *error) {
            evalResult = [NSString stringWithFormat:@"%@", result];
            callbackComplete = true;
        }];
        TestWebKitAPI::Util::run(&callbackComplete);
        const unichar ch = 0x2022;
        RetainPtr<NSString> bullet = [NSString stringWithCharacters:&ch length:1];
        EXPECT_WK_STREQ(evalResult.get(), bullet.get());
    }
}
