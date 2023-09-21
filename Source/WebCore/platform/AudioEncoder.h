/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Igalia S.L
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

#include "PlatformRawAudioData.h"

#if ENABLE(WEB_CODECS)

#include "AudioEncoderActiveConfiguration.h"
#include "WebCodecsAudioInternalData.h"
#include <span>
#include <wtf/CompletionHandler.h>
#include <wtf/Expected.h>
#include <wtf/Vector.h>

namespace WebCore {

class AudioEncoder {
public:
    virtual ~AudioEncoder() = default;

    struct OpusConfig {
        bool isOggBitStream { false };
        uint64_t frameDuration { 20000 };
        std::optional<size_t> complexity;
        size_t packetlossperc { 0 };
        bool useinbandfec { false };
        bool usedtx { false };
    };

    struct FlacConfig {
        size_t blockSize;
        size_t compressLevel;
    };

    struct Config {
        std::optional<size_t> sampleRate;
        std::optional<size_t> numberOfChannels;
        uint64_t bitRate { 0 };
        std::optional<OpusConfig> opusConfig;
        std::optional<bool> isAacADTS;
        std::optional<FlacConfig> flacConfig;
    };
    using ActiveConfiguration = AudioEncoderActiveConfiguration;
    struct EncodedFrame {
        Vector<uint8_t> data;
        bool isKeyFrame { false };
        int64_t timestamp { 0 };
        std::optional<uint64_t> duration;
    };
    struct RawFrame {
        RefPtr<PlatformRawAudioData> frame;
        int64_t timestamp { 0 };
        std::optional<uint64_t> duration;
    };
    using CreateResult = Expected<UniqueRef<AudioEncoder>, String>;

    using PostTaskCallback = Function<void(Function<void()>&&)>;
    using DescriptionCallback = Function<void(ActiveConfiguration&&)>;
    using OutputCallback = Function<void(EncodedFrame&&)>;
    using CreateCallback = Function<void(CreateResult&&)>;

    static void create(const String&, const Config&, CreateCallback&&, DescriptionCallback&&, OutputCallback&&, PostTaskCallback&&);

    using EncodeCallback = Function<void(String&&)>;
    virtual void encode(RawFrame&&, EncodeCallback&&) = 0;

    virtual void flush(Function<void()>&&) = 0;
    virtual void reset() = 0;
    virtual void close() = 0;
};

}

#endif // ENABLE(WEB_CODECS)
