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
#import "Helpers/cocoa/WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebExtensionNotification.h>

namespace TestWebKitAPI {

#pragma mark - Constants

static auto *notificationsManifest = @{
    @"manifest_version": @3,
    @"name": @"Notifications Test",
    @"description": @"Notifications",
    @"version": @"1",

    @"permissions": @[ @"notifications" ],
    @"background": @{
        @"service_worker": @"background.js",
        @"type": @"module"
    },
};

#pragma mark - Test Fixture

class WKWebExtensionAPINotifications : public testing::Test {
protected:
    WKWebExtensionAPINotifications()
    {
        notificationsConfig = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
        if (!notificationsConfig.webViewConfiguration)
            notificationsConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in WKPreferences._features) {
            if ([feature.key isEqualToString:@"WebExtensionNotificationsEnabled"])
                [notificationsConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script)
    {
        return Util::parseExtension(notificationsManifest, @{ @"background.js" : Util::constructScript(script) }, notificationsConfig);
    }

    WKWebExtensionControllerConfiguration *notificationsConfig;
};

#pragma mark - Notifications Tests

TEST_F(WKWebExtensionAPINotifications, APIsAvailableWhenPermissionGranted)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.notifications === undefined)",
        @"browser.test.assertFalse(browser.notifications.create === undefined)",
        @"browser.test.assertFalse(browser.notifications.update === undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(notificationsManifest, @{ @"background.js": Util::constructScript(script) }, notificationsConfig);
}

TEST_F(WKWebExtensionAPINotifications, CreateArgumentValidation)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.notifications.create('id', {}), /missing required keys/)",
        @"browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', message: 'M' }), /missing required keys/)",
        @"browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', title: 'T' }), /missing required keys/)",
        @"browser.test.assertThrows(() => browser.notifications.create('id', { title: 'T', message: 'M' }), /missing required keys/)",

        // Each button requires a title.
        @"browser.test.assertThrows(() => browser.notifications.create('id', { type: 'basic', title: 'T', message: 'M', buttons: [ { } ] }), /missing required keys/)",

        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(notificationsManifest, @{ @"background.js": Util::constructScript(script) }, notificationsConfig);
}

TEST_F(WKWebExtensionAPINotifications, CreateAcceptsUnsupportedTypeAsBasic)
{
    auto *script = @[
        @"await browser.notifications.create('typed', { type: 'progress', iconUrl: 'icon.png', imageUrl: 'image.png', title: 'T', message: 'M', priority: 2, eventTime: 1700000000000, silent: true, progress: 50 })",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    RetainPtr<_WKWebExtensionNotification> presentedNotification;
    auto *presentedNotificationPtr = &presentedNotification;
    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *notification) {
        *presentedNotificationPtr = notification;
    };

    [manager loadAndRun];

    EXPECT_NOT_NULL(presentedNotification.get());
    EXPECT_NS_EQUAL(presentedNotification.get().identifier, @"typed");
}

TEST_F(WKWebExtensionAPINotifications, CreatePresentsNotificationThroughDelegate)
{
    auto *script = @[
        @"const identifier = await browser.notifications.create('my-notification', {",
        @"  type: 'basic',",
        @"  title: 'Primary Title',",
        @"  message: 'Body text',",
        @"  contextMessage: 'Secondary line',",
        @"  buttons: [ { title: 'First' }, { title: 'Second' } ],",
        @"})",
        @"browser.test.assertEq(identifier, 'my-notification')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    RetainPtr<_WKWebExtensionNotification> presentedNotification;
    auto *presentedNotificationPtr = &presentedNotification;
    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *notification) {
        *presentedNotificationPtr = notification;
    };

    [manager loadAndRun];

    EXPECT_NOT_NULL(presentedNotification.get());
    EXPECT_EQ(presentedNotification.get().webExtensionContext, manager.get().context);
    EXPECT_NS_EQUAL(presentedNotification.get().identifier, @"my-notification");
    EXPECT_NS_EQUAL(presentedNotification.get().title, @"Primary Title");
    EXPECT_NS_EQUAL(presentedNotification.get().body, @"Body text");
    EXPECT_NS_EQUAL(presentedNotification.get().subtitle, @"Secondary line");
    EXPECT_EQ(presentedNotification.get().buttons.count, 2UL);
    EXPECT_NS_EQUAL(presentedNotification.get().buttons.firstObject.title, @"First");
    EXPECT_NS_EQUAL(presentedNotification.get().buttons.lastObject.title, @"Second");
}

TEST_F(WKWebExtensionAPINotifications, CreateOmitsSubtitleAndButtonsWhenNotProvided)
{
    auto *script = @[
        @"await browser.notifications.create('minimal', { type: 'basic', title: 'T', message: 'M' })",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    RetainPtr<_WKWebExtensionNotification> presentedNotification;
    auto *presentedNotificationPtr = &presentedNotification;
    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *notification) {
        *presentedNotificationPtr = notification;
    };

    [manager loadAndRun];

    EXPECT_NOT_NULL(presentedNotification.get());
    EXPECT_NULL(presentedNotification.get().subtitle);
    EXPECT_EQ(presentedNotification.get().buttons.count, 0UL);
}

