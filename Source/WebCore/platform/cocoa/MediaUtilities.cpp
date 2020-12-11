/*
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
#include "MediaUtilities.h"

#include "AudioStreamDescription.h"
#include "WebAudioBufferList.h"
#include <wtf/SoftLinking.h>
#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

using namespace PAL;

RetainPtr<CMFormatDescriptionRef> createAudioFormatDescription(const AudioStreamDescription& description, size_t magicCookieSize, const void* magicCookie)
{
    auto basicDescription = WTF::get<const AudioStreamBasicDescription*>(description.platformDescription().description);
    CMFormatDescriptionRef format = nullptr;
    auto error = CMAudioFormatDescriptionCreate(kCFAllocatorDefault, basicDescription, 0, nullptr, magicCookieSize, magicCookie, nullptr, &format);
    if (error) {
        LOG_ERROR("createAudioFormatDescription failed with %d", error);
        return nullptr;
    }
    return adoptCF(format);
}

RetainPtr<CMSampleBufferRef> createAudioSampleBuffer(const PlatformAudioData& data, const AudioStreamDescription& description, CMTime time, size_t sampleCount)
{
    // FIXME: check if we can reuse the format for multiple sample buffers.
    auto format = createAudioFormatDescription(description);
    if (!format)
        return nullptr;

    CMSampleBufferRef sampleBuffer = nullptr;
    auto error = CMAudioSampleBufferCreateWithPacketDescriptions(kCFAllocatorDefault, nullptr, false, nullptr, nullptr, format.get(), sampleCount, time, nullptr, &sampleBuffer);
    if (error) {
        LOG_ERROR("createAudioSampleBuffer with packet descriptions failed - %d", error);
        return nullptr;
    }
    auto buffer = adoptCF(sampleBuffer);

    error = CMSampleBufferSetDataBufferFromAudioBufferList(buffer.get(), kCFAllocatorDefault, kCFAllocatorDefault, 0, downcast<WebAudioBufferList>(data).list());
    if (error) {
        LOG_ERROR("createAudioSampleBuffer from audio buffer list failed - %d", error);
        return nullptr;
    }
    return buffer;
}

} // namespace WebCore
