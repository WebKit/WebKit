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

#pragma once

#include "AreaAllocator.h"
#include "NicosiaBuffer.h"
#include <wtf/RefPtr.h>

#if USE(COORDINATED_GRAPHICS)

namespace WebCore {

class IntRect;
class IntSize;

class UpdateAtlas {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UpdateAtlas);
public:
    using ID = uint32_t;

    class Client {
    public:
        virtual void createUpdateAtlas(ID, Ref<Nicosia::Buffer>&&) = 0;
        virtual void removeUpdateAtlas(ID) = 0;
    };

    WEBCORE_EXPORT UpdateAtlas(Client&, const IntSize&, Nicosia::Buffer::Flags);
    WEBCORE_EXPORT ~UpdateAtlas();

    const IntSize& size() const { return m_buffer->size(); }

    WEBCORE_EXPORT RefPtr<Nicosia::Buffer> getCoordinatedBuffer(const IntSize&, uint32_t&, IntRect&);
    WEBCORE_EXPORT void didSwapBuffers();
    bool supportsAlpha() const { return m_buffer->supportsAlpha(); }

    void addTimeInactive(double seconds)
    {
        ASSERT(!isInUse());
        m_inactivityInSeconds += seconds;
    }
    bool isInactive() const
    {
        const double inactiveSecondsTolerance = 3;
        return m_inactivityInSeconds > inactiveSecondsTolerance;
    }
    bool isInUse() const { return !!m_areaAllocator; }

private:
    void buildLayoutIfNeeded();

    ID m_id { 0 };
    Client& m_client;
    std::unique_ptr<GeneralAreaAllocator> m_areaAllocator;
    Ref<Nicosia::Buffer> m_buffer;
    double m_inactivityInSeconds { 0 };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
