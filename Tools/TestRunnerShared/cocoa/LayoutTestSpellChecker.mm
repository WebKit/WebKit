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

#import "InstanceMethodSwizzler.h"
#import "JSBasics.h"
#import <objc/runtime.h>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPIForTesting.h"
#endif

#if PLATFORM(MAC)
using TextCheckingCompletionHandler = void(^)(NSInteger, NSArray<NSTextCheckingResult *> *, NSOrthography *, NSInteger);
#else
#define NSGrammarRange @"NSGrammarRange"
#define NSGrammarCorrections @"NSGrammarCorrections"
#endif

static LayoutTestSpellChecker *globalSpellChecker = nil;
static BOOL hasSwizzledLayoutTestSpellChecker = NO;

#if PLATFORM(MAC)
static IMP globallySwizzledSharedSpellCheckerImplementation;
static Method originalSharedSpellCheckerMethod;
#else
static IMP globallySwizzledInitializeTextCheckerImplementation;
static Method originalInitializeTextCheckerMethod;

@protocol TextComposerPostEditorProtocol <NSObject>
- (NSDictionary<NSString *, id> *)grammarDetailForString:(NSString *)stringToCheck range:(NSRange)range language:(NSString *)language;
@end

@interface UITextChecker (TestingSupport)
@property (readonly, nonatomic) id<TextComposerPostEditorProtocol> postEditor;
@end
#endif

static LayoutTestSpellChecker *ensureGlobalLayoutTestSpellChecker()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        globalSpellChecker = [[LayoutTestSpellChecker alloc] init];
    });
    return globalSpellChecker;
}

static NSTextCheckingType nsTextCheckingType(NSString *typeString)
{
    if ([typeString isEqualToString:@"orthography"])
        return NSTextCheckingTypeOrthography;

    if ([typeString isEqualToString:@"spelling"])
        return NSTextCheckingTypeSpelling;

    if ([typeString isEqualToString:@"grammar"])
        return NSTextCheckingTypeGrammar;

    if ([typeString isEqualToString:@"date"])
        return NSTextCheckingTypeDate;

    if ([typeString isEqualToString:@"address"])
        return NSTextCheckingTypeAddress;

    if ([typeString isEqualToString:@"link"])
        return NSTextCheckingTypeLink;

    if ([typeString isEqualToString:@"quote"])
        return NSTextCheckingTypeQuote;

    if ([typeString isEqualToString:@"dash"])
        return NSTextCheckingTypeDash;

    if ([typeString isEqualToString:@"replacement"])
        return NSTextCheckingTypeReplacement;

    if ([typeString isEqualToString:@"correction"])
        return NSTextCheckingTypeCorrection;

    if ([typeString isEqualToString:@"regular-expression"])
        return NSTextCheckingTypeRegularExpression;

    if ([typeString isEqualToString:@"phone-number"])
        return NSTextCheckingTypePhoneNumber;

    if ([typeString isEqualToString:@"transit-information"])
        return NSTextCheckingTypeTransitInformation;

    ASSERT_NOT_REACHED();
    return NSTextCheckingTypeSpelling;
}

@interface LayoutTestTextCheckingResult : NSTextCheckingResult
- (instancetype)initWithType:(NSTextCheckingType)type range:(NSRange)range replacement:(NSString *)replacement details:(NSArray<NSDictionary<NSString *, id> *> *)details;
@end

@implementation LayoutTestTextCheckingResult {
    RetainPtr<NSString> _replacement;
    NSTextCheckingType _type;
    NSRange _range;
    RetainPtr<NSArray<NSDictionary *>> _details;
}

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
    return [NSString stringWithFormat:@"<%@ %p type=%llu range=[%u, %u] replacement='%@'>", self.class, self, _type, static_cast<unsigned>(_range.location), static_cast<unsigned>(_range.location + _range.length), _replacement.get()];
}

@end

@implementation LayoutTestSpellChecker

@synthesize spellCheckerLoggingEnabled = _spellCheckerLoggingEnabled;

#if PLATFORM(IOS_FAMILY)

static LayoutTestSpellChecker *swizzledInitializeTextChecker()
{
    return [ensureGlobalLayoutTestSpellChecker() retain];
}

#endif

