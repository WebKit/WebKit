/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import "TestCocoa.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/_WKWebExtensionPermission.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/_WKWebExtensionPrivate.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionContext, DefaultPermissionChecks)
{
    // Extensions are expected to have no permissions or access by default.
    // Only Requested states should be reported with out any granting / denying.

    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"permissions": @[ ] } mutableCopy];
    _WKWebExtension *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    _WKWebExtensionContext *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"https://*.example.com/*" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"<all_urls>" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"*://*/*" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"manifest_version"] = @3;
    testManifestDictionary[@"permissions"] = @[ ];
    testManifestDictionary[@"host_permissions"] = @[ ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs" ];
    testManifestDictionary[@"host_permissions"] = @[ @"https://*.example.com/*" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"host_permissions"] = @[ @"<all_urls>" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"host_permissions"] = @[ @"*://*/*" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], _WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);
}

TEST(WKWebExtensionContext, PermissionGranting)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];
    testManifestDictionary[@"permissions"] = @[ @"tabs", @"https://*.example.com/*" ];

    // Test defaults.
    _WKWebExtension *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    _WKWebExtensionContext *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_EQ(testExtension.errors.count, 0ul);;
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionTabs], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:_WKWebExtensionPermissionCookies], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Grant a specific permission.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionTabs];

    EXPECT_TRUE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Grant a specific URL.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);

    // Deny a specific URL.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);

    // Deny a specific permission.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusDeniedExplicitly forPermission:_WKWebExtensionPermissionTabs];

    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 1ul);

    // Reset all permissions.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusUnknown forURL:[NSURL URLWithString:@"https://example.com/"]];
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusUnknown forPermission:_WKWebExtensionPermissionTabs];

    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Grant the all URLs match pattern.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:_WKWebExtensionMatchPattern.allURLsMatchPattern];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Reset a specific URL (should do nothing).
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusUnknown forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Deny a specific URL (should do nothing).
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusDeniedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Reset all match patterns.
    testContext.grantedPermissionMatchPatterns = @{ };
    testContext.deniedPermissionMatchPatterns = @{ };

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Mass grant with the permission setter.
    testContext.grantedPermissions = @{ _WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_TRUE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Mass deny with the permission setter.
    testContext.deniedPermissions = @{ _WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.deniedPermissions.count, 1ul);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);

    // Mass grant with the permission setter again.
    testContext.grantedPermissions = @{ _WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_TRUE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);

    // Mass grant with the match pattern setter.
    testContext.grantedPermissionMatchPatterns = @{ _WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Mass deny with the match pattern setter.
    testContext.deniedPermissionMatchPatterns = @{ _WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);

    // Mass grant with the match pattern setter again.
    testContext.grantedPermissionMatchPatterns = @{ _WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Reset all permissions.
    testContext.grantedPermissionMatchPatterns = @{ };
    testContext.deniedPermissionMatchPatterns = @{ };
    testContext.grantedPermissions = @{ };
    testContext.deniedPermissions = @{ };

    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Test granting a match pattern that expire in 2 seconds.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:_WKWebExtensionMatchPattern.allURLsMatchPattern expirationDate:[NSDate dateWithTimeIntervalSinceNow:2]];

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);

    // Sleep until after the match pattern expires.
    sleep(3);

    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], _WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);

    // Test granting a permission that expire in 2 seconds.
    [testContext setPermissionStatus:_WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:_WKWebExtensionPermissionTabs expirationDate:[NSDate dateWithTimeIntervalSinceNow:2]];

    EXPECT_TRUE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Sleep until after the permission expires.
    sleep(3);

    EXPECT_FALSE([testContext hasPermission:_WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
