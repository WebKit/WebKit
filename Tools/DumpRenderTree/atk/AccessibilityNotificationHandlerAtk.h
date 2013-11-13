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

#ifndef AccessibilityNotificationHandlerAtk_h
#define AccessibilityNotificationHandlerAtk_h

#if HAVE(ACCESSIBILITY)

#include <JavaScriptCore/JSObjectRef.h>
#include <atk/atk.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

class AccessibilityNotificationHandler : public RefCounted<AccessibilityNotificationHandler> {
public:
    static PassRefPtr<AccessibilityNotificationHandler> create()
    {
        return adoptRef(new AccessibilityNotificationHandler());
    }
    AccessibilityNotificationHandler(void);
    ~AccessibilityNotificationHandler();

    void setPlatformElement(AtkObject* platformElement) { m_platformElement = platformElement; }
    AtkObject* platformElement(void) const { return m_platformElement; }
    void setNotificationFunctionCallback(JSObjectRef);
    JSObjectRef notificationFunctionCallback(void) const { return m_notificationFunctionCallback; }

private:
    AtkObject* m_platformElement;
    JSObjectRef m_notificationFunctionCallback;
};

#endif // HAVE(ACCESSIBILITY)

#endif // AccessibilityNotificationHandlerAtk_h
