/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionCommand.h>

namespace TestWebKitAPI {

NSString * const NSHTTPCookieHTTPOnly = @"HttpOnly";

static auto *cookiesManifest = @{
    @"manifest_version": @3,

    @"name": @"Test Cookies",
    @"description": @"Test Cookies",
    @"version": @"1.0",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"permissions": @[ @"cookies" ],
};

TEST(WKWebExtensionAPICookies, ErrorsRead)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 123, name: 'Test' }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: '', name: 'Test' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'bad', name: 'Test' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', name: '' }), /'name' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.get({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: '' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ url: 'bad' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ domain: 123 }), /'domain' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ path: 123 }), /'path' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ secure: 'bad' }), /'secure' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ session: 'bad' }), /'session' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.getAll({ storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(cookiesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPICookies, ErrorsWrite)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: '' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'bad' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', value: 123 }), /'value' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', domain: 123 }), /'domain' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', path: 123 }), /'path' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', secure: 'bad' }), /'secure' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', httpOnly: 'bad' }), /'httpOnly' is expected to be a boolean, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', sameSite: 123 }), /'sameSite' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', expirationDate: 'bad' }), /'expirationDate' is expected to be a number, but a string was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.set({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 123 }), /'url' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: '', name: 'Test' }), /'url' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'bad', name: 'Test' }), /'url' value is invalid, because 'bad' is not a valid URL/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', name: 123 }), /'name' is expected to be a string, but a number was provided/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', name: '' }), /'name' value is invalid, because it must not be empty/i)",
        @"browser.test.assertThrows(() => browser.cookies.remove({ url: 'http://example.com', storeId: 123 }), /'storeId' is expected to be a string, but a number was provided/i)",

        @"browser.test.notifyPass()",
    ]);

    Util::loadAndRunExtension(cookiesManifest, @{ @"background.js": backgroundScript });
}

