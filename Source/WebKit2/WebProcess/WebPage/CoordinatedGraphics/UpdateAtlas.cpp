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
#include "UpdateAtlas.h"

#if USE(COORDINATED_GRAPHICS)

#include "GraphicsContext.h"
#include "IntRect.h"
#include <wtf/MathExtras.h>

using namespace WebCore;

namespace WebKit {

UpdateAtlas::UpdateAtlas(UpdateAtlasClient* client, int dimension, CoordinatedSurface::Flags flags)
    : m_client(client)
    , m_flags(flags)
    , m_inactivityInSeconds(0)
    , m_isVaild(true)
{
    static int nextID = 0;
    m_ID = ++nextID;
    IntSize size = nextPowerOfTwo(IntSize(dimension, dimension));
    m_surface = CoordinatedSurface::create(size, flags);

    if (!static_cast<WebCoordinatedSurface*>(m_surface.get())->createHandle(m_handle)) {
        m_isVaild = false;
        return;
    }
    m_client->createUpdateAtlas(m_ID, m_handle);
}

UpdateAtlas::~UpdateAtlas()
{
    if (m_isVaild)
        m_client->removeUpdateAtlas(m_ID);
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
}

PassOwnPtr<GraphicsContext> UpdateAtlas::beginPaintingOnAvailableBuffer(int& atlasID, const WebCore::IntSize& size, IntPoint& offset)
{
    m_inactivityInSeconds = 0;
    buildLayoutIfNeeded();
    IntRect rect = m_areaAllocator->allocate(size);

    // No available buffer was found, returning null.
    if (rect.isEmpty())
        return PassOwnPtr<GraphicsContext>();

    if (!m_isVaild)
        return PassOwnPtr<GraphicsContext>();

    atlasID = m_ID;

    // FIXME: Use tri-state buffers, to allow faster updates.
    offset = rect.location();
    OwnPtr<GraphicsContext> graphicsContext = m_surface->createGraphicsContext(rect);

    if (supportsAlpha()) {
        graphicsContext->setCompositeOperation(CompositeCopy);
        graphicsContext->fillRect(IntRect(IntPoint::zero(), size), Color::transparent, ColorSpaceDeviceRGB);
        graphicsContext->setCompositeOperation(CompositeSourceOver);
    }

    return graphicsContext.release();
}

}
#endif
