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
#import <wtf/BlockPtr.h>

#if PLATFORM(MAC)

using TextCheckingCompletionHandler = void(^)(NSInteger, NSArray<NSTextCheckingResult *> *, NSOrthography *, NSInteger);

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

static NSTextCheckingType nsTextCheckingType(JSStringRef jsType)
{
    auto cfType = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, jsType));
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
    RetainPtr<NSArray<NSDictionary *>> _details;
}

- (instancetype)initWithType:(NSTextCheckingType)type range:(NSRange)range replacement:(NSString *)replacement details:(NSArray<NSDictionary<NSString *, id> *> *)details;
@end

@implementation LayoutTestTextCheckingResult

- (instancetype)initWithType:(NSTextCheckingType)type range:(NSRange)range replacement:(NSString *)replacement details:(NSArray<NSDictionary<NSString *, id> *> *)details
{
    if (!(self = [super init]))
        return nil;

    _type = type;
    _range = range;
    _replacement = adoptNS(replacement.copy);
    _details = adoptNS(details.copy);

    return self;
}

- (NSArray<NSDictionary<NSString *, id> *> *)grammarDetails
{
    return _details.get();
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
    return [NSString stringWithFormat:@"<%@ %p type=%llu range=[%tu, %tu] replacement='%@'>", self.class, self, _type, _range.location, _range.location + _range.length, _replacement.get()];
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
    self.results = nil;
    self.spellCheckerLoggingEnabled = NO;
}

- (TextCheckingResultsDictionary *)results
{
    return _results.get();
}

- (void)setResults:(TextCheckingResultsDictionary *)results
{
    _results = adoptNS(results.copy);
}

- (void)setResultsFromJSValue:(JSValueRef)resultsValue inContext:(JSContextRef)context
{
    auto resultsObject = JSValueToObject(context, resultsValue, nullptr);
    auto fromPropertyName = adopt(JSStringCreateWithUTF8CString("from"));
    auto toPropertyName = adopt(JSStringCreateWithUTF8CString("to"));
    auto typePropertyName = adopt(JSStringCreateWithUTF8CString("type"));
    auto replacementPropertyName = adopt(JSStringCreateWithUTF8CString("replacement"));
    auto detailsPropertyName = adopt(JSStringCreateWithUTF8CString("details"));
    auto results = adoptNS([[NSMutableDictionary alloc] init]);

    // FIXME: Using the Objective-C API would make this logic easier to follow.
    auto properties = JSObjectCopyPropertyNames(context, resultsObject);
    for (size_t index = 0; index < JSPropertyNameArrayGetCount(properties); ++index) {
        JSStringRef textToCheck = JSPropertyNameArrayGetNameAtIndex(properties, index);
        JSObjectRef resultsArray = JSValueToObject(context, JSObjectGetProperty(context, resultsObject, textToCheck, nullptr), nullptr);
        auto resultsArrayPropertyNames = JSObjectCopyPropertyNames(context, resultsArray);
        auto resultsForWord = adoptNS([[NSMutableArray alloc] init]);
        for (size_t resultIndex = 0; resultIndex < JSPropertyNameArrayGetCount(resultsArrayPropertyNames); ++resultIndex) {
            auto resultsObject = JSValueToObject(context, JSObjectGetPropertyAtIndex(context, resultsArray, resultIndex, nullptr), nullptr);
            long fromValue = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, resultsObject, fromPropertyName.get(), nullptr), nullptr));
            long toValue = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, resultsObject, toPropertyName.get(), nullptr), nullptr));
            auto typeValue = adopt(JSValueToStringCopy(context, JSObjectGetProperty(context, resultsObject, typePropertyName.get(), nullptr), nullptr));
            auto replacementValue = JSObjectGetProperty(context, resultsObject, replacementPropertyName.get(), nullptr);
            RetainPtr<CFStringRef> replacementText;
            if (!JSValueIsUndefined(context, replacementValue)) {
                auto replacementJSString = adopt(JSValueToStringCopy(context, replacementValue, nullptr));
                replacementText = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, replacementJSString.get()));
            }
            auto details = adoptNS([[NSMutableArray alloc] init]);
            auto detailsValue = JSObjectGetProperty(context, resultsObject, detailsPropertyName.get(), nullptr);
            if (!JSValueIsUndefined(context, detailsValue)) {
                auto detailsObject = JSValueToObject(context, detailsValue, nullptr);
                auto detailsObjectProperties = JSObjectCopyPropertyNames(context, detailsObject);
                for (size_t detailIndex = 0; detailIndex < JSPropertyNameArrayGetCount(detailsObjectProperties); ++detailIndex) {
                    auto detailObject = JSValueToObject(context, JSObjectGetPropertyAtIndex(context, detailsObject, detailIndex, nullptr), nullptr);
                    long from = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, detailObject, fromPropertyName.get(), nullptr), nullptr));
                    long to = lroundl(JSValueToNumber(context, JSObjectGetProperty(context, detailObject, toPropertyName.get(), nullptr), nullptr));
                    [details addObject:@{ NSGrammarRange: [NSValue valueWithRange:NSMakeRange(from, to - from)] }];
                }
                JSPropertyNameArrayRelease(detailsObjectProperties);
            }
            [resultsForWord addObject:[[[LayoutTestTextCheckingResult alloc] initWithType:nsTextCheckingType(typeValue.get()) range:NSMakeRange(fromValue, toValue - fromValue) replacement:(__bridge NSString *)replacementText.get() details:details.get()] autorelease]];
        }
        auto cfTextToCheck = adoptCF(JSStringCopyCFString(kCFAllocatorDefault, textToCheck));
        [results setObject:resultsForWord.get() forKey:(__bridge NSString *)cfTextToCheck.get()];
        JSPropertyNameArrayRelease(resultsArrayPropertyNames);
    }
    JSPropertyNameArrayRelease(properties);

    _results = WTFMove(results);
}

