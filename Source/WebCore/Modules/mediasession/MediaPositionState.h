/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SESSION)

namespace WebCore {

struct MediaPositionState {
    double duration = std::numeric_limits<double>::infinity();
    double playbackRate = 1;
    double position = 0;

    String toJSONString() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaPositionState> decode(Decoder&);

    bool operator==(const MediaPositionState& other) const { return duration == other.duration && playbackRate == other.playbackRate && position == other.position; }
};

template<class Encoder> inline void MediaPositionState::encode(Encoder& encoder) const
{
    encoder << duration << playbackRate << position;
}

template<class Decoder> inline std::optional<MediaPositionState> MediaPositionState::decode(Decoder& decoder)
{
    double duration;
    if (!decoder.decode(duration))
        return { };

    double playbackRate;
    if (!decoder.decode(playbackRate))
        return { };

    double position;
    if (!decoder.decode(position))
        return { };

    return MediaPositionState { WTFMove(duration), WTFMove(playbackRate), WTFMove(position) };
}

}

namespace WTF {

template<typename> struct LogArgument;

template<> struct LogArgument<WebCore::MediaPositionState> {
    static String toString(const WebCore::MediaPositionState& state) { return state.toJSONString(); }
};

} // namespace WTF

#endif // ENABLE(MEDIA_SESSION)
