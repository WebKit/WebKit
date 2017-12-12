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

#include "CoordinatedGraphicsState.h"
#include "IntRect.h"

namespace WebCore {

UpdateAtlas::UpdateAtlas(Client& client, const IntSize& size, Nicosia::Buffer::Flags flags)
    : m_client(client)
    , m_buffer(Nicosia::Buffer::create(size, flags))
{
    static ID s_nextID { 0 };
    m_id = ++s_nextID;

    m_client.createUpdateAtlas(m_id, m_buffer.copyRef());
}

UpdateAtlas::~UpdateAtlas()
{
    m_client.removeUpdateAtlas(m_id);
}

void UpdateAtlas::buildLayoutIfNeeded()
{
    if (m_areaAllocator)
        return;
    m_areaAllocator = std::make_unique<GeneralAreaAllocator>(size());
    m_areaAllocator->setMinimumAllocation(IntSize(32, 32));
}

void UpdateAtlas::didSwapBuffers()
{
    m_areaAllocator = nullptr;
}

RefPtr<Nicosia::Buffer> UpdateAtlas::getCoordinatedBuffer(const IntSize& size, uint32_t& atlasID, IntRect& allocatedRect)
{
    m_inactivityInSeconds = 0;
    buildLayoutIfNeeded();
    allocatedRect = m_areaAllocator->allocate(size);

    if (allocatedRect.isEmpty())
        return nullptr;

    atlasID = m_id;
    return m_buffer.copyRef();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
