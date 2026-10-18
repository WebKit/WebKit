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

#pragma once

#include <WebCore/MediaSamplesBlock.h>
#include <WebCore/PlatformMediaError.h>
#include <wtf/NativePromise.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class TrackInfo;

class MediaSamplesDecoder final : public ThreadSafeRefCounted<MediaSamplesDecoder> {
    WTF_MAKE_TZONE_ALLOCATED(MediaSamplesDecoder);
public:
    enum class Filter : uint8_t {
        InProcess = 1 << 0,
        SoftwareAllowed = 1 << 1,
        HardwareAllowed = 1 << 2,
        UseSharedMemory = 1 << 3,
    };
    using Filters = OptionSet<Filter>;

    using CreatePromise = NativePromise<Ref<MediaSamplesDecoder>, PlatformMediaError>;
    using DecodePromise = NativePromise<Vector<MediaSamplesBlock>, PlatformMediaError>;

    static bool LEGACY_NODELETE canDecode(const TrackInfo&, Filters);
    static Ref<CreatePromise> create(const TrackInfo&, Filters);
    ~MediaSamplesDecoder();

    bool LEGACY_NODELETE canContinueWith(const TrackInfo&) const;

    Ref<DecodePromise> decode(MediaSamplesBlock&&);
    Ref<DecodePromise> drain();
    Ref<MediaPromise> flush();

private:
    MediaSamplesDecoder();
};

} // namespace WebCore
