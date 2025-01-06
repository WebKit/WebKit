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

#import "HTTPServer.h"
#import "TestCocoa.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/WKWebExtensionPrivate.h>

namespace TestWebKitAPI {

static NSError *matchingError(NSArray<NSError *> *errors, WKWebExtensionError code)
{
    for (NSError *error in errors) {
        if ([error.domain isEqualToString:WKWebExtensionErrorDomain] && error.code == code)
            return error;
    }

    return nil;
}

TEST(WKWebExtension, BasicManifestParsing)
{
    auto parse = ^(NSString *manifestString) {
        return [[WKWebExtension alloc] _initWithResources:@{ @"manifest.json": manifestString }].manifest;
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
    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NULL(testExtension.displayName);
    EXPECT_NULL(testExtension.displayShortName);
    EXPECT_NULL(testExtension.displayVersion);
    EXPECT_NULL(testExtension.version);
    EXPECT_NULL(testExtension.displayDescription);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"name"] = @"Test";
    testManifestDictionary[@"version"] = @"1.0";
    testManifestDictionary[@"description"] = @"Test description.";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.displayName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayShortName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayVersion, @"1.0");
    EXPECT_NS_EQUAL(testExtension.version, @"1.0");
    EXPECT_NS_EQUAL(testExtension.displayDescription, @"Test description.");
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"short_name"] = @"Tst";
    testManifestDictionary[@"version_name"] = @"1.0 Final";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.displayName, @"Test");
    EXPECT_NS_EQUAL(testExtension.displayShortName, @"Tst");
    EXPECT_NS_EQUAL(testExtension.displayVersion, @"1.0 Final");
    EXPECT_NS_EQUAL(testExtension.version, @"1.0");
    EXPECT_NS_EQUAL(testExtension.displayDescription, @"Test description.");
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
}

TEST(WKWebExtension, DefaultLocaleParsing)
{
    // Test no default locale.
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @3, @"name": @"Test", @"version": @"1.0", @"description": @"Test" } mutableCopy];
    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NULL(testExtension.defaultLocale);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    // Test English without a region.
    testManifestDictionary[@"default_locale"] = @"en";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"_locales/en/messages.json": @"{}" }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *defaultLocale = testExtension.defaultLocale;
    EXPECT_NOT_NULL(defaultLocale);
    EXPECT_NS_EQUAL(defaultLocale.localeIdentifier, @"en");
    EXPECT_NS_EQUAL(defaultLocale.languageCode, @"en");
    EXPECT_NULL(defaultLocale.countryCode);

    // Test English with US region.
    testManifestDictionary[@"default_locale"] = @"en_US";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"_locales/en_US/messages.json": @"{}" }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    defaultLocale = testExtension.defaultLocale;
    EXPECT_NOT_NULL(defaultLocale);
    EXPECT_NS_EQUAL(defaultLocale.localeIdentifier, @"en_US");
    EXPECT_NS_EQUAL(defaultLocale.languageCode, @"en");
    EXPECT_NS_EQUAL(defaultLocale.countryCode, @"US");

    // Test Simplified Chinese.
    testManifestDictionary[@"default_locale"] = @"zh_CN";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"_locales/zh_CN/messages.json": @"{}" }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    defaultLocale = testExtension.defaultLocale;
    EXPECT_NOT_NULL(defaultLocale);
    EXPECT_NS_EQUAL(defaultLocale.localeIdentifier, @"zh_CN");
    EXPECT_NS_EQUAL(defaultLocale.languageCode, @"zh");
    EXPECT_NS_EQUAL(defaultLocale.countryCode, @"CN");

    // Test invalid locale.
    testManifestDictionary[@"default_locale"] = @"invalid";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NULL(testExtension.defaultLocale);
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
}