+ (instancetype)checker
{
    auto *spellChecker = ensureGlobalLayoutTestSpellChecker();
    if (hasSwizzledLayoutTestSpellChecker)
        return spellChecker;

#if PLATFORM(MAC)
    originalSharedSpellCheckerMethod = class_getClassMethod(objc_getMetaClass("NSSpellChecker"), @selector(sharedSpellChecker));
    globallySwizzledSharedSpellCheckerImplementation = method_setImplementation(originalSharedSpellCheckerMethod, reinterpret_cast<IMP>(ensureGlobalLayoutTestSpellChecker));
#else
    originalInitializeTextCheckerMethod = class_getInstanceMethod(UITextChecker.class, @selector(_initWithAsynchronousLoading:));
    globallySwizzledInitializeTextCheckerImplementation = method_setImplementation(originalInitializeTextCheckerMethod, reinterpret_cast<IMP>(swizzledInitializeTextChecker));
#endif

    hasSwizzledLayoutTestSpellChecker = YES;
    return spellChecker;
}

+ (void)uninstallAndReset
{
    [globalSpellChecker reset];
    if (!hasSwizzledLayoutTestSpellChecker)
        return;

#if PLATFORM(MAC)
    method_setImplementation(originalSharedSpellCheckerMethod, globallySwizzledSharedSpellCheckerImplementation);
#else
    method_setImplementation(originalInitializeTextCheckerMethod, globallySwizzledInitializeTextCheckerImplementation);
#endif

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

- (void)setResultsFromJSValue:(JSValueRef)resultsValue inContext:(JSContextRef)jsContext
{
    auto context = [JSContext contextWithJSGlobalContextRef:JSContextGetGlobalContext(jsContext)];
    auto resultsDictionary = [JSValue valueWithJSValueRef:resultsValue inContext:context].toDictionary;
    auto finalResults = adoptNS([NSMutableDictionary new]);
    for (NSString *stringToCheck in resultsDictionary) {
        auto resultsForWord = adoptNS([NSMutableArray new]);
        for (NSDictionary *result in resultsDictionary[stringToCheck]) {
            auto from = [result[@"from"] intValue];
            auto to = [result[@"to"] intValue];
            NSString *type = result[@"type"];
            NSString *replacement = result[@"replacement"];
            auto details = adoptNS([NSMutableArray<NSDictionary *> new]);
            for (NSDictionary *detail in result[@"details"]) {
                NSNumber *detailFrom = detail[@"from"];
                NSNumber *detailTo = detail[@"to"];
                auto detailRange = NSMakeRange(detailFrom.intValue, detailTo.intValue - detailFrom.intValue);
                [details addObject:@{ NSGrammarRange: [NSValue valueWithRange:detailRange], NSGrammarCorrections: detail[@"corrections"] ?: @[ ] }];
            }
            [resultsForWord addObject:adoptNS([[LayoutTestTextCheckingResult alloc] initWithType:nsTextCheckingType(type) range:NSMakeRange(from, to - from) replacement:replacement details:details.get()]).get()];
        }
        [finalResults setObject:resultsForWord.get() forKey:stringToCheck];
    }
    _results = WTFMove(finalResults);
}

#if PLATFORM(MAC)

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

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY) && ENABLE(POST_EDITING_GRAMMAR_CHECKING)

static NSDictionary *swizzledGrammarDetailsForString(id, SEL, NSString *stringToCheck, NSRange range, NSString *language)
{
    for (LayoutTestTextCheckingResult *result in [globalSpellChecker->_results objectForKey:stringToCheck]) {
        if (!NSEqualRanges(result.range, range))
            continue;

        if (result.resultType != NSTextCheckingTypeGrammar)
            continue;

        if (NSDictionary *details = result.grammarDetails.firstObject)
            return details;
    }
    return @{ };
}

- (NSArray<NSTextAlternatives *> *)grammarAlternativesForString:(NSString *)string
{
    InstanceMethodSwizzler swizzler { self.postEditor.class, @selector(grammarDetailForString:range:language:), reinterpret_cast<IMP>(swizzledGrammarDetailsForString) };

    return [super grammarAlternativesForString:string];
}

- (NSArray<NSTextCheckingResult *> *)checkString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes languages:(NSArray<NSString *> *)languagesArray options:(NSDictionary<NSString *, id> *)options
{
    if (auto *overrideResult = [_results objectForKey:stringToCheck])
        return overrideResult;

    return [super checkString:stringToCheck range:range types:checkingTypes languages:languagesArray options:options];
}

- (BOOL)_doneLoading
{
    return YES;
}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(POST_EDITING_GRAMMAR_CHECKING)

@end
