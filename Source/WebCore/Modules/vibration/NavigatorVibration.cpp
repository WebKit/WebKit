/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "NavigatorVibration.h"

#if ENABLE(VIBRATION)

#include "ExceptionCode.h"
#include "Frame.h"
#include "Navigator.h"
#include "Page.h"
#include "Vibration.h"
#include <wtf/Uint32Array.h>

namespace WebCore {

NavigatorVibration::NavigatorVibration()
{
}

NavigatorVibration::~NavigatorVibration()
{
}

void NavigatorVibration::webkitVibrate(Navigator* navigator, unsigned long time, ExceptionCode& ec)
{
    if (!navigator->frame()->page())
        return;

#if ENABLE(PAGE_VISIBILITY_API)
    if (navigator->frame()->page()->visibilityState() == PageVisibilityStateHidden)
        return;
#endif

    if (!Vibration::isActive(navigator->frame()->page())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vibration::from(navigator->frame()->page())->vibrate(time);
}

void NavigatorVibration::webkitVibrate(Navigator* navigator, const VibrationPattern& pattern, ExceptionCode& ec)
{
    if (!navigator->frame()->page())
        return;

#if ENABLE(PAGE_VISIBILITY_API)
    if (navigator->frame()->page()->visibilityState() == PageVisibilityStateHidden)
        return;
#endif

    if (!Vibration::isActive(navigator->frame()->page())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Vibration::from(navigator->frame()->page())->vibrate(pattern);
}

} // namespace WebCore

#endif // ENABLE(VIBRATION)

