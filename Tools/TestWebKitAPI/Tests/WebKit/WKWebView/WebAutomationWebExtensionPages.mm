/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER) && ENABLE(REMOTE_INSPECTOR)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/WebExtensionUtilities.h"
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebExtensionContextPrivate.h>
#import <WebKit/WKWebExtensionPrivate.h>
#import <WebKit/_WKAutomationSession.h>
#import <WebKit/_WKAutomationSessionConfiguration.h>
#import <WebKit/_WKAutomationSessionDelegate.h>
#import <WebKit/_WKAutomationSessionPrivateForTesting.h>
#import <wtf/RetainPtr.h>
#import <wtf/Scope.h>

static NSString * const testExtensionIdentifier = @"org.webkit.automation-test-extension";

@interface TestWebExtensionPagesAutomationDelegate : NSObject <_WKAutomationSessionDelegate>
- (instancetype)initWithExtensionContext:(WKWebExtensionContext *)extensionContext;
@end

@implementation TestWebExtensionPagesAutomationDelegate {
    RetainPtr<WKWebExtensionContext> _extensionContext;
}

- (instancetype)initWithExtensionContext:(WKWebExtensionContext *)extensionContext
{
    if (!(self = [super init]))
        return nil;

    _extensionContext = extensionContext;

    return self;
}

- (WKWebExtensionContext *)_automationSession:(_WKAutomationSession *)automationSession webExtensionContextForIdentifier:(NSString *)identifier
{
    return [identifier isEqualToString:testExtensionIdentifier] ? _extensionContext.get() : nil;
}

@end

namespace TestWebKitAPI {

static auto *backgroundPageManifest = @{
    @"manifest_version": @3,

    @"name": @"Automation Test",
    @"description": @"Automation Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"persistent": @NO,
    },

    @"action": @{
        @"default_popup": @"popup.html",
    },
};

static RetainPtr<NSString> dispatchExtensionAutomationCommand(_WKAutomationSession *session, NSString *command)
{
    __block bool gotReply = false;
    __block RetainPtr<NSString> reply;
    [session _setMessageToFrontendHandlerForTesting:^(NSString *message) {
        if (![message containsString:@"\"id\":1"])
            return;
        reply = message;
        gotReply = true;
    }];
    [session _dispatchMessageFromRemoteForTesting:command];
    Util::run(&gotReply);
    [session _setMessageToFrontendHandlerForTesting:nil];
    return reply;
}

static NSDictionary *parsedReply(NSString *reply)
{
    return [NSJSONSerialization JSONObjectWithData:[reply dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
}

static RetainPtr<NSString> getWebExtensionPages(_WKAutomationSession *session, NSString *identifier)
{
    return dispatchExtensionAutomationCommand(session, [NSString stringWithFormat:@"{\"id\":1,\"method\":\"Automation.getWebExtensionPages\",\"params\":{\"identifier\":\"%@\"}}", identifier]);
}

static NSArray<NSDictionary *> *pagesOfType(NSString *reply, NSString *type)
{
    NSArray<NSDictionary *> *pages = parsedReply(reply)[@"result"][@"pages"];
    return [pages filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"type == %@", type]];
}

static RetainPtr<_WKAutomationSession> createAutomationSession()
{
    RetainPtr sessionConfiguration = adoptNS([_WKAutomationSessionConfiguration new]);
    return adoptNS([[_WKAutomationSession alloc] initWithConfiguration:sessionConfiguration.get()]);
}

TEST(WebAutomationWebExtensionPages, ListsBackgroundPage)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{ @"background.js": @"browser.test.sendMessage('Background Ready')" });
    [manager runUntilTestMessage:@"Background Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    NSString *baseURL = manager.get().context.baseURL.absoluteString;
    EXPECT_NS_EQUAL(parsedReply(reply.get())[@"result"][@"baseURL"], baseURL);

    NSArray<NSDictionary *> *backgroundPages = pagesOfType(reply.get(), @"Background");
    EXPECT_EQ(backgroundPages.count, 1u);
    EXPECT_TRUE([backgroundPages.firstObject[@"url"] hasPrefix:baseURL]) << [reply UTF8String];
}

TEST(WebAutomationWebExtensionPages, WakesUnloadedBackgroundPage)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{ @"background.js": @"browser.test.sendMessage('Background Ready')" });
    [manager runUntilTestMessage:@"Background Ready"];

    [manager.get().context _unloadBackgroundContentForTesting];
    EXPECT_NULL(manager.get().context._backgroundWebView);

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    EXPECT_EQ(pagesOfType(reply.get(), @"Background").count, 1u) << [reply UTF8String];
    EXPECT_NOT_NULL(manager.get().context._backgroundWebView);
}

