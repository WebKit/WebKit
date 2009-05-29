/*
 * Copyright (C) 2008 Nuanti Ltd.
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
#include "AXObjectCache.h"

#include "AccessibilityObject.h"
#include "AccessibilityObjectWrapperAtk.h"

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    webkit_accessible_detach(WEBKIT_ACCESSIBLE(obj->wrapper()));
}

void AXObjectCache::attachWrapper(AccessibilityObject* obj)
{
    AtkObject* atkObj = ATK_OBJECT(webkit_accessible_new(obj));
    obj->setWrapper(atkObj);
    g_object_unref(atkObj);
}

void AXObjectCache::postPlatformNotification(AccessibilityObject*, const String&)
{
}
    
void AXObjectCache::handleFocusedUIElementChanged()
{
}

void AXObjectCache::handleFocusedUIElementChangedWithRenderers(RenderObject* oldFocusedRender, RenderObject* newFocusedRender)
{
    RefPtr<AccessibilityObject> oldObject = getOrCreate(oldFocusedRender);
    if (oldObject) {
        g_signal_emit_by_name(oldObject->wrapper(), "focus-event", false);
        g_signal_emit_by_name(oldObject->wrapper(), "state-change", "focused", false);
    }
    RefPtr<AccessibilityObject> newObject = getOrCreate(newFocusedRender);
    if (newObject) {
        g_signal_emit_by_name(newObject->wrapper(), "focus-event", true);
        g_signal_emit_by_name(newObject->wrapper(), "state-change", "focused", true);
    }
}

} // namespace WebCore
