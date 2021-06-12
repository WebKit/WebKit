/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>

namespace WebCore {

class AudioBus;

struct AudioIOPosition {
    // Audio stream position in seconds.
    Seconds position;
    // System's monotonic time in seconds corresponding to the contained |position| value.
    MonotonicTime timestamp;

    template<typename Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << position;
        encoder << timestamp;
    }

    template<typename Decoder> static std::optional<AudioIOPosition> decode(Decoder& decoder)
    {
        std::optional<Seconds> position;
        decoder >> position;
        if (!position)
            return std::nullopt;

        std::optional<MonotonicTime> timestamp;
        decoder >> timestamp;
        if (!timestamp)
            return std::nullopt;

        return AudioIOPosition { *position, *timestamp };
    }
};

// Abstract base-class for isochronous audio I/O client.
class AudioIOCallback {
public:
    // render() is called periodically to get the next render quantum of audio into destinationBus.
    // Optional audio input is given in sourceBus (if it's not 0).
    virtual void render(AudioBus* sourceBus, AudioBus* destinationBus, size_t framesToProcess, const AudioIOPosition& outputPosition) = 0;

    virtual void isPlayingDidChange() = 0;

    virtual ~AudioIOCallback() = default;
};

} // WebCore
