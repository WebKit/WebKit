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
class RemoteLayerBackingStoreCollection;

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

    void setContents(WTF::MachSendRight&& surfaceHandle);
    // Returns true if the backing store changed.
    bool display();

    WebCore::FloatSize size() const { return m_size; }
    float scale() const { return m_scale; }
    WebCore::PixelFormat pixelFormat() const;
    Type type() const { return m_type; }
    bool isOpaque() const { return m_isOpaque; }
    unsigned bytesPerPixel() const;

    PlatformCALayerRemote* layer() const { return m_layer; }

    enum class LayerContentsType { IOSurface, CAMachPort };
    void applyBackingStoreToLayer(CALayer *, LayerContentsType, bool replayCGDisplayListsIntoBackingStore);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RemoteLayerBackingStore&);

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^)(WebCore::FloatRect));

    bool hasFrontBuffer() const
    {
        return m_contentsBufferHandle || !!m_frontBuffer.imageBuffer;
    }

    // Just for RemoteBackingStoreCollection.
    void applySwappedBuffers(RefPtr<WebCore::ImageBuffer>&& front, RefPtr<WebCore::ImageBuffer>&& back, RefPtr<WebCore::ImageBuffer>&& secondaryBack, bool frontBufferNeedsDisplay);
    void swapToValidFrontBuffer();

    Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> takePendingFlushers();

    enum class BufferType {
        Front,
        Back,
        SecondaryBack
    };

    void willMakeBufferVolatile(BufferType);
    void didMakeFrontBufferNonVolatile(WebCore::VolatilityState);

    RefPtr<WebCore::ImageBuffer> bufferForType(BufferType) const;

    // Returns true if it was able to fulfill the request. This can fail when trying to mark an in-use surface as volatile.
    bool setBufferVolatility(BufferType, bool isVolatile);

    MonotonicTime lastDisplayTime() const { return m_lastDisplayTime; }

    void clearBackingStore();

private:
    RemoteLayerBackingStoreCollection* backingStoreCollection() const;

    void drawInContext(WebCore::GraphicsContext&);

    struct Buffer {
        RefPtr<WebCore::ImageBuffer> imageBuffer;
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        RefPtr<WebCore::ImageBuffer> displayListImageBuffer;
#endif
        // FIXME: This flag needs to be part of ImageBuffer[Backend]. Currently it's not correctly maintained
        // in the GPU Process code path.
        bool isVolatile = false;

        explicit operator bool() const
        {
            return !!imageBuffer;
        }

        void discard();
    };

    bool setBufferVolatile(Buffer&);
    WebCore::VolatilityState setBufferNonVolatile(Buffer&);

    void swapBuffers();

    bool supportsPartialRepaint() const;

    PlatformCALayerRemote* m_layer;

    WebCore::FloatSize m_size;
    float m_scale { 1.0f };
    bool m_isOpaque { false };

    WebCore::Region m_dirtyRegion;

    // Used in the WebContent Process.
    Buffer m_frontBuffer;
    Buffer m_backBuffer;
    Buffer m_secondaryBackBuffer;

    // Used in the UI Process.
    std::optional<ImageBufferBackendHandle> m_bufferHandle;
    // FIXME: This should be removed and m_bufferHandle should be used to ref the buffer once ShareableBitmap::Handle
    // can be encoded multiple times. http://webkit.org/b/234169
    std::optional<MachSendRight> m_contentsBufferHandle;

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
