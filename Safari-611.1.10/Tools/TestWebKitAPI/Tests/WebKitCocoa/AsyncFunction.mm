/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKContentWorld.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

namespace TestWebKitAPI {

TEST(AsyncFunction, Basic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    NSError *error;

    // Function with no return value.
    // Returns JavaScript undefined which translates to nil.
    id result = [webView objectByCallingAsyncFunction:@"1" withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_EQ(result, nil);

    // Function returns explicit null.
    // Returns JavaScript null which translates to NSNull.
    result = [webView objectByCallingAsyncFunction:@"return null" withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSNull class]]);

    // Function returns a number.
    result = [webView objectByCallingAsyncFunction:@"return 1" withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
    EXPECT_TRUE([result isEqualToNumber:@1]);

    // Function returns a string.
    result = [webView objectByCallingAsyncFunction:@"return '1'" withArguments:nil error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSString class]]);
    EXPECT_TRUE([result isEqualToString:@"1"]);

    // Takes multiple arguments.
    result = [webView objectByCallingAsyncFunction:@"return a + b" withArguments:@{ @"a" : @40, @"b" : @2 } error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
    EXPECT_TRUE([result isEqualToNumber:@42]);

    // Takes multiple arguments of different types, follows javascripts "type appending" rules.
    result = [webView objectByCallingAsyncFunction:@"return foo + bar" withArguments:@{ @"foo" : @"foo", @"bar" : @42 } error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([result isKindOfClass:[NSString class]]);
    EXPECT_TRUE([result isEqualToString:@"foo42"]);

    // Invalid JavaScript, should return an error.
    result = [webView objectByCallingAsyncFunction:@"retunr null" withArguments:nil error:&error];
    EXPECT_FALSE(error == nil);
    EXPECT_NULL(result);
}

TEST(AsyncFunction, InvalidArguments)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    NSError *error;

    // Values can only be NSString, NSNumber, NSDate, NSNull, NS(Mutable)Array, NS(Mutable)Dictionary, NSNull, and may only contain those 6 types.
    id result = [webView objectByCallingAsyncFunction:@"return 1" withArguments:@{ @"a" : [NSData data] } error:&error];
    EXPECT_NULL(result);
    EXPECT_FALSE(error == nil);

    result = [webView objectByCallingAsyncFunction:@"return 1" withArguments:@{ @"a" : @[ @1, [NSData data] ] } error:&error];
    EXPECT_NULL(result);
    EXPECT_FALSE(error == nil);

    // References an argument that was not provided.
    result = [webView objectByCallingAsyncFunction:@"return a + b" withArguments:@{ @"a" : @40 } error:&error];
    EXPECT_NULL(result);
    EXPECT_FALSE(error == nil);
}

TEST(AsyncFunction, RoundTrip)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    NSError *error;

    // Tests round tripping a whole bunch of argument inputs and verifying the result.

    id value = @42;
    id arguments = @{ @"a" : value };
    id result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = [NSNull null];
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = @"Foo";
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = [NSDate dateWithTimeIntervalSinceReferenceDate:0];
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = @[ @1, @2 ];
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    NSMutableArray *mutableArray = [[NSMutableArray alloc] init];
    [mutableArray addObject:value];
    arguments = @{ @"a" : mutableArray };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([mutableArray isEqual:result]);
    [mutableArray release];

    value = @[ @"foo", [NSDate dateWithTimeIntervalSinceReferenceDate:0] ];
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = @{ @"a" : @1, @"b" : @2 };
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    NSMutableDictionary<NSString *, id> *mutableDictionary = [[NSMutableDictionary alloc] init];
    mutableDictionary[@"foo"] = value;
    arguments = @{ @"a" : mutableDictionary };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([mutableDictionary isEqual:result]);
    [mutableDictionary release];

    value = @{ @"a" : @"foo", @"b" : [NSDate dateWithTimeIntervalSinceReferenceDate:0] };
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);

    value = @{ @"a" : @{ @"a" : @1 } };
    arguments = @{ @"a" : value };
    result = [webView objectByCallingAsyncFunction:@"return a" withArguments:arguments error:&error];
    EXPECT_NULL(error);
    EXPECT_TRUE([value isEqual:result]);
}

TEST(AsyncFunction, Promise)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

    [webView callAsyncJavaScript:@"shouldn't crash" arguments:@{ @"invalidparameter" : webView.get() } inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:nil];

    NSString *functionBody = @"return new Promise(function(resolve, reject) { setTimeout(function(){ resolve(42) }, 0); })";

    bool done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([result isEqualToNumber:@42]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    done = false;

    functionBody = @"return new Promise(function(resolve, reject) { setTimeout(function(){ reject('Rejected!') }, 0); })";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE(error != nil);
        EXPECT_TRUE([[error description] containsString:@"Rejected!"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    functionBody = @"let p = new Proxy(function(resolve, reject) { setTimeout(function() { resolve(42); }, 0); }, { }); return { then: p };";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([result isEqualToNumber:@42]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    functionBody = @"let p = new Proxy(function(resolve, reject) { setTimeout(function() { reject('Rejected!'); }, 0); }, { }); return { then: p };";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(result);
        EXPECT_TRUE(error != nil);
        EXPECT_TRUE([[error description] containsString:@"Rejected!"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Verify we can await for a promise to be resolved before returning.
    functionBody = @"var r = 0; var p = new Promise(function(fulfill, reject) { setTimeout(function(){ r = 42; fulfill(); }, 5);}); await p; return r;";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSNumber class]]);
        EXPECT_TRUE([result isEqualToNumber:@42]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Returning an already resolved promise gives the value it was resolved with.
    functionBody = @"var p = new Promise(function(fulfill, reject) { setTimeout(function(){ fulfill('Fulfilled!') }, 5);}); await p; return p;";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"Fulfilled!"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Chaining thenables should work.
    functionBody = @"var p = new Promise(function (r) { r(new Promise(function (r) { r(42); })); }); await p; return 'Done';";

    done = false;
    [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
        EXPECT_NULL(error);
        EXPECT_TRUE([result isKindOfClass:[NSString class]]);
        EXPECT_TRUE([result isEqualToString:@"Done"]);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    // Promises known to become unreachable (e.g. via garbage collection) should call back with an error.
    done = false;
    functionBody = @"return new Promise(function(resolve, reject) { })";
    for (int i = 0; i < 500; ++i) {
        [webView callAsyncJavaScript:functionBody arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&] (id result, NSError *error) {
            EXPECT_NULL(result);
            EXPECT_TRUE(error != nil);
            EXPECT_TRUE([[error description] containsString:@"no longer reachable"]);
            done = true;
        }];
    }

    [webView.get().configuration.processPool _garbageCollectJavaScriptObjectsForTesting];
    TestWebKitAPI::Util::run(&done);
}

}

