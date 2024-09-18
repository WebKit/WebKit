/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebExtensionCommand.h>
#import <WebKit/WKWebExtensionContextPrivate.h>
#import <WebKit/WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/WKWebExtensionPermission.h>
#import <WebKit/WKWebExtensionPrivate.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

TEST(WKWebExtensionContext, DefaultPermissionChecks)
{
    // Extensions are expected to have no permissions or access by default.
    // Only Requested states should be reported with out any granting / denying.

    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"permissions": @[ ] } mutableCopy];
    WKWebExtension *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    WKWebExtensionContext *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"https://*.example.com/*" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"<all_urls>" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs", @"*://*/*" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"manifest_version"] = @3;
    testManifestDictionary[@"permissions"] = @[ ];
    testManifestDictionary[@"host_permissions"] = @[ ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"permissions"] = @[ @"tabs" ];
    testManifestDictionary[@"host_permissions"] = @[ @"https://*.example.com/*" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"host_permissions"] = @[ @"<all_urls>" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    testManifestDictionary[@"host_permissions"] = @[ @"*://*/*" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://unknown.com/"]], WKWebExtensionContextPermissionStatusRequestedImplicitly);
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
    WKWebExtension *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    WKWebExtensionContext *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionCookies]);
    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE(testContext.hasAccessToAllHosts);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://webkit.org/"]]);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionTabs], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForPermission:WKWebExtensionPermissionCookies], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusUnknown);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Grant a specific permission.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionTabs];

    EXPECT_TRUE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Grant a specific URL.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);

    // Deny a specific URL.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);

    // Deny a specific permission.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forPermission:WKWebExtensionPermissionTabs];

    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 1ul);

    // Reset all permissions.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusUnknown forURL:[NSURL URLWithString:@"https://example.com/"]];
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusUnknown forPermission:WKWebExtensionPermissionTabs];

    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Grant the all URLs match pattern.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:WKWebExtensionMatchPattern.allURLsMatchPattern];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Reset a specific URL (should do nothing).
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusUnknown forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Deny a specific URL (should do nothing).
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusDeniedExplicitly forURL:[NSURL URLWithString:@"https://example.com/"]];

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusDeniedExplicitly);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://webkit.org/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);

    // Reset all match patterns.
    testContext.grantedPermissionMatchPatterns = @{ };
    testContext.deniedPermissionMatchPatterns = @{ };

    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Mass grant with the permission setter.
    testContext.grantedPermissions = @{ WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_TRUE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Mass deny with the permission setter.
    testContext.deniedPermissions = @{ WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.deniedPermissions.count, 1ul);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);

    // Mass grant with the permission setter again.
    testContext.grantedPermissions = @{ WKWebExtensionPermissionTabs: NSDate.distantFuture };

    EXPECT_TRUE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissions.count, 0ul);

    // Mass grant with the match pattern setter.
    testContext.grantedPermissionMatchPatterns = @{ WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 0ul);

    // Mass deny with the match pattern setter.
    testContext.deniedPermissionMatchPatterns = @{ WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ(testContext.deniedPermissionMatchPatterns.count, 1ul);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);

    // Mass grant with the match pattern setter again.
    testContext.grantedPermissionMatchPatterns = @{ WKWebExtensionMatchPattern.allURLsMatchPattern: NSDate.distantFuture };

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);
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
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forMatchPattern:WKWebExtensionMatchPattern.allURLsMatchPattern expirationDate:[NSDate dateWithTimeIntervalSinceNow:2]];

    EXPECT_TRUE(testContext.hasAccessToAllURLs);
    EXPECT_TRUE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusGrantedImplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 1ul);

    // Sleep until after the match pattern expires.
    sleep(3);

    EXPECT_FALSE(testContext.hasAccessToAllURLs);
    EXPECT_FALSE([testContext hasAccessToURL:[NSURL URLWithString:@"https://example.com/"]]);
    EXPECT_EQ([testContext permissionStatusForURL:[NSURL URLWithString:@"https://example.com/"]], WKWebExtensionContextPermissionStatusRequestedExplicitly);
    EXPECT_EQ(testContext.grantedPermissionMatchPatterns.count, 0ul);

    // Test granting a permission that expire in 2 seconds.
    [testContext setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:WKWebExtensionPermissionTabs expirationDate:[NSDate dateWithTimeIntervalSinceNow:2]];

    EXPECT_TRUE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 1ul);

    // Sleep until after the permission expires.
    sleep(3);

    EXPECT_FALSE([testContext hasPermission:WKWebExtensionPermissionTabs]);
    EXPECT_EQ(testContext.grantedPermissions.count, 0ul);
}

