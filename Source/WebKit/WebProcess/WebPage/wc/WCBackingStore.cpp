/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "WCBackingStore.h"

#if USE(GRAPHICS_LAYER_WC)

#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/ShareableBitmap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WCBackingStore);

WCBackingStore::WCBackingStore(std::optional<ImageBufferBackendHandle>&& handle)
{
    if (auto* imageHandle = handle ? std::get_if<WebCore::ShareableBitmap::Handle>(&*handle) : nullptr)
        m_bitmap = WebCore::ShareableBitmap::create(WTFMove(*imageHandle));
}

std::optional<ImageBufferBackendHandle> WCBackingStore::handle() const
{
    if (!m_imageBuffer)
        return std::nullopt;

    auto* sharing = m_imageBuffer->toBackendSharing();
    if (!is<ImageBufferBackendHandleSharing>(sharing))
        return std::nullopt;

    return dynamicDowncast<ImageBufferBackendHandleSharing>(*sharing)->createBackendHandle();
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
