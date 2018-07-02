/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "LayoutTestSpellChecker.h"

#import <JavaScriptCore/JSRetainPtr.h>
#import <objc/runtime.h>
#import <wtf/Assertions.h>

#if PLATFORM(MAC)

static LayoutTestSpellChecker *globalSpellChecker = nil;
static BOOL hasSwizzledLayoutTestSpellChecker = NO;
static IMP globallySwizzledSharedSpellCheckerImplementation;
static Method originalSharedSpellCheckerMethod;

static LayoutTestSpellChecker *ensureGlobalLayoutTestSpellChecker()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalSpellChecker = [[LayoutTestSpellChecker alloc] init];
    });
    return globalSpellChecker;
}

static const char *stringForCorrectionResponse(NSCorrectionResponse correctionResponse)
{
    switch (correctionResponse) {
    case NSCorrectionResponseNone:
        return "none";
    case NSCorrectionResponseAccepted:
        return "accepted";
    case NSCorrectionResponseRejected:
        return "rejected";
    case NSCorrectionResponseIgnored:
        return "ignored";
    case NSCorrectionResponseEdited:
        return "edited";
    case NSCorrectionResponseReverted:
        return "reverted";
    }
    return "invalid";
}

static NSTextCheckingType nsTextCheckingType(JSRetainPtr<JSStringRef>&& jsType)
{
    auto cfType = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsType.get()));
    if (CFStringCompare(cfType.get(), CFSTR("orthography"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeOrthography;

    if (CFStringCompare(cfType.get(), CFSTR("spelling"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeSpelling;

    if (CFStringCompare(cfType.get(), CFSTR("grammar"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeGrammar;

    if (CFStringCompare(cfType.get(), CFSTR("date"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeDate;

    if (CFStringCompare(cfType.get(), CFSTR("address"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeAddress;

    if (CFStringCompare(cfType.get(), CFSTR("link"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeLink;

    if (CFStringCompare(cfType.get(), CFSTR("quote"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeQuote;

    if (CFStringCompare(cfType.get(), CFSTR("dash"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeDash;

    if (CFStringCompare(cfType.get(), CFSTR("replacement"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeReplacement;

    if (CFStringCompare(cfType.get(), CFSTR("correction"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeCorrection;

    if (CFStringCompare(cfType.get(), CFSTR("regular-expression"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeRegularExpression;

    if (CFStringCompare(cfType.get(), CFSTR("phone-number"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypePhoneNumber;

    if (CFStringCompare(cfType.get(), CFSTR("transit-information"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        return NSTextCheckingTypeTransitInformation;

    ASSERT_NOT_REACHED();
    return NSTextCheckingTypeSpelling;
}

@interface LayoutTestTextCheckingResult : NSTextCheckingResult {
@private
    RetainPtr<NSString> _replacement;
    NSTextCheckingType _type;
    NSRange _range;
}

- (instancetype)initWithType:(NSTextCheckingType)type range:(NSRange)range replacement:(NSString *)replacement;
@end

@implementation LayoutTestTextCheckingResult

- (instancetype)initWithType:(NSTextCheckingType)type range:(NSRange)range replacement:(NSString *)replacement
{
    if (!(self = [super init]))
        return nil;

    _type = type;
    _range = range;
    _replacement = replacement;

    return self;
}

- (NSRange)range
{
    return _range;
}

- (NSTextCheckingType)resultType
{
    return _type;
}

- (NSString *)replacementString
{
    return _replacement.get();
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p type=%tu range=[%tu, %tu] replacement='%@'>", self.class, self, _type, _range.location, _range.location + _range.length, _replacement.get()];
}

@end

@implementation LayoutTestSpellChecker

@synthesize spellCheckerLoggingEnabled=_spellCheckerLoggingEnabled;

+ (instancetype)checker
{
    auto *spellChecker = ensureGlobalLayoutTestSpellChecker();
    if (hasSwizzledLayoutTestSpellChecker)
        return spellChecker;

    originalSharedSpellCheckerMethod = class_getClassMethod(objc_getMetaClass("NSSpellChecker"), @selector(sharedSpellChecker));
    globallySwizzledSharedSpellCheckerImplementation = method_setImplementation(originalSharedSpellCheckerMethod, reinterpret_cast<IMP>(ensureGlobalLayoutTestSpellChecker));
    hasSwizzledLayoutTestSpellChecker = YES;
    return spellChecker;
}

+ (void)uninstallAndReset
{
    [globalSpellChecker reset];
    if (!hasSwizzledLayoutTestSpellChecker)
        return;

    method_setImplementation(originalSharedSpellCheckerMethod, globallySwizzledSharedSpellCheckerImplementation);
    hasSwizzledLayoutTestSpellChecker = NO;
}

- (void)reset
{
    self.replacements = nil;
    self.spellCheckerLoggingEnabled = NO;
}

- (NSDictionary<NSString *, LayoutTestTextCheckingResult *> *)replacements
{
    return _replacements.get();
}

- (void)setReplacements:(NSDictionary<NSString *, LayoutTestTextCheckingResult *> *)replacements
{
    _replacements = adoptNS(replacements.copy);
}

- (void)setReplacementsFromJSObject:(JSObjectRef)replacements inContext:(JSContextRef)context
{
    auto fromPropertyName = adopt(JSStringCreateWithUTF8CString("from"));
    auto toPropertyName = adopt(JSStringCreateWithUTF8CString("to"));
    auto typePropertyName = adopt(JSStringCreateWithUTF8CString("type"));
    auto replacementPropertyName = adopt(JSStringCreateWithUTF8CString("replacement"));
    auto properties = JSObjectCopyPropertyNames(context, replacements);
    auto result = adoptNS([[NSMutableDictionary alloc] init]);
    for (size_t index = 0; index < JSPropertyNameArrayGetCount(properties); ++index) {
        JSStringRef wordToReplace = JSPropertyNameArrayGetNameAtIndex(properties, index);
        JSObjectRef replacement = JSValueToObject(context, JSObjectGetProperty(context, replacements, wordToReplace, nullptr), nullptr);
        long fromValue = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, replacement, fromPropertyName.get(), nullptr), nullptr));
        long toValue = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, replacement, toPropertyName.get(), nullptr), nullptr));
        auto typeValue = adopt(JSValueToStringCopy(context, JSObjectGetProperty(context, replacement, typePropertyName.get(), nullptr), nullptr));
        auto replacementValue = JSObjectGetProperty(context, replacement, replacementPropertyName.get(), nullptr);
        RetainPtr<CFStringRef> replacementText;
        if (!JSValueIsUndefined(context, replacementValue)) {
            auto replacementJSString = adopt(JSValueToStringCopy(context, replacementValue, nullptr));
            replacementText = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, replacementJSString.get()));
        }
        auto spellCheckResult = adoptNS([[LayoutTestTextCheckingResult alloc] initWithType:nsTextCheckingType(WTFMove(typeValue)) range:NSMakeRange(fromValue, toValue - fromValue) replacement:(NSString *)replacementText.get()]);
        auto cfWordToReplace = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, wordToReplace));
        [result setObject:spellCheckResult.get() forKey:(NSString *)cfWordToReplace.get()];
    }
    JSPropertyNameArrayRelease(properties);

    _replacements = WTFMove(result);
}

- (NSArray<NSTextCheckingResult *> *)checkString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes options:(NSDictionary<NSString *, id> *)options inSpellDocumentWithTag:(NSInteger)tag orthography:(NSOrthography **)orthography wordCount:(NSInteger *)wordCount
{
    if (auto *result = [_replacements objectForKey:stringToCheck])
        return @[ result ];

    return [super checkString:stringToCheck range:range types:checkingTypes options:options inSpellDocumentWithTag:tag orthography:orthography wordCount:wordCount];
}

- (void)recordResponse:(NSCorrectionResponse)response toCorrection:(NSString *)correction forWord:(NSString *)word language:(NSString *)language inSpellDocumentWithTag:(NSInteger)tag
{
    if (_spellCheckerLoggingEnabled)
        printf("NSSpellChecker recordResponseToCorrection: %s -> %s (response: %s)\n", [word UTF8String], [correction UTF8String], stringForCorrectionResponse(response));

    [super recordResponse:response toCorrection:correction forWord:word language:language inSpellDocumentWithTag:tag];
}

@end

#endif // PLATFORM(MAC)
