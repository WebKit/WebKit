/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "VideoColorPrimaries.h"
#include "VideoColorSpaceInit.h"
#include "VideoMatrixCoefficients.h"
#include "VideoTransferCharacteristics.h"
#include <wtf/FastMalloc.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class VideoColorSpace : public RefCounted<VideoColorSpace> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<VideoColorSpace> create() { return adoptRef(*new VideoColorSpace()); };
    static Ref<VideoColorSpace> create(const VideoColorSpaceInit& init) { return adoptRef(*new VideoColorSpace(init)); }
    static Ref<VideoColorSpace> create(VideoColorSpaceInit&& init) { return adoptRef(*new VideoColorSpace(WTFMove(init))); }

    void setState(const VideoColorSpaceInit& state) { m_state = state; }

    const std::optional<VideoColorPrimaries>& primaries() const { return m_state.primaries; }
    void setPrimaries(std::optional<VideoColorPrimaries>&& primaries) { m_state.primaries = WTFMove(primaries); }

    const std::optional<VideoTransferCharacteristics>& transfer() const { return m_state.transfer; }
    void setTransfer(std::optional<VideoTransferCharacteristics>&& transfer) { m_state.transfer = WTFMove(transfer); }

    const std::optional<VideoMatrixCoefficients>& matrix() const { return m_state.matrix; }
    void setMatrix(std::optional<VideoMatrixCoefficients>&& matrix) { m_state.matrix = WTFMove(matrix); }

    const std::optional<bool>& fullRange() const { return m_state.fullRange; }
    void setfFullRange(std::optional<bool>&& fullRange) { m_state.fullRange = WTFMove(fullRange); }

    VideoColorSpaceInit state() const { return m_state; }

private:
    VideoColorSpace() = default;
    VideoColorSpace(const VideoColorSpaceInit& init)
        : m_state(init)
    {
    }
    VideoColorSpace(VideoColorSpaceInit&& init)
        : m_state(WTFMove(init))
    {
    }

    VideoColorSpaceInit m_state { };
};

}

#endif
