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

#if ENABLE(IMAGE_EXTRACTION)

#include "FloatQuad.h"
#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ImageExtractionTextData {
    ImageExtractionTextData(const String& theText, FloatQuad&& quad)
        : text(theText)
        , normalizedQuad(WTFMove(quad))
    {
    }

    String text;
    FloatQuad normalizedQuad;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ImageExtractionTextData> decode(Decoder&);
};

template<class Encoder> void ImageExtractionTextData::encode(Encoder& encoder) const
{
    encoder << text;
    encoder << normalizedQuad;
}

template<class Decoder> Optional<ImageExtractionTextData> ImageExtractionTextData::decode(Decoder& decoder)
{
    Optional<String> text;
    decoder >> text;
    if (!text)
        return WTF::nullopt;

    Optional<FloatQuad> normalizedQuad;
    decoder >> normalizedQuad;
    if (!normalizedQuad)
        return WTF::nullopt;

    return {{ WTFMove(*text), WTFMove(*normalizedQuad) }};
}

struct ImageExtractionLineData {
    ImageExtractionLineData(FloatQuad&& quad, Vector<ImageExtractionTextData>&& theChildren)
        : normalizedQuad(WTFMove(quad))
        , children(WTFMove(theChildren))
    {
    }

    FloatQuad normalizedQuad;
    Vector<ImageExtractionTextData> children;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ImageExtractionLineData> decode(Decoder&);
};

template<class Encoder> void ImageExtractionLineData::encode(Encoder& encoder) const
{
    encoder << normalizedQuad;
    encoder << children;
}

template<class Decoder> Optional<ImageExtractionLineData> ImageExtractionLineData::decode(Decoder& decoder)
{
    Optional<FloatQuad> normalizedQuad;
    decoder >> normalizedQuad;
    if (!normalizedQuad)
        return WTF::nullopt;

    Optional<Vector<ImageExtractionTextData>> children;
    decoder >> children;
    if (!children)
        return WTF::nullopt;

    return {{ WTFMove(*normalizedQuad), WTFMove(*children) }};
}

struct ImageExtractionResult {
    Vector<ImageExtractionLineData> lines;

    bool isEmpty() const { return lines.isEmpty(); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ImageExtractionResult> decode(Decoder&);
};

template<class Encoder> void ImageExtractionResult::encode(Encoder& encoder) const
{
    encoder << lines;
}

template<class Decoder> Optional<ImageExtractionResult> ImageExtractionResult::decode(Decoder& decoder)
{
    Optional<Vector<ImageExtractionLineData>> lines;
    decoder >> lines;
    if (!lines)
        return WTF::nullopt;

    return {{ WTFMove(*lines) }};
}

} // namespace WebCore

#endif // ENABLE(IMAGE_EXTRACTION)
