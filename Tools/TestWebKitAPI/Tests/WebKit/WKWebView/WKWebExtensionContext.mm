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

#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/TestCocoa.h"
#import "Helpers/cocoa/WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebExtensionCommand.h>
#import <WebKit/WKWebExtensionContextPrivate.h>
#import <WebKit/WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/WKWebExtensionPermission.h>
#import <WebKit/WKWebExtensionPrivate.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

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
#if PLATFORM(MAC)
            EXPECT_NS_EQUAL(command.activationKey, @"u");
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
            EXPECT_NS_EQUAL(command.activationKey, @"U");
            EXPECT_EQ(command.modifierFlags, UIKeyModifierAlternate | UIKeyModifierShift);
#endif
        } else if ([command.identifier isEqualToString:@"do-another-thing"]) {
            EXPECT_NS_EQUAL(command.title, @"Find A Thing");
#if PLATFORM(MAC)
            EXPECT_NS_EQUAL(command.activationKey, @"y");
            EXPECT_EQ(command.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
            EXPECT_NS_EQUAL(command.activationKey, @"Y");
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

#if PLATFORM(MAC)
    EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
    EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagOption | NSEventModifierFlagShift);
#else
    EXPECT_NS_EQUAL(testCommand.activationKey, @"M");
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

#if PLATFORM(MAC)
    EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
    EXPECT_EQ(testCommand.modifierFlags, NSEventModifierFlagCommand | NSEventModifierFlagShift);
#else
    EXPECT_NS_EQUAL(testCommand.activationKey, @"M");
    EXPECT_EQ(testCommand.modifierFlags, UIKeyModifierCommand | UIKeyModifierShift);
#endif

    @try {
        testCommand.activationKey = @"F10";
    } @catch (NSException *exception) {
        EXPECT_NS_EQUAL(exception.name, NSInternalInconsistencyException);
    } @finally {
#if PLATFORM(MAC)
        EXPECT_NS_EQUAL(testCommand.activationKey, @"m");
#else
        EXPECT_NS_EQUAL(testCommand.activationKey, @"M");
#endif
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

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionErrorResourceNotFound);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Unable to find \"non-existent-image.png\" in the extension’s resources. It is an invalid path.");
}

TEST(WKWebExtensionContext, TopLevelThrowInModuleBackground)
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
        },
    };

    auto *backgroundScript = @"throw new Error('Top level module error')";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Top level module error (background.js:1:16)");
}

TEST(WKWebExtensionContext, ReferenceErrorInBackground)
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
        },
    };

    auto *backgroundScript = @"undeclaredVariable.foo";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"ReferenceError: Can't find variable: undeclaredVariable (background.js:1:19)");
}

TEST(WKWebExtensionContext, CallingMissingBrowserAPIInBackground)
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
        },
    };

    auto *backgroundScript = @"browser.runtime.nonExistentMethod()";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"TypeError: browser.runtime.nonExistentMethod is not a function. (In 'browser.runtime.nonExistentMethod()', 'browser.runtime.nonExistentMethod' is undefined) (background.js:1:34)");
}

TEST(WKWebExtensionContext, UncaughtScriptErrorInBackground)
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
        },
    };

    // Use setTimeout so the throw happens as a runtime uncaught exception, not a module evaluation rejection.
    auto *backgroundScript = @"setTimeout(() => { throw new Error('Test uncaught error') }, 0)";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Test uncaught error (background.js:1:58)");
}

TEST(WKWebExtensionContext, UnhandledPromiseRejectionInBackground)
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
        },
    };

    auto *backgroundScript = @"Promise.reject(new Error('Test rejection'))";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Test rejection");
}

TEST(WKWebExtensionContext, UncaughtScriptErrorInServiceWorkerBackground)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"background": @{
            @"service_worker": @"background.js",
        },
    };

    auto *backgroundScript = @"setTimeout(() => { throw new Error('Service worker uncaught error') }, 0)";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Service worker uncaught error (background.js:1:68)");
}

TEST(WKWebExtensionContext, UnhandledPromiseRejectionInServiceWorkerBackground)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"background": @{
            @"service_worker": @"background.js",
        },
    };

    auto *backgroundScript = @"Promise.reject(new Error('Service worker rejection'))";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Service worker rejection");
}

TEST(WKWebExtensionContext, SyntaxErrorInBackground)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"persistent": @NO,
        },
    };

    // A bare syntax error: fails at parse time, so exception->stack() will be empty.
    // Validates that the source URL fallback via error.sourceURL is correctly reported.
    auto *backgroundScript = @")(";

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"SyntaxError: Unexpected token ')' (background.js:1)");
}

