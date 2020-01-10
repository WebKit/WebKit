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

typedef void(^ItemCallback)(_WKTextManipulationItem *);

@interface TextManipulationDelegate : NSObject <_WKTextManipulationDelegate>

- (void)_webView:(WKWebView *)webView didFindTextManipulationItem:(_WKTextManipulationItem *)item;

@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;
@property (strong) ItemCallback itemCallback;

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
    if (self.itemCallback)
        self.itemCallback(item);
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

TEST(TextManipulation, StartTextManipulationFindNewlyInsertedParagraph)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hello</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    __block auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 3)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.body.appendChild(document.createElement('div')).innerHTML = 'world<br><b>Web</b>Kit';"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 3UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("world", items[1].tokens[0].content.UTF8String);
    EXPECT_EQ(items[2].tokens.count, 2UL);
    EXPECT_STREQ("Web", items[2].tokens[0].content.UTF8String);
    EXPECT_STREQ("Kit", items[2].tokens[1].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationFindNewlyDisplayedParagraph)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>"
        "<style> .hidden { display: none; } </style>"
        "<p>hello</p><div><span class='hidden'>Web</span><span class='hidden'>Kit</span></div><div class='hidden'>hey</div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    __block auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 2)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('span.hidden').forEach((span) => span.classList.remove('hidden'));"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[1].tokens.count, 2UL);
    EXPECT_STREQ("Web", items[1].tokens[0].content.UTF8String);
    EXPECT_STREQ("Kit", items[1].tokens[1].content.UTF8String);

    // This has to happen separately in order to have a deterministic ordering.
    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 3)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('div.hidden').forEach((div) => div.classList.remove('hidden'));"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 3UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_STREQ("hey", items[2].tokens[0].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationFindSameParagraphWithNewContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hello</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        done = true;
    };

    [webView stringByEvaluatingJavaScript:@"b = document.createElement('b');"
        "b.textContent = ' world';"
        "document.querySelector('p').appendChild(b); ''"];

    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[1].tokens.count, 2UL);
    EXPECT_STREQ("hello", items[1].tokens[0].content.UTF8String);
    EXPECT_STREQ(" world", items[1].tokens[1].content.UTF8String);
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

TEST(TextManipulation, StartTextManipulationApplyInclusionExclusionRulesForClass)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><span class='someClass exclude'>Message: <b>hello, </b><span>world</span></span></body></html>"];

    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:YES forClass:@"exclude"] autorelease],
    ]];

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("Message: ", items[0].tokens[0].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[0].isExcluded);
    EXPECT_STREQ("hello, ", items[0].tokens[1].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[1].isExcluded);
    EXPECT_STREQ("world", items[0].tokens[2].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[2].isExcluded);
}

TEST(TextManipulation, StartTextManipulationApplyInclusionExclusionRulesForClassAndAttribute)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><span class='someClass exclude'>Message: <b data-exclude=no>hello, </b><span>world</span></span></body></html>"];

    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:YES forAttribute:@"data-exclude" value:@"yes"] autorelease],
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:NO forAttribute:@"data-exclude" value:@"no"] autorelease],
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:YES forClass:@"exclude"] autorelease],
    ]];

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("Message: ", items[0].tokens[0].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[0].isExcluded);
    EXPECT_STREQ("hello, ", items[0].tokens[1].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[1].isExcluded);
    EXPECT_STREQ("world", items[0].tokens[2].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[2].isExcluded);
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

