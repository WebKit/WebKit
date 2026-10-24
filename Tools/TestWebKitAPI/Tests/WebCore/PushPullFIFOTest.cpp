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

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include <WebCore/AudioBus.h>
#include <WebCore/PushPullFIFO.h>

TEST(PushPullFIFO, resetDropsBufferedAudio)
{
    constexpr size_t frames = 128;
    WebCore::PushPullFIFO fifo(1, frames);
    auto input = WebCore::AudioBus::create(1, frames);
    input->channel(0)->mutableSpan().fill(1.0f);
    fifo.push(input);

    EXPECT_EQ(fifo.framesAvailable(), frames);

    fifo.reset();

    EXPECT_EQ(fifo.framesAvailable(), 0u);

    auto output = WebCore::AudioBus::create(1, frames);
    output->channel(0)->mutableSpan().fill(0.5f);
    size_t framesToRender = fifo.pull(output, frames);

    EXPECT_EQ(framesToRender, frames);
    EXPECT_EQ(fifo.framesAvailable(), 0u);
    for (float sample : output->channel(0)->span())
        EXPECT_FLOAT_EQ(sample, 0.0f);
}

#endif
