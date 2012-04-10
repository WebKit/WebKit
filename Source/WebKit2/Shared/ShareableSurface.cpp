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

#include "config.h"
#include "ShareableSurface.h"

#include "WebCoreArgumentCoders.h"

#if USE(TEXTURE_MAPPER)
#include "TextureMapperGL.h"
#endif

using namespace WebCore;

namespace WebKit {

ShareableSurface::Handle::Handle()
#if USE(GRAPHICS_SURFACE)
    : m_graphicsSurfaceToken(0)
#endif
{
}

void ShareableSurface::Handle::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_size);
    encoder->encode(m_flags);
#if USE(GRAPHICS_SURFACE)
    encoder->encode(m_graphicsSurfaceToken);
    if (m_graphicsSurfaceToken)
        return;
#endif
    encoder->encode(m_bitmapHandle);
}

bool ShareableSurface::Handle::decode(CoreIPC::ArgumentDecoder* decoder, Handle& handle)
{
    if (!decoder->decode(handle.m_size))
        return false;
    if (!decoder->decode(handle.m_flags))
        return false;
#if USE(GRAPHICS_SURFACE)
    if (!decoder->decode(handle.m_graphicsSurfaceToken))
        return false;
    if (handle.m_graphicsSurfaceToken)
        return true;
#endif
    if (!decoder->decode(handle.m_bitmapHandle))
        return false;

    return true;
}

PassRefPtr<ShareableSurface> ShareableSurface::create(const IntSize& size, ShareableBitmap::Flags flags, Hints hints)
{
#if USE(GRAPHICS_SURFACE)
    if (hints & SupportsGraphicsSurface) {
        RefPtr<ShareableSurface> surface = createWithSurface(size, flags);
        if (surface)
            return surface.release();
    }
#endif

    return create(size, flags, ShareableBitmap::createShareable(size, flags));
}

#if USE(GRAPHICS_SURFACE)
PassRefPtr<ShareableSurface> ShareableSurface::createWithSurface(const IntSize& size, ShareableBitmap::Flags flags)
{
    GraphicsSurface::Flags surfaceFlags =
            GraphicsSurface::SupportsSoftwareWrite
            | GraphicsSurface::SupportsCopyToTexture
            | GraphicsSurface::SupportsSharing;

    if (flags & ShareableBitmap::SupportsAlpha)
        surfaceFlags |= GraphicsSurface::SupportsAlpha;

    // This might return null, if the system is unable to provide a new graphics surface.
    // In that case, this function would return null and allow falling back to ShareableBitmap.
    RefPtr<GraphicsSurface> surface = GraphicsSurface::create(size, surfaceFlags);
    if (!surface)
        return 0;

    ASSERT(surface);
    return adoptRef(new ShareableSurface(size, flags, surface.release()));
}
#endif

PassOwnPtr<WebCore::GraphicsContext> ShareableSurface::createGraphicsContext(const IntRect& rect)
{    
#if USE(GRAPHICS_SURFACE)
    if (isBackedByGraphicsSurface())
        return m_graphicsSurface->beginPaint(rect, 0 /* Write without retaining pixels*/);
#endif

    ASSERT(m_bitmap);
    OwnPtr<GraphicsContext> graphicsContext = m_bitmap->createGraphicsContext();
    graphicsContext->clip(rect);
    graphicsContext->translate(rect.x(), rect.y());
    return graphicsContext.release();
}

PassRefPtr<ShareableSurface> ShareableSurface::create(const IntSize& size, ShareableBitmap::Flags flags, PassRefPtr<ShareableBitmap> bitmap)
{
    return adoptRef(new ShareableSurface(size, flags, bitmap));
}

ShareableSurface::ShareableSurface(const IntSize& size, ShareableBitmap::Flags flags, PassRefPtr<ShareableBitmap> bitmap)
    : m_size(size)
    , m_flags(flags)
    , m_bitmap(bitmap)
{
}

#if USE(GRAPHICS_SURFACE)
ShareableSurface::ShareableSurface(const WebCore::IntSize& size, ShareableBitmap::Flags flags, PassRefPtr<WebCore::GraphicsSurface> surface)
    : m_size(size)
    , m_flags(flags)
    , m_graphicsSurface(surface)
{
}

PassRefPtr<ShareableSurface> ShareableSurface::create(const IntSize& size, ShareableBitmap::Flags flags, PassRefPtr<GraphicsSurface> surface)
{
    return adoptRef(new ShareableSurface(size, flags, surface));
}
#endif

ShareableSurface::~ShareableSurface()
{
}

PassRefPtr<ShareableSurface> ShareableSurface::create(const Handle& handle)
{
#if USE(GRAPHICS_SURFACE)
        RefPtr<GraphicsSurface> surface = GraphicsSurface::create(handle.m_size, handle.m_flags, handle.m_graphicsSurfaceToken);
        if (surface)
            return adoptRef(new ShareableSurface(handle.m_size, handle.m_flags, PassRefPtr<GraphicsSurface>(surface)));
#endif

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle.m_bitmapHandle);
    if (!bitmap)
        return 0;

    return create(handle.m_size, handle.m_flags, bitmap.release());
}

bool ShareableSurface::createHandle(Handle& handle)
{
    handle.m_size = m_size;
    handle.m_flags = m_flags;

#if USE(GRAPHICS_SURFACE)
    handle.m_graphicsSurfaceToken = m_graphicsSurface ? m_graphicsSurface->exportToken() : 0;
    if (handle.m_graphicsSurfaceToken)
        return true;
#endif
    if (!m_bitmap->createHandle(handle.m_bitmapHandle))
        return false;

    return true;
}

#if USE(TEXTURE_MAPPER)
void ShareableSurface::copyToTexture(PassRefPtr<WebCore::BitmapTexture> passTexture, const IntRect& target, const IntPoint& sourceOffset)
{
    RefPtr<BitmapTexture> texture(passTexture);

#if USE(GRAPHICS_SURFACE)
    if (isBackedByGraphicsSurface()) {
        RefPtr<BitmapTextureGL> textureGL = toBitmapTextureGL(texture.get());
        if (textureGL) {
            uint32_t textureID = textureGL->id();
            uint32_t textureTarget = textureGL->textureTarget();
            m_graphicsSurface->copyToGLTexture(textureTarget, textureID, target, sourceOffset);
            return;
        }

        RefPtr<Image> image = m_graphicsSurface->createReadOnlyImage(IntRect(sourceOffset, target.size()));
        texture->updateContents(image.get(), target, IntPoint::zero());
    }
#endif

    ASSERT(m_bitmap);
    RefPtr<Image> image = m_bitmap->createImage();
    texture->updateContents(image.get(), target, sourceOffset);
    return;
}
#endif // USE(TEXTURE_MAPPER)

} // namespace WebKit
