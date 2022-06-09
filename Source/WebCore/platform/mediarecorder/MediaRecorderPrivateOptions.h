/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

struct MediaRecorderPrivateOptions {
    String mimeType;
    std::optional<unsigned> audioBitsPerSecond;
    std::optional<unsigned> videoBitsPerSecond;
    std::optional<unsigned> bitsPerSecond;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaRecorderPrivateOptions> decode(Decoder&);
};

template<class Encoder>
inline void MediaRecorderPrivateOptions::encode(Encoder& encoder) const
{
    encoder << mimeType;
    encoder << audioBitsPerSecond;
    encoder << videoBitsPerSecond;
    encoder << bitsPerSecond;
}

template<class Decoder>
inline std::optional<MediaRecorderPrivateOptions> MediaRecorderPrivateOptions::decode(Decoder& decoder)
{
    String mimeType;
    if (!decoder.decode(mimeType))
        return std::nullopt;

    std::optional<std::optional<unsigned>> audioBitsPerSecond;
    decoder >> audioBitsPerSecond;
    if (!audioBitsPerSecond)
        return std::nullopt;

    std::optional<std::optional<unsigned>> videoBitsPerSecond;
    decoder >> videoBitsPerSecond;
    if (!videoBitsPerSecond)
        return std::nullopt;

    std::optional<std::optional<unsigned>> bitsPerSecond;
    decoder >> bitsPerSecond;
    if (!bitsPerSecond)
        return std::nullopt;

    return MediaRecorderPrivateOptions { WTFMove(mimeType), *audioBitsPerSecond, *videoBitsPerSecond, *bitsPerSecond };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
