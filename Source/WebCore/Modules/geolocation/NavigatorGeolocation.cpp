/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Inc.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(GEOLOCATION)

#include "NavigatorGeolocation.h"

#include "Document.h"
#include "Geolocation.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Navigator.h"

namespace WebCore {

NavigatorGeolocation::NavigatorGeolocation(Navigator& navigator)
    : m_navigator(navigator)
{
}

NavigatorGeolocation::~NavigatorGeolocation() = default;

ASCIILiteral NavigatorGeolocation::supplementName()
{
    return "NavigatorGeolocation"_s;
}

NavigatorGeolocation* NavigatorGeolocation::from(Navigator& navigator)
{
    NavigatorGeolocation* supplement = static_cast<NavigatorGeolocation*>(Supplement<Navigator>::from(&navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorGeolocation>(navigator);
        supplement = newSupplement.get();
        provideTo(&navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

#if PLATFORM(IOS_FAMILY)
void NavigatorGeolocation::resetAllGeolocationPermission()
{
    if (m_geolocation)
        m_geolocation->resetAllGeolocationPermission();
}
#endif // PLATFORM(IOS_FAMILY)

Geolocation* NavigatorGeolocation::geolocation(Navigator& navigator)
{
    return NavigatorGeolocation::from(navigator)->geolocation();
}

Geolocation* NavigatorGeolocation::geolocation() const
{
    if (!m_geolocation)
        m_geolocation = Geolocation::create(m_navigator);
    return m_geolocation.get();
}

} // namespace WebCore

#endif // ENABLE(GEOLOCATION)
