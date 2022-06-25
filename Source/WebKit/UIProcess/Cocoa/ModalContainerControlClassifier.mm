/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "ModalContainerControlClassifier.h"

#import <WebCore/ModalContainerTypes.h>
#import <unicode/uspoof.h>

#import <wtf/CompletionHandler.h>
#import <wtf/CrossThreadCopier.h>
#import <wtf/RunLoop.h>

#import <pal/cocoa/CoreMLSoftLink.h>
#import <pal/cocoa/NaturalLanguageSoftLink.h>

static NSString *const classifierInputFeatureKey = @"text";
static NSString *const classifierOutputFeatureKey = @"label";

@interface WKModalContainerClassifierBatch : NSObject<MLBatchProvider>
- (instancetype)initWithRawInputs:(Vector<String>&&)inputStrings;
@end

@interface WKModalContainerClassifierInput : NSObject<MLFeatureProvider>
- (instancetype)initWithTokenizer:(NLTokenizer *)tokenizer rawInput:(NSString *)rawInput;
@end

@implementation WKModalContainerClassifierBatch {
    Vector<RetainPtr<WKModalContainerClassifierInput>> _inputs;
}

- (instancetype)initWithRawInputs:(Vector<String>&&)inputStrings
{
    if (!(self = [super init]))
        return nil;

    auto tokenizer = adoptNS([PAL::allocNLTokenizerInstance() initWithUnit:NLTokenUnitWord]);
    _inputs = inputStrings.map([&](auto& rawInput) {
        return adoptNS([[WKModalContainerClassifierInput alloc] initWithTokenizer:tokenizer.get() rawInput:rawInput]);
    });
    return self;
}

- (NSInteger)count
{
    return _inputs.size();
}

- (id <MLFeatureProvider>)featuresAtIndex:(NSInteger)index
{
    if (index >= static_cast<NSInteger>(_inputs.size())) {
        ASSERT_NOT_REACHED();
        return nil;
    }

    return _inputs[index].get();
}

@end

namespace WebKit {

class SpoofChecker {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ~SpoofChecker()
    {
        if (m_checker)
            uspoof_close(m_checker);
    }

    bool areConfusable(NSString *potentialSpoofString, const char* stringToSpoof)
    {
        return checker() && uspoof_areConfusableUTF8(checker(), potentialSpoofString.UTF8String, -1, stringToSpoof, -1, &m_status);
    }

private:
    USpoofChecker* checker()
    {
        if (!m_checker && U_SUCCESS(m_status))
            m_checker = uspoof_open(&m_status);
        return m_checker;
    }

    UErrorCode m_status { U_ZERO_ERROR };
    USpoofChecker* m_checker { nullptr };
};

} // namespace WebKit

@implementation WKModalContainerClassifierInput {
    RetainPtr<NSString> _canonicalInput;
}

- (instancetype)initWithTokenizer:(NLTokenizer *)tokenizer rawInput:(NSString *)rawInput
{
    if (!(self = [super init]))
        return nil;

    [tokenizer setString:rawInput];

    auto tokens = adoptNS([NSMutableArray<NSString *> new]);
    [tokenizer enumerateTokensInRange:NSMakeRange(0, rawInput.length) usingBlock:[&](NSRange range, NLTokenizerAttributes attributes, BOOL *stop) {
        if (attributes & NLTokenizerAttributeNumeric)
            return;

        NSString *lowercaseToken = [rawInput substringWithRange:range].lowercaseString;
        if (!lowercaseToken.length)
            return;

        if (attributes & (NLTokenizerAttributeSymbolic | NLTokenizerAttributeEmoji)) {
            WebKit::SpoofChecker checker;
            if ([lowercaseToken isEqualToString:@"✕"] || [lowercaseToken isEqualToString:@"✖"] || checker.areConfusable(lowercaseToken, "x") || checker.areConfusable(lowercaseToken, "X")) {
                // ICU does not consider two unicode symbols to be confusable with the letter x, but for the purposes of the classifier we need to treat them as if they were.
                [tokens addObject:@"x"];
            }
            return;
        }

        [tokens addObject:lowercaseToken];
    }];

    _canonicalInput = [tokens componentsJoinedByString:@" "];
    return self;
}

