/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#if USE(GRAPHICS_LAYER_WC)

#include "ImageBufferBackendHandle.h"
#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/ImageBuffer.h>

namespace WebKit {

class WCBackingStore {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WCBackingStore() { }
    WebCore::ImageBuffer* imageBuffer() { return m_imageBuffer.get(); }
    void setImageBuffer(RefPtr<WebCore::ImageBuffer>&& image) { m_imageBuffer = WTFMove(image); }
    ShareableBitmap* bitmap() const { return m_bitmap.get(); }

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        bool hasImageBuffer = m_imageBuffer;
        encoder << hasImageBuffer;
        if (hasImageBuffer) {
            ImageBufferBackendHandle handle;
            if (auto* backend = m_imageBuffer->ensureBackendCreated()) {
                auto* sharing = backend->toBackendSharing();
                if (is<ImageBufferBackendHandleSharing>(sharing))
                    handle = downcast<ImageBufferBackendHandleSharing>(*sharing).createBackendHandle();
            }
            encoder << handle;
        }
    }

    template <class Decoder>
    static WARN_UNUSED_RETURN bool decode(Decoder& decoder, WCBackingStore& result)
    {
        bool hasImageBuffer;
        if (!decoder.decode(hasImageBuffer))
            return false;
        if (hasImageBuffer) {
            ImageBufferBackendHandle handle;
            if (!decoder.decode(handle))
                return false;
            result.m_bitmap = ShareableBitmap::create(std::get<ShareableBitmapHandle>(handle));
        }
        return true;
    }

private:
    RefPtr<WebCore::ImageBuffer> m_imageBuffer;
    RefPtr<ShareableBitmap> m_bitmap;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
