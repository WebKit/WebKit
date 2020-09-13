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
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKTextManipulationConfiguration.h>
#import <WebKit/_WKTextManipulationDelegate.h>
#import <WebKit/_WKTextManipulationExclusionRule.h>
#import <WebKit/_WKTextManipulationItem.h>
#import <WebKit/_WKTextManipulationToken.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringConcatenate.h>
#import <wtf/text/StringConcatenateNumbers.h>

static bool done = false;

typedef void(^ItemCallback)(_WKTextManipulationItem *);
typedef void(^ItemListCallback)(NSArray<_WKTextManipulationItem *> *);

@interface TextManipulationDelegate : NSObject <_WKTextManipulationDelegate>

- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items;

@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;
@property (strong) ItemCallback itemCallback;
@property (strong) ItemListCallback itemListCallback;

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

- (void)_webView:(WKWebView *)webView didFindTextManipulationItems:(NSArray<_WKTextManipulationItem *> *)items
{
    [_items addObjectsFromArray:items];
    if (self.itemListCallback)
        self.itemListCallback(items);
    if (self.itemCallback) {
        for (_WKTextManipulationItem *item in items)
            self.itemCallback(item);
    }
}

- (NSArray<_WKTextManipulationItem *> *)items
{
    return _items.get();
}

@end

@interface LegacyTextManipulationDelegate : NSObject <_WKTextManipulationDelegate>

- (void)_webView:(WKWebView *)webView didFindTextManipulationItem:(_WKTextManipulationItem *)item;

@property (nonatomic, readonly, copy) NSArray<_WKTextManipulationItem *> *items;
@property (strong) ItemCallback itemCallback;

@end

@implementation LegacyTextManipulationDelegate {
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
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 5UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("\n", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[2].content.UTF8String);
    EXPECT_STREQ("\n", items[0].tokens[3].content.UTF8String);
    EXPECT_STREQ("WebKit", items[0].tokens[4].content.UTF8String);
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

TEST(TextManipulation, StartTextManipulationFindAttributeContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><head><title>hey</title></head>"
        "<body><div><span aria-label=\"this is greet\">hello</span><img src=\"apple.gif\" alt=\"fruit\"></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 4UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hey", items[0].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[0].isExcluded);

    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("this is greet", items[1].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[1].tokens[0].isExcluded);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_STREQ("fruit", items[2].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[2].tokens[0].isExcluded);

    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[3].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[3].tokens[0].isExcluded);
}

TEST(TextManipulation, StartTextManipulationSupportsLegacyDelegateCallback)
{
    auto delegate = adoptNS([[LegacyTextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hello, <span>world</span></p><p>WebKit</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    __block auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("WebKit", items[1].tokens[0].content.UTF8String);
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
        "<p>hello</p>"
        "<div>"
            "<span class='hidden'>Web</span>"
            "<span class='hidden'>Kit</span>"
        "</div>"
        "<div class='hidden'>hey</div>"
        "</body></html>"];

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
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ(" world", items[1].tokens[0].content.UTF8String);
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
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("world", items[0].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[0].isExcluded);
}

TEST(TextManipulation, StartTextManipulationApplyInclusionExclusionRulesForClass)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body>Message: <span class='someClass exclude'><b>hello, </b><span>world</span></span></body></html>"];

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
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("Message: ", items[0].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[0].isExcluded);
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
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[0].isExcluded);
}

