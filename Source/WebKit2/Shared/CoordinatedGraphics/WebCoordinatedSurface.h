/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012 Company 100, Inc.

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

#ifndef WebCoordinatedSurface_h
#define WebCoordinatedSurface_h

#if USE(COORDINATED_GRAPHICS)
#include "ShareableBitmap.h"
#include <WebCore/CoordinatedSurface.h>

namespace WebCore {
class BitmapTexture;
class GraphicsContext;
}

namespace WebKit {

class WebCoordinatedSurface : public WebCore::CoordinatedSurface {
public:
    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    public:
        Handle();

        void encode(IPC::Encoder&) const;
        static bool decode(IPC::Decoder&, Handle&);

    private:
        friend class WebCoordinatedSurface;
        mutable ShareableBitmap::Handle m_bitmapHandle;
        WebCore::IntSize m_size;
        WebCore::CoordinatedSurface::Flags m_flags;
    };

    // Create a new WebCoordinatedSurface, and allocate either a GraphicsSurface or a ShareableBitmap as backing.
    static RefPtr<WebCoordinatedSurface> create(const WebCore::IntSize&, Flags);

    // Create a shareable surface from a handle.
    static RefPtr<WebCoordinatedSurface> create(const Handle&);

    // Create a handle.
    bool createHandle(Handle&);

    virtual ~WebCoordinatedSurface();

    void paintToSurface(const WebCore::IntRect&, WebCore::CoordinatedSurface::Client&) override;

#if USE(TEXTURE_MAPPER)
    void copyToTexture(RefPtr<WebCore::BitmapTexture>, const WebCore::IntRect& target, const WebCore::IntPoint& sourceOffset) override;
#endif

private:
    WebCoordinatedSurface(const WebCore::IntSize&, Flags, RefPtr<ShareableBitmap>);

    // Create a WebCoordinatedSurface referencing an existing ShareableBitmap.
    static Ref<WebCoordinatedSurface> create(const WebCore::IntSize&, Flags, RefPtr<ShareableBitmap>);

    std::unique_ptr<WebCore::GraphicsContext> createGraphicsContext(const WebCore::IntRect&);

    RefPtr<ShareableBitmap> m_bitmap;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
#endif // WebCoordinatedSurface_h
