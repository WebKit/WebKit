/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include "config.h"
#include "DeviceOrientationClientQt.h"

#include "qwebpage.h"

namespace WebCore {

DeviceOrientationClientQt::DeviceOrientationClientQt(QWebPage* page)
    : m_page(page)
    , m_controller(0)
{
}

void DeviceOrientationClientQt::setController(DeviceOrientationController* controller)
{
    m_controller = controller;
}

void DeviceOrientationClientQt::startUpdating()
{
    // call start method from a orientation provider.
}

void DeviceOrientationClientQt::stopUpdating()
{
    // call stop method from a orientation provider.
}

DeviceOrientation* DeviceOrientationClientQt::lastOrientation() const
{
    return 0;
}

void DeviceOrientationClientQt::deviceOrientationControllerDestroyed()
{
    delete this;
}

} // namespace WebCore

#include "moc_DeviceOrientationClientQt.cpp"
