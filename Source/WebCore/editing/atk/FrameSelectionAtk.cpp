/*
 * Copyright (C) 2009 Igalia S.L.
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
#include "FrameSelection.h"

#if ENABLE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "Document.h"
#include "Frame.h"
#include "RenderListItem.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"
#include <glib.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static void emitTextSelectionChange(AccessibilityObject* object, VisibleSelection selection, int offset)
{
    auto* axObject = object->wrapper();
    if (!axObject || !ATK_IS_TEXT(axObject))
        return;

    // We need to adjust the offset for the list item marker in Left-To-Right text because
    // the list item marker is exposed through the text of the accessible list item rather
    // than through a separate accessible object.
    RenderObject* renderer = object->renderer();
    if (is<RenderListItem>(renderer) && renderer->style().direction() == TextDirection::LTR)
        offset += downcast<RenderListItem>(*renderer).markerTextWithSuffix().length();

    g_signal_emit_by_name(axObject, "text-caret-moved", offset);
    if (selection.isRange())
        g_signal_emit_by_name(axObject, "text-selection-changed");
}

static void maybeEmitTextFocusChange(RefPtr<AccessibilityObject>&& object)
{
    // This static variable is needed to keep track of the old object
    // as per previous calls to this function, in order to properly
    // decide whether to emit some signals or not.
    static NeverDestroyed<RefPtr<AccessibilityObject>> oldObject;

    // Ensure the oldObject belongs to the same document that the
    // current object so further comparisons make sense. Otherwise,
    // just reset oldObject to 0 so it won't be taken into account in
    // the immediately following call to this function.
    if (object && oldObject.get() && oldObject.get()->document() != object->document())
        oldObject.get() = nullptr;

    auto* axObject = object ? object->wrapper() : nullptr;
    auto* oldAxObject = oldObject.get() ? oldObject.get()->wrapper() : nullptr;

    if (axObject != oldAxObject) {
        if (oldAxObject && ATK_IS_TEXT(oldAxObject)) {
            g_signal_emit_by_name(oldAxObject, "focus-event", FALSE);
            atk_object_notify_state_change(ATK_OBJECT(oldAxObject), ATK_STATE_FOCUSED, FALSE);
        }
        if (axObject && ATK_IS_TEXT(axObject)) {
            g_signal_emit_by_name(axObject, "focus-event", TRUE);
            atk_object_notify_state_change(ATK_OBJECT(axObject), ATK_STATE_FOCUSED, TRUE);
        }
    }

    // Update pointer to last focused object.
    oldObject.get() = WTFMove(object);
}


void FrameSelection::notifyAccessibilityForSelectionChange(const AXTextStateChangeIntent&)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    if (!m_selection.start().isNotNull() || !m_selection.end().isNotNull())
        return;

    Node* focusedNode = m_selection.end().containerNode();
    if (!focusedNode)
        return;

    AXObjectCache* cache = m_frame->document()->existingAXObjectCache();
    if (!cache)
        return;

    AccessibilityObject* accessibilityObject = cache->getOrCreate(focusedNode->renderer());
    if (!accessibilityObject)
        return;

    int offset;
    RefPtr<AccessibilityObject> object = objectFocusedAndCaretOffsetUnignored(accessibilityObject, offset);
    if (!object)
        return;

    emitTextSelectionChange(object.get(), m_selection, offset);
    maybeEmitTextFocusChange(WTFMove(object));
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY)
