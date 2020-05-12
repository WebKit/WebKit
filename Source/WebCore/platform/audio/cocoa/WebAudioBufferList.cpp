/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebAudioBufferList.h"

#include "CAAudioStreamDescription.h"
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {
using namespace PAL;

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format)
{
    // AudioBufferList is a variable-length struct, so create on the heap with a generic new() operator
    // with a custom size, and initialize the struct manually.
    uint32_t bufferCount = format.numberOfChannelStreams();
    uint32_t channelCount = format.numberOfInterleavedChannels();

    uint64_t bufferListSize = offsetof(AudioBufferList, mBuffers) + (sizeof(AudioBuffer) * std::max(1U, bufferCount));
    ASSERT(bufferListSize <= SIZE_MAX);

    m_listBufferSize = static_cast<size_t>(bufferListSize);
    m_canonicalList = std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_listBufferSize)));
    memset(m_canonicalList.get(), 0, m_listBufferSize);
    m_canonicalList->mNumberBuffers = bufferCount;
    for (uint32_t buffer = 0; buffer < bufferCount; ++buffer)
        m_canonicalList->mBuffers[buffer].mNumberChannels = channelCount;

    reset();
}

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format, uint32_t sampleCount)
    : WebAudioBufferList(format)
{
    if (!sampleCount)
        return;

    uint32_t bufferCount = format.numberOfChannelStreams();
    uint32_t channelCount = format.numberOfInterleavedChannels();

    size_t bytesPerBuffer = sampleCount * channelCount * format.bytesPerFrame();
    m_flatBuffer.reserveInitialCapacity(bufferCount * bytesPerBuffer);
    auto data = m_flatBuffer.data();

    for (uint32_t buffer = 0; buffer < m_canonicalList->mNumberBuffers; ++buffer) {
        m_canonicalList->mBuffers[buffer].mData = data;
        m_canonicalList->mBuffers[buffer].mDataByteSize = bytesPerBuffer;
        data += bytesPerBuffer;
    }

    reset();
}

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format, CMSampleBufferRef sampleBuffer)
    : WebAudioBufferList(format)
{

    if (!sampleBuffer)
        return;

    CMBlockBufferRef buffer = nullptr;
    if (noErr == CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, nullptr, m_canonicalList.get(), m_listBufferSize, kCFAllocatorSystemDefault, kCFAllocatorSystemDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &buffer))
        m_blockBuffer = adoptCF(buffer);

    reset();
}

void WebAudioBufferList::reset()
{
    if (!m_list)
        m_list = std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_listBufferSize)));
    memcpy(m_list.get(), m_canonicalList.get(), m_listBufferSize);
}

WTF::IteratorRange<AudioBuffer*> WebAudioBufferList::buffers() const
{
    return WTF::makeIteratorRange(&m_list->mBuffers[0], &m_list->mBuffers[m_list->mNumberBuffers]);
}

uint32_t WebAudioBufferList::bufferCount() const
{
    return m_list->mNumberBuffers;
}

AudioBuffer* WebAudioBufferList::buffer(uint32_t index) const
{
    ASSERT(index < m_list->mNumberBuffers);
    if (index < m_list->mNumberBuffers)
        return &m_list->mBuffers[index];
    return nullptr;
}

}
