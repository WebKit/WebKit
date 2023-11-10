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
#import <WebKit/_WKWebExtensionCommand.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/_WKWebExtensionPermission.h>
#import <WebKit/_WKWebExtensionPrivate.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

TEST(WKWebExtensionContext, DefaultPermissionChecks)
{
    // Extensions are expected to have no permissions or access by default.
    // Only Requested states should be reported with out any granting / denying.

    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"permissions": @[ ] } mutableCopy];
    _WKWebExtension *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    _WKWebExtensionContext *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
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

TEST(WKWebExtensionContext, OptionsPageURLParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @"options.html"
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    auto *expectedOptionsURL = [NSURL URLWithString:@"options.html" relativeToURL:testContext.baseURL].absoluteURL;
    EXPECT_NS_EQUAL(testContext.optionsPageURL, expectedOptionsURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @123
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{
            @"page": @"options.html"
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    expectedOptionsURL = [NSURL URLWithString:@"options.html" relativeToURL:testContext.baseURL].absoluteURL;
    EXPECT_NS_EQUAL(testContext.optionsPageURL, expectedOptionsURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{
            @"page": @123
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @""
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);
}

TEST(WKWebExtensionContext, URLOverridesParsing)
{
    NSDictionary *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{
            @"newtab": @"newtab.html"
        }
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    auto *expectedURL = [NSURL URLWithString:@"newtab.html" relativeToURL:testContext.baseURL].absoluteURL;
    EXPECT_NS_EQUAL(testContext.overrideNewTabPageURL, expectedURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{
            @"newtab": @""
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{
            @"newtab": @123
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{
            @"newtab": @"newtab.html"
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    expectedURL = [NSURL URLWithString:@"newtab.html" relativeToURL:testContext.baseURL].absoluteURL;
    EXPECT_NS_EQUAL(testContext.overrideNewTabPageURL, expectedURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{
            @"newtab": @123
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{
            @"newtab": @""
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);
}

TEST(WKWebExtensionContext, CommandsParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @{
            @"toggle-feature": @{
                @"suggested_key": @{
                    @"default": @"Alt+Shift+U",
                    @"linux": @"Shift+Ctrl+U"
                },
                @"description": @"Send A Thing"
            },
            @"do-another-thing": @{
                @"suggested_key": @{
                    @"default": @"Alt+Shift+Y",
                    @"mac": @"Ctrl+Shift+Y",
                },
                @"description": @"Find A Thing"
            },
            @"special-command": @{
                @"suggested_key": @{
                    @"default": @"Alt+F10"
                },
                @"description": @"Do A Thing"
            },
            @"escape-command": @{
                @"suggested_key": @{
                    @"ios": @"MacCtrl+Down"
                },
                @"description": @"Be A Thing"
            },
            @"unassigned-command": @{
                @"description": @"Maybe A Thing"
            }
        }
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NS_EQUAL(testContext.webExtension.errors, @[ ]);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 5lu);

    _WKWebExtensionCommand *testCommand = nil;

    for (_WKWebExtensionCommand *command in testContext.commands) {
        if ([command.identifier isEqualToString:@"toggle-feature"]) {
            testCommand = command;

            EXPECT_NS_EQUAL(command.discoverabilityTitle, @"Send A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"u");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierAlternate | UIKeyModifierShift);
#endif
        } else if ([command.identifier isEqualToString:@"do-another-thing"]) {
            EXPECT_NS_EQUAL(command.discoverabilityTitle, @"Find A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"y");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierCommand | UIKeyModifierShift);
#endif
        } else if ([command.identifier isEqualToString:@"special-command"]) {
            EXPECT_NS_EQUAL(command.discoverabilityTitle, @"Do A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"\uF70D");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagOption);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierAlternate);
#endif
        } else if ([command.identifier isEqualToString:@"escape-command"]) {
            EXPECT_NS_EQUAL(command.discoverabilityTitle, @"Be A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"\uF701");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagControl);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierControl);
#endif
        } else if ([command.identifier isEqualToString:@"unassigned-command"]) {
            EXPECT_NS_EQUAL(command.discoverabilityTitle, @"Maybe A Thing");
            EXPECT_NULL(command.activationKey);
            EXPECT_EQ((uint32_t)command.modifierFlags, 0lu);
        }
    }

    EXPECT_NOT_NULL(testCommand);

    testCommand.activationKey = nil;

    EXPECT_NULL(testCommand.activationKey);
    EXPECT_EQ((uint32_t)testCommand.modifierFlags, 0lu);

    testCommand.activationKey = @"\uF70D";

    EXPECT_NS_EQUAL(testCommand.activationKey, @"\uF70D");
#if PLATFORM(MAC)
    EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
    EXPECT_EQ(testCommand.modifierFlags, UIKeyModifierAlternate | UIKeyModifierShift);
#endif

    testCommand.activationKey = @"M";

    EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
#if PLATFORM(MAC)
    EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
    EXPECT_EQ(testCommand.modifierFlags, UIKeyModifierAlternate | UIKeyModifierShift);
#endif

    testCommand.modifierFlags = 0;

    EXPECT_NULL(testCommand.activationKey);
    EXPECT_EQ((uint32_t)testCommand.modifierFlags, 0lu);

#if PLATFORM(MAC)
    testCommand.modifierFlags = NSEventModifierFlagCommand | NSEventModifierFlagShift;
#else
    testCommand.modifierFlags = UIKeyModifierCommand | UIKeyModifierShift;
#endif

    EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
#if PLATFORM(MAC)
    EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
    EXPECT_EQ(testCommand.modifierFlags, UIKeyModifierCommand | UIKeyModifierShift);
#endif

    @try {
        testCommand.activationKey = @"F10";
    } @catch (NSException *exception) {
        EXPECT_NS_EQUAL(exception.name, NSInternalInconsistencyException);
    } @finally {
        EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
    }

    @try {
        testCommand.modifierFlags = 1 << 16;
    } @catch (NSException *exception) {
        EXPECT_NS_EQUAL(exception.name, NSInternalInconsistencyException);
    } @finally {
#if PLATFORM(MAC)
        EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
        EXPECT_EQ(testCommand.modifierFlags, UIKeyModifierCommand | UIKeyModifierShift);
#endif
    }

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NS_EQUAL(testContext.webExtension.errors, @[ ]);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 0lu);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @{
            @"command-without-description": @{
                @"suggested_key": @{
                    @"default": @"Ctrl+Shift+X"
                }
            }
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_EQ(testContext.webExtension.errors.count, 1lu);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 0lu);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @[
            @"Invalid"
        ]
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[_WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_EQ(testContext.webExtension.errors.count, 1lu);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 0lu);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