TEST(TextManipulation, StartTextManipulationBreaksParagraphInBetweenListItems)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        li.block { display: block; }"
        "        li.float-left { float: left; margin: 1em; }"
        "        li.inline { display: inline; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <ul><li class='block'>One</li><li class='block'>Two<span>-three</span></li></ul>"
        "    <div><br></div>"
        "    <ul><li class='block float-left'>Four</li><li class='block float-left'>Five<span>-six</span></li></ul>"
        "    <div><br></div>"
        "    <ol><li class='inline'>Seven</li><li class='inline'>Eight</li></ol>"
        "    <div><br></div>"
        "    <ul><li>Nine</li><li>Ten</li></ol>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    NSArray<_WKTextManipulationItem *> *items = [delegate items];
    EXPECT_EQ(items.count, 7UL);

    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("One", items[0].tokens[0].content);

    EXPECT_EQ(items[1].tokens.count, 2UL);
    EXPECT_WK_STREQ("Two", items[1].tokens[0].content);
    EXPECT_WK_STREQ("-three", items[1].tokens[1].content);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_WK_STREQ("Four", items[2].tokens[0].content);

    EXPECT_EQ(items[3].tokens.count, 2UL);
    EXPECT_WK_STREQ("Five", items[3].tokens[0].content);
    EXPECT_WK_STREQ("-six", items[3].tokens[1].content);

    EXPECT_EQ(items[4].tokens.count, 2UL);
    EXPECT_WK_STREQ("Seven", items[4].tokens[0].content);
    EXPECT_WK_STREQ("Eight", items[4].tokens[1].content);

    EXPECT_EQ(items[5].tokens.count, 1UL);
    EXPECT_WK_STREQ("Nine", items[5].tokens[0].content);

    EXPECT_EQ(items[6].tokens.count, 1UL);
    EXPECT_WK_STREQ("Ten", items[6].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationIncludesFullyClippedText)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        div { overflow: hidden; width: 200px; height: 0; }"
        "        p { visibility: hidden; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <div><span>Hello</span> world</div>"
        "    <br>"
        "    <p>More text</p>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    NSArray<_WKTextManipulationItem *> *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_WK_STREQ("Hello", items[0].tokens[0].content);
    EXPECT_WK_STREQ(" world", items[0].tokens[1].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("More text", items[1].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationFindsInsertedClippedText)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        span { overflow: hidden; width: 200px; height: 0; }"
        "        p { visibility: hidden; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <div>hello, world</div>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 2)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"var beforeElement = document.createElement('span');"
        "beforeElement.innerHTML='before';"
        "document.querySelector('div').before(beforeElement);"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 3)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"var afterElement = document.createElement('p');"
        "afterElement.innerHTML='after';"
        "document.querySelector('div').after(afterElement)"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello, world", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("before", items[1].tokens[0].content);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_WK_STREQ("after", items[2].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationTreatsInlineBlockLinksAndButtonsAndSpansAsParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        a, span {"
        "            display: inline-block;"
        "            border: 1px blue solid;"
        "            margin-left: 1em;"
        "        }"
        "    </style>"
        "</head>"
        "<body>"
        "    <button>One</button><button>Two</button>"
        "    <div><br></div>"
        "    <a href='#'>Three</a><a href='#'>Four</a>"
        "    <span role='button'>Five</span>"
        "    <span>Six</span>"
        "    <b>End</b>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    NSArray<_WKTextManipulationItem *> *items = [delegate items];
    EXPECT_EQ(items.count, 7UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_EQ(items[4].tokens.count, 1UL);
    EXPECT_EQ(items[5].tokens.count, 1UL);
    EXPECT_EQ(items[6].tokens.count, 1UL);
    EXPECT_WK_STREQ("One", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Two", items[1].tokens[0].content);
    EXPECT_WK_STREQ("Three", items[2].tokens[0].content);
    EXPECT_WK_STREQ("Four", items[3].tokens[0].content);
    EXPECT_WK_STREQ("Five", items[4].tokens[0].content);
    EXPECT_WK_STREQ("Six", items[5].tokens[0].content);
    EXPECT_WK_STREQ("End", items[6].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationTreatsLinksInNavigationElementsAsParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        li { display: inline; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <div role='navigation'>"
        "        <ul>"
        "            <li><a href='#'>Foo</a></li>"
        "            <li><a href='#'>Bar</a></li>"
        "        </ul>"
        "    </div>"
        "    <nav>"
        "        <ul>"
        "            <li><a href='#'>Baz</a></li>"
        "            <li><a href='#'>Garply</a></li>"
        "        </ul>"
        "    </nav>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    NSArray<_WKTextManipulationItem *> *items = [delegate items];
    EXPECT_EQ(items.count, 4UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_WK_STREQ("Foo", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Bar", items[1].tokens[0].content);
    EXPECT_WK_STREQ("Baz", items[2].tokens[0].content);
    EXPECT_WK_STREQ("Garply", items[3].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationTreatsNestedInlineBlockListItemsAndLinksAsParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "    <style>"
        "        li, a { display: inline-block; margin: 2em; }"
        "    </style>"
        "</head>"
        "<body>"
        "    <ol>"
        "        <li>One<a href='#'>Two</a><a href='#'>Three</a>Four</li>"
        "    </ol>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 4UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_WK_STREQ("One", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Two", items[1].tokens[0].content);
    EXPECT_WK_STREQ("Three", items[2].tokens[0].content);
    EXPECT_WK_STREQ("Four", items[3].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationExtractsUserInfo)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "    <title>This is a test</title>"
        "    <p>First</p>"
        "    <div role='button'>Second</div>"
        "    <span>Third</span>"
        "    <div style='margin-top: 2000px;'>Fourth</div>"
        "    <script>scrollTo(0, 2000);</script>"
        "</body>"];

    [webView waitForNextPresentationUpdate];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 5UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_EQ(items[4].tokens.count, 1UL);
    EXPECT_WK_STREQ("This is a test", items[0].tokens[0].content);
    EXPECT_WK_STREQ("First", items[1].tokens[0].content);
    EXPECT_WK_STREQ("Second", items[2].tokens[0].content);
    EXPECT_WK_STREQ("Third", items[3].tokens[0].content);
    EXPECT_WK_STREQ("Fourth", items[4].tokens[0].content);
    {
        auto userInfo = items[0].tokens[0].userInfo;
        EXPECT_WK_STREQ("TestWebKitAPI.resources", [(NSURL *)userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] lastPathComponent]);
        EXPECT_WK_STREQ("TITLE", (NSString *)userInfo[_WKTextManipulationTokenUserInfoTagNameKey]);
        EXPECT_FALSE([userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] boolValue]);
    }
    {
        auto userInfo = items[1].tokens[0].userInfo;
        EXPECT_WK_STREQ("TestWebKitAPI.resources", [(NSURL *)userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] lastPathComponent]);
        EXPECT_WK_STREQ("P", (NSString *)userInfo[_WKTextManipulationTokenUserInfoTagNameKey]);
        EXPECT_FALSE([userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] boolValue]);
    }
    {
        auto userInfo = items[2].tokens[0].userInfo;
        EXPECT_WK_STREQ("TestWebKitAPI.resources", [(NSURL *)userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] lastPathComponent]);
        EXPECT_WK_STREQ("DIV", (NSString *)userInfo[_WKTextManipulationTokenUserInfoTagNameKey]);
        EXPECT_WK_STREQ("button", (NSString *)userInfo[_WKTextManipulationTokenUserInfoRoleAttributeKey]);
        EXPECT_FALSE([userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] boolValue]);
    }
    {
        auto userInfo = items[3].tokens[0].userInfo;
        EXPECT_WK_STREQ("TestWebKitAPI.resources", [(NSURL *)userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] lastPathComponent]);
        EXPECT_WK_STREQ("SPAN", (NSString *)userInfo[_WKTextManipulationTokenUserInfoTagNameKey]);
        EXPECT_FALSE([userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] boolValue]);
    }
    {
        auto userInfo = items[4].tokens[0].userInfo;
        EXPECT_WK_STREQ("TestWebKitAPI.resources", [(NSURL *)userInfo[_WKTextManipulationTokenUserInfoDocumentURLKey] lastPathComponent]);
        EXPECT_WK_STREQ("DIV", (NSString *)userInfo[_WKTextManipulationTokenUserInfoTagNameKey]);
        EXPECT_TRUE([userInfo[_WKTextManipulationTokenUserInfoVisibilityKey] boolValue]);
    }
}