TEST(TextManipulation, CompleteTextManipulationReplaceSimpleSingleParagraph)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p>helllo, wooorld</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("helllo, wooorld", items[0].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, world" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hello, world</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationDisgardsTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p>hello, <b>world</b>. <i>WebKit</i></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 4UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(". ", items[0].tokens[2].content.UTF8String);
    EXPECT_STREQ("WebKit", items[0].tokens[3].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, " },
        { items[0].tokens[3].identifier, @"WebKit" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hello, <i>WebKit</i></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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

TEST(TextManipulation, CompleteTextManipulationReordersContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p><a href=\"https://en.wikipedia.org/wiki/Cat\">cats</a>, <i>I</i> are</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 4UL);
    EXPECT_STREQ("cats", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ(", ", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ("I", items[0].tokens[2].content.UTF8String);
    EXPECT_STREQ(" are", items[0].tokens[3].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[2].identifier, @"I" },
        { items[0].tokens[3].identifier, @"'m a " },
        { items[0].tokens[0].identifier, @"cat" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p><i>I</i>'m a <a href=\"https://en.wikipedia.org/wiki/Cat\">cat</a></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationCanSplitContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p id=\"paragraph\"><b class=\"hello-world\">hello world</b> WebKit</p></body></html>"];
    [webView stringByEvaluatingJavaScript:@"paragraph.firstChild.addEventListener('click', () => window.didClick = true)"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello world", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ(" WebKit", items[0].tokens[1].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello" },
        { items[0].tokens[1].identifier, @" WebKit " },
        { items[0].tokens[0].identifier, @"world" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p id=\"paragraph\"><b class=\"hello-world\">hello</b> WebKit <b class=\"hello-world\">world</b></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didClick = false; paragraph.firstChild.click(); didClick"].boolValue);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"didClick = false; paragraph.lastChild.click(); didClick"].boolValue);
}

TEST(TextManipulation, CompleteTextManipulationCanMergeContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p><b>hello <i>world</i> WebKit</b></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("hello ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(" WebKit", items[0].tokens[2].content.UTF8String);

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello " },
        { items[0].tokens[2].identifier, @"world" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p><b>hello world</b></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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

TEST(TextManipulation, CompleteTextManipulationFailWhenContentIsRemoved)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hello, world</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello, world", items[0].tokens[0].content.UTF8String);

    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = 'new content'"];

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hey" },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("new content", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forElement:@"em"] autorelease],
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
        { items[0].tokens[0].identifier, @"Hello," },
        { items[0].tokens[1].identifier, @"WebKit" },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hi, <em>WebKitten</em></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenExcludedContentAppearsMoreThanOnce)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hi, <em>WebKitten</em></p>'"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forElement:@"em"] autorelease],
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
        { items[0].tokens[1].identifier, nil },
        { items[0].tokens[0].identifier, @"Hello," },
        { items[0].tokens[1].identifier, nil },
    }).get() completion:^(BOOL success) {
        EXPECT_FALSE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hi, <em>WebKitten</em></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationPreservesExcludedContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hi, <em>WebKitten</em></p></body></html>"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    [configuration setExclusionRules:@[
        [[[_WKTextManipulationExclusionRule alloc] initExclusion:(BOOL)YES forElement:@"em"] autorelease],
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
        { items[0].tokens[0].identifier, @"Hello, " },
        { items[0].tokens[1].identifier, nil },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, <em>WebKitten</em></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationDoesNotCreateMoreTextManipulationItems)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>Foo <strong>bar</strong> baz</p></body></html>"];

    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);

    auto* firstItem = items.firstObject;
    EXPECT_EQ(firstItem.tokens.count, 3UL);

    __block BOOL foundNewItemAfterCompletingTextManipulation = false;
    [delegate setItemCallback:^(_WKTextManipulationItem *) {
        foundNewItemAfterCompletingTextManipulation = true;
    }];

    done = false;
    [webView _completeTextManipulation:(_WKTextManipulationItem *)createItem(firstItem.identifier, {
        { firstItem.tokens[0].identifier, @"bar " },
        { firstItem.tokens[1].identifier, @"garply" },
        { firstItem.tokens[2].identifier, @" foo" },
    }).get() completion:^(BOOL success) {
        EXPECT_TRUE(success);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(foundNewItemAfterCompletingTextManipulation);
    EXPECT_WK_STREQ("<p>bar <strong>garply</strong> foo</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, TextManipulationTokenDebugDescription)
{
    auto token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier:@"foo_is_the_identifier"];
    [token setContent:@"bar_is_the_content"];

    NSString *description = [token description];
    EXPECT_TRUE([description containsString:@"foo_is_the_identifier"]);
    EXPECT_FALSE([description containsString:@"bar_is_the_content"]);

    NSString *debugDescription = [token debugDescription];
    EXPECT_TRUE([debugDescription containsString:@"foo_is_the_identifier"]);
    EXPECT_TRUE([debugDescription containsString:@"bar_is_the_content"]);
}

TEST(TextManipulation, TextManipulationTokenNotEqualToNil)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    [tokenA setContent:@"A"];

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:nil includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:nil includingContentEquality:NO]);

    [tokenA setIdentifier:nil];
    [tokenA setContent:nil];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:nil includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:nil includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenEqualityWithEqualIdentifiers)
{
    auto token1 = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token1 setIdentifier:@"A"];
    auto token2 = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token2 setIdentifier:@"A"];

    EXPECT_TRUE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:YES]);
    EXPECT_TRUE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:NO]);

    // Same identifiers, different content.
    [token1 setContent:@"1"];
    [token2 setContent:@"2"];
    EXPECT_FALSE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:YES]);
    EXPECT_TRUE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:NO]);

    // Same identifiers, different exclusion.
    [token1 setExcluded:NO];
    [token2 setExcluded:YES];
    [token1 setContent:nil];
    [token2 setContent:nil];
    EXPECT_FALSE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:YES]);
    EXPECT_FALSE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:NO]);

    // Same identifiers, different exclusion and different content.
    [token1 setContent:@"1"];
    [token2 setContent:@"2"];
    EXPECT_FALSE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:YES]);
    EXPECT_FALSE([token1 isEqualToTextManipulationToken:token2.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenEqualityWithDifferentIdentifiers)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"B"];

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    // Different identifiers, same content.
    [tokenA setContent:@"content"];
    [tokenB setContent:@"content"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    // Different identifiers, same exclusion.
    [tokenA setContent:nil];
    [tokenB setContent:nil];
    [tokenA setExcluded:YES];
    [tokenB setExcluded:YES];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    // Different identifiers, same content and same exclusion.
    [tokenA setContent:@"content"];
    [tokenB setContent:@"content"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenEqualityWithNilIdentifiers)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    EXPECT_NULL([tokenA identifier]);
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"B"];
    auto tokenC = adoptNS([[_WKTextManipulationToken alloc] init]);
    EXPECT_NULL([tokenC identifier]);

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);

    // Equal content.
    [tokenA setContent:@"content"];
    [tokenB setContent:@"content"];
    [tokenC setContent:@"content"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);

    // Different content.
    [tokenA setContent:@"contentA"];
    [tokenB setContent:@"contentB"];
    [tokenC setContent:@"contentC"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenEqualityWithEmptyIdentifiers)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@""];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"B"];
    auto tokenC = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenC setIdentifier:@""];

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);

    // Equal content.
    [tokenA setContent:@"content"];
    [tokenB setContent:@"content"];
    [tokenC setContent:@"content"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);

    // Different content.
    [tokenA setContent:@"contentA"];
    [tokenB setContent:@"contentB"];
    [tokenC setContent:@"contentC"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenWithNilContent)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];

    EXPECT_NULL([tokenA content]);
    EXPECT_NULL([tokenB content]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    [tokenB setContent:@""];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    [tokenB setContent:@"B"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenWithEmptyContent)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    [tokenA setContent:@""];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];
    [tokenB setContent:@""];

    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    [tokenB setContent:nil];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);

    [tokenB setContent:@"B"];
    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenWithIdenticalContent)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    [tokenA setContent:@"content"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];
    [tokenB setContent:@"content"];

    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenWithPointerEqualContent)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];

    NSString *contentString = @"content";
    [tokenA setContent:contentString];
    [tokenB setContent:contentString];

    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenWithTrailingSpace)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    [tokenA setContent:@"content"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];
    [tokenB setContent:@"content "];

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationTokenEqualToSelf)
{
    auto token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier:@"A"];
    [token setContent:@"content"];

    EXPECT_TRUE([token isEqualToTextManipulationToken:token.get() includingContentEquality:YES]);
    EXPECT_TRUE([token isEqualToTextManipulationToken:token.get() includingContentEquality:NO]);
    EXPECT_TRUE([token isEqual:token.get()]);
}