- (NSSet<NSString *> *)featureNames
{
    return [NSSet<NSString *> setWithObject:classifierInputFeatureKey];
}

- (MLFeatureValue *)featureValueForName:(NSString *)featureName
{
    return [featureName isEqualToString:classifierInputFeatureKey] ? [PAL::getMLFeatureValueClass() featureValueWithString:_canonicalInput.get()] : nil;
}

@end

namespace WebKit {
using namespace WebCore;

ModalContainerControlClassifier::ModalContainerControlClassifier()
    : m_queue(WorkQueue::create("com.apple.WebKit.ModalContainerControlClassifier"))
{
    ASSERT(RunLoop::isMain());
}

ModalContainerControlClassifier& ModalContainerControlClassifier::sharedClassifier()
{
    static NeverDestroyed<std::unique_ptr<ModalContainerControlClassifier>> classifier;
    if (!classifier.get())
        classifier.get() = makeUnique<ModalContainerControlClassifier>();
    return *classifier.get();
}

static Vector<ModalContainerControlType> computePredictions(MLModel *model, Vector<String>&& texts)
{
    ASSERT(!RunLoop::isMain());
    if (!model)
        return { };

    auto batch = adoptNS([[WKModalContainerClassifierBatch alloc] initWithRawInputs:WTFMove(texts)]);
    NSError *predictionError = nil;
    auto resultProvider = [model predictionsFromBatch:batch.get() error:&predictionError];
    if (predictionError || resultProvider.count < [batch count]) {
        // FIXME: We may want to log the error here.
        return { };
    }

    Vector<ModalContainerControlType> results;
    results.reserveInitialCapacity(resultProvider.count);

    for (NSInteger index = 0; index < resultProvider.count; ++index) {
        auto result = [resultProvider featuresAtIndex:index];
        auto stringResult = [result featureValueForName:classifierOutputFeatureKey].stringValue;
        if ([stringResult isEqualToString:@"neutral"])
            results.uncheckedAppend(ModalContainerControlType::Neutral);
        else if ([stringResult isEqualToString:@"positive"])
            results.uncheckedAppend(ModalContainerControlType::Positive);
        else if ([stringResult isEqualToString:@"negative"])
            results.uncheckedAppend(ModalContainerControlType::Negative);
        else
            results.uncheckedAppend(ModalContainerControlType::Other);
    }

    return results;
}

void ModalContainerControlClassifier::classify(Vector<String>&& texts, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    ASSERT(RunLoop::isMain());
    m_queue->dispatch([this, texts = crossThreadCopy(WTFMove(texts)), completion = WTFMove(completion)]() mutable {
        loadModelIfNeeded();
        RunLoop::main().dispatch([completion = WTFMove(completion), predictions = computePredictions(m_model.get(), WTFMove(texts))]() mutable {
            completion(WTFMove(predictions));
        });
    });
}

void ModalContainerControlClassifier::loadModelIfNeeded()
{
    ASSERT(!RunLoop::isMain());
    if (m_model)
        return;

    auto bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    auto compiledModelURL = [bundle URLForResource:@"ModalContainerControls" withExtension:@"mlmodelc"];
    if (!compiledModelURL)
        return;

    auto configuration = adoptNS([PAL::allocMLModelConfigurationInstance() init]);
    [configuration setComputeUnits:MLComputeUnitsCPUOnly];
    NSError *loadingError = nil;
    m_model = [PAL::getMLModelClass() modelWithContentsOfURL:compiledModelURL configuration:configuration.get() error:&loadingError];
}

} // namespace WebKit
