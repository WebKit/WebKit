/*
 * Copyright (C) 2008 Apple Ltd.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
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
#include "AccessibilityObject.h"

#include <glib-object.h>

#if HAVE(ACCESSIBILITY)

namespace WebCore {

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    return false;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    AccessibilityObject* parent = parentObject();
    if (!parent)
        return DefaultBehavior;

    if (roleValue() == SplitterRole)
        return IncludeObject;

    if (isGroup()) {
        // When a list item is made up entirely of children (e.g. paragraphs)
        // the list item gets ignored. We need it.
        if (parent->isList())
            return IncludeObject;

        // We expect the parent of a table cell to be a table.
        AccessibilityObject* child = firstChild();
        if (child && child->roleValue() == CellRole)
            return IgnoreObject;
    }

    // Entries and password fields have extraneous children which we want to ignore.
    if (parent->isPasswordField() || parent->isTextControl())
        return IgnoreObject;

    AccessibilityRole role = roleValue();

    // Include all tables, even layout tables. The AT can decide what to do with each.
    if (role == CellRole || role == TableRole)
        return IncludeObject;

    // We at some point might have a need to expose a table row; but it's not standard Gtk+.
    if (role == RowRole)
        return IgnoreObject;

    // The object containing the text should implement AtkText itself.
    if (role == StaticTextRole)
        return IgnoreObject;

    return DefaultBehavior;
}

AccessibilityObjectWrapper* AccessibilityObject::wrapper() const
{
    return m_wrapper;
}

void AccessibilityObject::setWrapper(AccessibilityObjectWrapper* wrapper)
{
    if (wrapper == m_wrapper)
        return;

    if (m_wrapper)
        g_object_unref(m_wrapper);

    m_wrapper = wrapper;

    if (m_wrapper)
        g_object_ref(m_wrapper);
}

} // namespace WebCore

#endif // HAVE(ACCESSIBILITY)