TEST_F(WKWebExtensionAPINotifications, CreateGeneratesIdentifierWhenOmitted)
{
    auto *script = @[
        @"const identifier = await browser.notifications.create({ type: 'basic', title: 'T', message: 'M' })",
        @"browser.test.assertTrue(typeof identifier === 'string' && identifier.length > 0, 'create() should resolve with a generated identifier')",
        @"browser.test.sendMessage('created', identifier)",
    ];

    auto manager = getManagerFor(script);

    RetainPtr<_WKWebExtensionNotification> presentedNotification;
    auto *presentedNotificationPtr = &presentedNotification;
    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *notification) {
        *presentedNotificationPtr = notification;
    };

    [manager load];

    id resolvedIdentifier = [manager runUntilTestMessage:@"created"];

    EXPECT_NOT_NULL(presentedNotification.get());
    EXPECT_GT([presentedNotification.get().identifier length], 0UL);
    EXPECT_NS_EQUAL(presentedNotification.get().identifier, resolvedIdentifier);
}

TEST_F(WKWebExtensionAPINotifications, CreateRejectsWhenBrowserReportsError)
{
    auto *script = @[
        @"await browser.test.assertRejects(browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' }), /notifications\\.create/i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(notificationsManifest, @{ @"background.js": Util::constructScript(script) }, notificationsConfig);
}

TEST_F(WKWebExtensionAPINotifications, UpdateReturnsFalseForUnknownIdentifier)
{
    auto *script = @[
        @"const wasUpdated = await browser.notifications.update('does-not-exist', { message: 'M' })",
        @"browser.test.assertFalse(wasUpdated, 'update() should resolve false when no notification matches')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    __block bool updateDelegateCalled = false;
    manager.get().internalDelegate.updateNotification = ^(_WKWebExtensionNotification *) {
        updateDelegateCalled = true;
    };

    [manager loadAndRun];

    EXPECT_FALSE(updateDelegateCalled);
}

TEST_F(WKWebExtensionAPINotifications, UpdateDoesNotRequireCreateKeys)
{
    auto *script = @[
        @"await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })",
        @"await browser.notifications.update('note', {})",
        @"await browser.notifications.update('note', { title: 'Only title' })",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *) { };
    manager.get().internalDelegate.updateNotification = ^(_WKWebExtensionNotification *) { };

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPINotifications, UpdateMergesSuppliedFieldsAndRePresentsThroughDelegate)
{
    auto *script = @[
        @"await browser.notifications.create('note', {",
        @"  type: 'basic',",
        @"  title: 'Original Title',",
        @"  message: 'Original body',",
        @"  contextMessage: 'Original subtitle',",
        @"  buttons: [ { title: 'Only' } ],",
        @"})",
        @"const wasUpdated = await browser.notifications.update('note', { message: 'New body' })",
        @"browser.test.assertTrue(wasUpdated, 'update() should resolve true for an existing notification')",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *) { };

    RetainPtr<_WKWebExtensionNotification> updatedNotification;
    auto *updatedNotificationPtr = &updatedNotification;
    manager.get().internalDelegate.updateNotification = ^(_WKWebExtensionNotification *notification) {
        *updatedNotificationPtr = notification;
    };

    [manager loadAndRun];

    EXPECT_NOT_NULL(updatedNotification.get());
    EXPECT_EQ(updatedNotification.get().webExtensionContext, manager.get().context);
    EXPECT_NS_EQUAL(updatedNotification.get().identifier, @"note");
    EXPECT_NS_EQUAL(updatedNotification.get().title, @"Original Title");
    EXPECT_NS_EQUAL(updatedNotification.get().body, @"New body");
    EXPECT_NS_EQUAL(updatedNotification.get().subtitle, @"Original subtitle");
    EXPECT_EQ(updatedNotification.get().buttons.count, 1UL);
    EXPECT_NS_EQUAL(updatedNotification.get().buttons.firstObject.title, @"Only");
}

TEST_F(WKWebExtensionAPINotifications, UpdateRejectsWhenBrowserReportsError)
{
    auto *script = @[
        @"await browser.notifications.create('note', { type: 'basic', title: 'T', message: 'M' })",
        @"await browser.test.assertRejects(browser.notifications.update('note', { message: 'New body' }), /notifications\\.update/i)",
        @"browser.test.notifyPass()",
    ];

    auto manager = getManagerFor(script);

    manager.get().internalDelegate.presentNotification = ^(_WKWebExtensionNotification *) { };

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS_NOTIFICATIONS)