TEST(WKWebExtension, DisplayStringParsingWithLocalization)
{
    NSMutableDictionary *testManifestDictionary = [@{
        @"manifest_version": @2,
        @"default_locale": @"en_US",

        @"name": @"__MSG_default_name__",
        @"short_name": @"__MSG_regional_name__",
        @"version": @"1.0",
        @"description": @"__MSG_default_description__"
    } mutableCopy];

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default String",
            @"description": @"The test name."
        },
        @"default_description": @{
            @"message": @"Default Description",
            @"description": @"The test description."
        }
    };

    auto *regionalMessages = @{
        @"regional_name": @{
            @"message": @"Regional String",
            @"description": @"The regional name."
        }
    };

    auto *resources = @{
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/en_US/messages.json": regionalMessages,
    };

    auto *extension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];

    EXPECT_NS_EQUAL(extension.displayName, @"Default String");
    EXPECT_NS_EQUAL(extension.displayShortName, @"Regional String");
    EXPECT_NS_EQUAL(extension.displayVersion, @"1.0");
    EXPECT_NS_EQUAL(extension.version, @"1.0");
    EXPECT_NS_EQUAL(extension.displayDescription, @"Default Description");
    EXPECT_NS_EQUAL(extension.errors, @[ ]);

    testManifestDictionary[@"short_name"] = @"__MSG_default_name__";
    extension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];

    EXPECT_NS_EQUAL(extension.displayShortName, @"Default String");
    EXPECT_NS_EQUAL(extension.errors, @[ ]);
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
TEST(WKWebExtension, MultipleIconVariants)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test",
        @"version": @"1.0",
        @"description": @"Test",

        @"icon_variants": @[
            @{ @"32": @"dark-32.png", @"64": @"dark-64.png", @"color_schemes": @[ @"dark" ] },
            @{ @"32": @"light-32.png", @"64": @"light-64.png", @"color_schemes": @[ @"light" ] }
        ]
    };

    auto *dark16Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));
    auto *dark32Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(whiteColor));
    auto *light16Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(blackColor));
    auto *light32Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(blackColor));

    auto *resources = @{
        @"dark-32.png": dark16Icon,
        @"dark-64.png": dark32Icon,
        @"light-32.png": light16Icon,
        @"light-64.png": light32Icon,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        auto *iconDark16 = [testExtension iconForSize:CGSizeMake(16, 16)];
        EXPECT_NOT_NULL(iconDark16);
        EXPECT_TRUE(CGSizeEqualToSize(iconDark16.size, CGSizeMake(16, 16)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconDark16), [CocoaColor whiteColor]));

        auto *iconDark32 = [testExtension iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(iconDark32);
        EXPECT_TRUE(CGSizeEqualToSize(iconDark32.size, CGSizeMake(32, 32)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconDark32), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        auto *iconLight16 = [testExtension iconForSize:CGSizeMake(16, 16)];
        EXPECT_NOT_NULL(iconLight16);
        EXPECT_TRUE(CGSizeEqualToSize(iconLight16.size, CGSizeMake(16, 16)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconLight16), [CocoaColor blackColor]));

        auto *iconLight32 = [testExtension iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(iconLight32);
        EXPECT_TRUE(CGSizeEqualToSize(iconLight32.size, CGSizeMake(32, 32)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconLight32), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtension, SingleIconVariant)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Single Variant",
        @"version": @"1.0",
        @"description": @"Test with single icon variant",

        @"icon_variants": @[
            @{ @"32": @"icon-32.png" }
        ]
    };

    auto *icon32 = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));

    auto *resources = @{
        @"icon-32.png": icon32,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));

    icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));
}

TEST(WKWebExtension, AnySizeIconVariant)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Any Size",
        @"version": @"1.0",
        @"description": @"Test with any size icon",

        @"icon_variants": @[
            @{ @"any": @"icon-any.svg" }
        ]
    };

    auto *iconAny = @"<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><circle cx='50' cy='50' r='50' fill='white' /></svg>";

    auto *resources = @{
        @"icon-any.svg": iconAny,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension iconForSize:CGSizeMake(64, 64)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(64, 64)));

    icon = [testExtension actionIconForSize:CGSizeMake(64, 64)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(64, 64)));
}

TEST(WKWebExtension, NoIconVariants)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test No Variants",
        @"version": @"1.0",
        @"description": @"Test with no icon variants"
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);

    icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);
}

TEST(WKWebExtension, IconsAndIconVariantsSpecified)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Icons and Variants",
        @"version": @"1.0",
        @"description": @"Test with both icons and icon variants specified",

        @"icons": @{
            @"32": @"icon-legacy.png"
        },

        @"icon_variants": @[
            @{ @"32": @"icon-variant.png" }
        ]
    };

    auto *iconLegacy = Util::makePNGData(CGSizeMake(32, 32), @selector(blackColor));
    auto *iconVariant = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));

    auto *resources = @{
        @"icon-legacy.png": iconLegacy,
        @"icon-variant.png": iconVariant,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));
    EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon), [CocoaColor whiteColor]));

    icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));
    EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon), [CocoaColor whiteColor]));
}