TEST(WKWebExtensionContext, NoErrorForCaughtExceptionsInBackground)
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
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        // Caught synchronous exception — should not populate errors.
        @"try { throw new Error('Caught error') } catch (e) {}",

        // Handled promise rejection — should not populate errors.
        @"Promise.reject(new Error('Handled rejection')).catch(() => {})",

        @"browser.test.notifyPass()",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });
    [manager run];

    EXPECT_EQ(manager.get().context.errors.count, 0ul);
}

TEST(WKWebExtensionContext, UncaughtScriptErrorInContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content.js" ],
        } ],
    };

    auto *contentScript = @"throw new Error('Content script error')";

    auto manager = Util::loadExtension(manifest, @{ @"content.js": contentScript });

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Content script error (content.js:1:40)");
}

TEST(WKWebExtensionContext, UncaughtScriptErrorInMainWorldContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content.js" ],
            @"world": @"MAIN",
        } ],
    };

    auto *contentScript = @"throw new Error('Main world error')";

    auto manager = Util::loadExtension(manifest, @{ @"content.js": contentScript });

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Main world error (content.js:1:36)");
}

TEST(WKWebExtensionContext, PageScriptErrorNotReportedToExtension)
{
    // Verify that errors thrown by page scripts (not extension scripts) are not reported to the extension,
    // even when a main-world content script is active on the same page.
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, "<script>throw new Error('Page error')</script>"_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"content_scripts": @[ @{
            @"matches": @[ @"*://localhost/*" ],
            @"js": @[ @"content.js" ],
            @"world": @"MAIN",
        } ],
    };

    // The content script itself does not throw; only the page script does.
    auto *contentScript = @"let result = 2 + 2;";

    auto manager = Util::loadExtension(manifest, @{ @"content.js": contentScript });

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager runForTimeInterval:3];

    EXPECT_NS_EQUAL(manager.get().context.errors, @[ ]);
}

TEST(WKWebExtensionContext, UncaughtScriptErrorInEventListener)
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
        },
        @"action": @{ },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.action.onClicked.addListener((tab) => {",
        @"  throw new Error('Error in event listener')",
        @"})",

        @"browser.test.sendMessage('Ready')",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Ready"];

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Error in event listener (background.js:2:18)");
}

TEST(WKWebExtensionContext, TopLevelThrowInPopup)
{
    auto *manifest = @{
        @"manifest_version": @3,
        @"name": @"Test Extension",
        @"description": @"Test",
        @"version": @"1.0",
        @"action": @{
            @"default_popup": @"popup.html",
        },
    };

    auto *popupScript = Util::constructScript(@[
        @"browser.test.sendMessage('Ready')",
        @"throw new Error('Popup error')",
    ]);

    auto *resources = @{
        @"popup.html": @"<script type='module' src='popup.js'></script>",
        @"popup.js": popupScript,
    };

    auto manager = Util::loadExtension(manifest, resources);

    manager.get().internalDelegate.presentPopupForAction = ^(WKWebExtensionAction *action) {
        EXPECT_NOT_NULL(action.popupWebView);
    };

    [manager.get().context performActionForTab:manager.get().defaultTab];

    [manager runUntilTestMessage:@"Ready"];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Error: Popup error (popup.js:2:16)");
}

TEST(WKWebExtensionContext, ConsoleErrorReportedNotLogOrWarn)
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
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"console.log('This is a log message')",
        @"console.warn('This is a warning message')",
        @"console.error('Failed to inject script:', new Error('Missing permission'))",

        @"browser.test.sendMessage('Ready')",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Ready"];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Failed to inject script: Error: Missing permission (background.js:3:14)");
}

TEST(WKWebExtensionContext, ConsoleAssertWithMessage)
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
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"console.assert(true, 'This should not appear')",
        @"console.assert(false, 'Something went wrong')",

        @"browser.test.sendMessage('Ready')",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Ready"];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"Something went wrong (background.js:2:15)");
}

TEST(WKWebExtensionContext, ConsoleAssertWithoutMessage)
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
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        @"console.assert(true)",
        @"console.assert(false)",

        @"browser.test.sendMessage('Ready')",
    ]);

    auto manager = Util::loadExtension(manifest, @{ @"background.js": backgroundScript });

    [manager runUntilTestMessage:@"Ready"];

    [manager runUntilContextError];

    EXPECT_EQ(manager.get().context.errors.count, 1ul);

    auto *error = manager.get().context.errors.firstObject;
    EXPECT_EQ(error.code, WKWebExtensionContextErrorScriptExecutionError);
    EXPECT_NS_EQUAL(error.localizedDescription, @"(background.js:2:15)");
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
