/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "TextExtractionTokenizer.h"

#if PLATFORM(COCOA)

#import <wtf/ASCIICType.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/StringView.h>

#import <pal/cocoa/NaturalLanguageSoftLink.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextExtractionTokenizer);

static RefPtr<TextExtractionTokenizer>& sharedInstance()
{
    static NeverDestroyed<RefPtr<TextExtractionTokenizer>> instance;
    return instance;
}

TextExtractionTokenizer& TextExtractionTokenizer::singleton()
{
    if (auto& instance = sharedInstance(); !instance)
        instance = adoptRef(*new TextExtractionTokenizer);
    return *sharedInstance();
}

TextExtractionTokenizer* TextExtractionTokenizer::singletonIfCreated()
{
    return sharedInstance().get();
}

void TextExtractionTokenizer::prewarm()
{
    ASSERT(RunLoop::isMain());
    loadEmbeddingsIfNeeded();
}

void TextExtractionTokenizer::loadEmbeddingsIfNeeded()
{
    ASSERT(RunLoop::isMain());
    if (m_loadAttempted)
        return;
    m_loadAttempted = true;

    if (!PAL::isNaturalLanguageFrameworkAvailable())
        return;

    static constexpr std::array supportedLanguages { "en"_s, "de"_s, "es"_s, "fr"_s, "it"_s, "pt"_s };

    m_embeddings.reserveInitialCapacity(supportedLanguages.size());
    for (auto language : supportedLanguages) {
        if (RetainPtr embedding = [PAL::getNLEmbeddingClassSingleton() wordEmbeddingForLanguage:language.createNSString()])
            m_embeddings.append(WTF::move(embedding));
    }
}

static Vector<String> tokenize(StringView input)
{
    Vector<String> tokens;
    StringBuilder current;

    auto flushIfMeaningful = [&] {
        if (current.isEmpty())
            return;
        auto candidate = current.toString();
        current.clear();

        bool allDigits = true;
        for (auto c : StringView { candidate }.codeUnits()) {
            if (!isASCIIDigit(c)) {
                allDigits = false;
                break;
            }
        }
        if (allDigits)
            return;

        tokens.append(candidate.convertToASCIILowercase());
    };

    for (size_t i = 0; i < input.length(); ++i) {
        auto character = input[i];
        bool isSeparator = character == '-' || character == '_' || character == ':' || character == '.' || character == '/' || isASCIIWhitespace(character);
        if (isSeparator) {
            flushIfMeaningful();
            continue;
        }

        if (i && !current.isEmpty()) {
            auto previous = input[i - 1];
            bool camelBoundary = isASCIILower(previous) && isASCIIUpper(character);
            bool digitBoundary = isASCIIDigit(previous) != isASCIIDigit(character);
            if (camelBoundary || digitBoundary)
                flushIfMeaningful();
        }

        current.append(character);
    }

    flushIfMeaningful();
    return tokens;
}

bool TextExtractionTokenizer::isMostlyRecognized(StringView input)
{
    ASSERT(RunLoop::isMain());

    loadEmbeddingsIfNeeded();
    if (m_embeddings.isEmpty())
        return false;

    auto tokens = tokenize(input);
    if (tokens.isEmpty())
        return false;

    size_t totalLength = 0;
    size_t recognizedLength = 0;
    for (auto& token : tokens) {
        totalLength += token.length();
        RetainPtr nsToken = token.createNSString();
        for (auto& embedding : m_embeddings) {
            if ([embedding.get() containsString:nsToken.get()]) {
                recognizedLength += token.length();
                break;
            }
        }
    }

    return recognizedLength * 2 > totalLength;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