TEST(TextManipulation, TextManipulationTokenNSObjectEqualityWithOtherToken)
{
    auto tokenA = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenA setIdentifier:@"A"];
    [tokenA setContent:@"content"];
    auto tokenB = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenB setIdentifier:@"A"];
    [tokenB setContent:@"content"];
    auto tokenC = adoptNS([[_WKTextManipulationToken alloc] init]);
    [tokenC setIdentifier:@"A"];
    [tokenC setContent:@"content "];

    EXPECT_TRUE([tokenA isEqualToTextManipulationToken:tokenB.get() includingContentEquality:YES]);
    EXPECT_TRUE([tokenA isEqual:tokenB.get()]);

    EXPECT_FALSE([tokenA isEqualToTextManipulationToken:tokenC.get() includingContentEquality:YES]);
    EXPECT_FALSE([tokenA isEqual:tokenC.get()]);
}

TEST(TextManipulation, TextManipulationTokenNSObjectEqualityWithNonToken)
{
    auto token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier:@"A"];
    [token setContent:@"content"];
    NSString *string = @"content";

    EXPECT_FALSE([token isEqual:string]);
    EXPECT_FALSE([token isEqual:nil]);
}

static RetainPtr<_WKTextManipulationToken> createTextManipulationToken(NSString *identifier, BOOL excluded, NSString *content)
{
    auto token = adoptNS([[_WKTextManipulationToken alloc] init]);
    [token setIdentifier:identifier];
    [token setExcluded:excluded];
    [token setContent:content];
    return token;
}

