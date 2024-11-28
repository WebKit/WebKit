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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "HTTPServer.h"
#import "WebExtensionUtilities.h"

@interface NSLocale ()
+ (NSString *)_deviceLanguage;
@end

static NSString * const messageKey = @"message";
static NSString * const placeholdersKey = @"placeholders";
static NSString * const placeholderDictionaryContentKey = @"content";

static NSLocale *currentLocale = NSLocale.currentLocale;
static NSWritingDirection writingDirection = [NSParagraphStyle defaultWritingDirectionForLanguage:currentLocale.languageCode];
static NSString *textDirection = writingDirection == NSWritingDirectionLeftToRight ? @"ltr" : @"rtl";
static NSString *reversedTextDirection = writingDirection == NSWritingDirectionLeftToRight ? @"rtl" : @"ltr";
static NSString *startEdge = writingDirection == NSWritingDirectionLeftToRight ? @"left" : @"right";
static NSString *endEdge = writingDirection == NSWritingDirectionLeftToRight ? @"right" : @"left";

namespace TestWebKitAPI {

static auto *localizationManifest = @{
    @"manifest_version": @3,

    @"name": @"Localization Test",
    @"description": @"Localization Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"default_locale": @"en"
};

static auto *messages = @{
    @"extension_name": @{
        @"message": @"Web Extension Test Extension",
        @"description": @"The display name for the extension."
    },

    @"extension_description": @{
        @"message": @"This is a Web Extension. Tell us what your extension does here",
        @"description": @"Description of what the extension does."
    }
};

static NSString *localeStringInWebExtensionFormat(NSLocale *locale)
{
    if (!locale.languageCode.length)
        return @"und";

    if (locale.countryCode.length)
        return [NSString stringWithFormat:@"%@-%@", locale.languageCode, locale.countryCode];
    return locale.languageCode;
}

static NSString *currentLocaleString = localeStringInWebExtensionFormat(currentLocale);
static NSString *currentSystemLocaleString = NSLocale._deviceLanguage;

static auto *baseURLString = @"test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3";

TEST(WKWebExtensionAPILocalization, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertThrows(() => browser.i18n.getMessage(''), /'name' value is invalid, because it cannot be empty/i)",
        @"browser.test.assertThrows(() => browser.i18n.getMessage(), /a required argument is missing/i)",
        @"browser.test.assertThrows(() => browser.i18n.getMessage(123), /'name' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.i18n.getMessage(null), /'name' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.i18n.getMessage(undefined), /'name' value is invalid, because a string is expected/i)",

        // Finish
        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": messages }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18n)
{
    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptedLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptedLanguages addObject:localeIdentifier];
        [acceptedLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    auto *acceptedLanguagesString = Util::constructJSArrayOfStrings(acceptedLanguages.array);
    auto *preferredLanguagesString = Util::constructJSArrayOfStrings(preferredLocaleIdentifiers);

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const preferredLanguages = %@", preferredLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
        [NSString stringWithFormat:@"const currentSystemUILanguage = '%@'", currentSystemLocaleString],
        [NSString stringWithFormat:@"const textDirection = '%@'", textDirection],
        [NSString stringWithFormat:@"const reversedTextDirection = '%@'", reversedTextDirection],
        [NSString stringWithFormat:@"const startEdge = '%@'", startEdge],
        [NSString stringWithFormat:@"const endEdge = '%@'", endEdge],

        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Web Extension Test Extension')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), 'en')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), textDirection)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), reversedTextDirection)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), startEdge)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), endEdge)",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertFalse(currentUILanguage === 'und')",
        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.assertFalse(currentSystemUILanguage === 'und')",
        @"browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": messages }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nWithFallback)
{
    // Temporarily set the current locale to US English for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"en-US" ] } forName:NSArgumentDomain];

    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptedLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptedLanguages addObject:localeIdentifier];
        [acceptedLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    auto *acceptedLanguagesString = Util::constructJSArrayOfStrings(acceptedLanguages.array);
    auto *preferredLanguagesString = Util::constructJSArrayOfStrings(preferredLocaleIdentifiers);

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const preferredLanguages = %@", preferredLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
        [NSString stringWithFormat:@"const currentSystemUILanguage = '%@'", currentSystemLocaleString],
        [NSString stringWithFormat:@"const textDirection = '%@'", textDirection],
        [NSString stringWithFormat:@"const reversedTextDirection = '%@'", reversedTextDirection],
        [NSString stringWithFormat:@"const startEdge = '%@'", startEdge],
        [NSString stringWithFormat:@"const endEdge = '%@'", endEdge],

        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Web Extension Test Extension')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default String')",
        @"browser.test.assertEq(browser.i18n.getMessage('regional_name'), 'Regional String')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), 'en_US')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), textDirection)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), reversedTextDirection)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), startEdge)",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), endEdge)",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertFalse(currentUILanguage === 'und')",
        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.assertFalse(currentSystemUILanguage === 'und')",
        @"browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto *manifest = @{
        @"manifest_version": @3,

        @"name": @"Localization Test",
        @"description": @"Localization Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        },

        @"default_locale": @"fr"
    };

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default String",
            @"description": @"The test name."
        }
    };

    auto *regionalMessages = @{
        @"regional_name": @{
            @"message": @"Regional String",
            @"description": @"The regional name."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/fr/messages.json": defaultMessages,
        @"_locales/en/messages.json": messages,
        @"_locales/en_US/messages.json": regionalMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nWithoutMessages)
{
    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptedLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptedLanguages addObject:localeIdentifier];
        [acceptedLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    auto *acceptedLanguagesString = Util::constructJSArrayOfStrings(acceptedLanguages.array);
    auto *preferredLanguagesString = Util::constructJSArrayOfStrings(preferredLocaleIdentifiers);

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const preferredLanguages = %@", preferredLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
        [NSString stringWithFormat:@"const currentSystemUILanguage = '%@'", currentSystemLocaleString],

        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertFalse(currentUILanguage === 'und')",
        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.assertFalse(currentSystemUILanguage === 'und')",
        @"browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nWithoutDefaultLocale)
{
    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptedLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptedLanguages addObject:localeIdentifier];
        [acceptedLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    auto *manifestWithoutLocale = @{
        @"manifest_version": @3,

        @"name": @"Localization Test",
        @"description": @"Localization Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module",
            @"persistent": @NO,
        }
    };

    auto *acceptedLanguagesString = Util::constructJSArrayOfStrings(acceptedLanguages.array);
    auto *preferredLanguagesString = Util::constructJSArrayOfStrings(preferredLocaleIdentifiers);

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const preferredLanguages = %@", preferredLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
        [NSString stringWithFormat:@"const currentSystemUILanguage = '%@'", currentSystemLocaleString],

        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertFalse(currentUILanguage === 'und')",
        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.assertFalse(currentSystemUILanguage === 'und')",
        @"browser.test.assertEq(await browser.i18n.getSystemUILanguage(), currentSystemUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getPreferredSystemLanguages(), preferredLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifestWithoutLocale resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, Placeholders)
{
    NSDictionary *localizationDictionary = @{
        @"key1": @{
            messageKey: @"$placeholder$",
            placeholdersKey: @{
                @"placeholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
            },
        },
        @"key2": @{
            messageKey: @"$Placeholder$ + $PlAcEhOlDeR$",
            placeholdersKey: @{
                @"placeholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
            },
        },
        @"key3": @{
            messageKey: @"$placeholder$ and $place_holder$",
            placeholdersKey: @{
                @"placeholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
                @"pLaCe_hoLder": @{
                    placeholderDictionaryContentKey: @"different value",
                },
            },
        },
        @"key4": @{
            messageKey: @"$placeholder$ with $$5",
            placeholdersKey: @{
                @"unusedplaceholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
            },
        },
        @"key5": @{
            messageKey: @"$placeholder$ and $positional$",
            placeholdersKey: @{
                @"unusedplaceholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
                @"positional": @{
                    placeholderDictionaryContentKey: @"$2",
                },
            },
        },
        @"key6": @{
            messageKey: @"$1 and $2 but not $3 or $0",
            placeholdersKey: @{
                @"unusedplaceholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
            },
        },
        @"key7": @{
            messageKey: @"$dontreplaceme or me$ or th$is or $$th$$at$",
            placeholdersKey: @{
                @"unusedplaceholder": @{
                    placeholderDictionaryContentKey: @"replacement value",
                },
            },
        },
        @"key8": @{
            messageKey: @"$total$명, 총 $found$명",
            placeholdersKey: @{
                @"total": @{
                    placeholderDictionaryContentKey: @"1234",
                },
                @"found": @{
                    placeholderDictionaryContentKey: @"42",
                },
            },
        },
    };

    auto *backgroundScript = Util::constructScript(@[
        // Variable setup
        [NSString stringWithFormat:@"var placeholders = %@", Util::constructJSArrayOfStrings(@[ @"irrelevant", @"unused" ])],

        // Test predefined localization
        @"browser.test.assertEq(browser.i18n.getMessage('key1', placeholders), 'replacement value')",
        @"browser.test.assertEq(browser.i18n.getMessage('key2', placeholders), 'replacement value + replacement value')",
        @"browser.test.assertEq(browser.i18n.getMessage('key3', placeholders), 'replacement value and different value')",
        @"browser.test.assertEq(browser.i18n.getMessage('key4', placeholders), ' with $5')",
        @"browser.test.assertEq(browser.i18n.getMessage('key7', placeholders), '$dontreplaceme or me$ or th$is or $th$at$')",
        @"browser.test.assertEq(browser.i18n.getMessage('key8', placeholders), '1234명, 총 42명')",

        [NSString stringWithFormat:@"placeholders = %@", Util::constructJSArrayOfStrings(@[ @"irrelevant", @"argument value" ])],
        @"browser.test.assertEq(browser.i18n.getMessage('key5', placeholders), ' and argument value')",

        [NSString stringWithFormat:@"placeholders = %@", Util::constructJSArrayOfStrings(@[ @"value1", @"value2" ])],
        @"browser.test.assertEq(browser.i18n.getMessage('key6', placeholders), 'value1 and value2 but not  or ')",

        // Finish
        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": localizationDictionary }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, CSSLocalization)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Load Tab')"
    ]);

    auto *htmlContent = @"<link rel='stylesheet' href='test.css'><script src='test.js'></script>";

    auto *cssContent = Util::constructScript(@[
        @"body {",
        @"  content: '__MSG_@@extension_id__';",
        @"  direction: __MSG_@@bidi_dir__;',",
        @"  text-align: __MSG_@@bidi_start_edge__;",
        @"}"
    ]);

    auto *scriptContent = Util::constructScript(@[
        @"window.addEventListener('load', () => {",
        @"  const bodyStyle = getComputedStyle(document.body)",
        @"  browser.test.assertEq(bodyStyle.content, '\"76C788B8-3374-400D-8259-40E5B9DF79D3\"', 'The content should be')",
        @"  browser.test.assertEq(bodyStyle.direction, 'ltr', 'The direction should be')",
        @"  browser.test.assertEq(bodyStyle.textAlign, 'start', 'The text-align should be')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"test.html": htmlContent,
        @"test.js": scriptContent,
        @"test.css": cssContent,
        @"_locales/en/messages.json": messages
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    auto *testPageURL = [NSURL URLWithString:@"test.html" relativeToURL:manager.get().context.baseURL];
    [manager.get().defaultTab changeWebViewIfNeededForURL:testPageURL forExtensionContext:manager.get().context];
    [manager.get().defaultTab.webView loadRequest:[NSURLRequest requestWithURL:testPageURL]];

    [manager run];
}

TEST(WKWebExtensionAPILocalization, CSSLocalizationInContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } },
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    auto *manifest = @{
        @"manifest_version": @3,
        @"default_locale": @"en",

        @"name": @"Localization Test",
        @"description": @"Localization Test",
        @"version": @"1",

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module"
        },

        @"content_scripts": @[@{
            @"matches": @[ @"<all_urls>" ],
            @"css": @[ @"content.css" ],
            @"js": @[ @"content.js" ]
        }]
    };

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentStyleSheet = Util::constructScript(@[
        @"body {",
        @"  content: '__MSG_@@extension_id__';",
        @"  direction: '__MSG_@@bidi_dir__';",
        @"  text-align: '__MSG_@@bidi_start_edge__';",
        @"}"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"window.addEventListener('load', () => {",
        @"  const bodyStyle = getComputedStyle(document.body)",
        @"  browser.test.assertEq(bodyStyle.content, '\"76C788B8-3374-400D-8259-40E5B9DF79D3\"', 'CSS content should be')",
        @"  browser.test.assertEq(bodyStyle.direction, 'ltr', 'CSS direction should be')",
        @"  browser.test.assertEq(bodyStyle.textAlign, 'start', 'CSS text-align should be')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
        @"content.css": contentStyleSheet,
        @"_locales/en/messages.json": messages
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPILocalization, CSSLocalizationInRegisteredContentScript)
{
    TestWebKitAPI::HTTPServer server({
        { "/"_s, { { { "Content-Type"_s, "text/html"_s } }, ""_s } }
    }, TestWebKitAPI::HTTPServer::Protocol::Http);

    static auto *localizationManifest = @{
        @"manifest_version": @3,
        @"default_locale": @"en",

        @"name": @"Localization Test",
        @"description": @"Localization Test",
        @"version": @"1.0",

        @"permissions": @[ @"scripting" ],

        @"background": @{
            @"scripts": @[ @"background.js" ],
            @"type": @"module"
        }
    };

    auto *backgroundScript = Util::constructScript(@[
        @"await browser.scripting.registerContentScripts([{",
        @"  id: 'test',",
        @"  matches: ['<all_urls>'],",
        @"  css: ['content.css'],",
        @"  js: ['content.js']",
        @"}])",

        @"browser.test.yield('Load Tab')"
    ]);

    auto *contentStyleSheet = Util::constructScript(@[
        @"body {",
        @"  content: '__MSG_@@extension_id__';",
        @"  direction: '__MSG_@@bidi_dir__';",
        @"  text-align: '__MSG_@@bidi_start_edge__';",
        @"}"
    ]);

    auto *contentScript = Util::constructScript(@[
        @"window.addEventListener('load', () => {",
        @"  const bodyStyle = getComputedStyle(document.body)",
        @"  browser.test.assertEq(bodyStyle.content, '\"76C788B8-3374-400D-8259-40E5B9DF79D3\"', 'CSS content should be')",
        @"  browser.test.assertEq(bodyStyle.direction, 'ltr', 'CSS direction should be')",
        @"  browser.test.assertEq(bodyStyle.textAlign, 'start', 'CSS text-align should be')",

        @"  browser.test.notifyPass()",
        @"})"
    ]);

    auto *resources = @{
        @"background.js": backgroundScript,
        @"content.js": contentScript,
        @"content.css": contentStyleSheet,
        @"_locales/en/messages.json": messages
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    auto *urlRequest = server.requestWithLocalhost();
    [manager.get().context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forURL:urlRequest.URL];

    // Set a base URL so it is a known value and not the default random one.
    [WKWebExtensionMatchPattern registerCustomURLScheme:@"test-extension"];
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Load Tab");

    [manager.get().defaultTab.webView loadRequest:urlRequest];

    [manager run];
}

TEST(WKWebExtensionAPILocalization, i18nChineseSimplified)
{
    // Temporarily set the current locale to Simplified Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseSimplifiedOnly)
{
    // Temporarily set the current locale to Simplified Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseSimplifiedScript)
{
    // Temporarily set the current locale to Simplified Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-US", @"en-US", @"zh-HK" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_Hans/messages.json": simplifiedChineseMessages,
        @"_locales/zh_Hant/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseTraditional)
{
    // Temporarily set the current locale to Traditional Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hant-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseSimplifiedInTaiwan)
{
    // Temporarily set the current locale to Simplified Chinese with the Taiwan country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-TW" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseTraditionalInChina)
{
    // Temporarily set the current locale to Traditional Chinese with the China country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hant-CN" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseSimplifiedInHongKong)
{
    // Temporarily set the current locale to Simplified Chinese with the Hong Kong country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-HK" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *cantoneseMessages = @{
        @"extension_name": @{
            @"message": @"香港擴展",
            @"description": @"The name of the extension in Cantonese (Hong Kong)."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
        @"_locales/zh_HK/messages.json": cantoneseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseTraditionalOnly)
{
    // Temporarily set the current locale to Traditional Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hant-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_TW/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseTraditionalScript)
{
    // Temporarily set the current locale to Simplified Chinese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hant-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), '繁體中文擴展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *traditionalChineseMessages = @{
        @"extension_name": @{
            @"message": @"繁體中文擴展",
            @"description": @"The name of the extension in Traditional Chinese."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"extension_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh_Hans/messages.json": simplifiedChineseMessages,
        @"_locales/zh_Hant/messages.json": traditionalChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nChineseLanguageFallback)
{
    // Temporarily set the current locale to Simplified Chinese in China for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"zh-Hans-CN" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('regional_name'), '简体中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('language_name'), '中文扩展')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *simplifiedChineseMessages = @{
        @"regional_name": @{
            @"message": @"简体中文扩展",
            @"description": @"The name of the extension in Simplified Chinese."
        }
    };

    auto *genericChineseMessages = @{
        @"language_name": @{
            @"message": @"中文扩展",
            @"description": @"The name of the extension in generic Chinese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/zh/messages.json": genericChineseMessages,
        @"_locales/zh_CN/messages.json": simplifiedChineseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseBrazilian)
{
    // Temporarily set the current locale to Brazilian Portuguese for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-BR" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Brasileira')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *brazilianPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Brasileira",
            @"description": @"The name of the extension in Brazilian Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt_BR/messages.json": brazilianPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseEuropean)
{
    // Temporarily set the current locale to European Portuguese for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-PT" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *europeanPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Portuguesa",
            @"description": @"The name of the extension in European Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt_PT/messages.json": europeanPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseUnitedStates)
{
    // Temporarily set the current locale to Portuguese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *europeanPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Portuguesa",
            @"description": @"The name of the extension in European Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt_PT/messages.json": europeanPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseUnitedStatesFallbackToBrazilian)
{
    // Temporarily set the current locale to Portuguese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Brasileira')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *brazilianPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Brasileira",
            @"description": @"The name of the extension in Brazilian Portuguese."
        }
    };

    auto *europeanPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Portuguesa",
            @"description": @"The name of the extension in European Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt_BR/messages.json": brazilianPortugueseMessages,
        @"_locales/pt_PT/messages.json": europeanPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseUnitedStatesFallbackToGeneric)
{
    // Temporarily set the current locale to Portuguese with the US country code for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-US" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensão Portuguesa Genérica')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *genericPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Portuguesa Genérica",
            @"description": @"The name of the extension in generic Portuguese."
        }
    };

    auto *europeanPortugueseMessages = @{
        @"extension_name": @{
            @"message": @"Extensão Portuguesa",
            @"description": @"The name of the extension in European Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt/messages.json": genericPortugueseMessages,
        @"_locales/pt_PT/messages.json": europeanPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nPortugueseLanguageFallback)
{
    // Temporarily set the current locale to Brazilian Portuguese for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"pt-BR" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('regional_name'), 'Extensão Brasileira')",
        @"browser.test.assertEq(browser.i18n.getMessage('language_name'), 'Extensão Portuguesa Genérica')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *brazilianPortugueseMessages = @{
        @"regional_name": @{
            @"message": @"Extensão Brasileira",
            @"description": @"The name of the extension in Brazilian Portuguese."
        }
    };

    auto *genericPortugueseMessages = @{
        @"language_name": @{
            @"message": @"Extensão Portuguesa Genérica",
            @"description": @"The name of the extension in generic Portuguese."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/pt/messages.json": genericPortugueseMessages,
        @"_locales/pt_BR/messages.json": brazilianPortugueseMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nSpanishLatinAmerica)
{
    // Temporarily set the current locale to Latin American Spanish.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"es-419" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en español')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *latinAmericanSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en español",
            @"description": @"The name of the extension in Latin American Spanish."
        }
    };

    auto *castilianSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en castellano",
            @"description": @"The name of the extension in Castilian Spanish."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/es_419/messages.json": latinAmericanSpanishMessages,
        @"_locales/es_ES/messages.json": castilianSpanishMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}


TEST(WKWebExtensionAPILocalization, i18nSpanishMexico)
{
    // Temporarily set the current locale to Spanish (Mexico).
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"es-MX" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en español')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *latinAmericanSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en español",
            @"description": @"The name of the extension in Latin American Spanish."
        }
    };

    auto *castilianSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en castellano",
            @"description": @"The name of the extension in Castilian Spanish."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/es_419/messages.json": latinAmericanSpanishMessages,
        @"_locales/es_ES/messages.json": castilianSpanishMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nSpanishSpain)
{
    // Temporarily set the current locale to Spanish (Spain).
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"es-ES" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Extensión en castellano')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_name'), 'Default English String')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"default_name": @{
            @"message": @"Default English String",
            @"description": @"The default name in English."
        }
    };

    auto *castilianSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en castellano",
            @"description": @"The name of the extension in Castilian Spanish."
        }
    };

    auto *latinAmericanSpanishMessages = @{
        @"extension_name": @{
            @"message": @"Extensión en español",
            @"description": @"The name of the extension in Latin American Spanish."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
        @"_locales/es_ES/messages.json": castilianSpanishMessages,
        @"_locales/es_419/messages.json": latinAmericanSpanishMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nSwedishDefaultToEnglish)
{
    // Temporarily set the current locale to Swedish (Sweden) for the test.
    [NSUserDefaults.standardUserDefaults setVolatileDomain:@{ @"AppleLanguages": @[ @"sv-SE" ] } forName:NSArgumentDomain];

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertEq(browser.i18n.getMessage('extension_name'), 'Default English Name', 'Should fall back to the default English name.')",
        @"browser.test.assertEq(browser.i18n.getMessage('default_description'), 'Default English Description', 'Should fall back to the default English description.')",

        @"browser.test.notifyPass()",
    ]);

    auto *defaultMessages = @{
        @"extension_name": @{
            @"message": @"Default English Name",
            @"description": @"The name of the extension in English."
        },
        @"default_description": @{
            @"message": @"Default English Description",
            @"description": @"A default description in English."
        }
    };

    auto *resources = @{
        @"background.js": backgroundScript,
        @"_locales/en/messages.json": defaultMessages,
    };

    auto extension = adoptNS([[WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
