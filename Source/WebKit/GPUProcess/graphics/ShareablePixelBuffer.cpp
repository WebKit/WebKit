/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ShareablePixelBuffer.h"

#include <WebCore/SharedMemory.h>

namespace WebKit {
using namespace WebCore;

RefPtr<ShareablePixelBuffer> ShareablePixelBuffer::tryCreate(const PixelBufferFormat& format, const IntSize& size)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto bufferSize = computeBufferSize(format.pixelFormat, size);
    if (bufferSize.hasOverflowed())
        return nullptr;
    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(bufferSize);
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareablePixelBuffer(format, size, sharedMemory.releaseNonNull()));
}

ShareablePixelBuffer::ShareablePixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<SharedMemory>&& data)
    : PixelBuffer(format, size, static_cast<uint8_t*>(data->data()), data->size())
    , m_data(WTFMove(data))
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION((m_size.area() * 4) <= sizeInBytes());
}

RefPtr<PixelBuffer> ShareablePixelBuffer::createScratchPixelBuffer(const IntSize& size) const
{
    return ShareablePixelBuffer::tryCreate(m_format, size);
}

} // namespace WebKit
