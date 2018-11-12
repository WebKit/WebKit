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

#if HAVE(ACCESSIBILITY)

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
#if ATK_CHECK_VERSION(2, 30, 0)
        case ATK_XY_PARENT:
            RELEASE_ASSERT_NOT_REACHED();
#endif
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

// FIXME: Different kinds of elements are putting the title tag to use
// in different AX fields. This might not be 100% correct but we will
// keep it now in order to achieve consistency with previous behavior.
static bool titleTagShouldBeUsedInDescriptionField(AccessibilityObject* coreObject)
{
    return (coreObject->isLink() && !coreObject->isImageMapLink()) || coreObject->isImage();
}

// This should be the "visible" text that's actually on the screen if possible.
// If there's alternative text, that can override the title.
String accessibilityTitle(AccessibilityObject* coreObject)
{
    Vector<AccessibilityText> textOrder;
    coreObject->accessibilityText(textOrder);

    for (const AccessibilityText& text : textOrder) {
        // Once we encounter visible text, or the text from our children that should be used foremost.
        if (text.textSource == AccessibilityTextSource::Visible || text.textSource == AccessibilityTextSource::Children)
            return text.text;

        // If there's an element that labels this object and it's not exposed, then we should use
        // that text as our title.
        if (text.textSource == AccessibilityTextSource::LabelByElement && !coreObject->exposesTitleUIElement())
            return text.text;

        // Elements of role AccessibilityRole::Toolbar will return its title as AccessibilityTextSource::Alternative.
        if (coreObject->roleValue() == AccessibilityRole::Toolbar && text.textSource == AccessibilityTextSource::Alternative)
            return text.text;

        // FIXME: The title tag is used in certain cases for the title. This usage should
        // probably be in the description field since it's not "visible".
        if (text.textSource == AccessibilityTextSource::TitleTag && !titleTagShouldBeUsedInDescriptionField(coreObject))
            return text.text;
    }

    return String();
}

String accessibilityDescription(AccessibilityObject* coreObject)
{
    Vector<AccessibilityText> textOrder;
    coreObject->accessibilityText(textOrder);

    bool visibleTextAvailable = false;
    for (const AccessibilityText& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative)
            return text.text;

        switch (text.textSource) {
        case AccessibilityTextSource::Visible:
        case AccessibilityTextSource::Children:
        case AccessibilityTextSource::LabelByElement:
            visibleTextAvailable = true;
        default:
            break;
        }

        if (text.textSource == AccessibilityTextSource::TitleTag && !visibleTextAvailable)
            return text.text;
    }

    return String();
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
    auto& node = *coreObject->node();
    auto* lastDescendant = node.lastDescendant();
    unsigned lastOffset = lastOffsetInNode(lastDescendant);
    auto intersectsResult = range->intersectsNode(node);
    return !intersectsResult.hasException()
        && intersectsResult.releaseReturnValue()
        && (&range->endContainer() != &node || range->endOffset())
        && (&range->startContainer() != lastDescendant || range->startOffset() != lastOffset);
}

#endif
