/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
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

#include "EqualPowerPanner.h"

#include "AudioBus.h"
#include "AudioUtilities.h"
#include "VectorMath.h"
#include <algorithm>
#include <wtf/MathExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

EqualPowerPanner::EqualPowerPanner()
    : Panner(PanningModelType::Equalpower)
{
}

void EqualPowerPanner::calculateDesiredGain(double& desiredGainL, double& desiredGainR, double azimuth, unsigned numberOfChannels)
{
    // Clamp azimuth to allowed range of -180 -> +180.
    azimuth = std::clamp(azimuth, -180.0, 180.0);

    // Alias the azimuth ranges behind us to in front of us:
    // -90 -> -180 to -90 -> 0 and 90 -> 180 to 90 -> 0
    if (azimuth < -90)
        azimuth = -180 - azimuth;
    else if (azimuth > 90)
        azimuth = 180 - azimuth;

    double desiredPanPosition;

    if (numberOfChannels == 1) { // For mono source case.
        // Pan smoothly from left to right with azimuth going from -90 -> +90
        // degrees.
        desiredPanPosition = (azimuth + 90) / 180;
    } else { // For stereo source case.
        if (azimuth <= 0) { // from -90 -> 0
            // sourceL -> destL and "equal-power pan" sourceR as in mono case
            // by transforming the "azimuth" value from -90 -> 0 degrees into the
            // range -90 -> +90.
            desiredPanPosition = (azimuth + 90) / 90;
        } else { // from 0 -> +90
            // sourceR -> destR and "equal-power pan" sourceL as in mono case
            // by transforming the "azimuth" value from 0 -> +90 degrees into the
            // range -90 -> +90.
            desiredPanPosition = azimuth / 90;
        }
    }

    desiredGainL = std::cos(piOverTwoDouble * desiredPanPosition);
    desiredGainR = std::sin(piOverTwoDouble * desiredPanPosition);
}

void EqualPowerPanner::pan(double azimuth, double /*elevation*/, const AudioBus* inputBus, AudioBus* outputBus, size_t framesToProcess)
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
    
    // Clamp azimuth to allowed range of -180 -> +180.
    azimuth = std::max(-180.0, azimuth);
    azimuth = std::min(180.0, azimuth);
    
    // Alias the azimuth ranges behind us to in front of us:
    // -90 -> -180 to -90 -> 0 and 90 -> 180 to 90 -> 0
    if (azimuth < -90)
        azimuth = -180 - azimuth;
    else if (azimuth > 90)
        azimuth = 180 - azimuth;

    double desiredPanPosition;

    if (numberOfInputChannels == 1) { // For mono source case.
        // Pan smoothly from left to right with azimuth going from -90 -> +90 degrees.
        desiredPanPosition = (azimuth + 90) / 180;
    } else { // For stereo source case.
        if (azimuth <= 0) { // from -90 -> 0
            // sourceL -> destL and "equal-power pan" sourceR as in mono case
            // by transforming the "azimuth" value from -90 -> 0 degrees into the range -90 -> +90.
            desiredPanPosition = (azimuth + 90) / 90;
        } else { // from 0 -> +90
            // sourceR -> destR and "equal-power pan" sourceL as in mono case
            // by transforming the "azimuth" value from 0 -> +90 degrees into the range -90 -> +90.
            desiredPanPosition = azimuth / 90;
        }
    }

    double desiredGainL = std::cos(piOverTwoDouble * desiredPanPosition);
    double desiredGainR = std::sin(piOverTwoDouble * desiredPanPosition);
    if (numberOfInputChannels == 1) { // For mono source case.
        VectorMath::multiplyByScalar(sourceL, desiredGainL, destinationL, framesToProcess);
        VectorMath::multiplyByScalar(sourceL, desiredGainR, destinationR, framesToProcess);
    } else { // For stereo source case.
        if (azimuth <= 0) { // from -90 -> 0
            VectorMath::multiplyByScalarThenAddToVector(sourceR, desiredGainL, sourceL, destinationL, framesToProcess);
            VectorMath::multiplyByScalar(sourceR, desiredGainR, destinationR, framesToProcess);
        } else { // from 0 -> +90
            VectorMath::multiplyByScalar(sourceL, desiredGainL, destinationL, framesToProcess);
            VectorMath::multiplyByScalarThenAddToVector(sourceL, desiredGainR, sourceR, destinationR, framesToProcess);
        }
    }
}

void EqualPowerPanner::panWithSampleAccurateValues(double* azimuth, double*, const AudioBus* inputBus, AudioBus* outputBus, size_t framesToProcess)
{
    ASSERT(inputBus);
    ASSERT(framesToProcess <= inputBus->length());
    ASSERT(inputBus->numberOfChannels() >= 1u);
    ASSERT(inputBus->numberOfChannels() <= 2u);

    unsigned numberOfInputChannels = inputBus->numberOfChannels();

    ASSERT(outputBus);
    ASSERT(outputBus->numberOfChannels() == 2u);
    ASSERT(framesToProcess <= outputBus->length());

    const float* sourceL = inputBus->channel(0)->data();
    const float* sourceR = numberOfInputChannels > 1 ? inputBus->channel(1)->data() : sourceL;
    float* destinationL = outputBus->channelByType(AudioBus::ChannelLeft)->mutableData();
    float* destinationR = outputBus->channelByType(AudioBus::ChannelRight)->mutableData();

    ASSERT(sourceL);
    ASSERT(sourceR);
    ASSERT(destinationL);
    ASSERT(destinationR);

    int n = framesToProcess;

    if (numberOfInputChannels == 1) { // For mono source case.
        for (int k = 0; k < n; ++k) {
            double desiredGainL;
            double desiredGainR;
            float inputL = *sourceL++;

            calculateDesiredGain(desiredGainL, desiredGainR, azimuth[k], numberOfInputChannels);
            *destinationL++ = static_cast<float>(inputL * desiredGainL);
            *destinationR++ = static_cast<float>(inputL * desiredGainR);
        }
    } else { // For stereo source case.
        for (int k = 0; k < n; ++k) {
            double desiredGainL;
            double desiredGainR;

            calculateDesiredGain(desiredGainL, desiredGainR, azimuth[k], numberOfInputChannels);
            if (azimuth[k] <= 0) { // from -90 -> 0
                float inputL = *sourceL++;
                float inputR = *sourceR++;
                *destinationL++ = static_cast<float>(inputL + inputR * desiredGainL);
                *destinationR++ = static_cast<float>(inputR * desiredGainR);
            } else { // from 0 -> +90
                float inputL = *sourceL++;
                float inputR = *sourceR++;
                *destinationL++ = static_cast<float>(inputL * desiredGainL);
                *destinationR++ = static_cast<float>(inputR + inputL * desiredGainR);
            }
        }
    }
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEB_AUDIO)
