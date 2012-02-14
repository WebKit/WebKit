/*
 * Copyright (C) 2009, 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "BackingStoreTile.h"

#include "GraphicsContext.h"
#include "SurfacePool.h"

#include <BlackBerryPlatformGraphics.h>

namespace BlackBerry {
namespace WebKit {

TileBuffer::TileBuffer(const Platform::IntSize& size)
    : m_size(size)
    , m_buffer(0)
    , m_blitGeneration(0)
{
}

TileBuffer::~TileBuffer()
{
    destroyBuffer(m_buffer);
}

Platform::IntSize TileBuffer::size() const
{
    return m_size;
}

Platform::IntRect TileBuffer::rect() const
{
    return Platform::IntRect(Platform::IntPoint::zero(), m_size);
}

bool TileBuffer::isRendered() const
{
    return isRendered(rect());
}

bool TileBuffer::isRendered(const Platform::IntRectRegion& contents) const
{
    return Platform::IntRectRegion::subtractRegions(contents, m_renderedRegion).isEmpty();
}

void TileBuffer::clearRenderedRegion(const Platform::IntRectRegion& region)
{
    m_renderedRegion = Platform::IntRectRegion::subtractRegions(m_renderedRegion, region);
}

void TileBuffer::clearRenderedRegion()
{
    m_renderedRegion = Platform::IntRectRegion();
}

void TileBuffer::addRenderedRegion(const Platform::IntRectRegion& region)
{
    m_renderedRegion = Platform::IntRectRegion::unionRegions(region, m_renderedRegion);
}

Platform::IntRectRegion TileBuffer::renderedRegion() const
{
    return m_renderedRegion;
}

Platform::IntRectRegion TileBuffer::notRenderedRegion() const
{
    return Platform::IntRectRegion::subtractRegions(rect(), renderedRegion());
}

Platform::Graphics::Buffer* TileBuffer::nativeBuffer() const
{
    if (!m_buffer)
        m_buffer = createBuffer(m_size, Platform::Graphics::TileBuffer, SurfacePool::globalSurfacePool()->sharedPixmapGroup());

    return m_buffer;
}


BackingStoreTile::BackingStoreTile(const Platform::IntSize& size, BufferingMode mode)
    : m_bufferingMode(mode)
    , m_committed(false)
    , m_backgroundPainted(false)
    , m_horizontalShift(0)
    , m_verticalShift(0)
{
    m_frontBuffer = new TileBuffer(size);
}

BackingStoreTile::~BackingStoreTile()
{
    delete m_frontBuffer;
    m_frontBuffer = 0;
}

Platform::IntSize BackingStoreTile::size() const
{
    return frontBuffer()->size();
}

Platform::IntRect BackingStoreTile::rect() const
{
    return frontBuffer()->rect();
}

TileBuffer* BackingStoreTile::frontBuffer() const
{
    ASSERT(m_frontBuffer);
    return m_frontBuffer;
}

TileBuffer* BackingStoreTile::backBuffer() const
{
    if (m_bufferingMode == SingleBuffered)
        return frontBuffer();

    return SurfacePool::globalSurfacePool()->backBuffer();
}

void BackingStoreTile::swapBuffers()
{
    if (m_bufferingMode == SingleBuffered)
        return;

    // Store temps.
    unsigned front = reinterpret_cast<unsigned>(frontBuffer());
    unsigned back = reinterpret_cast<unsigned>(backBuffer());

    // Atomic change.
    _smp_xchg(reinterpret_cast<unsigned*>(&m_frontBuffer), back);
    _smp_xchg(reinterpret_cast<unsigned*>(&SurfacePool::globalSurfacePool()->m_backBuffer), front);
}

void BackingStoreTile::reset()
{
    setCommitted(false);
    frontBuffer()->clearRenderedRegion();
    backBuffer()->clearRenderedRegion();
    clearShift();
}

void BackingStoreTile::paintBackground()
{
    m_backgroundPainted = true;

    clearBuffer(backBuffer()->nativeBuffer(), 0, 0, 0, 0);
}

}
}
