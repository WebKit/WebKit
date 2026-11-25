/*
 * Copyright (C) 2026 Sergey Rubanov. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(WEB_AUDIO)

#include <cstddef>
#include <stdint.h>
#include <wtf/Atomics.h>

namespace WebKit {

// Shared by the GPU process audio callback and the web process render thread.
// sequence is odd while consumedFrame and hostTime are being stored.
struct RemoteAudioOutputState {
    uint32_t frameCount { 0 };
    uint32_t sequence { 0 };
    uint64_t consumedFrame { 0 };
    uint64_t hostTime { 0 };

    void storePlayback(uint64_t frame, uint64_t time)
    {
        WTF::atomicExchangeAdd(&sequence, 1u);
        WTF::atomicStore(&consumedFrame, frame);
        WTF::atomicStore(&hostTime, time);
        WTF::atomicExchangeAdd(&sequence, 1u);
    }

    bool loadPlayback(uint64_t& frame, uint64_t& time)
    {
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            auto start = WTF::atomicLoad(&sequence);
            if (start & 1)
                continue;
            auto loadedFrame = WTF::atomicLoad(&consumedFrame);
            auto loadedTime = WTF::atomicLoad(&hostTime);
            if (start != WTF::atomicLoad(&sequence))
                continue;
            frame = loadedFrame;
            time = loadedTime;
            return true;
        }
        return false;
    }
};

static_assert(sizeof(RemoteAudioOutputState) == 24);
static_assert(alignof(RemoteAudioOutputState) <= alignof(std::max_align_t));

} // namespace WebKit

#endif
