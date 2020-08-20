/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "config.h"
#include "StereoPanner.h"

#if ENABLE(WEB_AUDIO)

#include <wtf/MathExtras.h>

namespace WebCore {

namespace StereoPanner {

void panWithSampleAccurateValues(const AudioBus* inputBus, AudioBus* outputBus, const float* panValues, size_t framesToProcess)
{
    bool isInputSafe = inputBus && (inputBus->numberOfChannels() == 1 || inputBus->numberOfChannels() == 2) && framesToProcess <= inputBus->length();
    ASSERT(isInputSafe);
    if (!isInputSafe)
        return;
    
    unsigned numberOfInputChannels = inputBus->numberOfChannels();
    
    bool isOutputSafe = outputBus && outputBus->numberOfChannels() == 2 && framesToProcess <= outputBus->length();
    ASSERT(isOutputSafe);
    if (!isOutputSafe)
        return;
    
    const float* sourceL = inputBus->channel(0)->data();
    const float* sourceR = numberOfInputChannels > 1 ? inputBus->channel(1)->data() : sourceL;
    float* destinationL = outputBus->channelByType(AudioBus::ChannelLeft)->mutableData();
    float* destinationR = outputBus->channelByType(AudioBus::ChannelRight)->mutableData();
    
    if (!sourceL || !sourceR || !destinationL || !destinationR)
        return;
    
    double gainL;
    double gainR;
    double panRadian;
    
    int n = framesToProcess;
    
    // Handles mono source case first, then stereo source case.
    if (numberOfInputChannels == 1) {
        while (n--) {
            float inputL = *sourceL++;
            double pan = clampTo(*panValues++, -1.0, 1.0);
            // Pan from left to right [-1; 1] will be normalized as [0; 1].
            panRadian = (pan * 0.5 + 0.5) * piOverTwoDouble;
            gainL = cos(panRadian);
            gainR = sin(panRadian);
            *destinationL++ = static_cast<float>(inputL * gainL);
            *destinationR++ = static_cast<float>(inputL * gainR);
        }
    } else {
        while (n--) {
            float inputL = *sourceL++;
            float inputR = *sourceR++;
            double pan = clampTo(*panValues++, -1.0, 1.0);
            // Normalize [-1; 0] to [0; 1]. Do nothing when [0; 1].
            panRadian = (pan <= 0 ? pan + 1 : pan) * piOverTwoDouble;
            gainL = cos(panRadian);
            gainR = sin(panRadian);
            if (pan <= 0) {
                *destinationL++ = static_cast<float>(inputL + inputR * gainL);
                *destinationR++ = static_cast<float>(inputR * gainR);
            } else {
                *destinationL++ = static_cast<float>(inputL * gainL);
                *destinationR++ = static_cast<float>(inputR + inputL * gainR);
            }
        }
    }
}

void panToTargetValue(const AudioBus* inputBus, AudioBus* outputBus, float panValue, size_t framesToProcess)
{
    bool isInputSafe = inputBus && (inputBus->numberOfChannels() == 1 || inputBus->numberOfChannels() == 2) && framesToProcess <= inputBus->length();
    ASSERT(isInputSafe);
    if (!isInputSafe)
        return;
    
    unsigned numberOfInputChannels = inputBus->numberOfChannels();
    
    bool isOutputSafe = outputBus && outputBus->numberOfChannels() == 2 && framesToProcess <= outputBus->length();
    ASSERT(isOutputSafe);
    if (!isOutputSafe)
        return;
    
    const float* sourceL = inputBus->channel(0)->data();
    const float* sourceR = numberOfInputChannels > 1 ? inputBus->channel(1)->data() : sourceL;
    float* destinationL = outputBus->channelByType(AudioBus::ChannelLeft)->mutableData();
    float* destinationR = outputBus->channelByType(AudioBus::ChannelRight)->mutableData();
    
    if (!sourceL || !sourceR || !destinationL || !destinationR)
        return;
    
    float targetPan = clampTo(panValue, -1.0, 1.0);
    
    int n = framesToProcess;
    
    if (numberOfInputChannels == 1) {
        double panRadian = (targetPan * 0.5 + 0.5) * piOverTwoDouble;
        
        double gainL = cos(panRadian);
        double gainR = sin(panRadian);
        
        while (n--) {
            float inputL = *sourceL++;
            *destinationL++ = static_cast<float>(inputL * gainL);
            *destinationR++ = static_cast<float>(inputL * gainR);
        }
    } else {
        double panRadian = (targetPan <= 0 ? targetPan + 1 : targetPan) * piOverTwoDouble;
        
        double gainL = cos(panRadian);
        double gainR = sin(panRadian);
        
        while (n--) {
            float inputL = *sourceL++;
            float inputR = *sourceR++;
            if (targetPan <= 0) {
                *destinationL++ = static_cast<float>(inputL + inputR * gainL);
                *destinationR++ = static_cast<float>(inputR * gainR);
            } else {
                *destinationL++ = static_cast<float>(inputL * gainL);
                *destinationR++ = static_cast<float>(inputR + inputL * gainR);
            }
        }
    }
}

} // namespace StereoPanner

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
