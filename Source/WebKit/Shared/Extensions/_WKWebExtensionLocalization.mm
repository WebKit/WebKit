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
#import "_WKWebExtensionLocalization.h"

#import "APIData.h"
#import "APIError.h"
#import "CocoaHelpers.h"
#import "Logging.h"
#import "WKWebExtensionInternal.h"
#import "WebExtension.h"
#import <wtf/Language.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

static NSString * const messageKey = @"message";
static NSString * const pathToJSONFile = @"_locales/%@/messages.json";
static NSString * const placeholdersKey = @"placeholders";
static NSString * const placeholderDictionaryContentKey = @"content";

static NSString * const predefinedMessageUILocale = @"@@ui_locale";
static NSString * const predefinedMessageLanguageDirection = @"@@bidi_dir";
static NSString * const predefinedMessageLanguageDirectionReversed = @"@@bidi_reversed_dir";
static NSString * const predefinedMessageTextLeadingEdge = @"@@bidi_start_edge";
static NSString * const predefinedMessageTextTrailingEdge = @"@@bidi_end_edge";
static NSString * const predefinedMessageExtensionID = @"@@extension_id";

static NSString * const predefinedMessageValueLeftToRight = @"ltr";
static NSString * const predefinedMessageValueRightToLeft = @"rtl";
static NSString * const predefinedMessageValueTextEdgeLeft = @"left";
static NSString * const predefinedMessageValueTextEdgeRight = @"right";

using LocalizationDictionary = NSDictionary<NSString *, NSDictionary *>;
using PlaceholderDictionary = NSDictionary<NSString *, NSDictionary<NSString *, NSString *> *>;

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

@interface _WKWebExtensionLocalization ()
{
    NSString *_localeString;
    NSLocale *_locale;
    NSString *_uniqueIdentifier;
}
@end

#endif // ENABLE(WK_WEB_EXTENSIONS)

@implementation _WKWebExtensionLocalization

+ (BOOL)supportsSecureCoding
{
    return YES;
}

#if ENABLE(WK_WEB_EXTENSIONS)

- (instancetype)initWithWebExtension:(WebExtension&)webExtension
{
    auto defaultLocaleString = webExtension.defaultLocale();
    if (defaultLocaleString.isEmpty()) {
        RELEASE_LOG_DEBUG(Extensions, "No default locale provided");
        return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:nil];
    }

    auto *defaultLocaleDictionary = [self _localizationDictionaryForWebExtension:webExtension withLocale:defaultLocaleString];
    if (!defaultLocaleDictionary) {
        RELEASE_LOG_DEBUG(Extensions, "No localization found for default locale %{public}@", static_cast<NSString *>(defaultLocaleString));
        return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:nil];
    }

    RELEASE_LOG_DEBUG(Extensions, "Loaded default locale %{public}@", static_cast<NSString *>(defaultLocaleString));

    auto bestLocaleString = webExtension.bestMatchLocale();
    auto defaultLocaleComponents = parseLocale(defaultLocaleString);
    auto bestLocaleComponents = parseLocale(bestLocaleString);
    auto bestLocaleLanguageOnlyString = bestLocaleComponents.languageCode;

    RELEASE_LOG_DEBUG(Extensions, "Best locale is %{public}@", static_cast<NSString *>(bestLocaleString));

    LocalizationDictionary *languageDictionary;
    if (!bestLocaleLanguageOnlyString.isEmpty() && bestLocaleLanguageOnlyString != defaultLocaleString) {
        languageDictionary = [self _localizationDictionaryForWebExtension:webExtension withLocale:bestLocaleLanguageOnlyString];
        if (languageDictionary)
            RELEASE_LOG_DEBUG(Extensions, "Loaded language-only locale %{public}@", static_cast<NSString *>(bestLocaleLanguageOnlyString));
    }

    LocalizationDictionary *regionalDictionary;
    if (bestLocaleString != bestLocaleLanguageOnlyString && bestLocaleString != defaultLocaleString) {
        regionalDictionary = [self _localizationDictionaryForWebExtension:webExtension withLocale:bestLocaleString];
        if (regionalDictionary)
            RELEASE_LOG_DEBUG(Extensions, "Loaded regional locale %{public}@", static_cast<NSString *>(bestLocaleString));
    }

    return [self initWithRegionalLocalization:regionalDictionary languageLocalization:languageDictionary defaultLocalization:defaultLocaleDictionary withBestLocale:bestLocaleString uniqueIdentifier:nil];
}