TEST(WKWebExtension, ActionIconVariantsMultiple)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Action Multiple Variants",
        @"version": @"1.0",
        @"description": @"Test action with multiple icon variants",

        @"action": @{
            @"icon_variants": @[
                @{ @"32": @"action-dark-32.png", @"64": @"action-dark-64.png", @"color_schemes": @[ @"dark" ] },
                @{ @"32": @"action-light-32.png", @"64": @"action-light-64.png", @"color_schemes": @[ @"light" ] }
            ]
        }
    };

    auto *dark32Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));
    auto *dark64Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(whiteColor));
    auto *light32Icon = Util::makePNGData(CGSizeMake(32, 32), @selector(blackColor));
    auto *light64Icon = Util::makePNGData(CGSizeMake(64, 64), @selector(blackColor));

    auto *resources = @{
        @"action-dark-32.png": dark32Icon,
        @"action-dark-64.png": dark64Icon,
        @"action-light-32.png": light32Icon,
        @"action-light-64.png": light64Icon,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    Util::performWithAppearance(Util::Appearance::Dark, ^{
        auto *iconDark32 = [testExtension actionIconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(iconDark32);
        EXPECT_TRUE(CGSizeEqualToSize(iconDark32.size, CGSizeMake(32, 32)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconDark32), [CocoaColor whiteColor]));

        auto *iconDark64 = [testExtension actionIconForSize:CGSizeMake(64, 64)];
        EXPECT_NOT_NULL(iconDark64);
        EXPECT_TRUE(CGSizeEqualToSize(iconDark64.size, CGSizeMake(64, 64)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconDark64), [CocoaColor whiteColor]));
    });

    Util::performWithAppearance(Util::Appearance::Light, ^{
        auto *iconLight32 = [testExtension actionIconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(iconLight32);
        EXPECT_TRUE(CGSizeEqualToSize(iconLight32.size, CGSizeMake(32, 32)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconLight32), [CocoaColor blackColor]));

        auto *iconLight64 = [testExtension actionIconForSize:CGSizeMake(64, 64)];
        EXPECT_NOT_NULL(iconLight64);
        EXPECT_TRUE(CGSizeEqualToSize(iconLight64.size, CGSizeMake(64, 64)));
        EXPECT_TRUE(Util::compareColors(Util::pixelColor(iconLight64), [CocoaColor blackColor]));
    });
}

TEST(WKWebExtension, ActionIconSingleVariant)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Action Single Variant",
        @"version": @"1.0",
        @"description": @"Test action with a single icon variant",

        @"action": @{
            @"icon_variants": @[
                @{ @"32": @"action-icon-32.png" }
            ]
        }
    };

    auto *icon32 = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));

    auto *resources = @{
        @"action-icon-32.png": icon32,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));
    EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon), [CocoaColor whiteColor]));

    icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);
}

TEST(WKWebExtension, ActionIconAnySizeVariant)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Action Any Size",
        @"version": @"1.0",
        @"description": @"Test action with any size icon",

        @"action": @{
            @"icon_variants": @[
                @{ @"any": @"action-icon-any.svg" }
            ]
        }
    };

    auto *iconAny = @"<svg width='100' height='100' xmlns='http://www.w3.org/2000/svg'><circle cx='50' cy='50' r='50' fill='white' /></svg>";

    auto *resources = @{
        @"action-icon-any.svg": iconAny,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension actionIconForSize:CGSizeMake(64, 64)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(64, 64)));

    icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);
}

TEST(WKWebExtension, ActionNoIconVariants)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Action No Variants",
        @"version": @"1.0",
        @"description": @"Test action with no icon variants"
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);

    icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);
}

