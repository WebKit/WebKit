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

#include <WebCore/DisplayList.h>

namespace WebKit {

class SharedDisplayListHandle {
public:
    using ItemBufferProvider = Function<uint8_t*(WebCore::DisplayList::ItemBufferIdentifier)>;

    SharedDisplayListHandle(const WebCore::DisplayList::DisplayList&);
    std::unique_ptr<WebCore::DisplayList::DisplayList> createDisplayList(ItemBufferProvider&&) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<SharedDisplayListHandle> decode(Decoder&);

private:
    SharedDisplayListHandle(Vector<std::pair<WebCore::DisplayList::ItemBufferIdentifier, size_t>>&& buffers)
        : m_buffers(WTFMove(buffers))
    {
    }

    Vector<std::pair<WebCore::DisplayList::ItemBufferIdentifier, size_t>> m_buffers;
};

template<class Encoder> void SharedDisplayListHandle::encode(Encoder& encoder) const
{
    encoder << m_buffers;
}

template<class Decoder> Optional<SharedDisplayListHandle> SharedDisplayListHandle::decode(Decoder& decoder)
{
    Optional<Vector<std::pair<WebCore::DisplayList::ItemBufferIdentifier, size_t>>> buffers;
    decoder >> buffers;
    if (!buffers)
        return WTF::nullopt;

    return {{ WTFMove(*buffers) }};
}

} // namespace WebKit
