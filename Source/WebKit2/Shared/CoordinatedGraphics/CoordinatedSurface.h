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

#ifndef CoordinatedSurface_h
#define CoordinatedSurface_h

#if USE(COORDINATED_GRAPHICS)
#include "IntRect.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class BitmapTexture;
class GraphicsContext;
}

namespace WebKit {

class CoordinatedSurface : public ThreadSafeRefCounted<CoordinatedSurface> {
public:
    enum Flag {
        NoFlags = 0,
        SupportsAlpha = 1 << 0,
    };
    typedef unsigned Flags;

    static PassRefPtr<CoordinatedSurface> create(const WebCore::IntSize&, Flags);

    virtual ~CoordinatedSurface() { }

    bool supportsAlpha() const { return flags() & SupportsAlpha; }
    virtual WebCore::IntSize size() const = 0;

    // Create a graphics context that can be used to paint into the backing store.
    virtual PassOwnPtr<WebCore::GraphicsContext> createGraphicsContext(const WebCore::IntRect&) = 0;

#if USE(TEXTURE_MAPPER)
    virtual void copyToTexture(PassRefPtr<WebCore::BitmapTexture>, const WebCore::IntRect& target, const WebCore::IntPoint& sourceOffset) = 0;
#endif

protected:
    virtual Flags flags() const = 0;
};

} // namespace WebKit

#endif
#endif // CoordinatedSurface_h
