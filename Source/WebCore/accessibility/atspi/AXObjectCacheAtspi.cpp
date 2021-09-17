/*
 * Copyright (C) 2021 Igalia S.L.
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

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
#include "AccessibilityObject.h"
#include "AccessibilityObjectAtspi.h"
#include "AccessibilityRenderObject.h"
#include "Node.h"

namespace WebCore {

void AXObjectCache::detachWrapper(AXCoreObject* axObject, AccessibilityDetachmentType detachmentType)
{
}

void AXObjectCache::attachWrapper(AXCoreObject* axObject)
{
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

bool AXObjectCache::isIsolatedTreeEnabled()
{
    return true;
}

void AXObjectCache::initializeSecondaryAXThread()
{
}

bool AXObjectCache::usedOnAXThread()
{
    return true;
}

void AXObjectCache::postPlatformNotification(AXCoreObject* coreObject, AXNotification notification)
{
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject* object, AXTextChange textChange, unsigned offset, const String& text)
{
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* object, AXLoadingEvent loadingEvent)
{
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode)
{
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
