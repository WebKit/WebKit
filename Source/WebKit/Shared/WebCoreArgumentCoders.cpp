/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#include "WebCoreArgumentCoders.h"

#include <WebCore/Font.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/SharedMemory.h>

namespace IPC {
using namespace WebCore;
using namespace WebKit;

#if OS(WINDOWS)
void ArgumentCoder<WebCore::FontCustomPlatformData>::encode(Encoder& encoder, const WebCore::FontCustomPlatformData& customPlatformData)
{
    std::optional<WebCore::SharedMemory::Handle> handle;
    {
        auto sharedMemoryBuffer = WebCore::SharedMemory::copyBuffer(customPlatformData.creationData.fontFaceData);
        handle = sharedMemoryBuffer->createHandle(WebCore::SharedMemory::Protection::ReadOnly);
    }
    encoder << customPlatformData.creationData.fontFaceData->size();
    encoder << WTFMove(handle);
    encoder << customPlatformData.creationData.itemInCollection;
    encoder << customPlatformData.m_renderingResourceIdentifier;
}

std::optional<Ref<FontCustomPlatformData>> ArgumentCoder<FontCustomPlatformData>::decode(Decoder& decoder)
{
    std::optional<uint64_t> bufferSize;
    decoder >> bufferSize;
    if (!bufferSize)
        return std::nullopt;

    auto handle = decoder.decode<std::optional<WebCore::SharedMemory::Handle>>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    if (!*handle)
        return std::nullopt;

    auto sharedMemoryBuffer = WebCore::SharedMemory::map(WTFMove(**handle), WebCore::SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return std::nullopt;

    if (sharedMemoryBuffer->size() < *bufferSize)
        return std::nullopt;

    auto fontFaceData = sharedMemoryBuffer->createSharedBuffer(*bufferSize);

    std::optional<String> itemInCollection;
    decoder >> itemInCollection;
    if (!itemInCollection)
        return std::nullopt;

    auto fontCustomPlatformData = FontCustomPlatformData::create(fontFaceData, *itemInCollection);
    if (!fontCustomPlatformData)
        return std::nullopt;

    std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    fontCustomPlatformData->m_renderingResourceIdentifier = *renderingResourceIdentifier;

    return fontCustomPlatformData.releaseNonNull();
}

#endif
} // namespace IPC
