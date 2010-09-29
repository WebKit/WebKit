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
#include "SelectionController.h"

#include "AccessibilityObjectWrapperAtk.h"
#include "AXObjectCache.h"
#include "Frame.h"

#include <gtk/gtk.h>

namespace WebCore {

void SelectionController::notifyAccessibilityForSelectionChange()
{
    if (AXObjectCache::accessibilityEnabled() && m_selection.start().isNotNull() && m_selection.end().isNotNull()) {
        RenderObject* focusedNode = m_selection.end().node()->renderer();
        AccessibilityObject* accessibilityObject = m_frame->document()->axObjectCache()->getOrCreate(focusedNode);

        // need to check this as getOrCreate could return 0
        if (!accessibilityObject)
            return;

        int offset;
        // Always report the events w.r.t. the non-linked unignored parent. (i.e. ignoreLinks == true)
        AccessibilityObject* object = objectAndOffsetUnignored(accessibilityObject, offset, true);
        if (!object)
            return;

        AtkObject* wrapper = object->wrapper();
        if (ATK_IS_TEXT(wrapper)) {
            g_signal_emit_by_name(wrapper, "text-caret-moved", offset);
            if (m_selection.isRange())
                g_signal_emit_by_name(wrapper, "text-selection-changed");
        }
    }
}

} // namespace WebCore
