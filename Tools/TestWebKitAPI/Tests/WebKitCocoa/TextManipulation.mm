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

#include "config.h"

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKTextManipulationConfiguration.h>
#import <WebKit/_WKTextManipulationDelegate.h>
#import <WebKit/_WKTextManipulationExclusionRule.h>
#import <WebKit/_WKTextManipulationItem.h>
#import <WebKit/_WKTextManipulationToken.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

static bool done = false;

@interface TextManipulationDelegate : NSObject <_WKTextManipulationDelegate>

- (void)_webView:(WKWebView *)webView didFindTextManipulationItem:(_WKTextManipulationItem *)item;

@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;

@end

@implementation TextManipulationDelegate {
    RetainPtr<NSMutableArray> _items;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _items = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView didFindTextManipulationItem:(_WKTextManipulationItem *)item
{
    [_items addObject:item];
}

- (NSArray<_WKTextManipulationItem *> *)items
{
    return _items.get();
}

@end

namespace TestWebKitAPI {

TEST(TextManipulation, StartTextManipulationExitEarlyWithoutDelegate)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body>hello<br>world<div>WebKit</div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ([delegate items].count, 0UL);
}

TEST(TextManipulation, StartTextManipulationFindSimpleParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body>hello<br>world<div>WebKit</div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 3UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);

    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("world", items[1].tokens[0].content.UTF8String);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_STREQ("WebKit", items[2].tokens[0].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationFindMultipleParagraphsInSingleTextNode)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><pre>hello\nworld\nWebKit</pre></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 3UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);

    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("world", items[1].tokens[0].content.UTF8String);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_STREQ("WebKit", items[2].tokens[0].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationFindParagraphsWithMultileTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body>hello,  <b>world</b><br><div><em> <b>Web</b>Kit</em>  </div></body></html>"];
    
    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    
    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    
    EXPECT_EQ(items[1].tokens.count, 2UL);
    EXPECT_STREQ("Web", items[1].tokens[0].content.UTF8String);
    EXPECT_STREQ("Kit", items[1].tokens[1].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationApplySingleExcluionRuleForElement)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body>Here's some code:<code>function <span>F</span>() { }</code>.</body></html>"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forElement:@"code"] autorelease],
    ]];

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 5UL);
    auto* tokens = items[0].tokens;
    EXPECT_STREQ("Here's some code:", tokens[0].content.UTF8String);
    EXPECT_FALSE(tokens[0].isExcluded);
    EXPECT_STREQ("function ", tokens[1].content.UTF8String);
    EXPECT_TRUE(tokens[1].isExcluded);
    EXPECT_STREQ("F", tokens[2].content.UTF8String);
    EXPECT_TRUE(tokens[2].isExcluded);
    EXPECT_STREQ("() { }", tokens[3].content.UTF8String);
    EXPECT_TRUE(tokens[3].isExcluded);
    EXPECT_STREQ(".", tokens[4].content.UTF8String);
    EXPECT_FALSE(tokens[4].isExcluded);
}

TEST(TextManipulation, StartTextManipulationApplyInclusionExclusionRulesForAttributes)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><span data-exclude=Yes><b>hello, </b><span data-exclude=NO>world</span></span></body></html>"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forAttribute:@"data-exclude" value:@"yes"] autorelease],
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)NO forAttribute:@"data-exclude" value:@"no"] autorelease],
    ]];

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[0].isExcluded);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[1].isExcluded);
}

struct Token {
    NSString *identifier;
    NSString *content;
};

template<size_t length>
static RetainPtr<_WKTextManipulationItem> createItem(NSString *itemIdentifier, const Token (&tokens)[length])
{
    RetainPtr<NSMutableArray> wkTokens = adoptNS([[NSMutableArray alloc] init]);
    for (size_t i = 0; i < length; i++) {
        RetainPtr<_WKTextManipulationToken> token = adoptNS([[_WKTextManipulationToken alloc] init]);
        [token setIdentifier: tokens[i].identifier];
        [token setContent: tokens[i].content];
        [wkTokens addObject:token.get()];
    }
    return adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:itemIdentifier tokens:wkTokens.get()]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceSimpleParagraphContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p>helllo, wooorld</p><p> hey, <b> Kits</b> is <em>cuuute</em></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("helllo, wooorld", items[0].tokens[0].content.UTF8String);

    EXPECT_EQ(items[1].tokens.count, 4UL);
    EXPECT_STREQ("hey, ", items[1].tokens[0].content.UTF8String);
    EXPECT_STREQ("Kits", items[1].tokens[1].content.UTF8String);
    EXPECT_STREQ(" is ", items[1].tokens[2].content.UTF8String);
    EXPECT_STREQ("cuuute", items[1].tokens[3].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[0].identifier, @"Hello, " },
        { items[1].tokens[1].identifier, @"kittens" },
        { items[1].tokens[2].identifier, @" are " },
        { items[1].tokens[3].identifier, @"cute" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>helllo, wooorld</p><p>Hello, <b>kittens</b> are <em>cute</em></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, world." },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, world.</p><p>Hello, <b>kittens</b> are <em>cute</em></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenContentIsChanged)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p> what <em>time</em> are they now?</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("what ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("time", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(" are they now?", items[0].tokens[2].content.UTF8String);

    [webView stringByEvaluatingJavaScript:@"document.querySelector('em').nextSibling.data = ' is it now in London?'"];

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"What " },
        { items[0].tokens[1].identifier, @"time" },
        { items[0].tokens[2].identifier, @" is it now?" },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p> what <em>time</em> is it now in London?</p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenDocumentHasBeenNavigatedAway)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hey, <em>earth</em></p>'"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hey, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("earth", items[0].tokens[1].content.UTF8String);

    [webView synchronouslyLoadTestPageNamed:@"copy-html"];
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hey, <em>earth</em></p>'"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, " },
        { items[0].tokens[1].identifier, @"world" },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenExclusionIsViolated)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hi, <em>WebKitten</em></p>'"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forElement:@"p"] autorelease],
    ]];

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hi, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("WebKitten", items[0].tokens[1].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[1].identifier, @"WebKit" },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hi, <em>WebKitten</em></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

} // namespace TestWebKitAPI

