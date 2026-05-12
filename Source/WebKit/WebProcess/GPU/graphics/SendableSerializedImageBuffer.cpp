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

#include "config.h"
#include "SendableSerializedImageBuffer.h"

#include "ImageBufferShareableBitmapBackend.h"
#include <WebCore/ShareableBitmap.h>
#include <wtf/TZoneMallocInlines.h>

#if HAVE(IOSURFACE)
#include "ImageBufferShareableMappedIOSurfaceBackend.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SendableSerializedImageBuffer);

SendableSerializedImageBuffer::SendableSerializedImageBuffer(const WebCore::ImageBuffer::Parameters& parameters, const WebCore::ImageBufferBackend::Info& info, ImageBufferBackendHandle&& handle)
    : m_parameters(parameters)
    , m_info(info)
    , m_handle(WTF::move(handle))
{
}

SendableSerializedImageBuffer::~SendableSerializedImageBuffer() = default;

ImageBufferBackendHandle SendableSerializedImageBuffer::takeHandle()
{
    return WTF::move(m_handle);
}

RefPtr<WebCore::ImageBuffer> SendableSerializedImageBuffer::sinkIntoImageBuffer()
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RefPtr<WebCore::ImageBuffer> materializeImageBufferFromHandle(SendableSerializedImageBuffer& sendable)
{
    auto handle = sendable.takeHandle();
    auto& parameters = sendable.parameters();
    auto backendParameters = WebCore::ImageBuffer::backendParameters(parameters);

    std::unique_ptr<WebCore::ImageBufferBackend> backend;

    if (std::holds_alternative<WebCore::ShareableBitmap::Handle>(handle)) {
        auto shareableHandle = std::get<WebCore::ShareableBitmap::Handle>(WTF::move(handle));
        shareableHandle.takeOwnershipOfMemory(WebCore::MemoryLedger::Graphics);
        backend = ImageBufferShareableBitmapBackend::create(backendParameters, WTF::move(shareableHandle));
    }
#if HAVE(IOSURFACE)
    else if (std::holds_alternative<MachSendRight>(handle))
        backend = ImageBufferShareableMappedIOSurfaceBackend::create(backendParameters, WTF::move(handle));
#endif

    if (!backend)
        return nullptr;
    return WebCore::ImageBuffer::create(parameters, sendable.backendInfo(), { }, WTF::move(backend), WebCore::RenderingResourceIdentifier::generate());
}

} // namespace WebKit
