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

#include "WebCoreArgumentCoders.h"
#include <WebCore/ImageBuffer.h>

namespace IPC {

class ImageDataReference {
public:
    ImageDataReference() = default;
    ImageDataReference(RefPtr<WebCore::ImageData>&& imageData)
        : m_imageData(WTFMove(imageData))
    {
    }

    RefPtr<WebCore::ImageData>& buffer() { return m_imageData; }
    const RefPtr<WebCore::ImageData>& buffer() const { return m_imageData; }

    void encode(Encoder& encoder) const
    {
        encoder << m_imageData;
    }

    static Optional<ImageDataReference> decode(Decoder& decoder)
    {
        Optional<RefPtr<WebCore::ImageData>> imageData;
        decoder >> imageData;
        if (!imageData)
            return WTF::nullopt;
        return { WTFMove(*imageData) };
    }

private:
    RefPtr<WebCore::ImageData> m_imageData;
};

}
