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
#import "_WKWebExtensionLocalization.h"

#import "CocoaHelpers.h"
#import "Logging.h"

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#endif

NSString * const _WKLocalizationDictionaryMessageKey = @"message";
NSString * const _WKLocalizationDictionaryDescriptionKey = @"description";
NSString * const _WKLocalizationDictionaryPlaceholdersKey = @"placeholders";

static NSString * const localizationDictionaryJSONPathFormat = @"_locales/%@/messages.json";
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

static NSString * const placeholderDictionaryContentKey = @"content";

// NSCoding keys
static NSString * const localizationDictionaryCodingKey = @"localizationDictionary";
static NSString * const localeStringDictionaryCodingKey = @"localeString";
static NSString * const localeCodingKey = @"locale";
static NSString * const uniqueIdentifierCodingKey = @"uniqueIdentifier";

using LocalizationDictionary = NSDictionary<NSString *, NSDictionary *>;
using PlaceholderDictionary = NSDictionary<NSString *, NSDictionary<NSString *, NSString *> *>;

using namespace WebKit;

#if ENABLE(WK_WEB_EXTENSIONS)

static LocalizationDictionary *localizationDictionaryAtURL(NSURL *dictionaryFileURL)
{
    NSData *dictionaryData = [NSData dataWithContentsOfURL:dictionaryFileURL];
    if (!dictionaryData)
        return nil;

    NSError *jsonParsingError;
    LocalizationDictionary *localizationDictionary = dynamic_objc_cast<NSDictionary>([NSJSONSerialization JSONObjectWithData:dictionaryData options:0 error:&jsonParsingError]);

    if (!localizationDictionary)
        RELEASE_LOG_ERROR(Extensions , "Could not parse localization dictionary: %{public}@", privacyPreservingDescription(jsonParsingError));

    return localizationDictionary;
}

@interface _WKWebExtensionLocalization ()
{
    LocalizationDictionary *_localizationDictionary;
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

- (instancetype)initWithBundleURL:(NSURL *)bundleURL defaultLocale:(NSString *)defaultLocaleString uniqueIdentifier:(NSString *)uniqueIdentifier
{
    if (!defaultLocaleString.length) {
        RELEASE_LOG(Extensions, "Not loading localization for extension bundle at URL %{private}@. No default locale provided.", bundleURL);
        return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:uniqueIdentifier];
    }

    NSLocale *currentLocale = NSLocale.autoupdatingCurrentLocale;
    NSString *languageCode = currentLocale.languageCode;
    NSString *countryCode = currentLocale.countryCode;

    NSString *regionalLocaleString = [NSString stringWithFormat:@"%@_%@", languageCode, countryCode];
    NSURL *defaultLocaleDictionaryURL = [bundleURL URLByAppendingPathComponent:[NSString stringWithFormat:localizationDictionaryJSONPathFormat, defaultLocaleString]];
    NSURL *languageDictionaryURL = [bundleURL URLByAppendingPathComponent:[NSString stringWithFormat:localizationDictionaryJSONPathFormat, languageCode]];
    NSURL *regionalDictionaryURL = [bundleURL URLByAppendingPathComponent:[NSString stringWithFormat:localizationDictionaryJSONPathFormat, regionalLocaleString]];

    NSString *bestLocaleString;

    LocalizationDictionary *defaultLocaleDictionary = localizationDictionaryAtURL(defaultLocaleDictionaryURL);
    if (defaultLocaleDictionary)
        bestLocaleString = defaultLocaleString;
    else
        RELEASE_LOG_DEBUG(Extensions, "Could not find localization for %{public}@ for extension bundle at URL %{private}@", defaultLocaleString, bundleURL);

    LocalizationDictionary *languageDictionary = localizationDictionaryAtURL(languageDictionaryURL);
    if (languageDictionary)
        bestLocaleString = languageCode;
    else
        RELEASE_LOG_DEBUG(Extensions, "Could not find localization for %{public}@ for extension bundle at URL %{private}@", languageCode, bundleURL);

    LocalizationDictionary *regionalDictionary = localizationDictionaryAtURL(regionalDictionaryURL);
    if (regionalDictionary)
        bestLocaleString = defaultLocaleString;
    else
        RELEASE_LOG_DEBUG(Extensions, "Could not find localization for %{public}@ for extension bundle at URL %{private}@", regionalLocaleString, bundleURL);

    return [self initWithRegionalLocalization:regionalDictionary languageLocalization:languageDictionary defaultLocalization:defaultLocaleDictionary withBestLocale:bestLocaleString uniqueIdentifier:uniqueIdentifier];
}

