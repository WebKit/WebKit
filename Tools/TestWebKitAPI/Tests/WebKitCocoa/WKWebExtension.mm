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
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/_WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/_WKWebExtensionPrivate.h>

namespace TestWebKitAPI {

static NSError *matchingError(NSArray<NSError *> *errors, _WKWebExtensionError code)
{
    for (NSError *error in errors) {
        if ([error.domain isEqualToString:_WKWebExtensionErrorDomain] && error.code == code)
            return error;
    }

    return nil;
}

TEST(WKWebExtension, BasicManifestParsing)
{
    auto parse = ^(NSString *manifestString) {
        return [[_WKWebExtension alloc] _initWithResources:@{ @"manifest.json": manifestString }].manifest;
    };

    EXPECT_NULL(parse(@""));
    EXPECT_NULL(parse(@"[]"));

    EXPECT_NS_EQUAL(parse(@"{}"), @{ });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 1 }"), @{ @"manifest_version" : @1 });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 4 }"), @{ @"manifest_version" : @4 });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": -2 }"), @{ @"manifest_version" : @(-2) });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 0 }"), @{ @"manifest_version" : @0 });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": \"1\" }"), @{ @"manifest_version" : @"1" });

    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 2 }"), @{ @"manifest_version" : @2 });
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 3 }"), @{ @"manifest_version" : @3 });

    NSDictionary *testResult = @{ @"manifest_version": @2, @"name": @"Test", @"version": @"1.0" };
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\" }"), testResult);

    testResult = @{ @"manifest_version": @3, @"name": @"Test", @"version": @"1.0" };
    EXPECT_NS_EQUAL(parse(@"{ \"manifest_version\": 3, \"name\": \"Test\", \"version\": \"1.0\" }"), testResult);
}

TEST(WKWebExtension, DisplayStringParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2 } mutableCopy];
    auto testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NULL(testExtension.displayName);
    EXPECT_NULL(testExtension.displayShortName);
    EXPECT_NULL(testExtension.displayVersion);
    EXPECT_NULL(testExtension.version);
    EXPECT_NULL(testExtension.displayDescription);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"name"] = @"Test";
    testManifestDictionary[@"version"] = @"1.0";
    testManifestDictionary[@"description"] = @"Test description.";
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.displayName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayShortName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayVersion, @"1.0");
    EXPECT_NS_EQUAL(testExtension.version, @"1.0");
    EXPECT_NS_EQUAL(testExtension.displayDescription, @"Test description.");
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"short_name"] = @"Tst";
    testManifestDictionary[@"version_name"] = @"1.0 Final";
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.displayName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayShortName, @"Tst");
    EXPECT_NS_EQUAL(testExtension.displayVersion, @"1.0 Final");
    EXPECT_NS_EQUAL(testExtension.version, @"1.0");
    EXPECT_NS_EQUAL(testExtension.displayDescription, @"Test description.");
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
}

TEST(WKWebExtension, ActionParsing)
{
    NSDictionary *testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" };
    auto testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ }, @"page_action": @{ } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"safari_action": @{ } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // action should be ignored in manifest v2.
    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Manifest v3 looks for the "action" key.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Manifest v3 should never find a browser_action.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Or a page action.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    auto *imageData = Util::makePNGData(CGSizeMake(16, 16), @selector(greenColor));

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title", @"default_icon": @"test.png" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title", @"default_icon": @{ @"16": @"test.png" } } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"icons": @{ @"16": @"test.png" }, @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);
}

TEST(WKWebExtension, ContentScriptsParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ] } ];
    auto testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    auto webkitURL = [NSURL URLWithString:@"https://webkit.org/"];
    auto exampleURL = [NSURL URLWithString:@"https://example.com/"];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ], @"exclude_matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    // Invalid cases

    testManifestDictionary[@"content_scripts"] = @[ ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @{ @"invalid": @YES };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ ] } ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:exampleURL]);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"run_at": @"invalid" } ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE([testExtension _hasStaticInjectedContentForURL:webkitURL]);
    EXPECT_TRUE([testExtension _hasStaticInjectedContentForURL:exampleURL]);
}

TEST(WKWebExtension, PermissionsParsing)
{
    // Neither of the "permissions" and "optional_permissions" keys are defined.
    NSDictionary *testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" };
    auto testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NOT_NULL(testExtension.requestedPermissions);
    EXPECT_EQ(testExtension.requestedPermissions.count, 0ul);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "permissions" key alone is defined but is empty.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NOT_NULL(testExtension.requestedPermissions);
    EXPECT_EQ(testExtension.requestedPermissions.count, 0ul);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key alone is defined but is empty.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NOT_NULL(testExtension.requestedPermissions);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissions.count, 0ul);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "permissions" and "optional_permissions" keys are defined as invalid types.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @(1), @"optional_permissions": @"foo" };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NOT_NULL(testExtension.requestedPermissions);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissions.count, 0ul);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"invalid" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet set]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid and an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"invalid" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid permission and a valid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"http://www.webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");

    // The "permissions" key is defined with a valid permission and an invalid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"foo://www.webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"invalid" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet set]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid and an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"invalid" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission and a valid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"http://www.webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NS_EQUAL(testExtension.optionalPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");

    // The "optional_permissions" key is defined with a valid permission and an invalid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"foo://www.webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission and a forbidden optional permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"geolocation" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key contains a permission already defined in the "permissions" key.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"tabs", @"geolocation" ], @"optional_permissions": @[@"tabs"] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);

    // The "optional_permissions" key contains an origin already defined in the "permissions" key.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // Make sure manifest v2 extensions ignore hosts from host_permissions (this should only be checked for manifest v3).
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testExtension.requestedPermissionMatchPatterns containsObject:[_WKWebExtensionMatchPattern matchPatternWithString:@"http://www.webkit.org/"]]);
    EXPECT_FALSE([testExtension.requestedPermissionMatchPatterns containsObject:[_WKWebExtensionMatchPattern matchPatternWithString:@"https://webkit.org/"]]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testExtension.optionalPermissionMatchPatterns containsObject:[_WKWebExtensionMatchPattern matchPatternWithString:@"http://www.example.com/"]]);
    EXPECT_FALSE([testExtension.optionalPermissionMatchPatterns containsObject:[_WKWebExtensionMatchPattern matchPatternWithString:@"https://webkit.org/"]]);

    // Make sure manifest v3 parses hosts from host_permissions, and ignores hosts in permissions and optional_permissions.
    testManifestDictionary = @{ @"manifest_version": @3, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 1ul);
    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"https://webkit.org/");
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // Make sure manifest v3 parses optional_host_permissions.
    testManifestDictionary = @{ @"manifest_version": @3, @"optional_host_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 1ul);
    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"https://webkit.org/");
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 1ul);
    EXPECT_NS_EQUAL(testExtension.optionalPermissionMatchPatterns.anyObject.description, @"http://www.example.com/");
}