- (instancetype)initWithLocalizedDictionary:(NSDictionary<NSString *, NSDictionary *> *)localizedDictionary uniqueIdentifier:(NSString *)uniqueIdentifier
{
    ASSERT(uniqueIdentifier);

    NSString *localeString = localizedDictionary[predefinedMessageUILocale][messageKey];
    return [self initWithRegionalLocalization:localizedDictionary languageLocalization:nil defaultLocalization:nil withBestLocale:localeString uniqueIdentifier:uniqueIdentifier];
}

- (instancetype)initWithRegionalLocalization:(LocalizationDictionary *)regionalLocalization languageLocalization:(LocalizationDictionary *)languageLocalization defaultLocalization:(LocalizationDictionary *)defaultLocalization withBestLocale:(NSString *)localeString uniqueIdentifier:(NSString *)uniqueIdentifier
{
    if (!(self = [super init]))
        return nil;

    _locale = [NSLocale localeWithLocaleIdentifier:localeString];
    _localeString = localeString;
    _uniqueIdentifier = uniqueIdentifier;

    LocalizationDictionary *localizationDictionary = self._predefinedMessages;
    localizationDictionary = dictionaryWithLowercaseKeys(localizationDictionary);
    localizationDictionary = mergeDictionaries(localizationDictionary, dictionaryWithLowercaseKeys(regionalLocalization));
    localizationDictionary = mergeDictionaries(localizationDictionary, dictionaryWithLowercaseKeys(languageLocalization));
    localizationDictionary = mergeDictionaries(localizationDictionary, dictionaryWithLowercaseKeys(defaultLocalization));

    ASSERT(localizationDictionary);

    _localizationDictionary = localizationDictionary;

    return self;
}

- (NSDictionary<NSString *, id> *)localizedDictionaryForDictionary:(NSDictionary<NSString *, id> *)dictionary
{
    if (!_localizationDictionary.count)
        return dictionary;

    NSMutableDictionary<NSString *, id> *localizedDictionary = [dictionary mutableCopy];

    for (NSString *key in localizedDictionary.allKeys) {
        id value = localizedDictionary[key];

        if (auto *str = dynamic_objc_cast<NSString>(value))
            localizedDictionary[key] = [self localizedStringForString:str];
        else if (auto *arr = dynamic_objc_cast<NSArray>(value))
            localizedDictionary[key] = [self _localizedArrayForArray:arr];
        else if (auto* dict = dynamic_objc_cast<NSDictionary>(value))
            localizedDictionary[key] = [self localizedDictionaryForDictionary:dict];
    }

    return localizedDictionary;
}

- (NSString *)localizedStringForKey:(NSString *)key withPlaceholders:(NSArray<NSString *> *)placeholders
{
    if (placeholders.count > 9)
        return nil;

    if (!_localizationDictionary.count)
        return @"";

    LocalizationDictionary *stringDictionary = objectForKey<NSDictionary>(_localizationDictionary, key.lowercaseString);

    NSString *localizedString = objectForKey<NSString>(stringDictionary, messageKey);
    if (!localizedString.length)
        return @"";

    PlaceholderDictionary *namedPlaceholders = dictionaryWithLowercaseKeys(objectForKey<NSDictionary>(stringDictionary, placeholdersKey));

    localizedString = [self _stringByReplacingNamedPlaceholdersInString:localizedString withNamedPlaceholders:namedPlaceholders];
    localizedString = [self _stringByReplacingPositionalPlaceholdersInString:localizedString withValues:placeholders];

    // Replace escaped $'s.
    localizedString = [localizedString stringByReplacingOccurrencesOfString:@"$$" withString:@"$"];

    return localizedString;
}

