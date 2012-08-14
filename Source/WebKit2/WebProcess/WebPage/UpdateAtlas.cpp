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
#include "UpdateAtlas.h"

#if USE(COORDINATED_GRAPHICS)

#include "GraphicsContext.h"
#include "IntRect.h"
#include <wtf/MathExtras.h>

using namespace WebCore;

namespace WebKit {

UpdateAtlas::UpdateAtlas(int dimension, ShareableBitmap::Flags flags)
    : m_flags(flags)
{
    IntSize size = nextPowerOfTwo(IntSize(dimension, dimension));
    m_surface = ShareableSurface::create(size, flags, ShareableSurface::SupportsGraphicsSurface);
}

void UpdateAtlas::buildLayoutIfNeeded()
{
    if (!m_areaAllocator) {
        m_areaAllocator = adoptPtr(new GeneralAreaAllocator(size()));
        m_areaAllocator->setMinimumAllocation(IntSize(32, 32));
    }
}

void UpdateAtlas::didSwapBuffers()
{
    m_areaAllocator.clear();
    buildLayoutIfNeeded();
}

PassOwnPtr<GraphicsContext> UpdateAtlas::beginPaintingOnAvailableBuffer(ShareableSurface::Handle& handle, const WebCore::IntSize& size, IntPoint& offset)
{
    buildLayoutIfNeeded();
    IntRect rect = m_areaAllocator->allocate(size);

    // No available buffer was found, returning null.
    if (rect.isEmpty())
        return PassOwnPtr<GraphicsContext>();

    if (!m_surface->createHandle(handle))
        return PassOwnPtr<WebCore::GraphicsContext>();

    // FIXME: Use tri-state buffers, to allow faster updates.
    offset = rect.location();
    OwnPtr<GraphicsContext> graphicsContext = m_surface->createGraphicsContext(rect);

    if (flags() & ShareableBitmap::SupportsAlpha) {
        graphicsContext->setCompositeOperation(CompositeCopy);
        graphicsContext->fillRect(IntRect(IntPoint::zero(), size), Color::transparent, ColorSpaceDeviceRGB);
        graphicsContext->setCompositeOperation(CompositeSourceOver);
    }

    return graphicsContext.release();
}

}
#endif