TEST(WKWebExtension, BackgroundParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];

#if TARGET_OS_IPHONE
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test.js" ], @"persistent": @NO };
#else
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test.js" ] };
#endif

    auto testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
#if TARGET_OS_IPHONE
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
#else
    EXPECT_TRUE(testExtension.backgroundContentIsPersistent);
#endif
    EXPECT_FALSE(testExtension._backgroundContentUsesModules);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

#if TARGET_OS_IPHONE
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"", @"test-2.js" ], @"persistent": @NO };
#else
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"", @"test-2.js" ], @"persistent": @YES };
#endif

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
#if TARGET_OS_IPHONE
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
#else
    EXPECT_TRUE(testExtension.backgroundContentIsPersistent);
#endif
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js" };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    // Invalid cases

#if TARGET_OS_IPHONE
    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @YES };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));
#endif

    testManifestDictionary[@"manifest_version"] = @3;
    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @YES };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"manifest_version"] = @2;
    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"persistent": @YES };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_FALSE(testExtension._backgroundContentUsesModules);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @[ @"invalid" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"scripts": @[ ], @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"page": @"", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"page": @[ @"test.html" ], @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @[ @"test.js" ] ], @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"service_worker": @"", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"service_worker": @[ @"test.js" ], @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.backgroundContentIsPersistent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, _WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"type": @"module", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._backgroundContentUsesModules);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test.js", @"test2.js" ], @"type": @"module", @"persistent": @NO };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._backgroundContentUsesModules);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
}

TEST(WKWebExtension, OptionsPageParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @"options.html"
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @""
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @123
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

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
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{
            @"bad": @"options.html"
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

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
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{
            @"page": @""
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);
}

TEST(WKWebExtension, URLOverridesParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{
            @"newtab": @"newtab.html"
        }
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasOverrideNewTabPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{
            @"bad": @"newtab.html"
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

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
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

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
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

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
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasOverrideNewTabPage);

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
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{ }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

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
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);
}

TEST(WKWebExtension, ContentSecurityPolicyParsing)
{
    NSMutableDictionary *testManifestDictionaryV2 = [@{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"content_security_policy": @"script-src 'self'; object-src 'self'"
    } mutableCopy];

    NSMutableDictionary *testManifestDictionaryV3 = [@{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"content_security_policy": @{
            @"extension_pages": @"script-src 'self'; object-src 'self'"
        }
    } mutableCopy];

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionaryV3[@"content_security_policy"] = @{ @"sandbox": @"sandbox allow-scripts allow-forms allow-popups allow-modals; script-src 'self'" };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionaryV3[@"content_security_policy"] = @{ };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV2[@"content_security_policy"] = @123;
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV2[@"content_security_policy"] = @[ @"invalid", @"type" ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV3[@"content_security_policy"] = @{ @"extension_pages": @123 };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_EQ(testExtension.errors.count, 1ul);
}

TEST(WKWebExtension, WebAccessibleResourcesV2)
{
    NSMutableDictionary *testManifestDictionary = [@{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"web_accessible_resources": @[ @"images/*.png", @"styles/*.css" ]
    } mutableCopy];

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ ];
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @"bad";
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @{ };
    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
}

TEST(WKWebExtension, WebAccessibleResourcesV3)
{
    NSMutableDictionary *testManifestDictionary = [@{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"web_accessible_resources": @[ @{
            @"resources": @[ @"images/*.png", @"styles/*.css" ],
            @"matches": @[ @"<all_urls>" ]
        },
        @{
            @"resources": @[ @"scripts/*.js" ],
            @"matches": @[ @"*://localhost/*" ]
        } ]
    } mutableCopy];

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ ],
        @"matches": @[ ]
    } ];

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @"bad",
        @"matches": @[ @"<all_urls>" ]
    } ];

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ @"images/*.png" ],
        @"matches": @"bad"
    } ];

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"matches": @[ @"<all_urls>" ]
    } ];

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ ]
    } ];

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
}

TEST(WKWebExtension, CommandsParsing)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @{
            @"show-popup": @{
                @"suggested_key": @{
                    @"default": @"Ctrl+Shift+P",
                    @"mac": @"Command+Shift+P"
                },
                @"description": @"Show the popup"
            }
        }
    };

    auto *testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0"
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"commands": @{
            @"show-popup": @"Invalid"
        }
    };

    testExtension = [[_WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasCommands);
    EXPECT_EQ(testExtension.errors.count, 1ul);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
