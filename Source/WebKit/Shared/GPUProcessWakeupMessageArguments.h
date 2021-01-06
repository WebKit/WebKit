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

#include <WebCore/DisplayList.h>
#include <WebCore/RenderingResourceIdentifier.h>

namespace WebKit {

enum class GPUProcessWakeupReason : uint8_t {
    Unspecified,
    ItemCountHysteresisExceeded
};

struct GPUProcessWakeupMessageArguments {
    WebCore::DisplayList::ItemBufferIdentifier itemBufferIdentifier;
    uint64_t offset { 0 };
    WebCore::RenderingResourceIdentifier destinationImageBufferIdentifier;
    GPUProcessWakeupReason reason { GPUProcessWakeupReason::Unspecified };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<GPUProcessWakeupMessageArguments> decode(Decoder&);
};

template<class Encoder>
void GPUProcessWakeupMessageArguments::encode(Encoder& encoder) const
{
    encoder << itemBufferIdentifier;
    encoder << offset;
    encoder << destinationImageBufferIdentifier;
    encoder << reason;
}

template<class Decoder>
Optional<GPUProcessWakeupMessageArguments> GPUProcessWakeupMessageArguments::decode(Decoder& decoder)
{
    Optional<WebCore::DisplayList::ItemBufferIdentifier> itemBufferIdentifier;
    decoder >> itemBufferIdentifier;
    if (!itemBufferIdentifier)
        return WTF::nullopt;

    Optional<uint64_t> offset;
    decoder >> offset;
    if (!offset)
        return WTF::nullopt;

    Optional<WebCore::RenderingResourceIdentifier> destinationImageBufferIdentifier;
    decoder >> destinationImageBufferIdentifier;
    if (!destinationImageBufferIdentifier)
        return WTF::nullopt;

    Optional<GPUProcessWakeupReason> reason;
    decoder >> reason;
    if (!reason)
        return WTF::nullopt;

    return {{ *itemBufferIdentifier, *offset, *destinationImageBufferIdentifier, *reason }};
}

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::GPUProcessWakeupReason> {
    using values = EnumValues<
        WebKit::GPUProcessWakeupReason,
        WebKit::GPUProcessWakeupReason::Unspecified,
        WebKit::GPUProcessWakeupReason::ItemCountHysteresisExceeded
    >;
};

} // namespace WTF

#endif // ENABLE(GPU_PROCESS)
