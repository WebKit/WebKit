/*
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(IMAGE_ANALYSIS)

OBJC_CLASS VKCImageAnalysis;

#if ENABLE(DATA_DETECTION)
OBJC_CLASS DDScannerResult;
#endif

#include "FloatQuad.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct CharacterRange;

struct TextRecognitionWordData {
    TextRecognitionWordData(const String& theText, FloatQuad&& quad, bool leadingWhitespace)
        : text(theText)
        , normalizedQuad(WTFMove(quad))
        , hasLeadingWhitespace(leadingWhitespace)
    {
    }

    String text;
    FloatQuad normalizedQuad;
    bool hasLeadingWhitespace { true };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TextRecognitionWordData> decode(Decoder&);
};

template<class Encoder> void TextRecognitionWordData::encode(Encoder& encoder) const
{
    encoder << text;
    encoder << normalizedQuad;
    encoder << hasLeadingWhitespace;
}

template<class Decoder> std::optional<TextRecognitionWordData> TextRecognitionWordData::decode(Decoder& decoder)
{
    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<FloatQuad> normalizedQuad;
    decoder >> normalizedQuad;
    if (!normalizedQuad)
        return std::nullopt;

    std::optional<bool> hasLeadingWhitespace;
    decoder >> hasLeadingWhitespace;
    if (!hasLeadingWhitespace)
        return std::nullopt;

    return {{ WTFMove(*text), WTFMove(*normalizedQuad), *hasLeadingWhitespace }};
}

struct TextRecognitionLineData {
    TextRecognitionLineData(FloatQuad&& quad, Vector<TextRecognitionWordData>&& theChildren, bool newline)
        : normalizedQuad(WTFMove(quad))
        , children(WTFMove(theChildren))
        , hasTrailingNewline(newline)
    {
    }

    FloatQuad normalizedQuad;
    Vector<TextRecognitionWordData> children;
    bool hasTrailingNewline { true };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TextRecognitionLineData> decode(Decoder&);
};

#if ENABLE(DATA_DETECTION)

struct TextRecognitionDataDetector {
    TextRecognitionDataDetector() = default;
    TextRecognitionDataDetector(DDScannerResult *scannerResult, Vector<FloatQuad>&& quads)
        : result(scannerResult)
        , normalizedQuads(WTFMove(quads))
    {
    }

    RetainPtr<DDScannerResult> result;
    Vector<FloatQuad> normalizedQuads;
};

#endif // ENABLE(DATA_DETECTION)

template<class Encoder> void TextRecognitionLineData::encode(Encoder& encoder) const
{
    encoder << normalizedQuad;
    encoder << children;
    encoder << hasTrailingNewline;
}

template<class Decoder> std::optional<TextRecognitionLineData> TextRecognitionLineData::decode(Decoder& decoder)
{
    std::optional<FloatQuad> normalizedQuad;
    decoder >> normalizedQuad;
    if (!normalizedQuad)
        return std::nullopt;

    std::optional<Vector<TextRecognitionWordData>> children;
    decoder >> children;
    if (!children)
        return std::nullopt;

    std::optional<bool> hasTrailingNewline;
    decoder >> hasTrailingNewline;
    if (!hasTrailingNewline)
        return std::nullopt;

    return { { WTFMove(*normalizedQuad), WTFMove(*children), *hasTrailingNewline } };
}

struct TextRecognitionBlockData {
    TextRecognitionBlockData(const String& theText, FloatQuad&& quad)
        : text(theText)
        , normalizedQuad(WTFMove(quad))
    {
    }

    String text;
    FloatQuad normalizedQuad;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TextRecognitionBlockData> decode(Decoder&);
};

template<class Encoder> void TextRecognitionBlockData::encode(Encoder& encoder) const
{
    encoder << text;
    encoder << normalizedQuad;
}

template<class Decoder> std::optional<TextRecognitionBlockData> TextRecognitionBlockData::decode(Decoder& decoder)
{
    std::optional<String> text;
    decoder >> text;
    if (!text)
        return std::nullopt;

    std::optional<FloatQuad> normalizedQuad;
    decoder >> normalizedQuad;
    if (!normalizedQuad)
        return std::nullopt;

    return { { WTFMove(*text), WTFMove(*normalizedQuad) } };
}

struct TextRecognitionResult {
    Vector<TextRecognitionLineData> lines;

#if ENABLE(DATA_DETECTION)
    Vector<TextRecognitionDataDetector> dataDetectors;
#endif

    Vector<TextRecognitionBlockData> blocks;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RetainPtr<VKCImageAnalysis> platformData;
#endif

    bool isEmpty() const
    {
        if (!lines.isEmpty())
            return false;

#if ENABLE(DATA_DETECTION)
        if (!dataDetectors.isEmpty())
            return false;
#endif

        if (!blocks.isEmpty())
            return false;

        return true;
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<TextRecognitionResult> decode(Decoder&);
};

template<class Encoder> void TextRecognitionResult::encode(Encoder& encoder) const
{
    encoder << lines;
#if ENABLE(DATA_DETECTION)
    encoder << dataDetectors;
#endif
    encoder << blocks;
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    encoder << platformData;
#endif
}

template<class Decoder> std::optional<TextRecognitionResult> TextRecognitionResult::decode(Decoder& decoder)
{
    std::optional<Vector<TextRecognitionLineData>> lines;
    decoder >> lines;
    if (!lines)
        return std::nullopt;

#if ENABLE(DATA_DETECTION)
    std::optional<Vector<TextRecognitionDataDetector>> dataDetectors;
    decoder >> dataDetectors;
    if (!dataDetectors)
        return std::nullopt;
#endif

    std::optional<Vector<TextRecognitionBlockData>> blocks;
    decoder >> blocks;
    if (!blocks)
        return std::nullopt;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    std::optional<RetainPtr<VKCImageAnalysis>> platformData;
    decoder >> platformData;
    if (!platformData)
        return std::nullopt;
#endif

    return {{
        WTFMove(*lines),
#if ENABLE(DATA_DETECTION)
        WTFMove(*dataDetectors),
#endif
        WTFMove(*blocks),
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
        WTFMove(*platformData),
#endif
    }};
}

} // namespace WebCore

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/TextRecognitionResultAdditions.h>
#endif

#endif // ENABLE(IMAGE_ANALYSIS)