- (NSArray *)_localizedArrayForArray:(NSArray *)array
{
    if (!_localizationDictionary.count)
        return array;

    return mapObjects(array, ^id(id key, id value) {
        if ([value isKindOfClass:NSString.class])
            return [self localizedStringForString:value];
        if ([value isKindOfClass:NSArray.class])
            return [self _localizedArrayForArray:value];
        if ([value isKindOfClass:NSDictionary.class])
            return [self localizedDictionaryForDictionary:value];
        return value;
    });
}

- (NSString *)localizedStringForString:(NSString *)sourceString
{
    NSString *localizedString = sourceString;

    NSRegularExpression *localizableStringRegularExpression = [NSRegularExpression regularExpressionWithPattern:@"__MSG_([A-Za-z0-9_@]+)__" options:0 error:nil];

    NSTextCheckingResult *localizableStringResult = [localizableStringRegularExpression firstMatchInString:localizedString options:0 range:NSMakeRange(0, localizedString.length)];
    while (localizableStringResult) {
        NSString *key = [localizedString substringWithRange:[localizableStringResult rangeAtIndex:1]];
        NSString *localizedReplacement = [self localizedStringForKey:key withPlaceholders:nil];

        NSString *stringToReplace = [localizedString substringWithRange:localizableStringResult.range];
        localizedString = [localizedString stringByReplacingOccurrencesOfString:stringToReplace withString:localizedReplacement];

        localizableStringResult = [localizableStringRegularExpression firstMatchInString:localizedString options:0 range:NSMakeRange(0, localizedString.length)];
    }

    return localizedString;
}

- (LocalizationDictionary *)_localizationDictionaryForWebExtension:(WebExtension&)webExtension withLocale:(NSString *)localeString
{
    auto *path = [NSString stringWithFormat:pathToJSONFile, localeString];

    RefPtr<API::Error> error;
    RefPtr data = webExtension.resourceDataForPath(path, error, WebExtension::CacheResult::No, WebExtension::SuppressNotFoundErrors::Yes);
    if (!data || error) {
        webExtension.recordErrorIfNeeded(error);
        return nil;
    }

    return parseJSON(static_cast<NSData *>(data->wrapper()));
}

- (LocalizationDictionary *)_predefinedMessages
{
    NSMutableDictionary *predefinedMessages;

    if (_locale && [NSParagraphStyle defaultWritingDirectionForLanguage:_locale.languageCode] == NSWritingDirectionLeftToRight) {
        predefinedMessages = [@{
            predefinedMessageLanguageDirection: @{ messageKey: predefinedMessageValueLeftToRight },
            predefinedMessageLanguageDirectionReversed: @{ messageKey: predefinedMessageValueRightToLeft },
            predefinedMessageTextLeadingEdge: @{ messageKey: predefinedMessageValueTextEdgeLeft },
            predefinedMessageTextTrailingEdge: @{ messageKey: predefinedMessageValueTextEdgeRight },
        } mutableCopy];
    } else if (_locale) {
        predefinedMessages = [@{
            predefinedMessageLanguageDirection: @{ messageKey: predefinedMessageValueRightToLeft },
            predefinedMessageLanguageDirectionReversed: @{ messageKey: predefinedMessageValueLeftToRight },
            predefinedMessageTextLeadingEdge: @{ messageKey: predefinedMessageValueTextEdgeRight },
            predefinedMessageTextTrailingEdge: @{ messageKey: predefinedMessageValueTextEdgeLeft },
        } mutableCopy];
    } else
        predefinedMessages = [NSMutableDictionary dictionary];

    if (_localeString)
        predefinedMessages[predefinedMessageUILocale] = @{ messageKey: _localeString };

    if (_uniqueIdentifier)
        predefinedMessages[predefinedMessageExtensionID] = @{ messageKey: _uniqueIdentifier };

    return [predefinedMessages copy];
}

