/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#if ENABLE(GEOLOCATION)

#include "LocalDOMWindowProperty.h"
#include "Supplementable.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Geolocation;
class Navigator;

class NavigatorGeolocation : public Supplement<Navigator> {
    WTF_MAKE_TZONE_ALLOCATED(NavigatorGeolocation);
public:
    explicit NavigatorGeolocation(Navigator&);
    virtual ~NavigatorGeolocation();
    static NavigatorGeolocation* from(Navigator&);

    static Geolocation& geolocation(Navigator&);
    static Geolocation* optionalGeolocation(Navigator&);
    Geolocation& geolocation() const;

#if PLATFORM(IOS_FAMILY)
    void resetAllGeolocationPermission();
#endif // PLATFORM(IOS_FAMILY)

private:
    static ASCIILiteral supplementName() { return "NavigatorGeolocation"_s; }
    bool isNavigatorGeolocation() const final { return true; }

    const RefPtr<Geolocation> m_geolocation;
    const CheckedRef<Navigator> m_navigator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::NavigatorGeolocation)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isNavigatorGeolocation(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GEOLOCATION)
