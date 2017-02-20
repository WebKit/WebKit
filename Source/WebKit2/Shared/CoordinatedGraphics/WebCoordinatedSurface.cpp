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

#include "config.h"
#include "WebCoordinatedSurface.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsArgumentCoders.h"
#include "GraphicsContext.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsSurfaceToken.h>

#if USE(TEXTURE_MAPPER)
#include "BitmapTextureGL.h"
#include "TextureMapperGL.h"
#endif

using namespace WebCore;

namespace WebKit {

WebCoordinatedSurface::Handle::Handle()
{
}

void WebCoordinatedSurface::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << m_size << m_flags;
    encoder << m_bitmapHandle;
}

bool WebCoordinatedSurface::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    if (!decoder.decode(handle.m_size))
        return false;
    if (!decoder.decode(handle.m_flags))
        return false;
    if (!decoder.decode(handle.m_bitmapHandle))
        return false;

    return true;
}

RefPtr<WebCoordinatedSurface> WebCoordinatedSurface::create(const IntSize& size, CoordinatedSurface::Flags flags)
{
    if (auto bitmap = ShareableBitmap::createShareable(size, (flags & SupportsAlpha) ? ShareableBitmap::SupportsAlpha : ShareableBitmap::NoFlags))
        return create(size, flags, WTFMove(bitmap));

    return nullptr;
}

std::unique_ptr<GraphicsContext> WebCoordinatedSurface::createGraphicsContext(const IntRect& rect)
{
    ASSERT(m_bitmap);
    auto graphicsContext = m_bitmap->createGraphicsContext();
    graphicsContext->clip(rect);
    graphicsContext->translate(rect.x(), rect.y());
    return graphicsContext;
}

Ref<WebCoordinatedSurface> WebCoordinatedSurface::create(const IntSize& size, CoordinatedSurface::Flags flags, RefPtr<ShareableBitmap> bitmap)
{
    return adoptRef(*new WebCoordinatedSurface(size, flags, bitmap));
}

WebCoordinatedSurface::WebCoordinatedSurface(const IntSize& size, CoordinatedSurface::Flags flags, RefPtr<ShareableBitmap> bitmap)
    : CoordinatedSurface(size, flags)
    , m_bitmap(bitmap)
{
}

WebCoordinatedSurface::~WebCoordinatedSurface()
{
}

RefPtr<WebCoordinatedSurface> WebCoordinatedSurface::create(const Handle& handle)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle.m_bitmapHandle);
    if (!bitmap)
        return nullptr;

    return create(handle.m_size, handle.m_flags, WTFMove(bitmap));
}

bool WebCoordinatedSurface::createHandle(Handle& handle)
{
    handle.m_size = m_size;
    handle.m_flags = m_flags;

    if (!m_bitmap->createHandle(handle.m_bitmapHandle))
        return false;

    return true;
}

void WebCoordinatedSurface::paintToSurface(const IntRect& rect, CoordinatedSurface::Client& client)
{
    auto context = createGraphicsContext(rect);
    client.paintToSurfaceContext(*context);
}

#if USE(TEXTURE_MAPPER)
void WebCoordinatedSurface::copyToTexture(RefPtr<WebCore::BitmapTexture> passTexture, const IntRect& target, const IntPoint& sourceOffset)
{
    RefPtr<BitmapTexture> texture(passTexture);

    ASSERT(m_bitmap);
    RefPtr<Image> image = m_bitmap->createImage();
    texture->updateContents(image.get(), target, sourceOffset, BitmapTexture::UpdateCanModifyOriginalImageData);
}
#endif // USE(TEXTURE_MAPPER)

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)
