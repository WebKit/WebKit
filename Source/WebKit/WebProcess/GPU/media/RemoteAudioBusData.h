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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "SharedMemory.h"
#include <WebCore/AudioIOCallback.h>
#include <WebCore/SharedBuffer.h>

namespace WebKit {

struct RemoteAudioBusData {
    uint64_t framesToProcess { 0 };
    WebCore::AudioIOPosition outputPosition;
    Vector<Ref<SharedMemory>> channelBuffers;

    template<typename Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << framesToProcess;
        encoder << outputPosition;
        encoder << static_cast<uint64_t>(channelBuffers.size());
        for (size_t i = 0; i < channelBuffers.size(); ++i) {
            SharedMemory::Handle handle;
            channelBuffers[i]->createHandle(handle, SharedMemory::Protection::ReadWrite);
            encoder << SharedMemory::IPCHandle { WTFMove(handle), channelBuffers[i]->size() };
        }
    }

    template<typename Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, RemoteAudioBusData& result)
    {
        uint64_t framesToProcess;
        if (!decoder.decode(framesToProcess))
            return false;
        result.framesToProcess = framesToProcess;

        WebCore::AudioIOPosition outputPosition;
        if (!decoder.decode(outputPosition))
            return false;
        result.outputPosition = outputPosition;

        uint64_t size = 0;
        if (!decoder.decode(size))
            return false;

        for (size_t i = 0; i < size; ++i) {
            SharedMemory::IPCHandle ipcHandle;
            if (!decoder.decode(ipcHandle))
                return false;
            if (auto memory = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadWrite))
                result.channelBuffers.append(memory.releaseNonNull());
            else
                return false;
        }

        return true;
    }
};

} // namespace WebKit

#endif
