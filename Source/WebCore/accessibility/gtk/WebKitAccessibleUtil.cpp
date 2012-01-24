/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2012 Igalia S.L.
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
#include "WebKitAccessibleUtil.h"

#include "AccessibilityObject.h"
#include "FrameView.h"
#include "IntRect.h"
#include "Node.h"
#include "Range.h"
#include "VisibleSelection.h"

#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

using namespace WebCore;

AtkAttributeSet* addToAtkAttributeSet(AtkAttributeSet* attributeSet, const char* name, const char* value)
{
    AtkAttribute* attribute = static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
    attribute->name = g_strdup(name);
    attribute->value = g_strdup(value);
    attributeSet = g_slist_prepend(attributeSet, attribute);
    return attributeSet;
}

void contentsRelativeToAtkCoordinateType(AccessibilityObject* coreObject, AtkCoordType coordType, IntRect rect, gint* x, gint* y, gint* width, gint* height)
{
    FrameView* frameView = coreObject->documentFrameView();

    if (frameView) {
        switch (coordType) {
        case ATK_XY_WINDOW:
            rect = frameView->contentsToWindow(rect);
            break;
        case ATK_XY_SCREEN:
            rect = frameView->contentsToScreen(rect);
            break;
        }
    }

    if (x)
        *x = rect.x();
    if (y)
        *y = rect.y();
    if (width)
        *width = rect.width();
    if (height)
        *height = rect.height();
}

// Used to provide const char* returns.
const char* returnString(const String& str)
{
    static CString returnedString;
    returnedString = str.utf8();
    return returnedString.data();
}

bool selectionBelongsToObject(AccessibilityObject* coreObject, VisibleSelection& selection)
{
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return false;

    if (selection.isNone())
        return false;

    RefPtr<Range> range = selection.toNormalizedRange();
    if (!range)
        return false;

    // We want to check that both the selection intersects the node
    // AND that the selection is not just "touching" one of the
    // boundaries for the selected node. We want to check whether the
    // node is actually inside the region, at least partially.
    Node* node = coreObject->node();
    Node* lastDescendant = node->lastDescendant();
    ExceptionCode ec = 0;
    return (range->intersectsNode(node, ec)
            && (range->endContainer() != node || range->endOffset())
            && (range->startContainer() != lastDescendant || range->startOffset() != lastOffsetInNode(lastDescendant)));
}
