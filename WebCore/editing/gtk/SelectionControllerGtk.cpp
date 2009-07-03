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

#include "AXObjectCache.h"
#include "Frame.h"

#include <gtk/gtk.h>

namespace WebCore {

void SelectionController::notifyAccessibilityForSelectionChange()
{
    if (AXObjectCache::accessibilityEnabled() && m_sel.start().isNotNull() && m_sel.end().isNotNull()) {
        RenderObject* focusedNode = m_sel.end().node()->renderer();
        AccessibilityObject* accessibilityObject = m_frame->document()->axObjectCache()->getOrCreate(focusedNode);
        AtkObject* wrapper = accessibilityObject->wrapper();
        if (ATK_IS_TEXT(wrapper)) {
            g_signal_emit_by_name(wrapper, "text-caret-moved", m_sel.end().computeOffsetInContainerNode());

            if (m_sel.isRange())
                g_signal_emit_by_name(wrapper, "text-selection-changed");
        }
    }
}

} // namespace WebCore