TEST(WKWebExtensionContext, ContentScriptsParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ] } ];
    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    auto *webkitURL = [NSURL URLWithString:@"https://webkit.org/"];
    auto *exampleURL = [NSURL URLWithString:@"https://example.com/"];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_TRUE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ], @"exclude_matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_TRUE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"world": @"MAIN" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"user", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"author", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    // Invalid cases

    testManifestDictionary[@"content_scripts"] = @[ ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_FALSE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @{ @"invalid": @YES };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_FALSE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_FALSE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"run_at": @"invalid" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"world": @"INVALID" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"bad", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_TRUE(testContext.hasInjectedContent);
    EXPECT_FALSE([testContext hasInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testContext hasInjectedContentForURL:exampleURL]);
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    auto *expectedOptionsURL = [NSURL URLWithString:@"options.html" relativeToURL:testContext.baseURL].absoluteURL;
    EXPECT_NS_EQUAL(testContext.optionsPageURL, expectedOptionsURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @123
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @""
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.optionsPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NULL(testContext.overrideNewTabPageURL);
}

TEST(WKWebExtensionContext, CommandsParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",

        @"action": @{
            @"default_title": @"Test Action"
        },

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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    auto *testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_NS_EQUAL(testContext.webExtension.errors, @[ ]);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 6lu);

    WKWebExtensionCommand *testCommand = nil;

    for (WKWebExtensionCommand *command in testContext.commands) {
        if ([command.identifier isEqualToString:@"toggle-feature"]) {
            testCommand = command;

            EXPECT_NS_EQUAL(command.title, @"Send A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"u");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierAlternate | UIKeyModifierShift);
#endif
        } else if ([command.identifier isEqualToString:@"do-another-thing"]) {
            EXPECT_NS_EQUAL(command.title, @"Find A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"y");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierCommand | UIKeyModifierShift);
#endif
        } else if ([command.identifier isEqualToString:@"special-command"]) {
            EXPECT_NS_EQUAL(command.title, @"Do A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"\uF70D");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagOption);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierAlternate);
#endif
        } else if ([command.identifier isEqualToString:@"escape-command"]) {
            EXPECT_NS_EQUAL(command.title, @"Be A Thing");
            EXPECT_NS_EQUAL(command.activationKey, @"\uF701");
#if PLATFORM(MAC)
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagControl);
#else
            EXPECT_EQ(command.modifierFlags, UIKeyModifierControl);
#endif
        } else if ([command.identifier isEqualToString:@"unassigned-command"]) {
            EXPECT_NS_EQUAL(command.title, @"Maybe A Thing");
            EXPECT_NULL(command.activationKey);
            EXPECT_EQ((uint32_t)command.modifierFlags, 0lu);
        } else if ([command.identifier isEqualToString:@"_execute_action"]) {
            EXPECT_NS_EQUAL(command.title, @"Test Action");
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_EQ(testContext.errors.count, 1lu);
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    testContext = [[WKWebExtensionContext alloc] initForExtension:testExtension];
    EXPECT_EQ(testContext.errors.count, 1lu);
    EXPECT_NOT_NULL(testContext.commands);
    EXPECT_EQ(testContext.commands.count, 0lu);
}

TEST(WKWebExtensionContext, LoadNonExistentImage)
{
    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"const img = new Image()",
        @"img.src = 'non-existent-image.png'",

        @"img.onload = () => {",
        @"  browser.test.notifyFail('Image should not load successfully')",
        @"}",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    EXPECT_NS_EQUAL(manager.get().context.errors, @[ ]);

    [NSNotificationCenter.defaultCenter addObserverForName:WKWebExtensionContextErrorsDidUpdateNotification object:nil queue:nil usingBlock:^(NSNotification *notification) {
        auto *extensionContext = dynamic_objc_cast<WKWebExtensionContext>(notification.object);

        EXPECT_EQ(extensionContext.errors.count, 1ul);

        auto *predicate = [NSPredicate predicateWithFormat:@"localizedDescription CONTAINS %@", @"Unable to find \"non-existent-image.png\" in the extension’s resources."];
        auto *filteredErrors = [extensionContext.errors filteredArrayUsingPredicate:predicate];

        EXPECT_NS_EQUAL(extensionContext.errors, filteredErrors);

        [manager done];
    }];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
