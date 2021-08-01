/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ImageBufferBackendHandle.h"
#include <WebCore/FloatRect.h>
#include <WebCore/IOSurface.h>
#include <WebCore/Region.h>
#include <wtf/MachSendRight.h>
#include <wtf/MonotonicTime.h>

OBJC_CLASS CALayer;

// FIXME: Make PlatformCALayerRemote.cpp Objective-C so we can include WebLayer.h here and share the typedef.
namespace WebCore {
class NativeImage;
class ThreadSafeImageBufferFlusher;
typedef Vector<WebCore::FloatRect, 5> RepaintRectList;
}

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerBackingStore {
    WTF_MAKE_NONCOPYABLE(RemoteLayerBackingStore);
    WTF_MAKE_FAST_ALLOCATED;
public:
    RemoteLayerBackingStore(PlatformCALayerRemote*);
    ~RemoteLayerBackingStore();

    enum class Type : uint8_t {
        IOSurface,
        Bitmap
    };

    enum class IncludeDisplayList : bool { No, Yes };
    void ensureBackingStore(Type, WebCore::FloatSize, float scale, bool deepColor, bool isOpaque, IncludeDisplayList);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    bool display();

    WebCore::FloatSize size() const { return m_size; }
    float scale() const { return m_scale; }
    Type type() const { return m_type; }
    bool isOpaque() const { return m_isOpaque; }
    unsigned bytesPerPixel() const;

    PlatformCALayerRemote* layer() const { return m_layer; }

    enum class LayerContentsType { IOSurface, CAMachPort };
    void applyBackingStoreToLayer(CALayer *, LayerContentsType);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RemoteLayerBackingStore&);

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^)(WebCore::FloatRect));

    bool hasFrontBuffer() const
    {
        return !!m_frontBuffer.imageBuffer;
    }

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> takePendingFlushers();

    enum class BufferType {
        Front,
        Back,
        SecondaryBack
    };

    // Returns true if it was able to fulfill the request. This can fail when trying to mark an in-use surface as volatile.
    bool setBufferVolatility(BufferType, bool isVolatile);

    MonotonicTime lastDisplayTime() const { return m_lastDisplayTime; }

private:
    void drawInContext(WebCore::GraphicsContext&);
    void clearBackingStore();
    void swapToValidFrontBuffer();

    bool supportsPartialRepaint();

    WebCore::PixelFormat pixelFormat() const;

    PlatformCALayerRemote* m_layer;

    WebCore::FloatSize m_size;
    float m_scale;
    bool m_isOpaque;

    WebCore::Region m_dirtyRegion;

    struct Buffer {
        RefPtr<WebCore::ImageBuffer> imageBuffer;
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        RefPtr<WebCore::ImageBuffer> displayListImageBuffer;
#endif
        bool isVolatile = false;

        explicit operator bool() const
        {
            return !!imageBuffer;
        }

        void discard();
    };

    Buffer m_frontBuffer;
    Buffer m_backBuffer;
    Buffer m_secondaryBackBuffer;
    std::optional<ImageBufferBackendHandle> m_bufferHandle;
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
    std::optional<ImageBufferBackendHandle> m_displayListBufferHandle;
#endif

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> m_frontBufferFlushers;

    Type m_type;
    IncludeDisplayList m_includeDisplayList { IncludeDisplayList::No };
    bool m_deepColor { false };

    WebCore::RepaintRectList m_paintingRects;

    MonotonicTime m_lastDisplayTime;
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::RemoteLayerBackingStore::Type> {
    using values = EnumValues<
        WebKit::RemoteLayerBackingStore::Type,
        WebKit::RemoteLayerBackingStore::Type::IOSurface,
        WebKit::RemoteLayerBackingStore::Type::Bitmap
    >;
};

template<> struct EnumTraits<WebKit::RemoteLayerBackingStore::IncludeDisplayList> {
    using values = EnumValues<
        WebKit::RemoteLayerBackingStore::IncludeDisplayList,
        WebKit::RemoteLayerBackingStore::IncludeDisplayList::No,
        WebKit::RemoteLayerBackingStore::IncludeDisplayList::Yes
    >;
};

} // namespace WTF