- (NSString *)_stringByReplacingNamedPlaceholdersInString:(NSString *)string withNamedPlaceholders:(PlaceholderDictionary *)namedPlaceholders
{
    NSRegularExpression *namedPlaceholderExpression = [NSRegularExpression regularExpressionWithPattern:@"(?:[^$]|^)(\\$([A-Za-z0-9_@]+)\\$)" options:0 error:nil];
    NSTextCheckingResult *namedPlaceholderMatch = [namedPlaceholderExpression firstMatchInString:string options:0 range:NSMakeRange(0, string.length)];
    while (namedPlaceholderMatch) {
        NSString *placeholderKey = [string substringWithRange:[namedPlaceholderMatch rangeAtIndex:2]].lowercaseString;
        NSString *replacement = objectForKey<NSString>(objectForKey<NSDictionary>(namedPlaceholders, placeholderKey), placeholderDictionaryContentKey) ?: @"";

        NSString *stringToReplace = [string substringWithRange:[namedPlaceholderMatch rangeAtIndex:1]];

        string = [string stringByReplacingOccurrencesOfString:stringToReplace withString:replacement options:NSCaseInsensitiveSearch range:NSMakeRange(0, string.length)];

        namedPlaceholderMatch = [namedPlaceholderExpression firstMatchInString:string options:0 range:NSMakeRange(0, string.length)];
    }

    return string;
}

- (NSString *)_stringByReplacingPositionalPlaceholdersInString:(NSString *)string withValues:(NSArray<NSString *> *)placeholderValues
{
    NSInteger placeholderCount = placeholderValues.count;

    NSRegularExpression *positionalPlaceholderExpression = [NSRegularExpression regularExpressionWithPattern:@"(?:[^$]|^)(\\$([0-9]))" options:0 error:nil];
    NSTextCheckingResult *positionalPlaceholderMatch = [positionalPlaceholderExpression firstMatchInString:string options:0 range:NSMakeRange(0, string.length)];

    while (positionalPlaceholderMatch) {
        NSInteger position = [string substringWithRange:[positionalPlaceholderMatch rangeAtIndex:2]].integerValue;

        NSString *replacement;
        if (position > 0 && position <= placeholderCount)
            replacement = placeholderValues[position - 1];
        else
            replacement = @"";

        NSString *stringToReplace = [string substringWithRange:[positionalPlaceholderMatch rangeAtIndex:1]];

        string = [string stringByReplacingOccurrencesOfString:stringToReplace withString:replacement options:NSCaseInsensitiveSearch range:NSMakeRange(0, string.length)];
        positionalPlaceholderMatch = [positionalPlaceholderExpression firstMatchInString:string options:0 range:NSMakeRange(0, string.length)];
    }

    return string;
}

#else

- (instancetype)initWithWebExtension:(WebExtension&)extension
{
    return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:nil];
}

- (instancetype)initWithLocalizedDictionary:(NSDictionary<NSString *, NSDictionary *> *)localizedDictionary uniqueIdentifier:(NSString *)uniqueIdentifier
{
    return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:nil];
}

- (instancetype)initWithRegionalLocalization:(LocalizationDictionary *)regionalLocalization languageLocalization:(LocalizationDictionary *)languageLocalization defaultLocalization:(LocalizationDictionary *)defaultLocalization withBestLocale:(NSString *)localeString uniqueIdentifier:(NSString *)uniqueIdentifier
{
    return nil;
}

- (NSDictionary<NSString *, id> *)localizedDictionaryForDictionary:(NSDictionary<NSString *, id> *)dictionary
{
    return nil;
}

- (NSString *)localizedStringForKey:(NSString *)key withPlaceholders:(NSArray<NSString *> *)placeholders
{
    return nil;
}

- (NSString *)localizedStringForString:(NSString *)string
{
    return nil;
}

#endif // !ENABLE(WK_WEB_EXTENSIONS)

@end
