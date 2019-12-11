/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2012 Igalia S.L.
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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
#include "WebKitAccessibleInterfaceComponent.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityObject.h"
#include "FrameView.h"
#include "IntRect.h"
#include "RenderLayer.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"

using namespace WebCore;

static IntPoint atkToContents(const AccessibilityObject& coreObject, AtkCoordType coordType, gint x, gint y)
{
    auto* frameView = coreObject.documentFrameView();
    if (!frameView)
        return { x, y };

    switch (coordType) {
    case ATK_XY_SCREEN:
        return frameView->screenToContents({ x, y });
    case ATK_XY_WINDOW:
        return frameView->windowToContents({ x, y });
#if ATK_CHECK_VERSION(2, 30, 0)
    case ATK_XY_PARENT:
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    return { x, y };
}

static AtkObject* webkitAccessibleComponentRefAccessibleAtPoint(AtkComponent* component, gint x, gint y, AtkCoordType coordType)
{
    auto* accessible = WEBKIT_ACCESSIBLE(component);
    returnValIfWebKitAccessibleIsInvalid(accessible, nullptr);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessible);
    auto* target = downcast<AccessibilityObject>(coreObject.accessibilityHitTest(atkToContents(coreObject, coordType, x, y)));
    if (!target)
        return nullptr;

    if (auto* wrapper = target->wrapper())
        return ATK_OBJECT(g_object_ref(wrapper));

    return nullptr;
}

static void webkitAccessibleComponentGetExtents(AtkComponent* component, gint* x, gint* y, gint* width, gint* height, AtkCoordType coordType)
{
    auto* accessible = WEBKIT_ACCESSIBLE(component);
    returnIfWebKitAccessibleIsInvalid(accessible);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessible);
    contentsRelativeToAtkCoordinateType(&coreObject, coordType, snappedIntRect(coreObject.elementRect()), x, y, width, height);
}

static gboolean webkitAccessibleComponentGrabFocus(AtkComponent* component)
{
    auto* accessible = WEBKIT_ACCESSIBLE(component);
    returnValIfWebKitAccessibleIsInvalid(accessible, FALSE);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessible);
    coreObject.setFocused(true);
    return coreObject.isFocused();
}

#if ATK_CHECK_VERSION(2, 30, 0)
static gboolean webkitAccessibleComponentScrollTo(AtkComponent* component, AtkScrollType scrollType)
{
    auto* accessible = WEBKIT_ACCESSIBLE(component);
    returnValIfWebKitAccessibleIsInvalid(accessible, FALSE);

    ScrollAlignment alignX;
    ScrollAlignment alignY;

    switch (scrollType) {
    case ATK_SCROLL_TOP_LEFT:
        alignX = ScrollAlignment::alignLeftAlways;
        alignY = ScrollAlignment::alignTopAlways;
        break;
    case ATK_SCROLL_BOTTOM_RIGHT:
        alignX = ScrollAlignment::alignRightAlways;
        alignY = ScrollAlignment::alignBottomAlways;
        break;
    case ATK_SCROLL_TOP_EDGE:
    case ATK_SCROLL_BOTTOM_EDGE:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignToEdgeIfNeeded;
        break;
    case ATK_SCROLL_LEFT_EDGE:
    case ATK_SCROLL_RIGHT_EDGE:
        // Align to a particular edge is not supported, it's always the closest edge.
        alignX = ScrollAlignment::alignToEdgeIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    case ATK_SCROLL_ANYWHERE:
        alignX = ScrollAlignment::alignCenterIfNeeded;
        alignY = ScrollAlignment::alignCenterIfNeeded;
        break;
    }

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessible);
    coreObject.scrollToMakeVisible({ SelectionRevealMode::Reveal, alignX, alignY, ShouldAllowCrossOriginScrolling::Yes });

    return TRUE;
}

static gboolean webkitAccessibleComponentScrollToPoint(AtkComponent* component, AtkCoordType coordType, gint x, gint y)
{
    auto* accessible = WEBKIT_ACCESSIBLE(component);
    returnValIfWebKitAccessibleIsInvalid(accessible, FALSE);

    auto& coreObject = webkitAccessibleGetAccessibilityObject(accessible);

    IntPoint point(x, y);
    if (coordType == ATK_XY_SCREEN) {
        if (auto* frameView = coreObject.documentFrameView())
            point = frameView->contentsToWindow(frameView->screenToContents(point));
    }

    coreObject.scrollToGlobalPoint(point);

    return TRUE;
}
#endif

void webkitAccessibleComponentInterfaceInit(AtkComponentIface* iface)
{
    iface->ref_accessible_at_point = webkitAccessibleComponentRefAccessibleAtPoint;
    iface->get_extents = webkitAccessibleComponentGetExtents;
    iface->grab_focus = webkitAccessibleComponentGrabFocus;
#if ATK_CHECK_VERSION(2, 30, 0)
    iface->scroll_to = webkitAccessibleComponentScrollTo;
    iface->scroll_to_point = webkitAccessibleComponentScrollToPoint;
#endif
}

#endif // ENABLE(ACCESSIBILITY)
