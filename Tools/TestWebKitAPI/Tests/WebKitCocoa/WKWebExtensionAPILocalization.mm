/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WebExtensionUtilities.h"

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
    if (!locale.languageCode)
        return @"";

    if (locale.countryCode.length)
        return [NSString stringWithFormat:@"%@-%@", locale.languageCode, locale.countryCode];
    return locale.languageCode;
}

static NSString *currentLocaleString = localeStringInWebExtensionFormat(currentLocale);

static auto *baseURLString = @"test-extension://76C788B8-3374-400D-8259-40E5B9DF79D3";

TEST(WKWebExtensionAPILocalization, Errors)
{
    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.assertRejects(() => browser.i18n.getMessage(''), /'message' argument should not be an empty string./)",
        @"browser.test.assertRejects(() => browser.i18n.getMessage(), /'message' argument is invalid because a string is expected./)",
        @"browser.test.assertRejects(() => browser.i18n.getMessage(123), /'message' argument is invalid because a string is expected./)",
        @"browser.test.assertRejects(() => browser.i18n.getMessage(null), /'message' argument is invalid because a string is expected./)",
        @"browser.test.assertRejects(() => browser.i18n.getMessage(undefined), /'message' argument is invalid because a string is expected./)",

        // Finish
        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": messages }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
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

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
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

        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": messages }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

TEST(WKWebExtensionAPILocalization, i18nWithFallback)
{
    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptedLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptedLanguages addObject:localeIdentifier];
        [acceptedLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    auto *acceptedLanguagesString = Util::constructJSArrayOfStrings(acceptedLanguages.array);

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],
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

        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
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

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],

        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
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

    auto *backgroundScript = Util::constructScript(@[
        [NSString stringWithFormat:@"const acceptedLanguages = %@", acceptedLanguagesString],
        [NSString stringWithFormat:@"const currentUILanguage = '%@'", currentLocaleString],

        @"browser.test.assertEq(browser.i18n.getMessage('@@extension_id'), '76C788B8-3374-400D-8259-40E5B9DF79D3')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@ui_locale'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_reversed_dir'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_start_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('@@bidi_end_edge'), '')",
        @"browser.test.assertEq(browser.i18n.getMessage('unknown_message'), '')",

        @"browser.test.assertEq(browser.i18n.getUILanguage(), currentUILanguage)",
        @"browser.test.assertDeepEq(await browser.i18n.getAcceptLanguages(), acceptedLanguages)",

        @"browser.test.notifyPass()",
    ]);

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:manifestWithoutLocale resources:@{ @"background.js": backgroundScript }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
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

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:localizationManifest resources:@{ @"background.js": backgroundScript, @"_locales/en/messages.json": localizationDictionary }]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    // Set a base URL so it is a known value and not the default random one.
    manager.get().context.baseURL = [NSURL URLWithString:baseURLString];

    [manager loadAndRun];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