TEST(WKWebExtensionAPICookies, GetAllCookieStores)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const cookieStores = await browser.test.assertSafeResolve(() => browser.cookies.getAllCookieStores())",
        @"browser.test.assertEq(cookieStores?.length, 1, 'Should have one cookie store')",

        @"const defaultStore = cookieStores?.find(store => !store.incognito)",
        @"const privateStore = cookieStores?.find(store => store.incognito)",

        @"browser.test.assertEq(typeof defaultStore?.id, 'string', 'Default store id should be a string')",
        @"browser.test.assertTrue(Array.isArray(defaultStore?.tabIds), 'Default store tabIds should be an array')",
        @"browser.test.assertEq(defaultStore?.tabIds?.length, 1, 'Default store should have one tabId')",
        @"browser.test.assertEq(defaultStore?.incognito, false, 'Default store should not be incognito')",

        @"browser.test.assertEq(privateStore, undefined, 'Private store should be undefined')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager openNewWindowUsingPrivateBrowsing:YES];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetAllCookieStoresWithPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const cookieStores = await browser.test.assertSafeResolve(() => browser.cookies.getAllCookieStores())",
        @"browser.test.assertEq(cookieStores?.length, 2, 'Should have two cookie stores')",

        @"const defaultStore = cookieStores?.find(store => !store.incognito)",
        @"const privateStore = cookieStores?.find(store => store.incognito)",

        @"browser.test.assertEq(typeof defaultStore?.id, 'string', 'Default store id should be a string')",
        @"browser.test.assertTrue(Array.isArray(defaultStore?.tabIds), 'Default store tabIds should be an array')",
        @"browser.test.assertEq(defaultStore?.tabIds?.length, 1, 'Default store should have one tabId')",
        @"browser.test.assertEq(defaultStore?.incognito, false, 'Default store should not be incognito')",

        @"browser.test.assertEq(typeof privateStore?.id, 'string', 'Private store id should be a string')",
        @"browser.test.assertTrue(Array.isArray(privateStore?.tabIds), 'Private store tabIds should be an array')",
        @"browser.test.assertEq(privateStore?.tabIds.length, 1, 'Private store should have one tabId')",
        @"browser.test.assertEq(privateStore?.incognito, true, 'Private store should be incognito')",

        @"browser.test.assertTrue(defaultStore?.id !== privateStore?.id, 'The store ids should be different')",
        @"browser.test.assertTrue(defaultStore?.tabIds[0] !== privateStore?.tabIds[0], 'The tabIds for each store should be different')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().context.hasAccessToPrivateData = YES;

    [manager openNewWindowUsingPrivateBrowsing:YES];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetAll)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const cookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ }))",
        @"browser.test.assertTrue(Array.isArray(cookies), 'Cookies should be an array')",
        @"browser.test.assertTrue(cookies?.length >= 2, 'There should be 2 or more cookies')",

        @"const cookie1 = cookies.find(cookie => cookie.name === 'testCookie1')",
        @"browser.test.assertEq(typeof cookie1, 'object')",
        @"browser.test.assertEq(cookie1?.value, 'value1')",
        @"browser.test.assertEq(cookie1?.domain, '.example.com')",
        @"browser.test.assertEq(cookie1?.path, '/')",
        @"browser.test.assertEq(cookie1?.secure, false)",

        @"const cookie2 = cookies.find(cookie => cookie.name === 'testCookie2')",
        @"browser.test.assertEq(typeof cookie2, 'object')",
        @"browser.test.assertEq(cookie2?.value, 'value2')",
        @"browser.test.assertEq(cookie2?.domain, 'www.example.com')",
        @"browser.test.assertEq(cookie2?.path, '/test')",
        @"browser.test.assertEq(cookie2?.secure, true)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *cookie1 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie1",
        NSHTTPCookieValue: @"value1",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/"
    }];

    auto *cookie2 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie2",
        NSHTTPCookieValue: @"value2",
        NSHTTPCookieDomain: @"www.example.com",
        NSHTTPCookiePath: @"/test",
        NSHTTPCookieSecure: @YES
    }];

    auto cookieGroup = dispatch_group_create();

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie1 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie2 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    __block bool done = false;
    dispatch_group_notify(cookieGroup, dispatch_get_main_queue(), ^{
        done = true;
    });

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetAllIncognito)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allCookieStores = await browser.test.assertSafeResolve(() => browser.cookies.getAllCookieStores())",
        @"const incognitoStore = allCookieStores.find(store => store.incognito)",
        @"browser.test.assertEq(incognitoStore, undefined, 'Incognito store should not be found')",

        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-1' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-2' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-3' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-4' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-5' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-6' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-7' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-8' }), /cookie store not found/i)",
        @"await browser.test.assertRejects(browser.cookies.getAll({ storeId: 'ephemeral-9' }), /cookie store not found/i)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.private.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *privateWindow = [manager openNewWindowUsingPrivateBrowsing:YES];
    auto *cookieStore = privateWindow.activeTab.webView.configuration.websiteDataStore.httpCookieStore;

    auto *cookie1 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie1",
        NSHTTPCookieValue: @"value1",
        NSHTTPCookieDomain: @".private.com",
        NSHTTPCookiePath: @"/"
    }];

    auto *cookie2 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie2",
        NSHTTPCookieValue: @"value2",
        NSHTTPCookieDomain: @"www.private.com",
        NSHTTPCookiePath: @"/test",
        NSHTTPCookieSecure: @YES
    }];

    auto cookieGroup = dispatch_group_create();

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie1 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie2 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    __block bool done = false;
    dispatch_group_notify(cookieGroup, dispatch_get_main_queue(), ^{
        done = true;
    });

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetAllIncognitoWithPrivateAccess)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const allCookieStores = await browser.test.assertSafeResolve(() => browser.cookies.getAllCookieStores())",
        @"const incognitoStore = allCookieStores.find(store => store.incognito)",
        @"browser.test.assertEq(typeof incognitoStore, 'object', 'Incognito store should be found')",

        @"const cookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ storeId: incognitoStore?.id }))",
        @"browser.test.assertTrue(Array.isArray(cookies), 'Cookies should be an array')",
        @"browser.test.assertEq(cookies.length, 2)",

        @"const cookie1 = cookies.find(cookie => cookie.name === 'testCookie1')",
        @"browser.test.assertEq(typeof cookie1, 'object')",
        @"browser.test.assertEq(cookie1?.value, 'value1')",
        @"browser.test.assertEq(cookie1?.domain, '.private.com')",
        @"browser.test.assertEq(cookie1?.path, '/')",
        @"browser.test.assertEq(cookie1?.secure, false)",

        @"const cookie2 = cookies.find(cookie => cookie.name === 'testCookie2')",
        @"browser.test.assertEq(typeof cookie2, 'object')",
        @"browser.test.assertEq(cookie2?.value, 'value2')",
        @"browser.test.assertEq(cookie2?.domain, 'www.private.com')",
        @"browser.test.assertEq(cookie2?.path, '/test')",
        @"browser.test.assertEq(cookie2?.secure, true)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.private.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    manager.get().context.hasAccessToPrivateData = YES;

    auto *privateWindow = [manager openNewWindowUsingPrivateBrowsing:YES];
    auto *cookieStore = privateWindow.activeTab.webView.configuration.websiteDataStore.httpCookieStore;

    auto *cookie1 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie1",
        NSHTTPCookieValue: @"value1",
        NSHTTPCookieDomain: @".private.com",
        NSHTTPCookiePath: @"/"
    }];

    auto *cookie2 = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie2",
        NSHTTPCookieValue: @"value2",
        NSHTTPCookieDomain: @"www.private.com",
        NSHTTPCookiePath: @"/test",
        NSHTTPCookieSecure: @YES
    }];

    auto cookieGroup = dispatch_group_create();

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie1 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:cookie2 completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    __block bool done = false;
    dispatch_group_notify(cookieGroup, dispatch_get_main_queue(), ^{
        done = true;
    });

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetAllWithFilters)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const nameCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ name: 'nameCookie' }))",
        @"browser.test.assertEq(nameCookies?.length, 1, 'Should find one cookie with name `nameCookie`')",
        @"browser.test.assertEq(nameCookies?.[0]?.value, 'nameValue', 'Value of `nameCookie` should be `nameValue`')",

        @"const sessionCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ session: true }))",
        @"browser.test.assertEq(sessionCookies?.length, 1, 'Should find one session cookie')",
        @"browser.test.assertEq(sessionCookies?.[0]?.name, 'sessionCookie', 'Name of session cookie should be `sessionCookie`')",

        @"const secureCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ secure: true }))",
        @"browser.test.assertEq(secureCookies?.length, 1, 'Should find one secure cookie')",
        @"browser.test.assertEq(secureCookies?.[0]?.name, 'secureCookie', 'Name of secure cookie should be `secureCookie`')",

        @"const pathCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ path: '/testPath' }))",
        @"browser.test.assertEq(pathCookies?.length, 1, 'Should find one cookie with path `/testPath`')",
        @"browser.test.assertEq(pathCookies?.[0]?.name, 'pathCookie', 'Name of path cookie should be `pathCookie`')",

        @"const domainCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ domain: '.example.com' }))",
        @"browser.test.assertEq(domainCookies?.length, 4, 'Should find four cookies for domain `.example.com`')",

        @"const urlCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ url: 'http://www.example.com/testPath' }))",
        @"browser.test.assertEq(urlCookies?.length, 3, 'Should find three cookies for URL `http://www.example.com/testPath`')",

        @"const nameAndSecureCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ name: 'secureCookie', secure: true }))",
        @"browser.test.assertEq(nameAndSecureCookies.length, 1, 'Should find one cookie with name `secureCookie` and secure attribute')",
        @"browser.test.assertEq(nameAndSecureCookies[0].value, 'secureValue', 'Value of secure cookie should be `secureValue`')",

        @"const domainAndPathCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ domain: 'www.example.com', path: '/testPath' }))",
        @"browser.test.assertEq(domainAndPathCookies.length, 1, 'Should find one cookie for domain `www.example.com` and path `/testPath`')",
        @"browser.test.assertEq(domainAndPathCookies[0].name, 'pathCookie', 'Name of cookie should be `pathCookie`')",

        @"const allCookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ }))",
        @"browser.test.assertEq(allCookies?.length, 4, 'Should find all cookies')",
        @"const expectedCookieNames = ['nameCookie', 'sessionCookie', 'secureCookie', 'pathCookie']",
        @"expectedCookieNames.forEach(name => {",
        @"  browser.test.assertTrue(allCookies.some(cookie => cookie.name === name), `Should find cookie with name ${name}`)",
        @"})",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *nameCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"nameCookie",
        NSHTTPCookieValue: @"nameValue",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieExpires: [NSDate dateWithTimeIntervalSinceNow:30]
    }];

    auto *sessionCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"sessionCookie",
        NSHTTPCookieValue: @"sessionValue",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieHTTPOnly: @YES
    }];

    auto *secureCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"secureCookie",
        NSHTTPCookieValue: @"secureValue",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/",
        NSHTTPCookieSecure: @YES,
        NSHTTPCookieExpires: [NSDate dateWithTimeIntervalSinceNow:30]
    }];

    auto *pathCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"pathCookie",
        NSHTTPCookieValue: @"pathValue",
        NSHTTPCookieDomain: @"www.example.com",
        NSHTTPCookiePath: @"/testPath",
        NSHTTPCookieExpires: [NSDate dateWithTimeIntervalSinceNow:30],
        NSHTTPCookieHTTPOnly: @YES
    }];

    auto cookieGroup = dispatch_group_create();

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:nameCookie completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:sessionCookie completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:secureCookie completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    dispatch_group_enter(cookieGroup);
    [cookieStore setCookie:pathCookie completionHandler:^{
        dispatch_group_leave(cookieGroup);
    }];

    __block bool done = false;
    dispatch_group_notify(cookieGroup, dispatch_get_main_queue(), ^{
        done = true;
    });

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, RemoveCookie)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const removedCookie = await browser.test.assertSafeResolve(() => browser.cookies.remove({ url: 'http://www.example.com/', name: 'testCookie' }))",
        @"browser.test.assertEq(removedCookie?.name, 'testCookie', 'Removed cookie name should be `testCookie`')",
        @"browser.test.assertEq(removedCookie?.value, 'value1', 'Removed cookie value should be `value1`')",
        @"browser.test.assertEq(removedCookie?.domain, 'www.example.com', 'Removed cookie domain should be `www.example.com`')",
        @"browser.test.assertEq(removedCookie?.path, '/', 'Removed cookie path should be `/`')",

        @"const cookies = await browser.test.assertSafeResolve(() => browser.cookies.getAll({ }))",
        @"const cookieExists = cookies.some(cookie => cookie.name === 'testCookie')",
        @"browser.test.assertFalse(cookieExists, 'testCookie should be removed')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *testCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie",
        NSHTTPCookieValue: @"value1",
        NSHTTPCookieDomain: @"www.example.com",
        NSHTTPCookiePath: @"/"
    }];

    __block bool done = false;
    [cookieStore setCookie:testCookie completionHandler:^{
        done = true;
    }];

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, RemoveCookieNotFound)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const removedCookie = await browser.test.assertSafeResolve(() => browser.cookies.remove({ url: 'http://www.example.com/', name: 'notFoundName' }))",
        @"browser.test.assertEq(removedCookie, null)",

        @"const testCookie = await browser.test.assertSafeResolve(() => browser.cookies.get({ url: 'http://www.example.com/', name: 'testCookie' }))",
        @"browser.test.assertEq(typeof testCookie, 'object', 'testCookie should still exist')",
        @"browser.test.assertEq(testCookie?.value, 'value1', 'testCookie should have the correct value')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *testCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testCookie",
        NSHTTPCookieValue: @"value1",
        NSHTTPCookieDomain: @"www.example.com",
        NSHTTPCookiePath: @"/"
    }];

    __block bool done = false;
    [cookieStore setCookie:testCookie completionHandler:^{
        done = true;
    }];

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, SetCookie)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const initialCookies = await browser.cookies.getAll({ url: 'http://www.example.com/' })",
        @"browser.test.assertTrue(!initialCookies.some(cookie => cookie.name === 'testCookie1' || cookie.name === 'testCookie2'), 'Cookies should not exist initially')",
        @"browser.test.assertTrue(initialCookies.some(cookie => cookie.name === 'replacedCookie'), 'Replaced cookie should exist')",

        @"await browser.test.assertSafeResolve(() => browser.cookies.set({ url: 'http://www.example.com/', name: 'testCookie1', value: 'value1' }))",
        @"await browser.test.assertSafeResolve(() => browser.cookies.set({ url: 'http://example.com/', name: 'testCookie2', value: 'value2', path: '/', domain: '.example.com' }))",

        @"const cookies = await browser.cookies.getAll({ url: 'http://www.example.com/' })",
        @"const cookie1 = cookies.find(cookie => cookie.name === 'testCookie1')",
        @"const cookie2 = cookies.find(cookie => cookie.name === 'testCookie2')",
        @"browser.test.assertEq(cookie1?.value, 'value1', 'testCookie1 should have the correct value')",
        @"browser.test.assertEq(cookie2?.value, 'value2', 'testCookie2 should have the correct value')",

        @"const replacedCookie = await browser.test.assertSafeResolve(() => browser.cookies.set({ url: 'http://www.example.com/', name: 'replacedCookie', value: 'newValue' }))",
        @"browser.test.assertEq(replacedCookie?.value, 'newValue', 'replacedCookie should have the new value')",
        @"browser.test.assertEq(replacedCookie?.domain, 'www.example.com', 'replacedCookie should have the new domain')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *testCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"replacedCookie",
        NSHTTPCookieValue: @"originalValue",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/"
    }];

    __block bool done = false;
    [cookieStore setCookie:testCookie completionHandler:^{
        done = true;
    }];

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, SetCookieWithExpirationDate)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const cookieName = 'token'",
        @"const testUrl = 'http://www.example.com/'",
        @"const expirationDate = Math.floor(Date.now() / 1000) + 2",

        @"await browser.cookies.set({",
        @"  url: testUrl,",
        @"  name: cookieName,",
        @"  value: 'value',",
        @"  domain: 'www.example.com',",
        @"  sameSite: 'lax',",
        @"  expirationDate",
        @"})",

        @"let cookie = await browser.cookies.get({",
        @"  url: testUrl,",
        @"  name: cookieName",
        @"})",

        @"browser.test.assertEq(cookie?.value, 'value', 'Cookie should be set successfully')",
        @"browser.test.assertEq(cookie?.expirationDate, expirationDate, 'expirationDate should be the same as it was set')",

        @"await new Promise(resolve => setTimeout(resolve, 2500))",

        @"cookie = await browser.cookies.get({",
        @"  url: testUrl,",
        @"  name: cookieName",
        @"})",

        @"browser.test.assertEq(cookie, null, 'Cookie should be expired and no longer available')",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, GetCookie)
{
    auto *backgroundScript = Util::constructScript(@[
        @"const cookie = await browser.test.assertSafeResolve(() => browser.cookies.get({ url: 'http://www.example.com/', name: 'testGetCookie' }))",
        @"browser.test.assertEq(cookie?.name, 'testGetCookie', 'The cookie name should match')",
        @"browser.test.assertEq(cookie?.value, 'getValue', 'The cookie value should match')",
        @"browser.test.assertEq(cookie?.domain, '.example.com', 'The cookie domain should match')",
        @"browser.test.assertEq(cookie?.path, '/', 'The cookie path should match')",

        @"const notFoundCookie = await browser.test.assertSafeResolve(() => browser.cookies.get({ url: 'http://www.example.com/', name: 'notFoundName' }))",
        @"browser.test.assertEq(notFoundCookie, null)",

        @"browser.test.notifyPass()"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    auto *cookieStore = WKWebsiteDataStore.defaultDataStore.httpCookieStore;

    auto *testCookie = [NSHTTPCookie cookieWithProperties:@{
        NSHTTPCookieName: @"testGetCookie",
        NSHTTPCookieValue: @"getValue",
        NSHTTPCookieDomain: @".example.com",
        NSHTTPCookiePath: @"/"
    }];

    __block bool done = false;
    [cookieStore setCookie:testCookie completionHandler:^{
        done = true;
    }];

    Util::run(&done);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPICookies, ChangedEvent)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.cookies.onChanged.addListener(() => {",
        @"  browser.test.notifyPass()",
        @"})",

        @"await browser.test.assertSafeResolve(() => browser.cookies.set({ url: 'http://www.example.com/', name: 'testCookie', value: 'testValue' }))"
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:cookiesManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *matchPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/*"];
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:matchPattern];

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