- (instancetype)initWithRegionalLocalization:(LocalizationDictionary *)regionalLocalization languageLocalization:(LocalizationDictionary *)languageLocalization defaultLocalization:(LocalizationDictionary *)defaultLocalization withBestLocale:(NSString *)localeString uniqueIdentifier:(NSString *)uniqueIdentifier
{
    if (!(self = [super init]))
        return nil;

    _locale = [NSLocale localeWithLocaleIdentifier:localeString];
    _localeString = localeString;
    if (_uniqueIdentifier)
        _uniqueIdentifier = uniqueIdentifier;

    LocalizationDictionary *localizationDictionary = [self _predefinedMessagesForLocale:_locale];
    localizationDictionary = dictionaryWithLowercaseKeys(localizationDictionary);
    localizationDictionary = mergeDictionaries(languageLocalization, localizationDictionary);
    localizationDictionary = mergeDictionaries(localizationDictionary, dictionaryWithLowercaseKeys(languageLocalization));
    localizationDictionary = mergeDictionaries(localizationDictionary, dictionaryWithLowercaseKeys(defaultLocalization));

    _localizationDictionary = localizationDictionary;

    return self;
}

- (NSDictionary<NSString *, id> *)localizedDictionaryForDictionary:(NSDictionary<NSString *, id> *)dictionary
{
    if (!_localizationDictionary)
        return dictionary;

    NSMutableDictionary<NSString *, id> *localizedDictionary = [dictionary mutableCopy];

    for (NSString *key in localizedDictionary.allKeys) {
        id value = localizedDictionary[key];

        if ([value isKindOfClass:NSString.class])
            localizedDictionary[key] = [self localizedStringForString:(NSString *)value];
        else if ([value isKindOfClass:NSArray.class])
            localizedDictionary[key] = [self _localizedArrayForArray:(NSArray *)value];
        else if ([value isKindOfClass:NSDictionary.class])
            localizedDictionary[key] = [self localizedDictionaryForDictionary:(NSDictionary *)value];
    }

    return localizedDictionary;
}

- (NSString *)localizedStringForKey:(NSString *)key withPlaceholders:(NSArray<NSString *> *)placeholders
{
    if (placeholders.count > 9)
        return nil;

    LocalizationDictionary *stringDictionary = objectForKey<NSDictionary>(_localizationDictionary, key.lowercaseString);

    NSString *localizedString = objectForKey<NSString>(stringDictionary, _WKLocalizationDictionaryMessageKey);
    if (!localizedString.length)
        return @"";

    PlaceholderDictionary *namedPlaceholders = dictionaryWithLowercaseKeys(objectForKey<NSDictionary>(stringDictionary, _WKLocalizationDictionaryPlaceholdersKey));

    localizedString = [self _stringByReplacingNamedPlaceholdersInString:localizedString withNamedPlaceholders:namedPlaceholders];
    localizedString = [self _stringByReplacingPositionalPlaceholdersInString:localizedString withValues:placeholders];

    // Replace escaped $'s.
    localizedString = [localizedString stringByReplacingOccurrencesOfString:@"$$" withString:@"$"];

    return localizedString;
}

- (NSArray *)_localizedArrayForArray:(NSArray *)array
{
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

- (LocalizationDictionary *)_predefinedMessagesForLocale:(NSLocale *)locale
{
    NSDictionary *predefindedMessage;
    if ([NSParagraphStyle defaultWritingDirectionForLanguage:_locale.languageCode] == NSWritingDirectionLeftToRight) {
        predefindedMessage = @{
            predefinedMessageUILocale: @{ _WKLocalizationDictionaryMessageKey: _localeString ?: @"" },
            predefinedMessageLanguageDirection: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueLeftToRight },
            predefinedMessageLanguageDirectionReversed: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueRightToLeft },
            predefinedMessageTextLeadingEdge: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueTextEdgeLeft },
            predefinedMessageTextTrailingEdge: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueTextEdgeRight },
        };
    } else {
        predefindedMessage = @{
            predefinedMessageUILocale: @{ _WKLocalizationDictionaryMessageKey: _localeString ?: @"" },
            predefinedMessageLanguageDirection: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueRightToLeft },
            predefinedMessageLanguageDirectionReversed: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueLeftToRight },
            predefinedMessageTextLeadingEdge: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueTextEdgeRight },
            predefinedMessageTextTrailingEdge: @{ _WKLocalizationDictionaryMessageKey: predefinedMessageValueTextEdgeLeft },
        };
    }

    if (_uniqueIdentifier)
        return mergeDictionaries(predefindedMessage, @{ predefinedMessageExtensionID: @{ _WKLocalizationDictionaryMessageKey: _uniqueIdentifier } });
    return predefindedMessage;
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

- (instancetype)initWithBundleURL:(NSURL *)bundleURL defaultLocale:(NSString *)defaultLocaleString uniqueIdentifier:(NSString *)uniqueIdentifier
{
    return [self initWithRegionalLocalization:nil languageLocalization:nil defaultLocalization:nil withBestLocale:nil uniqueIdentifier:uniqueIdentifier];
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
