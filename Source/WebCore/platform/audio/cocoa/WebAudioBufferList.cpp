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
#include "Logging.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/IndexedRange.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebAudioBufferList);

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format)
    : m_bytesPerFrame(format.bytesPerFrame())
    , m_channelCount(format.numberOfInterleavedChannels())
{
    // AudioBufferList is a variable-length struct, so create on the heap with a generic new() operator
    // with a custom size, and initialize the struct manually.
    uint32_t bufferCount = format.numberOfChannelStreams();

    CheckedSize bufferListSize = offsetof(AudioBufferList, mBuffers);
    bufferListSize += CheckedSize { sizeof(AudioBuffer) } * std::max(1U, bufferCount);
    m_listBufferSize = bufferListSize;
    lazyInitialize(m_canonicalList, std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_listBufferSize))));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memset(m_canonicalList.get(), 0, m_listBufferSize);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    m_canonicalList->mNumberBuffers = bufferCount;
    auto canonicalListBuffers = span(*m_canonicalList);
    for (uint32_t buffer = 0; buffer < bufferCount; ++buffer)
        canonicalListBuffers[buffer].mNumberChannels = m_channelCount;

    reset();
}

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format, size_t sampleCount)
    : WebAudioBufferList(format)
{
    setSampleCount(sampleCount);
}

WebAudioBufferList::~WebAudioBufferList() = default;

static inline std::optional<std::pair<size_t, size_t>> computeBufferSizes(uint32_t numberOfInterleavedChannels, uint32_t bytesPerFrame, uint32_t numberOfChannelStreams, size_t sampleCount)
{
    size_t totalSampleCount;
    bool result = WTF::safeMultiply(sampleCount, numberOfInterleavedChannels, totalSampleCount);
    if (!result)
        return { };

    size_t bytesPerBuffer;
    result = WTF::safeMultiply(bytesPerFrame, totalSampleCount, bytesPerBuffer);
    if (!result)
        return { };

    size_t flatBufferSize;
    result = WTF::safeMultiply(numberOfChannelStreams, bytesPerBuffer, flatBufferSize);
    if (!result)
        return { };

    return std::make_pair(bytesPerBuffer, flatBufferSize);
}

bool WebAudioBufferList::isSupportedDescription(const CAAudioStreamDescription& format, size_t sampleCount)
{
    return !!computeBufferSizes(format.numberOfInterleavedChannels(), format.bytesPerFrame(), format.numberOfChannelStreams(), sampleCount);
}

void WebAudioBufferList::setSampleCount(size_t sampleCount)
{
    if (!sampleCount || m_sampleCount == sampleCount)
        return;

    auto bufferSizes = computeBufferSizes(m_channelCount, m_bytesPerFrame, m_canonicalList->mNumberBuffers, sampleCount);
    ASSERT(bufferSizes);
    if (!bufferSizes)
        return;

    m_sampleCount = sampleCount;

    m_flatBuffer.resize(bufferSizes->second);
    initializeList(m_flatBuffer.mutableSpan().first(bufferSizes->second), bufferSizes->first);
}

std::optional<std::pair<UniqueRef<WebAudioBufferList>, RetainPtr<CMBlockBufferRef>>> WebAudioBufferList::createWebAudioBufferListWithBlockBuffer(const CAAudioStreamDescription& description, size_t sampleCount)
{
    ASSERT(sampleCount);

    if (!sampleCount)
        return { };

    auto instance = makeUniqueRef<WebAudioBufferList>(description);
    if (RetainPtr block = instance->setSampleCountWithBlockBuffer(sampleCount))
        return std::make_pair(WTFMove(instance), WTFMove(block));

    return { };
}

RetainPtr<CMBlockBufferRef> WebAudioBufferList::setSampleCountWithBlockBuffer(size_t sampleCount)
{
    auto bufferSizes = computeBufferSizes(m_channelCount, m_bytesPerFrame, m_canonicalList->mNumberBuffers, sampleCount);
    ASSERT(bufferSizes);
    if (!bufferSizes) {
        RELEASE_LOG_ERROR(Media, "WebAudioBufferList::setSampleCountWithBlockBuffer overflow");
        return { };
    }

    CMBlockBufferRef blockBuffer = nullptr;
    if (auto error = PAL::CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, nullptr, bufferSizes->second, kCFAllocatorDefault, nullptr, 0, bufferSizes->second, kCMBlockBufferAssureMemoryNowFlag, &blockBuffer)) {
        RELEASE_LOG_ERROR(Media, "WebAudioBufferList::setSampleCountWithBlockBuffer CMBlockBufferCreateWithMemoryBlock failed with: %d", static_cast<int>(error));
        return { };
    }
    RetainPtr block = adoptCF(blockBuffer);

    auto data = PAL::CMBlockBufferGetDataSpan(blockBuffer);
    if (!data.data()) {
        RELEASE_LOG_ERROR(Media, "WebAudioBufferList::setSampleCountWithBlockBuffer CMBlockBufferGetDataSpan failed.");
        return { };
    }
    m_blockBuffer = WTFMove(block);

    initializeList(data.first(bufferSizes->second), bufferSizes->first);

    return m_blockBuffer;
}

void WebAudioBufferList::initializeList(std::span<uint8_t> buffer, size_t channelLength)
{
    zeroSpan(buffer);

    for (auto [index, canonicalListBuffer] : indexedRange(span(*m_canonicalList))) {
        canonicalListBuffer.mData = buffer.subspan(channelLength * index).data();
        canonicalListBuffer.mDataByteSize = channelLength;
    }

    reset();
}

WebAudioBufferList::WebAudioBufferList(const CAAudioStreamDescription& format, CMSampleBufferRef sampleBuffer)
    : WebAudioBufferList(format)
{
    if (!sampleBuffer)
        return;

    CMBlockBufferRef buffer = nullptr;
    if (noErr == PAL::CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, nullptr, m_canonicalList.get(), m_listBufferSize, kCFAllocatorSystemDefault, kCFAllocatorSystemDefault, kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &buffer))
        m_blockBuffer = adoptCF(buffer);

    reset();
}

void WebAudioBufferList::reset()
{
    if (!m_list)
        lazyInitialize(m_list, std::unique_ptr<AudioBufferList>(static_cast<AudioBufferList*>(::operator new (m_listBufferSize))));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    memcpy(m_list.get(), m_canonicalList.get(), m_listBufferSize);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

IteratorRange<AudioBuffer*> WebAudioBufferList::buffers() const
{
    auto buffers = span(*m_list);
    return WTF::makeIteratorRange(std::to_address(buffers.begin()), std::to_address(buffers.end()));
}

uint32_t WebAudioBufferList::bufferCount() const
{
    return m_list->mNumberBuffers;
}

AudioBuffer* WebAudioBufferList::buffer(uint32_t index) const
{
    auto buffers = span(*m_list);
    ASSERT(index < buffers.size());
    if (index < buffers.size())
        return &buffers[index];
    return nullptr;
}

void WebAudioBufferList::zeroFlatBuffer()
{
    m_flatBuffer.fill(0);
}

}
