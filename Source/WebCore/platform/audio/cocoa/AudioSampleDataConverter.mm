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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AudioSampleDataConverter.h"

#import "AudioSampleBufferList.h"
#import "DeprecatedGlobalSettings.h"
#import "Logging.h"
#import <AudioToolbox/AudioConverter.h>
#include <wtf/FastMalloc.h>

#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

AudioSampleDataConverter::AudioSampleDataConverter()
    : m_latencyAdaptationEnabled(DeprecatedGlobalSettings::webRTCAudioLatencyAdaptationEnabled())
{
}

AudioSampleDataConverter::~AudioSampleDataConverter()
{
}

OSStatus AudioSampleDataConverter::setFormats(const CAAudioStreamDescription& inputDescription, const CAAudioStreamDescription& outputDescription)
{
    constexpr double buffer100ms = 0.100;
    constexpr double buffer60ms = 0.060;
    constexpr double buffer50ms = 0.050;
    constexpr double buffer40ms = 0.040;
    constexpr double buffer20ms = 0.020;
    m_highBufferSize = outputDescription.sampleRate() * buffer100ms;
    m_regularHighBufferSize = outputDescription.sampleRate() * buffer60ms;
    m_regularBufferSize = outputDescription.sampleRate() * buffer50ms;
    m_regularLowBufferSize = outputDescription.sampleRate() * buffer40ms;
    m_lowBufferSize = outputDescription.sampleRate() * buffer20ms;

    m_selectedConverter = nullptr;

    auto converterOutputDescription = outputDescription.streamDescription();
    constexpr double slightlyHigherPitch = 1.05;
    converterOutputDescription.mSampleRate = slightlyHigherPitch * outputDescription.streamDescription().mSampleRate;
    if (auto error = m_lowConverter.initialize(inputDescription.streamDescription(), converterOutputDescription); error != noErr)
        return error;

    constexpr double slightlyLowerPitch = 0.95;
    converterOutputDescription.mSampleRate = slightlyLowerPitch * outputDescription.streamDescription().mSampleRate;
    if (auto error = m_highConverter.initialize(inputDescription.streamDescription(), converterOutputDescription); error != noErr)
        return error;

    if (inputDescription == outputDescription)
        return noErr;

    if (auto error = m_regularConverter.initialize(inputDescription.streamDescription(), outputDescription.streamDescription()); error != noErr)
        return error;

    m_selectedConverter = m_regularConverter;
    return noErr;
}

bool AudioSampleDataConverter::updateBufferedAmount(size_t currentBufferedAmount, size_t pushedSampleSize)
{
    if (!m_latencyAdaptationEnabled)
        return !!m_selectedConverter;

    if (currentBufferedAmount) {
        DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
        if (m_selectedConverter == m_regularConverter) {
            if (currentBufferedAmount <= m_lowBufferSize) {
                m_selectedConverter = m_lowConverter;
                callOnMainThread([] {
                    RELEASE_LOG(WebRTC, "AudioSampleDataConverter::updateBufferedAmount low buffer");
                });
            } else if (currentBufferedAmount >= m_highBufferSize && currentBufferedAmount >= 4 * pushedSampleSize) {
                m_selectedConverter = m_highConverter;
                callOnMainThread([] {
                    RELEASE_LOG(WebRTC, "AudioSampleDataConverter::updateBufferedAmount high buffer");
                });
            }
        } else if (m_selectedConverter == m_highConverter) {
            if (currentBufferedAmount < m_regularLowBufferSize) {
                m_selectedConverter = m_regularConverter;
                callOnMainThread([] {
                    RELEASE_LOG(WebRTC, "AudioSampleDataConverter::updateBufferedAmount going down to regular buffer");
                });
            }
        } else if (currentBufferedAmount > m_regularHighBufferSize) {
            m_selectedConverter = m_regularConverter;
            callOnMainThread([] {
                RELEASE_LOG(WebRTC, "AudioSampleDataConverter::updateBufferedAmount going up to regular buffer");
            });
        }
    }
    return !!m_selectedConverter;
}

OSStatus AudioSampleDataConverter::convert(const AudioBufferList& inputBuffer, AudioSampleBufferList& outputBuffer, size_t sampleCount)
{
    outputBuffer.reset();
    return outputBuffer.copyFrom(inputBuffer, sampleCount, m_selectedConverter);
}

OSStatus AudioSampleDataConverter::Converter::initialize(const AudioStreamBasicDescription& inputDescription, const AudioStreamBasicDescription& outputDescription)
{
    if (m_audioConverter) {
        PAL::AudioConverterDispose(m_audioConverter);
        m_audioConverter = nullptr;
    }

    return PAL::AudioConverterNew(&inputDescription, &outputDescription, &m_audioConverter);
}

AudioSampleDataConverter::Converter::~Converter()
{
    if (m_audioConverter)
        PAL::AudioConverterDispose(m_audioConverter);
}

} // namespace WebCore