TEST(WKWebExtension, ActionIconsAndIconVariantsSpecified)
{
    auto *testManifestDictionary = @{
        @"manifest_version": @3,

        @"name": @"Test Action Icons and Variants",
        @"version": @"1.0",
        @"description": @"Test action with both icons and icon variants specified",

        @"action": @{
            @"default_icon": @{
                @"32": @"action-icon-legacy.png"
            },
            @"icon_variants": @[
                @{ @"32": @"action-icon-variant.png" }
            ]
        }
    };

    auto *iconLegacy = Util::makePNGData(CGSizeMake(32, 32), @selector(blackColor));
    auto *iconVariant = Util::makePNGData(CGSizeMake(32, 32), @selector(whiteColor));

    auto *resources = @{
        @"action-icon-legacy.png": iconLegacy,
        @"action-icon-variant.png": iconVariant,
    };

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:resources];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    auto *icon = [testExtension actionIconForSize:CGSizeMake(32, 32)];
    EXPECT_NOT_NULL(icon);
    EXPECT_TRUE(CGSizeEqualToSize(icon.size, CGSizeMake(32, 32)));
    EXPECT_TRUE(Util::compareColors(Util::pixelColor(icon), [CocoaColor whiteColor]));

    icon = [testExtension iconForSize:CGSizeMake(32, 32)];
    EXPECT_NULL(icon);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

TEST(WKWebExtension, ActionParsing)
{
    NSDictionary *testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" };
    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ }, @"page_action": @{ } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"safari_action": @{ } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // action should be ignored in manifest v2.
    testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Manifest v3 looks for the "action" key.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Manifest v3 should never find a browser_action.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"browser_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    // Or a page action.
    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"page_action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    auto *imageData = Util::makePNGData(CGSizeMake(16, 16), @selector(greenColor));

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title", @"default_icon": @"test.png" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ @"default_title": @"Button Title", @"default_icon": @{ @"16": @"test.png" } } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"icons": @{ @"16": @"test.png" }, @"action": @{ @"default_title": @"Button Title" } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary resources:@{ @"test.png": imageData }];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NS_EQUAL(testExtension.displayActionLabel, @"Button Title");
    EXPECT_NOT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);

    testManifestDictionary = @{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0", @"action": @{ } };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_NULL(testExtension.displayActionLabel);
    EXPECT_NULL([testExtension actionIconForSize:NSMakeSize(16, 16)]);
}

TEST(WKWebExtension, ContentScriptsParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ] } ];
    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*/" ], @"exclude_matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js", @1, @"" ], @"css": @[ @NO, @"test.css", @"" ], @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"world": @"MAIN" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"user", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"author", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasInjectedContent);

    // Invalid cases

    testManifestDictionary[@"content_scripts"] = @[ ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @{ @"invalid": @YES };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_FALSE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"run_at": @"invalid" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"js": @[ @"test.js" ], @"matches": @[ @"*://*.example.com/" ], @"world": @"INVALID" } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_TRUE(testExtension.hasInjectedContent);

    testManifestDictionary[@"content_scripts"] = @[ @{ @"css": @[ @NO, @"test.css", @"" ], @"css_origin": @"bad", @"matches": @[ @"*://*.example.com/" ] } ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_TRUE(testExtension.hasInjectedContent);
}