TEST(TextManipulation, StartTextManipulationExtractsValuesFromButtonInputs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<input type='button' value='One'>"
        "<input type='submit' value='Two'>"
        "<input type='password' value='Three'>"
        "<input type='range' value='4'>"
        "<input type='date' value='2020-05-01'>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("One", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Two", items[1].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationExtractsValuesFromTextInputs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<input type='search' value='One'>"
        "<input type='text' value='Two'>"
        "<input id='a' type='search'>"
        "<input id='b' type='text'>"
        "<input type='number' value='6'>"
        "<input type='email' value='foo@bar.com'>"
        "<input type='url' value='https://www.apple.com'>"
        "<script>"
        "document.getElementById('a').focus();"
        "document.execCommand('InsertText', true, 'Three');"
        "document.getElementById('b').focus();"
        "document.execCommand('InsertText', true, 'Four');"
        "</script>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("One", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Two", items[1].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationExtractsVisibleLineBreaksInTextAsExcludedTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<span style='white-space:pre;'>&#10; one&#10;  two</span>"
        "<span style='white-space:pre;'>&#10;</span>"
        "<span>&#10; three&#10;  four&#10;</span>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 4UL);
    EXPECT_STREQ("\n ", items[0].tokens[0].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[0].isExcluded);
    EXPECT_STREQ("one", items[0].tokens[1].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[1].isExcluded);
    EXPECT_STREQ("\n  ", items[0].tokens[2].content.UTF8String);
    EXPECT_TRUE(items[0].tokens[2].isExcluded);
    EXPECT_STREQ("two", items[0].tokens[3].content.UTF8String);
    EXPECT_FALSE(items[0].tokens[3].isExcluded);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("three four", items[1].tokens[0].content.UTF8String);
}

TEST(TextManipulation, StartTextManipulationExtractsPrivateUseCharactersAsExcludedTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body>foobarbaz</body>"];
    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);

    auto* item = items.firstObject;
    EXPECT_EQ(item.tokens.count, 5UL);
    EXPECT_WK_STREQ("foo", item.tokens[0].content);
    EXPECT_FALSE(item.tokens[0].isExcluded);
    EXPECT_WK_STREQ("", item.tokens[1].content);
    EXPECT_TRUE(item.tokens[1].isExcluded);
    EXPECT_WK_STREQ("bar", item.tokens[2].content);
    EXPECT_FALSE(item.tokens[2].isExcluded);
    EXPECT_WK_STREQ("", item.tokens[3].content);
    EXPECT_TRUE(item.tokens[3].isExcluded);
    EXPECT_WK_STREQ("baz", item.tokens[4].content);
    EXPECT_FALSE(item.tokens[4].isExcluded);
}

TEST(TextManipulation, StartTextManipulationExtractsValuesByNode)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<span>one</span><span style='white-space:pre;'>two &#10; three</span><span>four</span>"
        "<span style='white-space:pre;'>&#10;five</span>"
        "<span>   six</span>        <span>seven</span>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 5UL);
    EXPECT_WK_STREQ("one", items[0].tokens[0].content);
    EXPECT_WK_STREQ("two", items[0].tokens[1].content);
    EXPECT_WK_STREQ(" \n ", items[0].tokens[2].content);
    EXPECT_WK_STREQ("three", items[0].tokens[3].content);
    EXPECT_WK_STREQ("four", items[0].tokens[4].content);
    EXPECT_EQ(items[1].tokens.count, 5UL);
    EXPECT_WK_STREQ("\n", items[1].tokens[0].content);
    EXPECT_WK_STREQ("five", items[1].tokens[1].content);
    EXPECT_WK_STREQ(" six", items[1].tokens[2].content);
    EXPECT_WK_STREQ(" ", items[1].tokens[3].content);
    EXPECT_WK_STREQ("seven", items[1].tokens[4].content);
}

TEST(TextManipulation, StartTextManipulationExcludesTextRenderedAsIcons)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "<style>"
        "@font-face {"
        "    font-family: Ahem;"
        "    src: url(Ahem.ttf);"
        "}"
        "@font-face {"
        "    font-family: SpaceOnly;"
        "    src: url(SpaceOnly.otf);"
        "}"
        ".Ahem { font-family: Ahem; }"
        ".SpaceOnly { font-family: SpaceOnly; }"
        ".Missing { font-family: Missing; }"
        ".SystemUI { font-family: system-ui; }"
        "</style>"
        "</head>"
        "<body>"
        "<span class='Ahem'>one</span><span class='SpaceOnly'>two</span><span class='Missing'>three</span><span class='SystemUI'>four</span>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    auto item = items.firstObject;
    EXPECT_EQ(item.tokens.count, 4UL);
    EXPECT_WK_STREQ(item.tokens[0].content, "one");
    EXPECT_FALSE(item.tokens[0].isExcluded);
    EXPECT_WK_STREQ(item.tokens[1].content, "two");
    EXPECT_TRUE(item.tokens[1].isExcluded);
    EXPECT_WK_STREQ(item.tokens[2].content, "three");
    EXPECT_FALSE(item.tokens[2].isExcluded);
    EXPECT_WK_STREQ(item.tokens[3].content, "four");
    EXPECT_FALSE(item.tokens[3].isExcluded);
}

TEST(TextManipulation, StartTextManipulationAvoidCrashWhenExtractingOrphanedPositions)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<p>hello world</p>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world", items[0].tokens[0].content);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        done = true;
    };

    [webView objectByEvaluatingJavaScript:@"(() => {"
        "    const objectElement = document.createElement('object');"
        "    document.body.appendChild(objectElement);"
        "    document.body.scrollTop;"
        "    objectElement.remove();"
        "    const text = document.createTextNode('testing');"
        "    const container = document.createElement('div');"
        "    container.appendChild(text);"
        "    document.body.appendChild(container);"
        "})();"];

    TestWebKitAPI::Util::run(&done);
}

TEST(TextManipulation, StartTextManipulationExtractsHeadingElementsAsSeparateItems)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "  <body>"
        "    <div style='float: left; width: 300px; height: 150px;'></div>"
        "    <p style='float: left; width: 600px;'>Hello world</p>"
        "    <h4 style='float: left; width: 600px;'>This is a heading</h4>"
        "  </body>"
        "</html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("Hello world", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("This is a heading", items[1].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationIgnoresSpaces)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "Hello"
        "<div style='background-color: lightblue;'>&nbsp;</div>"
        "World"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("Hello", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("World", items[1].tokens[0].content);
}

