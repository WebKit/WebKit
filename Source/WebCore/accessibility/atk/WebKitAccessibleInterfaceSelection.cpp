/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2010, 2011, 2012 Igalia S.L.
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
#include "WebKitAccessibleInterfaceSelection.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityListBox.h"
#include "AccessibilityObject.h"
#include "HTMLSelectElement.h"
#include "RenderObject.h"
#include "WebKitAccessible.h"
#include "WebKitAccessibleUtil.h"

using namespace WebCore;

static AccessibilityObject* core(AtkSelection* selection)
{
    if (!WEBKIT_IS_ACCESSIBLE(selection))
        return nullptr;

    return &webkitAccessibleGetAccessibilityObject(WEBKIT_ACCESSIBLE(selection));
}

static AccessibilityObject* listObjectForCoreSelection(AccessibilityObject* coreSelection)
{
    // Only list boxes and menu lists supported so far.
    if (!coreSelection->isListBox() && !coreSelection->isMenuList())
        return nullptr;

    // For list boxes the list object is just itself.
    if (coreSelection->isListBox())
        return coreSelection;

    // For menu lists we need to return the first accessible child,
    // with role MenuListPopupRole, since that's the one holding the list
    // of items with role MenuListOptionRole.
    const AccessibilityObject::AccessibilityChildrenVector& children = coreSelection->children();
    if (!children.size())
        return nullptr;

    AccessibilityObject* listObject = children.at(0).get();
    if (!listObject->isMenuListPopup())
        return nullptr;

    return listObject;
}

static AccessibilityObject* optionFromList(AtkSelection* selection, gint index)
{
    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || index < 0)
        return nullptr;

    // Need to select the proper list object depending on the type.
    AccessibilityObject* listObject = listObjectForCoreSelection(coreSelection);
    if (!listObject)
        return nullptr;

    const AccessibilityObject::AccessibilityChildrenVector& options = listObject->children();
    if (index < static_cast<gint>(options.size()))
        return options.at(index).get();

    return nullptr;
}

static AccessibilityObject* optionFromSelection(AtkSelection* selection, gint index)
{
    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || !coreSelection->isAccessibilityRenderObject() || index < 0)
        return nullptr;

    // This method provides the functionality expected by atk_selection_ref_selection().
    // According to the ATK documentation for this method, the index is "a gint specifying
    // the index in the selection set. (e.g. the ith selection as opposed to the ith child)."
    // There is different API, namely atk_object_ref_accessible_child(), when the ith child
    // from the set of all children is sought.
    AccessibilityObject::AccessibilityChildrenVector options;
    coreSelection->selectedChildren(options);
    if (index < static_cast<gint>(options.size()))
        return options.at(index).get();

    return nullptr;
}

static gboolean webkitAccessibleSelectionAddSelection(AtkSelection* selection, gint index)
{
    g_return_val_if_fail(ATK_SELECTION(selection), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), FALSE);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection)
        return FALSE;

    AccessibilityObject* option = optionFromList(selection, index);
    if (option && (coreSelection->isListBox() || coreSelection->isMenuList())) {
        option->setSelected(true);
        return option->isSelected();
    }

    return FALSE;
}

static gboolean webkitAccessibleSelectionClearSelection(AtkSelection* selection)
{
    g_return_val_if_fail(ATK_SELECTION(selection), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), FALSE);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection)
        return FALSE;

    AccessibilityObject::AccessibilityChildrenVector selectedItems;
    if (is<AccessibilityListBox>(*coreSelection)) {
        // Set the list of selected items to an empty list; then verify that it worked.
        auto& listBox = downcast<AccessibilityListBox>(*coreSelection);
        listBox.setSelectedChildren(selectedItems);
        listBox.selectedChildren(selectedItems);
        return selectedItems.isEmpty();
    }
    return FALSE;
}

static AtkObject* webkitAccessibleSelectionRefSelection(AtkSelection* selection, gint index)
{
    g_return_val_if_fail(ATK_SELECTION(selection), nullptr);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), nullptr);

    AccessibilityObject* option = optionFromSelection(selection, index);
    if (option) {
        auto* child = option->wrapper();
        g_object_ref(child);
        return ATK_OBJECT(child);
    }

    return nullptr;
}

static gint webkitAccessibleSelectionGetSelectionCount(AtkSelection* selection)
{
    g_return_val_if_fail(ATK_SELECTION(selection), 0);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), 0);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || !coreSelection->isAccessibilityRenderObject())
        return 0;

    if (coreSelection->isMenuList()) {
        RenderObject* renderer = coreSelection->renderer();
        if (!renderer)
            return 0;

        int selectedIndex = downcast<HTMLSelectElement>(renderer->node())->selectedIndex();
        return selectedIndex >= 0 && selectedIndex < static_cast<int>(downcast<HTMLSelectElement>(renderer->node())->listItems().size());
    }

    AccessibilityObject::AccessibilityChildrenVector selectedItems;
    coreSelection->selectedChildren(selectedItems);
    return static_cast<gint>(selectedItems.size());
}

static gboolean webkitAccessibleSelectionIsChildSelected(AtkSelection* selection, gint index)
{
    g_return_val_if_fail(ATK_SELECTION(selection), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), FALSE);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection)
        return FALSE;

    AccessibilityObject* option = optionFromList(selection, index);
    if (option && (coreSelection->isListBox() || coreSelection->isMenuList()))
        return option->isSelected();

    return FALSE;
}

static gboolean webkitAccessibleSelectionRemoveSelection(AtkSelection* selection, gint index)
{
    g_return_val_if_fail(ATK_SELECTION(selection), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), FALSE);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection)
        return FALSE;

    AccessibilityObject* option = optionFromSelection(selection, index);
    if (option && (coreSelection->isListBox() || coreSelection->isMenuList())) {
        option->setSelected(false);
        return !option->isSelected();
    }

    return FALSE;
}

static gboolean webkitAccessibleSelectionSelectAllSelection(AtkSelection* selection)
{
    g_return_val_if_fail(ATK_SELECTION(selection), FALSE);
    returnValIfWebKitAccessibleIsInvalid(WEBKIT_ACCESSIBLE(selection), FALSE);

    AccessibilityObject* coreSelection = core(selection);
    if (!coreSelection || !coreSelection->isMultiSelectable())
        return FALSE;

    if (is<AccessibilityListBox>(*coreSelection)) {
        const AccessibilityObject::AccessibilityChildrenVector& children = coreSelection->children();
        AccessibilityListBox& listBox = downcast<AccessibilityListBox>(*coreSelection);
        listBox.setSelectedChildren(children);
        AccessibilityObject::AccessibilityChildrenVector selectedItems;
        listBox.selectedChildren(selectedItems);
        return selectedItems.size() == children.size();
    }

    return FALSE;
}

void webkitAccessibleSelectionInterfaceInit(AtkSelectionIface* iface)
{
    iface->add_selection = webkitAccessibleSelectionAddSelection;
    iface->clear_selection = webkitAccessibleSelectionClearSelection;
    iface->ref_selection = webkitAccessibleSelectionRefSelection;
    iface->get_selection_count = webkitAccessibleSelectionGetSelectionCount;
    iface->is_child_selected = webkitAccessibleSelectionIsChildSelected;
    iface->remove_selection = webkitAccessibleSelectionRemoveSelection;
    iface->select_all_selection = webkitAccessibleSelectionSelectAllSelection;
}

#endif