TEST(WKWebExtension, PermissionsParsing)
{
    // Neither of the "permissions" and "optional_permissions" keys are defined.
    NSDictionary *testManifestDictionary = @{ @"manifest_version": @2, @"name": @"Test", @"description": @"Test", @"version": @"1.0" };
    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

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
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

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
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

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
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

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
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet set]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid and an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"invalid" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "permissions" key is defined with a valid permission and a valid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"http://www.webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");

    // The "permissions" key is defined with a valid permission and an invalid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions": @[ @"tabs", @"foo://www.webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.requestedPermissionMatchPatterns);
    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"invalid" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet set]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid and an invalid permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"invalid" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission and a valid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"http://www.webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:@[ @"tabs" ]]);
    EXPECT_NS_EQUAL(testExtension.optionalPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");

    // The "optional_permissions" key is defined with a valid permission and an invalid origin.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"foo://www.webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key is defined with a valid permission and a forbidden optional permission.
    testManifestDictionary = @{ @"manifest_version": @2, @"optional_permissions": @[ @"tabs", @"geolocation" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.optionalPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // The "optional_permissions" key contains a permission already defined in the "permissions" key.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"tabs", @"geolocation" ], @"optional_permissions": @[@"tabs"] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissions, [NSSet setWithArray:(@[ @"tabs" ])]);
    EXPECT_NOT_NULL(testExtension.optionalPermissions);
    EXPECT_EQ(testExtension.optionalPermissions.count, 0ul);

    // The "optional_permissions" key contains an origin already defined in the "permissions" key.
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"http://www.webkit.org/");
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // Make sure manifest v2 extensions ignore hosts from host_permissions (this should only be checked for manifest v3).
    testManifestDictionary = @{ @"manifest_version": @2, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testExtension.requestedPermissionMatchPatterns containsObject:[WKWebExtensionMatchPattern matchPatternWithString:@"http://www.webkit.org/"]]);
    EXPECT_FALSE([testExtension.requestedPermissionMatchPatterns containsObject:[WKWebExtensionMatchPattern matchPatternWithString:@"https://webkit.org/"]]);
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 1ul);
    EXPECT_TRUE([testExtension.optionalPermissionMatchPatterns containsObject:[WKWebExtensionMatchPattern matchPatternWithString:@"http://www.example.com/"]]);
    EXPECT_FALSE([testExtension.optionalPermissionMatchPatterns containsObject:[WKWebExtensionMatchPattern matchPatternWithString:@"https://webkit.org/"]]);

    // Make sure manifest v3 parses hosts from host_permissions, and ignores hosts in permissions and optional_permissions.
    testManifestDictionary = @{ @"manifest_version": @3, @"permissions" : @[ @"http://www.webkit.org/" ], @"optional_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_EQ(testExtension.requestedPermissionMatchPatterns.count, 1ul);
    EXPECT_NS_EQUAL(testExtension.requestedPermissionMatchPatterns.anyObject.description, @"https://webkit.org/");
    EXPECT_NOT_NULL(testExtension.optionalPermissionMatchPatterns);
    EXPECT_EQ(testExtension.optionalPermissionMatchPatterns.count, 0ul);

    // Make sure manifest v3 parses optional_host_permissions.
    testManifestDictionary = @{ @"manifest_version": @3, @"optional_host_permissions": @[ @"http://www.example.com/" ], @"host_permissions": @[ @"https://webkit.org/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

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

    auto testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
#if TARGET_OS_IPHONE
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
#else
    EXPECT_TRUE(testExtension.hasPersistentBackgroundContent);
#endif
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

#if TARGET_OS_IPHONE
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"", @"test-2.js" ], @"persistent": @NO };
#else
    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"", @"test-2.js" ], @"persistent": @YES };
#endif

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
#if TARGET_OS_IPHONE
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
#else
    EXPECT_TRUE(testExtension.hasPersistentBackgroundContent);
#endif
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js" };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"test-2.js" ], @"service_worker": @"test.js", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"service_worker": @"test.js", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test-1.js", @"test-2.js" ], @"page": @"test.html", @"service_worker": @"test.js", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"type": @"module", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_TRUE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @"test.js", @"test2.js" ], @"type": @"module", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_TRUE(testExtension._hasModularBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    // Invalid cases

#if TARGET_OS_IPHONE
    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @YES };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));
#endif

    testManifestDictionary[@"manifest_version"] = @3;
    testManifestDictionary[@"background"] = @{ @"page": @"test.html", @"persistent": @YES };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"manifest_version"] = @2;
    testManifestDictionary[@"background"] = @{ @"service_worker": @"test.js", @"persistent": @YES };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_FALSE(testExtension._hasModularBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @[ @"invalid" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"scripts": @[ ], @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"page": @"", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"page": @[ @"test.html" ], @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"scripts": @[ @[ @"test.js" ] ], @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"service_worker": @"", @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));

    testManifestDictionary[@"background"] = @{ @"service_worker": @[ @"test.js" ], @"persistent": @NO };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];

    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension.hasPersistentBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
    EXPECT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidBackgroundPersistence));
}