- (NSArray<NSTextCheckingResult *> *)checkString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes options:(NSDictionary<NSString *, id> *)options inSpellDocumentWithTag:(NSInteger)tag orthography:(NSOrthography **)orthography wordCount:(NSInteger *)wordCount
{
    NSArray *result = [super checkString:stringToCheck range:range types:checkingTypes options:options inSpellDocumentWithTag:tag orthography:orthography wordCount:wordCount];
    if (auto *overrideResult = [_results objectForKey:stringToCheck])
        return overrideResult;

    return result;
}

- (void)recordResponse:(NSCorrectionResponse)response toCorrection:(NSString *)correction forWord:(NSString *)word language:(NSString *)language inSpellDocumentWithTag:(NSInteger)tag
{
    if (_spellCheckerLoggingEnabled)
        printf("NSSpellChecker recordResponseToCorrection: %s -> %s (response: %s)\n", [word UTF8String], [correction UTF8String], stringForCorrectionResponse(response));

    [super recordResponse:response toCorrection:correction forWord:word language:language inSpellDocumentWithTag:tag];
}

- (NSInteger)requestCheckingOfString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes options:(NSDictionary<NSString *, id> *)options inSpellDocumentWithTag:(NSInteger)tag completionHandler:(TextCheckingCompletionHandler)completionHandler
{
    return [super requestCheckingOfString:stringToCheck range:range types:checkingTypes options:options inSpellDocumentWithTag:tag completionHandler:[overrideResult = retainPtr([_results objectForKey:stringToCheck]), completion = makeBlockPtr(completionHandler), stringToCheck = retainPtr(stringToCheck)] (NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *result, NSOrthography *orthography, NSInteger wordCount) {
        if (overrideResult) {
            completion(sequenceNumber, overrideResult.get(), orthography, wordCount);
            return;
        }

        completion(sequenceNumber, result, orthography, wordCount);
    }];
}

@end

#endif // PLATFORM(MAC)