TEST(TextManipulation, TextManipulationItemDebugDescription)
{
    auto tokenA = createTextManipulationToken(@"public_identifier_A", NO, @"private_content_A");
    auto tokenB = createTextManipulationToken(@"public_identifier_B", NO, @"private_content_B");
    auto item = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"public_item_identifier" tokens:@[ tokenA.get(), tokenB.get() ]]);

    NSString *debugDescription = [item debugDescription];
    EXPECT_TRUE([debugDescription containsString:@"public_identifier_A"]);
    EXPECT_TRUE([debugDescription containsString:@"public_identifier_B"]);
    EXPECT_TRUE([debugDescription containsString:@"private_content_A"]);
    EXPECT_TRUE([debugDescription containsString:@"private_content_B"]);
    EXPECT_TRUE([debugDescription containsString:@"public_item_identifier"]);

    NSString *description = [item description];
    EXPECT_TRUE([description containsString:@"public_identifier_A"]);
    EXPECT_TRUE([description containsString:@"public_identifier_B"]);
    EXPECT_FALSE([description containsString:@"private_content_A"]);
    EXPECT_FALSE([description containsString:@"private_content_B"]);
    EXPECT_TRUE([description containsString:@"public_item_identifier"]);
}

TEST(TextManipulation, TextManipulationItemEqualityToNilItem)
{
    auto item = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ ]]);

    EXPECT_FALSE([item isEqualToTextManipulationItem:nil includingContentEquality:YES]);
    EXPECT_FALSE([item isEqualToTextManipulationItem:nil includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityToSelf)
{
    auto token = createTextManipulationToken(@"A", NO, @"token");
    auto item = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"B" tokens:@[ token.get() ]]);

    EXPECT_TRUE([item isEqualToTextManipulationItem:item.get() includingContentEquality:YES]);
    EXPECT_TRUE([item isEqualToTextManipulationItem:item.get() includingContentEquality:NO]);
    EXPECT_TRUE([item isEqual:item.get()]);
}

