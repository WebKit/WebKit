/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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
 */

#include "config.h"
#include "AccessibilityNotificationHandlerAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AccessibilityCallbacks.h"

AccessibilityNotificationHandler::AccessibilityNotificationHandler(void)
    : m_platformElement(nullptr)
    , m_notificationFunctionCallback(nullptr)
{
}

AccessibilityNotificationHandler::~AccessibilityNotificationHandler()
{
    removeAccessibilityNotificationHandler(this);
    disconnectAccessibilityCallbacks();
}

void AccessibilityNotificationHandler::setNotificationFunctionCallback(JSObjectRef notificationFunctionCallback)
{
    if (!notificationFunctionCallback) {
        removeAccessibilityNotificationHandler(this);
        disconnectAccessibilityCallbacks();
        return;
    }

    if (m_notificationFunctionCallback)
        removeAccessibilityNotificationHandler(this);

    m_notificationFunctionCallback = notificationFunctionCallback;
    connectAccessibilityCallbacks();
    addAccessibilityNotificationHandler(this);
}

#endif // HAVE(ACCESSIBILITY)