TEST(WKWebExtension, BackgroundPreferredEnvironmentParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{ @"manifest_version": @3, @"name": @"Test", @"description": @"Test", @"version": @"1.0" } mutableCopy];

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"service_worker", @"document" ],
        @"service_worker": @"background.js",
        @"scripts": @[ @"background.js" ],
        @"page": @"background.html",
    };

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"document", @"service_worker" ],
        @"service_worker": @"background.js",
        @"scripts": @[ @"background.js" ],
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @"service_worker",
        @"service_worker": @"background.js",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"document" ],
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @"document",
        @"scripts": @[ @"background.js" ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"document", @"service_worker" ],
        @"scripts": @[ @"background.js" ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"document", @42, @"unknown" ],
        @"scripts": @[ @"background.js" ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"unknown", @42 ],
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @"unknown",
        @"service_worker": @"background.js",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"unknown", @"document"],
        @"service_worker": @"background.js",
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    // Invalid cases

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ ],
        @"service_worker": @"background.js",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_TRUE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @42,
        @"service_worker": @"background.js",
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasBackgroundContent);
    EXPECT_FALSE(testExtension._hasServiceWorkerBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @[ @"service_worker", @"document" ],
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @"document",
        @"service_worker": @"background.js",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));

    testManifestDictionary[@"background"] = @{
        @"preferred_environment": @"service_worker",
        @"page": @"background.html",
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasBackgroundContent);
    EXPECT_NE(testExtension.errors.count, 0ul);
    EXPECT_NOT_NULL(matchingError(testExtension.errors, WKWebExtensionErrorInvalidManifestEntry));
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);
    EXPECT_TRUE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @""
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_page": @123
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOptionsPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"options_ui": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_url_overrides": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);
    EXPECT_FALSE(testExtension.hasOverrideNewTabPage);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"chrome_url_overrides": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionaryV3[@"content_security_policy"] = @{ @"sandbox": @"sandbox allow-scripts allow-forms allow-popups allow-modals; script-src 'self'" };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionaryV3[@"content_security_policy"] = @{ };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV2[@"content_security_policy"] = @123;
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV2[@"content_security_policy"] = @[ @"invalid", @"type" ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV2];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionaryV3[@"content_security_policy"] = @{ @"extension_pages": @123 };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionaryV3];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ ];
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @"bad";
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @{ };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ ],
        @"matches": @[ ]
    } ];

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @"bad",
        @"matches": @[ @"<all_urls>" ]
    } ];

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ @"images/*.png" ],
        @"matches": @"bad"
    } ];

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"matches": @[ @"<all_urls>" ]
    } ];

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"web_accessible_resources"] = @[ @{
        @"resources": @[ ]
    } ];

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
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

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"action": @{
            @"default_title": @"Test Action"
        },
        @"commands": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_action": @{
            @"default_title": @"Test Action"
        },
        @"commands": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"page_action": @{
            @"default_title": @"Test Action"
        },
        @"commands": @{ }
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0"
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"action": @{
            @"default_title": @"Test Action"
        },
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"browser_action": @{
            @"default_title": @"Test Action"
        },
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    testManifestDictionary = @{
        @"manifest_version": @2,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"page_action": @{
            @"default_title": @"Test Action"
        },
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasCommands);
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

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasCommands);
    EXPECT_EQ(testExtension.errors.count, 1ul);
}

