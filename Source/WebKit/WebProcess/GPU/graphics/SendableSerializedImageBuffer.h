/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "ImageBufferBackendHandle.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferBackend.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class SendableSerializedImageBuffer final : public WebCore::SerializedImageBuffer {
    WTF_MAKE_TZONE_ALLOCATED(SendableSerializedImageBuffer);
public:
    SendableSerializedImageBuffer(const WebCore::ImageBuffer::Parameters&, const WebCore::ImageBufferBackend::Info&, ImageBufferBackendHandle&&);
    ~SendableSerializedImageBuffer();

    size_t memoryCost() const final { return m_info.memoryCost; }

    const WebCore::ImageBuffer::Parameters& parameters() const LIFETIME_BOUND { return m_parameters; }
    const WebCore::ImageBufferBackend::Info& backendInfo() const LIFETIME_BOUND { return m_info; }
    const ImageBufferBackendHandle& handle() const LIFETIME_BOUND { return m_handle; }
    ImageBufferBackendHandle takeHandle();

    bool isSendableSerializedImageBuffer() const final { return true; }

private:
    RefPtr<WebCore::ImageBuffer> sinkIntoImageBuffer() final;

    WebCore::ImageBuffer::Parameters m_parameters;
    WebCore::ImageBufferBackend::Info m_info;
    ImageBufferBackendHandle m_handle;
};

} // namespace WebKit

namespace WebKit {
RefPtr<WebCore::ImageBuffer> materializeImageBufferFromHandle(SendableSerializedImageBuffer&);
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::SendableSerializedImageBuffer)
    static bool isType(const WebCore::SerializedImageBuffer& buffer) { return buffer.isSendableSerializedImageBuffer(); }
SPECIALIZE_TYPE_TRAITS_END()
