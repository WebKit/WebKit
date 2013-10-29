/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <public/MultiChannelPCMData.h>

#if ENABLE(WEB_AUDIO)
#include "AudioBus.h"
#endif

namespace Nix {

static WebCore::AudioBus* toAudioBus(void* data)
{
    return static_cast<WebCore::AudioBus*>(data);
}

MultiChannelPCMData::MultiChannelPCMData(unsigned numberOfChannels, size_t length, double sampleRate)
{
#if ENABLE(WEB_AUDIO)
    WebCore::AudioBus* bus = WebCore::AudioBus::create(numberOfChannels, length).leakRef();
    bus->setSampleRate(sampleRate);
    m_data = bus;
#else
    UNUSED_PARAM(numberOfChannels);
    UNUSED_PARAM(length);
    UNUSED_PARAM(sampleRate);
    m_data = nullptr;
#endif
}

MultiChannelPCMData::~MultiChannelPCMData()
{
    delete toAudioBus(m_data);
}

void* MultiChannelPCMData::getInternalData()
{
    // This transfer the ownership of m_data to the caller.
    void* data = m_data;
    m_data = 0;
    return data;
}

unsigned MultiChannelPCMData::numberOfChannels() const
{
#if ENABLE(WEB_AUDIO)
    if (m_data)
        return toAudioBus(m_data)->numberOfChannels();
#endif
    return 0;
}

size_t MultiChannelPCMData::length() const
{
#if ENABLE(WEB_AUDIO)
    if (m_data)
        return toAudioBus(m_data)->length();
#endif
    return 0;
}

double MultiChannelPCMData::sampleRate() const
{
#if ENABLE(WEB_AUDIO)
    if (m_data)
        return toAudioBus(m_data)->sampleRate();
#endif
    return 0;
}

float* MultiChannelPCMData::channelData(unsigned channelIndex)
{
#if ENABLE(WEB_AUDIO)
    if (m_data) {
        ASSERT(channelIndex < toAudioBus(m_data)->numberOfChannels());
        return toAudioBus(m_data)->channel(channelIndex)->mutableData();
    }
#else
    UNUSED_PARAM(channelIndex);
#endif
    return nullptr;
}

} // namespace Nix
