/*
 * Copyright (C) 2017 Igalia, S.L.
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
#include "AXObjectCache.h"

#if HAVE(ACCESSIBILITY)

#include "NotImplemented.h"

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject*, AccessibilityDetachmentType)
{
    notImplemented();
}

void AXObjectCache::attachWrapper(AccessibilityObject*)
{
    notImplemented();
}

void AXObjectCache::postPlatformNotification(AccessibilityObject*, AXNotification)
{
    notImplemented();
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, const String&)
{
    notImplemented();
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject*, AXLoadingEvent)
{
    notImplemented();
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node*, Node*)
{
    notImplemented();
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
    notImplemented();
}

} // namespace WebCore

#endif // HAVE(ACCESSIBILITY)
