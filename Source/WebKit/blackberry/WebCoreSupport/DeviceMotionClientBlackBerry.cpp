/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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
#include "DeviceMotionClientBlackBerry.h"

#include "DeviceMotionController.h"
#include "DeviceMotionData.h"
#include "WebPage_p.h"
#include <BlackBerryPlatformDeviceMotionTracker.h>

using namespace WebCore;

DeviceMotionClientBlackBerry::DeviceMotionClientBlackBerry(BlackBerry::WebKit::WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
    , m_tracker(0)
    , m_controller(0)
    , m_lastEventTime(0)
{
}

DeviceMotionClientBlackBerry::~DeviceMotionClientBlackBerry()
{
    if (m_tracker)
        m_tracker->destroy();
}

void DeviceMotionClientBlackBerry::setController(DeviceMotionController* controller)
{
    m_controller = controller;
}

void DeviceMotionClientBlackBerry::deviceMotionControllerDestroyed()
{
    delete this;
}

void DeviceMotionClientBlackBerry::startUpdating()
{
    if (m_tracker)
        m_tracker->resume();
    else
        m_tracker = BlackBerry::Platform::DeviceMotionTracker::create(this);
}

void DeviceMotionClientBlackBerry::stopUpdating()
{
    if (m_tracker)
        m_tracker->suspend();
}

DeviceMotionData* DeviceMotionClientBlackBerry::currentDeviceMotion() const
{
    return m_currentMotion.get();
}

void DeviceMotionClientBlackBerry::onMotion(const BlackBerry::Platform::DeviceMotionEvent* event)
{
    RefPtr<DeviceMotionData::Acceleration> accel = DeviceMotionData::Acceleration::create(
            true, event->x, true, event->y, true, event->z);

    double now = WTF::currentTimeMS();
    m_currentMotion = DeviceMotionData::create(0, accel, 0, m_lastEventTime, m_lastEventTime - now);
    m_lastEventTime = now;

    if (!m_controller)
        return;

    m_controller->didChangeDeviceMotion(currentDeviceMotion());
}
