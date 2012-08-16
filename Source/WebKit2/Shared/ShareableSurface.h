/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef ShareableSurface_h
#define ShareableSurface_h

#include "ShareableBitmap.h"

#include <wtf/ThreadSafeRefCounted.h>

#if USE(GRAPHICS_SURFACE)
#include "GraphicsSurface.h"
#endif

namespace WebCore {
class BitmapTexture;
class Image;
class GraphicsContext;
}

namespace WebKit {

class ShareableSurface : public ThreadSafeRefCounted<ShareableSurface> {
public:
    enum Hint {
        SupportsGraphicsSurface = 0x01
    };

    typedef unsigned Hints;

    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();

        bool isNull() const;

        void encode(CoreIPC::ArgumentEncoder*) const;
        static bool decode(CoreIPC::ArgumentDecoder*, Handle&);

#if USE(GRAPHICS_SURFACE)
        uint32_t graphicsSurfaceToken() const { return m_graphicsSurfaceToken; }
#endif

    private:
        friend class ShareableSurface;
        mutable ShareableBitmap::Handle m_bitmapHandle;
#if USE(GRAPHICS_SURFACE)
        uint64_t m_graphicsSurfaceToken;
#endif
        WebCore::IntSize m_size;
        ShareableBitmap::Flags m_flags;
    };

    // Create a new ShareableSurface, and allocate either a GraphicsSurface or a ShareableBitmap as backing.
    static PassRefPtr<ShareableSurface> create(const WebCore::IntSize&, ShareableBitmap::Flags, Hints);

    // Create a shareable surface from a handle.
    static PassRefPtr<ShareableSurface> create(const Handle&);

    ShareableBitmap::Flags flags() const { return m_flags; }

    // Create a handle.
    bool createHandle(Handle&);

    ~ShareableSurface();

    const WebCore::IntSize& size() const { return m_size; }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }

    // Create a graphics context that can be used to paint into the backing store.
    PassOwnPtr<WebCore::GraphicsContext> createGraphicsContext(const WebCore::IntRect&);

#if USE(TEXTURE_MAPPER)
    void copyToTexture(PassRefPtr<WebCore::BitmapTexture>, const WebCore::IntRect& target, const WebCore::IntPoint& sourceOffset);
#endif

private:
    ShareableSurface(const WebCore::IntSize&, ShareableBitmap::Flags, PassRefPtr<ShareableBitmap>);

    // Create a ShareableSurface referencing an existing ShareableBitmap.
    static PassRefPtr<ShareableSurface> create(const WebCore::IntSize&, ShareableBitmap::Flags, PassRefPtr<ShareableBitmap>);

#if USE(GRAPHICS_SURFACE)
    ShareableSurface(const WebCore::IntSize&, ShareableBitmap::Flags, PassRefPtr<WebCore::GraphicsSurface>);
    // Create a shareable bitmap backed by a graphics surface.
    static PassRefPtr<ShareableSurface> createWithSurface(const WebCore::IntSize&, ShareableBitmap::Flags);
    // Create a ShareableSurface referencing an existing GraphicsSurface.
    static PassRefPtr<ShareableSurface> create(const WebCore::IntSize&, ShareableBitmap::Flags, PassRefPtr<WebCore::GraphicsSurface>);

    bool isBackedByGraphicsSurface() const { return !!m_graphicsSurface; }
#endif

    WebCore::IntSize m_size;
    ShareableBitmap::Flags m_flags;
    RefPtr<ShareableBitmap> m_bitmap;

#if USE(GRAPHICS_SURFACE)
    RefPtr<WebCore::GraphicsSurface> m_graphicsSurface;
#endif
};

} // namespace WebKit

#endif // ShareableSurface_h