TEST(TextManipulation, StartTextManipulationExtractsTableCellsAsSeparateItems)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "  <style>"
        "    td { border: solid 1px tomato; }"
        "    a { border: solid 1px black; display: table-cell; }"
        "  </style>"
        "</head>"
        "<body>"
        "  <table>"
        "    <tbody>"
        "      <tr>"
        "        <a><span>Hello</span></a>"
        "        <a>World</a>"
        "        <td>Foo</td>"
        "        <td>Bar</td>"
        "        <td>Baz</td>"
        "      </tr>"
        "    </tbody>"
        "  </table>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 5UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("Hello", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("World", items[1].tokens[0].content);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_WK_STREQ("Foo", items[2].tokens[0].content);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_WK_STREQ("Bar", items[3].tokens[0].content);
    EXPECT_EQ(items[4].tokens.count, 1UL);
    EXPECT_WK_STREQ("Baz", items[4].tokens[0].content);
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, world" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hello, world</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, LegacyCompleteTextManipulationReplaceSimpleSingleParagraph)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>helllo, wooorld</p></body></html>"];

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
    }) completion:^(BOOL success) {
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, " },
        { items[0].tokens[3].identifier, @"WebKit" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hello, <i>WebKit</i></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceTwoSimpleParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<p>hello</p>world"];
    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("world", items[1].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"World" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p>Hello</p>World", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceMultipleSimpleParagraphs)
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[0].identifier, @"Hello, " },
        { items[1].tokens[1].identifier, @"kittens" },
        { items[1].tokens[2].identifier, @" are " },
        { items[1].tokens[3].identifier, @"cute" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>helllo, wooorld</p><p>Hello, <b>kittens</b> are <em>cute</em></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, world." },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, world.</p><p>Hello, <b>kittens</b> are <em>cute</em></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceMultipleSimpleParagraphsAtOnce)
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[0].identifier, @"Hello, " },
        { items[1].tokens[1].identifier, @"kittens" },
        { items[1].tokens[2].identifier, @" are " },
        { items[1].tokens[3].identifier, @"cute" },
    }), (_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, world." },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, world.</p><p>Hello, <b>kittens</b> are <em>cute</em></p>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceMultipleSimpleParagraphsSeparatedByBR)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p>helllo, wooorld<br>webKit</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("helllo, wooorld", items[0].tokens[0].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("webKit", items[1].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, World" },
    }), (_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[0].identifier, @"WebKit" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, World<br>WebKit</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationReplaceParagraphsSeparatedByWrappedBR)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html><body><p>earth, <b>hey<br></b>webKit</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("earth, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("hey", items[0].tokens[1].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("webKit", items[1].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[1].identifier, @"Hello, " },
        { items[0].tokens[0].identifier, @"World" },
    }), (_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[0].identifier, @"WebKit" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p><b>Hello, </b>World<b><br></b>WebKit</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenBRIsInserted)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>helllo, <b>worrld</b></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("helllo, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("worrld", items[0].tokens[1].content.UTF8String);

    [webView stringByEvaluatingJavaScript:@"document.querySelector('b').before(document.createElement('br'))"];

    done = false;
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, " },
        { items[0].tokens[1].identifier, @"World" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>helllo, <br><b>worrld</b></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationAvoidCrashingWhenContentIsRemoved)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    auto *tokens = items[0].tokens;
    EXPECT_EQ(tokens.count, 1UL);

    __block bool done = false;
    [webView performAfterReceivingMessage:@"DoneRemovingParagraph" action:^{
        done = true;
    }];

    [webView stringByEvaluatingJavaScript:
        @"const paragraph = document.createElement('p');"
        "paragraph.textContent = 'Hello world';"
        "document.body.appendChild(paragraph);"
        "setTimeout(() => { paragraph.remove(); webkit.messageHandlers.testHandler.postMessage('DoneRemovingParagraph') })"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { tokens[0].identifier, @"Simple HTML file!" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("Simple HTML file!", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldPreserveImagesAsExcludedTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><div>hello, <img src=\"apple.gif\"> world</div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    auto *tokens = items[0].tokens;
    EXPECT_EQ(tokens.count, 3UL);
    EXPECT_STREQ("hello, ", tokens[0].content.UTF8String);
    EXPECT_FALSE(tokens[0].isExcluded);
    EXPECT_STREQ("[]", tokens[1].content.UTF8String);
    EXPECT_TRUE(tokens[1].isExcluded);
    EXPECT_STREQ(" world", tokens[2].content.UTF8String);
    EXPECT_FALSE(tokens[2].isExcluded);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { tokens[0].identifier, @"this is " },
        { tokens[1].identifier, nil },
        { tokens[2].identifier, @" a test" }
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<div>this is <img src=\"apple.gif\"> a test</div>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldPreserveSVGAsExcludedTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>"
    "<section><div style=\"display: inline-block;\">hello"
    "<span style=\"display: inline-flex;\"><svg viewBox=\"0 0 20 20\" width=\"20\" height=\"20\"><rect width=\"20\" height=\"20\" fill=\"#06f\"></rect></svg></span>"
    "webkit</div></section>"
    "<p>world</p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    auto *tokens = items[0].tokens;
    EXPECT_EQ(tokens.count, 3UL);
    EXPECT_STREQ("hello", tokens[0].content.UTF8String);
    EXPECT_FALSE(tokens[0].isExcluded);
    EXPECT_STREQ("[]", tokens[1].content.UTF8String);
    EXPECT_TRUE(tokens[1].isExcluded);
    EXPECT_STREQ("webkit", tokens[2].content.UTF8String);
    EXPECT_FALSE(tokens[2].isExcluded);
    
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("world", items[1].tokens[0].content.UTF8String);
    EXPECT_FALSE(items[1].tokens[0].isExcluded);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { tokens[0].identifier, @"hey" },
        { tokens[1].identifier, nil },
        { tokens[2].identifier, @"WebKit" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<section><div style=\"display: inline-block;\">hey<span style=\"display: inline-flex;\"><svg viewBox=\"0 0 20 20\" width=\"20\" height=\"20\"><rect width=\"20\" height=\"20\" fill=\"#06f\"></rect></svg></span>WebKit</div></section><p>world</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldPreserveOrderOfBlockImage)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><svg viewBox=\"0 0 10 10\" width=\"100\" height=\"100\">"
    "<rect width=\"10\" height=\"10\" fill=\"red\"></rect></svg><img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAE"
    "AAAABCAIAAACQd1PeAAAAAXNSR0IArs4c6QAAAAxJREFUCNdjYKhnAAABAgCAbV7tZwAAAABJRU5ErkJggg==\""
    "style=\"display: block; width: 100px;\"><section>helllo world</section></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("helllo world", items[0].tokens[0].content.UTF8String);
    EXPECT_TRUE(!items[0].tokens[0].isExcluded);

    done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"hello, world" } }),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<svg viewBox=\"0 0 10 10\" width=\"100\" height=\"100\"><rect width=\"10\" height=\"10\" fill=\"red\"></rect></svg>"
    "<img src=\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAAAXNSR0IArs4c6QAAAAxJREFUCN"
    "djYKhnAAABAgCAbV7tZwAAAABJRU5ErkJggg==\" style=\"display: block; width: 100px;\"><section>hello, world</section>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldReplaceAttributeContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><head><title>hey</title></head>"
        "<body><div><span aria-label=\"this is greet\">hello</span><img src=\"apple.gif\" alt=\"fruit\"></div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 4UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hey", items[0].tokens[0].content.UTF8String);

    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("this is greet", items[1].tokens[0].content.UTF8String);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_STREQ("fruit", items[2].tokens[0].content.UTF8String);

    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_STREQ("hello", items[3].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"Hello" } }),
        (_WKTextManipulationItem *)createItem(items[1].identifier, { { items[1].tokens[0].identifier, @"This is a greeting" } }),
        (_WKTextManipulationItem *)createItem(items[2].identifier, { { items[2].tokens[0].identifier, @"Apple" } }),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<head><title>Hello</title></head><body><div><span aria-label=\"This is a greeting\">hello</span>"
        "<img src=\"apple.gif\" alt=\"Apple\"></div></body>", [webView stringByEvaluatingJavaScript:@"document.documentElement.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldReplaceContentFollowedAfterImageInCSSTable)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>"
        "<div style=\"display: table\"><div style=\"float: left;\"><img src=\"apple.gif\" style=\"display: flex;\"></div>"
        "<div><span style=\"display: block\">hello world</span></div></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("hello world", items[0].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"hello, world" } }),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<div style=\"display: table\"><div style=\"float: left;\"><img src=\"apple.gif\" style=\"display: flex;\"></div>"
        "<div><span style=\"display: block\">hello, world</span></div></div>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldReplaceTextContentWithMultipleTokens)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html>"
        "<title>This is a test</title>"
        "<select>"
        "    <option selected>Hello world</option>"
        "    <option>Should not be replaced</option>"
        "</select>"
        "<span aria-label='label'>Text</span>"
        "<img src='apple.gif' alt='image'>"
        "</html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 6UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_EQ(items[4].tokens.count, 1UL);
    EXPECT_EQ(items[5].tokens.count, 1UL);
    EXPECT_WK_STREQ("This is a test", items[0].tokens[0].content);
    EXPECT_WK_STREQ("Hello world", items[1].tokens[0].content);
    EXPECT_WK_STREQ("Should not be replaced", items[2].tokens[0].content);
    EXPECT_WK_STREQ("label", items[3].tokens[0].content);
    EXPECT_WK_STREQ("image", items[4].tokens[0].content);
    EXPECT_WK_STREQ("Text", items[5].tokens[0].content);

    auto replacementItems = retainPtr(@[
        createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"Replacement" }, { items[0].tokens[0].identifier, @"title" } }).autorelease(),
        createItem(items[1].identifier, { { items[1].tokens[0].identifier, @"Replacement" }, { items[1].tokens[0].identifier, @"option" } }).autorelease(),
        createItem(items[2].identifier, { { items[2].tokens[0].identifier, @"Failed replacement" }, { @"12345", @"option" } }).autorelease(),
        createItem(items[3].identifier, { { items[3].tokens[0].identifier, @"Replacement" }, { items[3].tokens[0].identifier, @"label" } }).autorelease(),
        createItem(items[4].identifier, { { items[4].tokens[0].identifier, @"Replacement" }, { items[4].tokens[0].identifier, @"image" } }).autorelease(),
    ]);

    done = false;
    [webView _completeTextManipulationForItems:replacementItems.get() completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorInvalidToken);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], [replacementItems objectAtIndex:2]);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    NSArray<NSString *> *options = [webView objectByEvaluatingJavaScript:@"Array.from(document.querySelector('select').options).map(option => option.textContent)"];
    EXPECT_WK_STREQ("Replacement option", options.firstObject);
    EXPECT_WK_STREQ("Should not be replaced", options.lastObject);
    EXPECT_WK_STREQ("Replacement title", [webView stringByEvaluatingJavaScript:@"document.title"]);
    EXPECT_WK_STREQ("Replacement label", [webView objectByEvaluatingJavaScript:@"document.querySelector('span').getAttribute('aria-label')"]);
    EXPECT_WK_STREQ("Replacement image", [webView objectByEvaluatingJavaScript:@"document.querySelector('img').getAttribute('alt')"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldReplaceContentsAroundParagraphWithJustImage)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><div>heeey</div><div><img src=\"apple.gif\"></div><span>woorld</span>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_STREQ("heeey", items[0].tokens[0].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("woorld", items[1].tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[
        (_WKTextManipulationItem *)createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"hello" } }),
        (_WKTextManipulationItem *)createItem(items[1].identifier, { { items[1].tokens[0].identifier, @"world" } }),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<div>hello</div><div><img src=\"apple.gif\"></div><span>world</span>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldBatchItemCallback)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body></body></html>"];

    [webView stringByEvaluatingJavaScript:@"html = ''; for (let i = 0; i < 2000; ++i) html += `<p>hello ${i}</p>`; document.body.innerHTML = html;"];

    done = false;
    __block unsigned itemListCallbackCount = 0;
    delegate.get().itemListCallback = ^(NSArray<_WKTextManipulationItem *> *items) {
        itemListCallbackCount++;
    };
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_GE(itemListCallbackCount, 2UL);

    auto *items = [delegate items];    
    EXPECT_EQ(items.count, 2000UL);
    for (unsigned i = 0; i < 2000; ++i) {
        EXPECT_EQ(items[i].tokens.count, 1UL);
        EXPECT_WK_STREQ(makeString("hello ", i), items[i].tokens[0].content.UTF8String);
    }
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[2].identifier, @"I" },
        { items[0].tokens[3].identifier, @"'m a " },
        { items[0].tokens[0].identifier, @"cat" },
    }).get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello" },
        { items[0].tokens[1].identifier, @" WebKit " },
        { items[0].tokens[0].identifier, @"world" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello " },
        { items[0].tokens[2].identifier, @"world" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p><b>hello world</b></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenItemIdentifierIsDuplicated)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hello, <b>world</b></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);

    done = false;
    auto firstItem = createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"done" } });
    __block auto secondItem = createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"bad" } });
    [webView _completeTextManipulationForItems:@[firstItem.get(), secondItem.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorInvalidItem);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], secondItem.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>done</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationCanHandleSubsetOfItemsToFail)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hey, <b>dude</b></p><p>this is <b>bad</b></p></body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hey, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("dude", items[0].tokens[1].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 2UL);
    EXPECT_STREQ("this is ", items[1].tokens[0].content.UTF8String);
    EXPECT_STREQ("bad", items[1].tokens[1].content.UTF8String);

    done = false;
    __block auto firstItem = createItem(items[0].identifier, { { items[1].tokens[0].identifier, @"bad" } });
    auto secondItem = createItem(items[1].identifier, { { items[1].tokens[0].identifier, @"good" } });
    [webView _completeTextManipulationForItems:@[firstItem.get(), secondItem.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorInvalidToken);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], firstItem.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hey, <b>dude</b></p><p>good</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"What " },
        { items[0].tokens[1].identifier, @"time" },
        { items[0].tokens[2].identifier, @" is it now?" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
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
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hey" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("new content", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenContentIsAdded)
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

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 3)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.querySelector('p').innerHTML = 'hello, world &#10; bye';"
        "document.body.appendChild(document.createElement('div')).innerHTML = 'end'"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    __block auto item = createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello, World" }});
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorContentChanged);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p>hello, world \n bye</p><div>end</div>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationSuccedsWhenContentOutOfParagraphIsAdded)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<p style='white-space:pre;background-color:blue;'><span>hello world</span><u>   </u></p>"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world", items[0].tokens[0].content);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 2)
            done = true;
    };

    [webView stringByEvaluatingJavaScript:@"var element = document.createElement('span');"
        "element.innerHTML='inserted';"
        "document.querySelector('u').before(element)"];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello World" }}).autorelease(),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p style=\"white-space:pre;background-color:blue;\"><span>Hello World</span><span>inserted</span><u>   </u></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"hello, " },
        { items[0].tokens[1].identifier, @"world" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorInvalidItem);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
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
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hi, <em>WebKitten</em> bye</p>'"];

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
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("hi, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("WebKitten", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(" bye", items[0].tokens[2].content.UTF8String);

    done = false;
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello," },
        { items[0].tokens[1].identifier, @"WebKit" },
        { items[0].tokens[2].identifier, @"Bye" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorExclusionViolation);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hi, <em>WebKitten</em> bye</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationFailWhenExcludedContentAppearsMoreThanOnce)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView stringByEvaluatingJavaScript:@"document.body.innerHTML = '<p>hi, <em>WebKitten</em> bye</p>'"];

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
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("hi, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("WebKitten", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(" bye", items[0].tokens[2].content.UTF8String);

    done = false;
    __block auto item = createItem(items[0].identifier, {
        { items[0].tokens[1].identifier, nil },
        { items[0].tokens[0].identifier, @"Hello," },
        { items[0].tokens[1].identifier, nil },
        { items[0].tokens[2].identifier, @"Bye" },
    });
    [webView _completeTextManipulationForItems:@[item.get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors.count, 1UL);
        EXPECT_EQ(errors.firstObject.domain, _WKTextManipulationItemErrorDomain);
        EXPECT_EQ(errors.firstObject.code, _WKTextManipulationItemErrorExclusionViolation);
        EXPECT_EQ(errors.firstObject.userInfo[_WKTextManipulationItemErrorItemKey], item.get());
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>hi, <em>WebKitten</em> bye</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationPreservesExcludedContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p>hi, <em>WebKitten</em> bye</p></body></html>"];

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
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("hi, ", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("WebKitten", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ(" bye", items[0].tokens[2].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[0].identifier, {
        { items[0].tokens[0].identifier, @"Hello, " },
        { items[0].tokens[1].identifier, nil },
        { items[0].tokens[2].identifier, @" Bye" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<p>Hello, <em>WebKitten</em> Bye</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(firstItem.identifier, {
        { firstItem.tokens[0].identifier, @"bar " },
        { firstItem.tokens[1].identifier, @"garply" },
        { firstItem.tokens[2].identifier, @" foo" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);
    [webView waitForNextPresentationUpdate];

    EXPECT_FALSE(foundNewItemAfterCompletingTextManipulation);
    EXPECT_WK_STREQ("<p>bar <strong>garply</strong> foo</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationCorrectParagraphRange)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<head><style>ul{display:block}li{display:inline-block}.inline {float: left;}.subframe {height: 42px;}.frame {position: absolute;top: -9999px;}</style></head><body><div class='frame'><div class='subframe'></div></div><style></style><div class='inline'><div><li><a href='#'>holle</a></li><li><a href='#'>wdrlo</a></li></div></div><div class='frame'><div class='subframe'></div></div></body>"];

    RetainPtr<_WKTextManipulationConfiguration> configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);
    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("holle", items[0].tokens[0].content);
    EXPECT_WK_STREQ("wdrlo", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"hello" }}).autorelease(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"world" }}).autorelease()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<div class=\"frame\"><div class=\"subframe\"></div></div><style></style><div class=\"inline\"><div><li><a href=\"#\">hello</a></li><li><a href=\"#\">world</a></li></div></div><div class=\"frame\"><div class=\"subframe\"></div></div>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationCanMergeContentAndPreserveLineBreaks)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<span style='white-space:pre;'>one &#10; two &#10;</span>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 4UL);
    EXPECT_STREQ("one", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ(" \n ", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ("two", items[0].tokens[2].content.UTF8String);
    EXPECT_STREQ(" \n", items[0].tokens[3].content.UTF8String);

    auto replacementItems = retainPtr(@[
        createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"ONE" }, { items[0].tokens[1].identifier, items[0].tokens[1].content }, { items[0].tokens[2].identifier, @"TWO" }, { items[0].tokens[3].identifier, items[0].tokens[3].content } }).autorelease(),
    ]);

    done = false;
    [webView _completeTextManipulationForItems:replacementItems.get() completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<span style=\"white-space:pre;\">ONE \n TWO \n</span>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationIgnoreWhiteSpacesBetweenParagraphs)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@""
        "<style>"
            ".inline-block { display: inline-block; }"
            ".list-item { display: list-item; }"
            ".hide-absolute { display: none; position: absolute; }"
            ".float-relative { float: left; position: relative; }"
        "</style>"
        "<ul class='float-relative'>"
        "   <li class='list-item float-relative'><a class='inline-block'>hello</a>           <div class='hide-absolute'><a class='inline-block float-relative'>hide</a></div></li>"
        "   <li class='list-item float-relative'><a class='inline-block'>world</a></li>"
        "</ul>"];
    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("world", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"World" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

TEST(TextManipulation, CompleteTextManipulationShouldPreserveNodesAfterParagraphRange)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<p>hello<b>world<br><br><i>from</i></b></p>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_STREQ("from", items[1].tokens[0].content.UTF8String);

    auto replacementItems = retainPtr(@[
        createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"Hello" }, { items[0].tokens[1].identifier, @"World" } }).autorelease(),
        createItem(items[1].identifier, { { items[1].tokens[0].identifier, @"From WebKit" } }).autorelease(),
    ]);

    done = false;
    [webView _completeTextManipulationForItems:replacementItems.get() completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p>Hello<b>World<br><br><i>From WebKit</i></b></p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationSPreserveNodesBeforeParagraphRange)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<p><b><br><br>hello<i>world</i></b>from</p>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 3UL);
    EXPECT_STREQ("hello", items[0].tokens[0].content.UTF8String);
    EXPECT_STREQ("world", items[0].tokens[1].content.UTF8String);
    EXPECT_STREQ("from", items[0].tokens[2].content.UTF8String);

    auto replacementItems = retainPtr(@[
        createItem(items[0].identifier, { { items[0].tokens[0].identifier, @"Hello" }, { items[0].tokens[1].identifier, @"World" }, { items[0].tokens[2].identifier, @"From WebKit" } }).autorelease()
    ]);

    done = false;
    [webView _completeTextManipulationForItems:replacementItems.get() completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p><b><br><br>Hello<i>World</i></b>From WebKit</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, InsertingContentIntoAlreadyManipulatedContentCreatesTextManipulationItem)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body><p><i><b>hey</b></i> dude</p></body></html>"];

    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);

    auto* firstItem = items.firstObject;
    EXPECT_EQ(firstItem.tokens.count, 2UL);
    EXPECT_STREQ("hey", firstItem.tokens[0].content.UTF8String);
    EXPECT_STREQ(" dude", firstItem.tokens[1].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(firstItem.identifier, {
        { firstItem.tokens[0].identifier, @"hello," },
        { firstItem.tokens[1].identifier, @" world" },
    })] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    [delegate setItemCallback:^(_WKTextManipulationItem *) {
        done = true;
    }];
    [webView stringByEvaluatingJavaScript:@"span = document.createElement('span'); span.textContent = ' WebKit!'; document.querySelector('b').after(span);"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 2UL);
    EXPECT_WK_STREQ("<p><i><b>hello,</b><span> WebKit!</span></i> world</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationInButtonsAndTextFields)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<input type='text' value='hello1'><input type='submit' value='hello2'>"];
    auto configuration = adoptNS([[_WKTextManipulationConfiguration alloc] init]);

    done = false;
    [webView _startTextManipulationsWithConfiguration:configuration.get() completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);

    auto* firstItem = items.firstObject;
    auto* lastItem = items.lastObject;
    EXPECT_EQ(firstItem.tokens.count, 1UL);
    EXPECT_EQ(lastItem.tokens.count, 1UL);
    EXPECT_STREQ("hello1", firstItem.tokens[0].content.UTF8String);
    EXPECT_STREQ("hello2", lastItem.tokens[0].content.UTF8String);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(firstItem.identifier, {{ firstItem.tokens[0].identifier, @"world1" }}).get(),
        createItem(lastItem.identifier, {{ lastItem.tokens[0].identifier, @"world2" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("world1", [webView stringByEvaluatingJavaScript:@"document.querySelector('input[type=text]').value"]);
    EXPECT_WK_STREQ("world2", [webView stringByEvaluatingJavaScript:@"document.querySelector('input[type=submit]').value"]);
}

TEST(TextManipulation, CompleteTextManipulationForNewlyDisplayedParagraph)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html><body>"
        "<style> .hidden { display: none; } </style>"
        "<div>"
            "<b>webkit!</b>"
            "<span>"
                "hello world"
                "<i class='hidden'>bye</i>"
            "</span>"
        "</div>"
        "</body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_WK_STREQ("webkit!", items[0].tokens[0].content);
    EXPECT_WK_STREQ("hello world", items[0].tokens[1].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"WebKit!" }, { items[0].tokens[1].identifier, @"Hello World" }}).get(),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.querySelector('i').removeAttribute('class');"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("bye", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"Bye" }}).get(),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<b>WebKit!</b><span>Hello World<i>Bye</i></span>", [webView stringByEvaluatingJavaScript:@"document.querySelector('div').innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationForNewlyDisplayedText)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<body>"
        "<a>hello world</a>"
        "</body>"
        "</html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world", items[0].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello World" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<a>Hello World</a>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 2)
            done = true;
    };
    NSString *scriptString = @"document.querySelector('a').innerHTML=''; document.querySelector('a').innerHTML='hello world again';";
    [webView evaluateJavaScript:scriptString completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world again", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"Hello World Again" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<a>Hello World Again</a>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationForManipulatedTextWithNewContent)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html><html>"
        "<body>"
            "<span>hello world</span>"
            "<p>hello webkit</p>"
        "</body></html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello webkit", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello World" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"Hello WebKit" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<span>Hello World</span><p>Hello WebKit</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        if (items.count == 4)
            done = true;
    };
    [webView stringByEvaluatingJavaScript:@"document.querySelector('span').innerHTML='hello world again';"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('p').childNodes[0].nodeValue='hello webkit again';"];
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello world again", items[2].tokens[0].content);
    EXPECT_EQ(items[3].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello webkit again", items[3].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[2].identifier, {{ items[2].tokens[0].identifier, @"Hello World Again" }}).get(),
        createItem(items[3].identifier, {{ items[3].tokens[0].identifier, @"Hello WebKit Again" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<span>Hello World Again</span><p>Hello WebKit Again</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationAvoidExtractingManipulatedTextAfterManipulation)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];

    [webView synchronouslyLoadHTMLString:@"<p>foo<br>bar</p>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("foo", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ("bar", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"FOO" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"BAR" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<p>FOO<br>BAR</p>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);

    __block bool itemCallbackFired = false;
    [delegate setItemCallback:^(_WKTextManipulationItem *) {
        itemCallbackFired = true;
    }];

    [webView objectByEvaluatingJavaScript:@"document.querySelector('p').style.display = 'none'"];
    [webView objectByEvaluatingJavaScript:@"document.querySelector('p').style.display = ''"];
    [webView stringByEvaluatingJavaScript:@"var element = document.createElement('span');"
        "element.innerHTML='end';"
        "document.querySelector('p').after(element)"];

    done = false;
    delegate.get().itemCallback = ^(_WKTextManipulationItem *item) {
        done = true;
    };
    TestWebKitAPI::Util::run(&done);

    EXPECT_EQ(items.count, 3UL);
    EXPECT_EQ(items[2].tokens.count, 1UL);
    EXPECT_WK_STREQ("end", items[2].tokens[0].content);
}

TEST(TextManipulation, CompleteTextManipulationAddsOverflowHiddenToAvoidBreakingLayout)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<html>"
        "<head>"
        "  <style>"
        "    span {"
        "      display: inline-block;"
        "      width: 28px;"
        "      height: 40px;"
        "      margin: 0;"
        "      word-break: break-all;"
        "      font-size: 32px;"
        "    }"
        "    a {"
        "      display: block;"
        "      width: 100px;"
        "      height: 100px;"
        "      overflow: hidden;"
        "    }"
        "  </style>"
        "</head>"
        "<body>"
        "  <a>"
        "    <span>A</span> Hello world"
        "  </a>"
        "</body>"
        "</html>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("A", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ(" Hello world", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"This is a long translation" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @" Foo bar" }}).get()
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("This is a long translation Foo bar", [webView stringByEvaluatingJavaScript:@"document.querySelector('a').textContent.trim()"]);
    EXPECT_WK_STREQ("hidden", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('span')).overflowX"]);
    EXPECT_WK_STREQ("auto", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('span')).overflowY"]);
}

TEST(TextManipulation, CompleteTextManipulationParagraphsContainCollapsedSpaces)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
        "<style>"
        "span { display: inline-block; }"
        "</style>"
        "</head>"
        "<body>"
        "<span>  hello</span>"
        "<span>  world</span>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 1UL);
    EXPECT_WK_STREQ("hello", items[0].tokens[0].content);
    EXPECT_EQ(items[1].tokens.count, 1UL);
    EXPECT_WK_STREQ(" world", items[1].tokens[0].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {{ items[0].tokens[0].identifier, @"Hello" }}).get(),
        createItem(items[1].identifier, {{ items[1].tokens[0].identifier, @"World" }}).get(),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<span>Hello</span><span>World</span>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldReplaceContentIgnoredByEditing)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<body>"
        "<div role='img'>"
            "images"
            "<a>link</a>"
            "<img src='hello.png'>"
            "<img src='webkit.png'>"
        "</div>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto items = [delegate items];
    EXPECT_EQ(items.count, 1UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_WK_STREQ("images", items[0].tokens[0].content);
    EXPECT_WK_STREQ("link", items[0].tokens[1].content);

    done = false;
    [webView _completeTextManipulationForItems:@[
        createItem(items[0].identifier, {
            { items[0].tokens[0].identifier, @"Images" },
            { items[0].tokens[1].identifier, @"Link" }
    }).get(),
    ] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    EXPECT_WK_STREQ("<div role=\"img\">Images<a>Link</a><img src=\"hello.png\"><img src=\"webkit.png\"></div>", [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
}

TEST(TextManipulation, CompleteTextManipulationShouldOnlyChangeNodesInParagraphRange)
{
    auto delegate = adoptNS([[TextManipulationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setTextManipulationDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
            "<style>"
                "span { white-space:pre; }"
            "</style>"
        "</head>"
        "<body>"
            "<span>zero &#10;<b>two four</b></span>"
            "one"
            "<i>three</i>"
        "</body>"];

    done = false;
    [webView _startTextManipulationsWithConfiguration:nil completion:^{
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);

    auto *items = [delegate items];
    EXPECT_EQ(items.count, 2UL);
    EXPECT_EQ(items[0].tokens.count, 2UL);
    EXPECT_WK_STREQ("zero", items[0].tokens[0].content);
    EXPECT_WK_STREQ(" \n", items[0].tokens[1].content);
    EXPECT_EQ(items[1].tokens.count, 3UL);
    EXPECT_WK_STREQ("two four", items[1].tokens[0].content);
    EXPECT_WK_STREQ("one", items[1].tokens[1].content);
    EXPECT_WK_STREQ("three", items[1].tokens[2].content);

    done = false;
    [webView _completeTextManipulationForItems:@[(_WKTextManipulationItem *)createItem(items[1].identifier, {
        { items[1].tokens[1].identifier, @"one" },
        { items[1].tokens[0].identifier, @"two" },
        { items[1].tokens[2].identifier, @"three" },
        { items[1].tokens[0].identifier, @"four" }
    }).get()] completion:^(NSArray<NSError *> *errors) {
        EXPECT_EQ(errors, nil);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ("<span>zero \n</span>one<span><b>two</b></span><i>three</i><span><b>four</b></span>",
        [webView stringByEvaluatingJavaScript:@"document.body.innerHTML"]);
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

