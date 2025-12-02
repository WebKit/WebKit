/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "TextExtractionFilter.h"

#if ENABLE(TEXT_EXTRACTION_FILTER)

#import <CoreML/CoreML.h>
#import <NaturalLanguage/NaturalLanguage.h>
#import <wtf/ApproximateTime.h>
#import <wtf/RunLoop.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

static RefPtr<TextExtractionFilter>& singleton()
{
    static NeverDestroyed<RefPtr<TextExtractionFilter>> singleton;
    return singleton;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextExtractionFilter);

TextExtractionFilter& TextExtractionFilter::singleton()
{
    if (auto& instance = WebKit::singleton(); !instance)
        instance = adoptRef(*new TextExtractionFilter);
    return *WebKit::singleton();
}

TextExtractionFilter* TextExtractionFilter::singletonIfCreated()
{
    return WebKit::singleton().get();
}

TextExtractionFilter::TextExtractionFilter()
    : m_modelQueue { WorkQueue::create("com.apple.WebKit.TextExtractionFilter"_s) }
{
}

void TextExtractionFilter::initializeModelIfNeeded()
{
    ASSERT(!RunLoop::isMain());

    if (m_model || m_failedInitialization)
        return;

    m_failedInitialization = true;

    RetainPtr modelURL = [[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] URLForResource:@"TextExtractionFilter" withExtension:@"mlmodel"];
    if (!modelURL)
        return;

    RetainPtr compiledModelName = [[[modelURL lastPathComponent] stringByDeletingPathExtension] stringByAppendingPathExtension:@"mlmodelc"];
    RetainPtr compiledModelURL = [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:compiledModelName.get()];

    auto needsRecompile = [&] -> bool {
        if (![[NSFileManager defaultManager] fileExistsAtPath:[compiledModelURL path]])
            return true;

        RetainPtr modelAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[modelURL path] error:nil];
        RetainPtr compiledAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:[compiledModelURL path] error:nil];
        RetainPtr modelTimestamp = [modelAttributes fileModificationDate];
        RetainPtr compiledTimestamp = [compiledAttributes fileModificationDate];
        return !compiledTimestamp || [modelTimestamp compare:compiledTimestamp.get()] == NSOrderedDescending;
    }();

    NSError *error = nil;
    if (needsRecompile) {
        RetainPtr compiledURL = [MLModel compileModelAtURL:modelURL.get() error:&error];
        if (error || !compiledURL)
            return;

        if (![compiledURL isEqual:compiledModelURL.get()]) {
            [[NSFileManager defaultManager] removeItemAtURL:compiledModelURL.get() error:nil];
            if (![[NSFileManager defaultManager] moveItemAtURL:compiledURL.get() toURL:compiledModelURL.get() error:&error])
                return;
        }
    }

    RetainPtr configuration = adoptNS([[MLModelConfiguration alloc] init]);
    [configuration setComputeUnits:MLComputeUnitsAll];

    m_model = [MLModel modelWithContentsOfURL:compiledModelURL.get() configuration:configuration.get() error:&error];
    m_tokenizer = adoptNS([[NLTokenizer alloc] initWithUnit:NLTokenUnitWord]);
    m_failedInitialization = !m_model || !m_tokenizer;
}

void TextExtractionFilter::prewarm()
{
    m_modelQueue->dispatch([protectedThis = Ref { *this }] {
        protectedThis->initializeModelIfNeeded();
    });
}

void TextExtractionFilter::resetCache()
{
    Locker locker { m_cacheLock };
    m_cache.clear();
}

void TextExtractionFilter::shouldFilter(const String& text, CompletionHandler<void(bool)>&& completionHandler)
{
    if (text.length() <= chunkSize)
        return completionHandler(false);

    m_modelQueue->dispatch([protectedThis = Ref { *this }, text = text.isolatedCopy(), completionHandler = WTFMove(completionHandler)] mutable {
        RunLoop::mainSingleton().dispatch([completionHandler = WTFMove(completionHandler), result = protectedThis->shouldFilter(text)] mutable {
            completionHandler(result);
        });
    });
}

bool TextExtractionFilter::shouldFilter(const String& text)
{
    ASSERT(!RunLoop::isMain());

    initializeModelIfNeeded();

    if (!m_model)
        return false;

    if (text.length() <= chunkSize)
        return false;

    auto cacheKey = text.hash();
    if (cacheKey) {
        Locker locker { m_cacheLock };
        if (auto cachedEntry = m_cache.find(cacheKey); cachedEntry != m_cache.end())
            return cachedEntry->value;
    }

    auto allLines = text.split('\n');
    for (auto& line : allLines) {
        if (line.length() <= chunkSize)
            continue;

        for (RetainPtr chunk : segmentText(line)) {
            NSError *error = nil;
            RetainPtr input = adoptNS([[MLDictionaryFeatureProvider alloc] initWithDictionary:@{ @"text": chunk.get() } error:&error]);
            if (error)
                break;

            RetainPtr output = [m_model predictionFromFeatures:input.get() error:&error];
            if (error)
                break;

            if ([[output featureValueForName:@"label"].stringValue isEqualToString:@"positive"]) {
                if (cacheKey) {
                    Locker locker { m_cacheLock };
                    m_cache.set(cacheKey, true);
                }
                return true;
            }
        }
    }

    if (cacheKey) {
        Locker locker { m_cacheLock };
        m_cache.set(cacheKey, false);
    }
    return false;
}

Vector<RetainPtr<NSString>> TextExtractionFilter::segmentText(const String& text)
{
    Vector<RetainPtr<NSString>> segments;
    if (text.length() <= chunkSize) {
        segments.append(text.createNSString());
        return segments;
    }

    RetainPtr nsText = text.createNSString();
    [m_tokenizer setString:nsText.get()];

    NSUInteger textLength = [nsText length];
    NSUInteger start = 0;

    while (start < textLength) {
        NSUInteger end = std::min(start + chunkSize, textLength);
        if (end < textLength) {
            NSRange tokenRange = [m_tokenizer tokenRangeAtIndex:end];
            if (tokenRange.location != NSNotFound && tokenRange.location > start)
                end = tokenRange.location;
        }

        NSRange segmentRange = NSMakeRange(start, end - start);
        segments.append([nsText substringWithRange:segmentRange]);

        if (end >= textLength)
            break;

        start = end;
        NSRange nextTokenRange = [m_tokenizer tokenRangeAtIndex:start];

        if ((nextTokenRange.location == NSNotFound || nextTokenRange.location != start)
            && nextTokenRange.location != NSNotFound && nextTokenRange.location > start)
            start = nextTokenRange.location;
    }

    return segments;
}

} // namespace WebKit

#endif // ENABLE(TEXT_EXTRACTION_FILTER)