TEST(TextManipulation, TextManipulationItemBasicEquality)
{
    auto token1 = createTextManipulationToken(@"1", NO, @"token1");
    auto token2 = createTextManipulationToken(@"1", NO, @"token1");
    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token1.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token2.get() ]]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemBasicEqualityWithMultipleTokens)
{
    auto tokenA1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenA2 = createTextManipulationToken(@"2", NO, @"token2");
    auto tokenB1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenB2 = createTextManipulationToken(@"2", NO, @"token2");

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA1.get(), tokenA2.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB1.get(), tokenB2.get() ]]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualitySimilarTokensWithDifferentContent)
{
    auto token1 = createTextManipulationToken(@"1", NO, @"token1");
    auto token2 = createTextManipulationToken(@"1", NO, @"token2");
    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token1.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token2.get() ]]);

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithOutOfOrderTokens)
{
    auto tokenA1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenA2 = createTextManipulationToken(@"2", NO, @"token2");
    auto tokenB1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenB2 = createTextManipulationToken(@"2", NO, @"token2");

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA1.get(), tokenA2.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB2.get(), tokenB1.get() ]]);

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithPointerEqualTokens)
{
    auto token1 = createTextManipulationToken(@"1", NO, @"token1");
    auto token2 = createTextManipulationToken(@"2", NO, @"token2");

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token1.get(), token2.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token1.get(), token2.get() ]]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithPointerEqualTokenArrays)
{
    auto token1 = createTextManipulationToken(@"1", NO, @"token1");
    auto token2 = createTextManipulationToken(@"2", NO, @"token2");
    NSArray<_WKTextManipulationToken *> *tokens = @[ token1.get(), token2.get() ];

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:tokens]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:tokens]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithMismatchedTokenCounts)
{
    auto tokenA1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenA2 = createTextManipulationToken(@"2", NO, @"token2");
    auto tokenA3 = createTextManipulationToken(@"3", NO, @"token3");
    auto tokenB1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenB2 = createTextManipulationToken(@"2", NO, @"token2");

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA1.get(), tokenA2.get(), tokenA3.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB1.get(), tokenB2.get() ]]);

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);

    EXPECT_FALSE([itemB isEqualToTextManipulationItem:itemA.get() includingContentEquality:YES]);
    EXPECT_FALSE([itemB isEqualToTextManipulationItem:itemA.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithDifferentTokenIdentifiers)
{
    auto tokenA = createTextManipulationToken(@"A", NO, @"token");
    auto tokenB = createTextManipulationToken(@"B", NO, @"token");
    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB.get() ]]);

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithNilIdentifiers)
{
    auto token = createTextManipulationToken(@"A", NO, @"token");
    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:nil tokens:@[ token.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:nil tokens:@[ token.get() ]]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemEqualityWithDifferentTokenExclusions)
{
    auto tokenA = createTextManipulationToken(@"1", NO, @"token");
    auto tokenB = createTextManipulationToken(@"1", YES, @"token");
    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB.get() ]]);

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
}

TEST(TextManipulation, TextManipulationItemNSObjectEqualityWithOtherItem)
{
    auto tokenA1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenA2 = createTextManipulationToken(@"2", NO, @"token2");
    auto tokenB1 = createTextManipulationToken(@"1", NO, @"token1");
    auto tokenB2 = createTextManipulationToken(@"2", NO, @"token2");

    auto itemA = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenA1.get(), tokenA2.get() ]]);
    auto itemB = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ tokenB1.get(), tokenB2.get() ]]);

    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
    EXPECT_TRUE([itemA isEqual:itemB.get()]);

    [tokenB2 setContent:@"something else"];

    EXPECT_FALSE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:YES]);
    EXPECT_TRUE([itemA isEqualToTextManipulationItem:itemB.get() includingContentEquality:NO]);
    EXPECT_FALSE([itemA isEqual:itemB.get()]);
}

TEST(TextManipulation, TextManipulationItemNSObjectEqualityWithNonToken)
{
    auto token = createTextManipulationToken(@"1", NO, @"token1");
    auto item = adoptNS([[_WKTextManipulationItem alloc] initWithIdentifier:@"A" tokens:@[ token.get() ]]);
    NSString *string = @"content";

    EXPECT_FALSE([token isEqual:string]);
    EXPECT_FALSE([token isEqual:nil]);
}

} // namespace TestWebKitAPI

