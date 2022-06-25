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

#include "BufferIdentifierSet.h"
#include "ImageBufferBackendHandle.h"
#include "SwapBuffersDisplayRequirement.h"
#include <WebCore/RenderingResourceIdentifier.h>

namespace WebKit {

struct PrepareBackingStoreBuffersInputData {
    BufferIdentifierSet bufferSet;
    bool supportsPartialRepaint { true };
    bool hasEmptyDirtyRegion { true };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PrepareBackingStoreBuffersInputData> decode(Decoder&);
};

template<class Encoder>
void PrepareBackingStoreBuffersInputData::encode(Encoder& encoder) const
{
    encoder << bufferSet;
    encoder << supportsPartialRepaint;
    encoder << hasEmptyDirtyRegion;
}

template<class Decoder>
std::optional<PrepareBackingStoreBuffersInputData> PrepareBackingStoreBuffersInputData::decode(Decoder& decoder)
{
    std::optional<BufferIdentifierSet> bufferSet;
    decoder >> bufferSet;
    if (!bufferSet)
        return std::nullopt;

    std::optional<bool> supportsPartialRepaint;
    decoder >> supportsPartialRepaint;
    if (!supportsPartialRepaint)
        return std::nullopt;

    std::optional<bool> hasEmptyDirtyRegion;
    decoder >> hasEmptyDirtyRegion;
    if (!hasEmptyDirtyRegion)
        return std::nullopt;

    return PrepareBackingStoreBuffersInputData { WTFMove(*bufferSet), *supportsPartialRepaint, *hasEmptyDirtyRegion };
}


struct PrepareBackingStoreBuffersOutputData {
    BufferIdentifierSet bufferSet;
    std::optional<ImageBufferBackendHandle> frontBufferHandle;
    SwapBuffersDisplayRequirement displayRequirement { SwapBuffersDisplayRequirement::NeedsNoDisplay };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PrepareBackingStoreBuffersOutputData> decode(Decoder&);
};

template<class Encoder>
void PrepareBackingStoreBuffersOutputData::encode(Encoder& encoder) const
{
    encoder << bufferSet;
    encoder << frontBufferHandle;
    encoder << displayRequirement;
}

template<class Decoder>
std::optional<PrepareBackingStoreBuffersOutputData> PrepareBackingStoreBuffersOutputData::decode(Decoder& decoder)
{
    std::optional<BufferIdentifierSet> bufferSet;
    decoder >> bufferSet;
    if (!bufferSet)
        return std::nullopt;

    std::optional<std::optional<ImageBufferBackendHandle>> bufferHandle;
    decoder >> bufferHandle;
    if (!bufferHandle)
        return std::nullopt;

    std::optional<SwapBuffersDisplayRequirement> displayRequirement;
    decoder >> displayRequirement;
    if (!displayRequirement)
        return std::nullopt;

    return PrepareBackingStoreBuffersOutputData { WTFMove(*bufferSet), WTFMove(*bufferHandle), *displayRequirement };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
