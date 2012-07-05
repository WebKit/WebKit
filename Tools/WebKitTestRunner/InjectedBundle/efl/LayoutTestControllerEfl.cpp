/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "LayoutTestController.h"

#include "InjectedBundle.h"
#include <Ecore.h>

namespace WTR {

static Eina_Bool waitToDumpWatchdogTimerCallback(void*)
{
    InjectedBundle::shared().layoutTestController()->waitToDumpWatchdogTimerFired();
    return false;
}

void LayoutTestController::platformInitialize()
{
    m_waitToDumpWatchdogTimer = 0;
}

void LayoutTestController::invalidateWaitToDumpWatchdogTimer()
{
    if (!m_waitToDumpWatchdogTimer)
        return;

    ecore_timer_del(m_waitToDumpWatchdogTimer);
    m_waitToDumpWatchdogTimer = 0;
}

void LayoutTestController::initializeWaitToDumpWatchdogTimerIfNeeded()
{
    if (m_waitToDumpWatchdogTimer)
        return;

    m_waitToDumpWatchdogTimer = ecore_timer_loop_add(waitToDumpWatchdogTimerInterval,
                                                     waitToDumpWatchdogTimerCallback, 0);
}

JSRetainPtr<JSStringRef> LayoutTestController::pathToLocalResource(JSStringRef url)
{
    return url;
}

JSRetainPtr<JSStringRef> LayoutTestController::platformName()
{
    JSRetainPtr<JSStringRef> platformName(Adopt, JSStringCreateWithUTF8CString("efl"));
    return platformName;
}

} // namespace WTR
