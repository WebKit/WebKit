/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/EnumTraits.h>

#if ENABLE(MEDIA_STREAM)

#include "MediaConstraints.h"

namespace WebCore {

struct MediaStreamRequest {
    enum class Type { UserMedia, DisplayMedia };
    Type type { Type::UserMedia };
    MediaConstraints audioConstraints;
    MediaConstraints videoConstraints;
    bool isUserGesturePriviledged { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder.encodeEnum(type);
        encoder << audioConstraints;
        encoder << videoConstraints;
        encoder << isUserGesturePriviledged;
    }

    template <class Decoder> static Optional<MediaStreamRequest> decode(Decoder& decoder)
    {
        MediaStreamRequest request;
        if (decoder.decodeEnum(request.type) && decoder.decode(request.audioConstraints) && decoder.decode(request.videoConstraints) && decoder.decode(request.isUserGesturePriviledged))
            return request;

        return WTF::nullopt;
    }
};

} // namespace WebCore

#else

namespace WebCore {

struct MediaStreamRequest {
    enum class Type { UserMedia, DisplayMedia };
    Type type;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

namespace WTF {

template<> struct EnumTraits<WebCore::MediaStreamRequest::Type> {
    using values = EnumValues<
        WebCore::MediaStreamRequest::Type,
        WebCore::MediaStreamRequest::Type::UserMedia,
        WebCore::MediaStreamRequest::Type::DisplayMedia
    >;
};

} // namespace WTF
