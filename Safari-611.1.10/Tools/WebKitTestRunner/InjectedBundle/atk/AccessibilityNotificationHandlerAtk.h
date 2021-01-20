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

#pragma once

#if HAVE(ACCESSIBILITY)

#include <JavaScriptCore/JSObjectRef.h>
#include <atk/atk.h>
#include <atk/atkobject.h>
#include <wtf/RefCounted.h>
#include <wtf/glib/GRefPtr.h>

namespace WTR {

class AccessibilityNotificationHandler : public RefCounted<AccessibilityNotificationHandler> {
public:
    static Ref<AccessibilityNotificationHandler> create()
    {
        return adoptRef(*new AccessibilityNotificationHandler());
    }
    ~AccessibilityNotificationHandler();
    void setPlatformElement(GRefPtr<AtkObject> platformElement) { m_platformElement = platformElement; }
    GRefPtr<AtkObject> platformElement() const { return m_platformElement; }
    void setNotificationFunctionCallback(JSValueRef);
    JSValueRef notificationFunctionCallback() const { return m_notificationFunctionCallback; }

private:
    AccessibilityNotificationHandler();
    void connectAccessibilityCallbacks();
    bool disconnectAccessibilityCallbacks();
    void removeAccessibilityNotificationHandler();

    GRefPtr<AtkObject> m_platformElement;
    JSValueRef m_notificationFunctionCallback;
};

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