TEST(WKWebExtension, DeclarativeNetRequestParsing)
{
    NSMutableDictionary *testManifestDictionary = [@{
        @"manifest_version": @3,
        @"name": @"Test",
        @"description": @"Test",
        @"version": @"1.0",
        @"permissions": @[ @"declarativeNetRequest" ],
        @"declarative_net_request": @{
            @"rule_resources": @[
                @{
                    @"id": @"test",
                    @"enabled": @YES,
                    @"path": @"rules.json"
                }
            ]
        }
    } mutableCopy];

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_TRUE(testExtension.hasContentModificationRules);
    EXPECT_NS_EQUAL(testExtension.errors, @[ ]);

    // Missing id
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[
            @{
                @"enabled": @YES,
                @"path": @"rules.json"
            }
        ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasContentModificationRules);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Missing enabled
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[
            @{
                @"id": @"test",
                @"path": @"rules.json"
            }
        ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasContentModificationRules);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Missing path
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[
            @{
                @"id": @"test",
                @"enabled": @YES
            }
        ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_FALSE(testExtension.hasContentModificationRules);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Duplicate names
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[
            @{
                @"id": @"test",
                @"enabled": @YES,
                @"path": @"rules.json"
            },
            @{
                @"id": @"test",
                @"enabled": @YES,
                @"path": @"rules2.json"
            }
        ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    // There should still be the first rule loaded.
    EXPECT_TRUE(testExtension.hasContentModificationRules);
    // But also an error.
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // One valid rule, one invalid
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[
            @{
                @"id": @"test",
                @"enabled": @YES,
                @"path": @"rules.json"
            },
            @{
                @"enabled": @YES,
                @"path": @"rules2.json"
            }
        ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    // There should still be the first rule loaded.
    EXPECT_TRUE(testExtension.hasContentModificationRules);
    // But also an error.
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // No rule sets
    testManifestDictionary[@"declarative_net_request"] = @{
        @"rule_resources": @[ ]
    };

    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    // There shouldn't be any rules.
    EXPECT_FALSE(testExtension.hasContentModificationRules);
    // Or any errors.
    EXPECT_EQ(testExtension.errors.count, 0ul);
}

TEST(WKWebExtension, ExternallyConnectableParsing)
{
    auto *testManifestDictionary = [NSMutableDictionary dictionaryWithDictionary:@{
        @"manifest_version": @3,
        @"name": @"ExternallyConnectableTest",
        @"description": @"ExternallyConnectableTest",
        @"version": @"1.0",
        @"externally_connectable": @{ },
    }];

    auto *testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Expect an error since 'externally_connectable' is specified, but there are no valid match patterns or extension ids.
    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ ], @"ids": @[ @"" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Expect an error if <all_urls> is specified.
    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"<all_urls>" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Still expect the error, but have a valid requested match pattern.
    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"*://*.example.com/", @"<all_urls>" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 1ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Expect an error for not have a second level domain.
    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"*://*.com/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 1ul);

    // Should have a match for *://*.example.com/*.
    auto *matchingPattern = [WKWebExtensionMatchPattern matchPatternWithString:@"*://*.example.com/" ];
    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"*://*.example.com/" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 1ul);
    EXPECT_EQ(testExtension.errors.count, 0ul);

    EXPECT_TRUE([testExtension.allRequestedMatchPatterns.allObjects.firstObject matchesPattern:matchingPattern]);

    testManifestDictionary[@"externally_connectable"] = @{ @"matches": @[ @"*://*.example.com/" ], @"ids": @[ @"*" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 1ul);
    EXPECT_EQ(testExtension.errors.count, 0ul);

    testManifestDictionary[@"externally_connectable"] = @{ @"ids": @[ @"*" ] };
    testExtension = [[WKWebExtension alloc] _initWithManifestDictionary:testManifestDictionary];
    EXPECT_EQ(testExtension.allRequestedMatchPatterns.count, 0ul);
    EXPECT_EQ(testExtension.errors.count, 0ul);

    // FIXME: <https://webkit.org/b/269299> Add more tests for externally_connectable "ids" keys.
}

TEST(WKWebExtension, LoadFromDirectory)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension" withExtension:@""];
    EXPECT_NOT_NULL(extensionURL);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithResourceBaseURL:extensionURL error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadFromZipArchiveWithoutParentDirectory)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension-without-parent-directory" withExtension:@"zip"];
    EXPECT_NOT_NULL(extensionURL);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithResourceBaseURL:extensionURL error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadFromZipArchiveWithParentDirectory)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension-with-parent-directory" withExtension:@"zip"];
    EXPECT_NOT_NULL(extensionURL);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithResourceBaseURL:extensionURL error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadFromChromeExtensionArchive)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension" withExtension:@"crx"];
    EXPECT_NOT_NULL(extensionURL);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithResourceBaseURL:extensionURL error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadFromMacAppExtensionBundle)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionBundleURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension-mac" withExtension:@"appex"];
    EXPECT_NOT_NULL(extensionBundleURL);

    auto *extensionBundle = [NSBundle bundleWithURL:extensionBundleURL];
    EXPECT_NOT_NULL(extensionBundle);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithAppExtensionBundle:extensionBundle error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadFromiOSAppExtensionBundle)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *extensionBundleURL = [NSBundle.test_resourcesBundle URLForResource:@"web-extension-ios" withExtension:@"appex"];
    EXPECT_NOT_NULL(extensionBundleURL);

    auto *extensionBundle = [NSBundle bundleWithURL:extensionBundleURL];
    EXPECT_NOT_NULL(extensionBundle);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithAppExtensionBundle:extensionBundle error:nullptr]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];
    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtension, LoadWithBadURL)
{
    auto *badURL = [NSURL fileURLWithPath:@"/does/not/exist" isDirectory:YES relativeToURL:nil];
    EXPECT_NOT_NULL(badURL);

    NSError *error;
    EXPECT_NULL([[WKWebExtension alloc] _initWithResourceBaseURL:badURL error:&error]);
    EXPECT_NOT_NULL(error);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
