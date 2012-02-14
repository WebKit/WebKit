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
#include "BackingStoreCompositingSurface.h"

#include "GraphicsContext.h"
#include "SurfacePool.h"

#if USE(ACCELERATED_COMPOSITING)

#include <BlackBerryPlatformGraphics.h>

namespace BlackBerry {
namespace WebKit {

CompositingSurfaceBuffer::CompositingSurfaceBuffer(const Platform::IntSize& size)
    : m_size(size)
    , m_buffer(0)
{
}

CompositingSurfaceBuffer::~CompositingSurfaceBuffer()
{
    destroyBuffer(m_buffer);
}

Platform::IntSize CompositingSurfaceBuffer::surfaceSize() const
{
    return m_size;
}

Platform::Graphics::Buffer* CompositingSurfaceBuffer::nativeBuffer() const
{
    if (!m_buffer) {
        m_buffer = createBuffer(m_size,
                                Platform::Graphics::TileBuffer,
                                Platform::Graphics::GLES2);
    }
    return m_buffer;
}

BackingStoreCompositingSurface::BackingStoreCompositingSurface(const Platform::IntSize& size, bool doubleBuffered)
    : m_isDoubleBuffered(doubleBuffered)
    , m_needsSync(true)
{
    m_frontBuffer = new CompositingSurfaceBuffer(size);
    m_backBuffer = !doubleBuffered ? 0 : new CompositingSurfaceBuffer(size);
}

BackingStoreCompositingSurface::~BackingStoreCompositingSurface()
{
    delete m_frontBuffer;
    m_frontBuffer = 0;

    delete m_backBuffer;
    m_backBuffer = 0;
}

CompositingSurfaceBuffer* BackingStoreCompositingSurface::frontBuffer() const
{
    ASSERT(m_frontBuffer);
    return m_frontBuffer;
}

CompositingSurfaceBuffer* BackingStoreCompositingSurface::backBuffer() const
{
    if (!m_isDoubleBuffered)
        return frontBuffer();

    ASSERT(m_backBuffer);
    return m_backBuffer;
}

void BackingStoreCompositingSurface::swapBuffers()
{
    if (!m_isDoubleBuffered)
        return;

    // Store temps.
    unsigned front = reinterpret_cast<unsigned>(frontBuffer());
    unsigned back = reinterpret_cast<unsigned>(backBuffer());

    // Atomic change.
    _smp_xchg(reinterpret_cast<unsigned*>(&m_frontBuffer), back);
    _smp_xchg(reinterpret_cast<unsigned*>(&m_backBuffer), front);
}

} // namespace WebKit
} // namespace BlackBerry
#endif // USE(ACCELERATED_COMPOSITING)