TEST(WebAutomationWebExtensionPages, ListsOpenPopup)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{
        @"background.js": @"browser.action.openPopup()",
        @"popup.html": @"<script src='popup.js'></script>",
        @"popup.js": @"browser.test.sendMessage('Popup Ready')",
    });

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *) { };

    [manager runUntilTestMessage:@"Popup Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    NSArray<NSDictionary *> *popupPages = pagesOfType(reply.get(), @"Popup");
    EXPECT_EQ(popupPages.count, 1u) << [reply UTF8String];
    EXPECT_TRUE([popupPages.firstObject[@"url"] hasSuffix:@"/popup.html"]) << [reply UTF8String];
}

TEST(WebAutomationWebExtensionPages, ListsExtensionTab)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{
        @"background.js": @"browser.tabs.create({ url: 'page.html' })",
        @"page.html": @"<script src='page.js'></script>",
        @"page.js": @"browser.test.sendMessage('Tab Ready')",
    });

    [manager runUntilTestMessage:@"Tab Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    NSArray<NSDictionary *> *tabPages = pagesOfType(reply.get(), @"Tab");
    EXPECT_EQ(tabPages.count, 1u) << [reply UTF8String];
    EXPECT_TRUE([tabPages.firstObject[@"url"] hasSuffix:@"/page.html"]) << [reply UTF8String];
}

TEST(WebAutomationWebExtensionPages, HandleRunsScriptInBackgroundPage)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{ @"background.js": @"browser.test.sendMessage('Background Ready')" });
    [manager runUntilTestMessage:@"Background Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<WKProcessPool> processPool = manager.get().context._backgroundWebView.configuration.processPool;
    ALLOW_DEPRECATED_DECLARATIONS_END
    [processPool _setAutomationSession:session.get()];
    auto detachAutomationSession = makeScopeExit([&] {
        [processPool _setAutomationSession:nil];
    });

    RetainPtr<NSString> handle = pagesOfType(getWebExtensionPages(session.get(), testExtensionIdentifier).get(), @"Background").firstObject[@"handle"];
    ASSERT_NOT_NULL(handle.get());

    RetainPtr reply = dispatchExtensionAutomationCommand(session.get(), [NSString stringWithFormat:@"{\"id\":1,\"method\":\"Automation.evaluateJavaScriptFunction\",\"params\":{\"browsingContextHandle\":\"%@\",\"function\":\"function() { return typeof browser.runtime.id }\",\"arguments\":[]}}", handle.get()]);
    EXPECT_NS_EQUAL(parsedReply(reply.get())[@"result"][@"result"], @"\"string\"");
}

TEST(WebAutomationWebExtensionPages, ExcludesServiceWorkerBackground)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Automation Test",
        @"description": @"Automation Test",
        @"version": @"1",

        @"background": @{
            @"service_worker": @"background.js",
        },
    };

    auto manager = Util::loadExtension(manifest, @{ @"background.js": @"browser.test.sendMessage('Background Ready')" });
    [manager runUntilTestMessage:@"Background Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    EXPECT_NOT_NULL(parsedReply(reply.get())[@"result"][@"pages"]) << [reply UTF8String];
    EXPECT_EQ(pagesOfType(reply.get(), @"Background").count, 0u) << [reply UTF8String];
}

TEST(WebAutomationWebExtensionPages, RejectsUnknownExtension)
{
    auto manager = Util::loadExtension(backgroundPageManifest, @{ @"background.js": @"browser.test.sendMessage('Background Ready')" });
    [manager runUntilTestMessage:@"Background Ready"];

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:manager.get().context]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), @"org.webkit.not-loaded");
    EXPECT_TRUE([reply containsString:@"NoSuchExtension"]) << [reply UTF8String];
}

TEST(WebAutomationWebExtensionPages, RejectsUnloadedExtension)
{
    RetainPtr extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:backgroundPageManifest resources:@{ @"background.js": @"browser.test.sendMessage('Background Ready')" }]);
    RetainPtr extensionContext = adoptNS([[WKWebExtensionContext alloc] initForExtension:extension.get()]);

    RetainPtr session = createAutomationSession();
    RetainPtr delegate = adoptNS([[TestWebExtensionPagesAutomationDelegate alloc] initWithExtensionContext:extensionContext.get()]);
    [session setDelegate:delegate.get()];

    RetainPtr reply = getWebExtensionPages(session.get(), testExtensionIdentifier);
    EXPECT_TRUE([reply containsString:@"NoSuchExtension"]) << [reply UTF8String];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_IN_WEBDRIVER) && ENABLE(REMOTE_INSPECTOR)
