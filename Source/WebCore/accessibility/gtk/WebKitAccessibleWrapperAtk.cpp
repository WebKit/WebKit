/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Alonzo
 * Copyright (C) 2009, 2010, 2011, 2012 Igalia S.L.
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
#include "WebKitAccessibleWrapperAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableRow.h"
#include "CharacterNames.h"
#include "Document.h"
#include "DocumentType.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameView.h"
#include "GOwnPtr.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableElement.h"
#include "HostWindow.h"
#include "InlineTextBox.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderText.h"
#include "Settings.h"
#include "TextEncoding.h"
#include "TextIterator.h"
#include "WebKitAccessibleHyperlink.h"
#include "WebKitAccessibleInterfaceAction.h"
#include "WebKitAccessibleInterfaceComponent.h"
#include "WebKitAccessibleInterfaceDocument.h"
#include "WebKitAccessibleInterfaceEditableText.h"
#include "WebKitAccessibleInterfaceHyperlinkImpl.h"
#include "WebKitAccessibleInterfaceHypertext.h"
#include "WebKitAccessibleInterfaceImage.h"
#include "WebKitAccessibleInterfaceSelection.h"
#include "WebKitAccessibleUtil.h"
#include "htmlediting.h"
#include "visible_units.h"

#include <atk/atk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgail-util/gail-util.h>
#include <pango/pango.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

using namespace WebCore;

static AccessibilityObject* fallbackObject()
{
    // FIXME: An AXObjectCache with a Document is meaningless.
    static AXObjectCache* fallbackCache = new AXObjectCache(0);
    static AccessibilityObject* object = 0;
    if (!object) {
        // FIXME: using fallbackCache->getOrCreate(ListBoxOptionRole) is a hack
        object = fallbackCache->getOrCreate(ListBoxOptionRole);
        object->ref();
    }

    return object;
}

static AccessibilityObject* core(WebKitAccessible* accessible)
{
    if (!accessible)
        return 0;

    return accessible->m_object;
}

static AccessibilityObject* core(AtkObject* object)
{
    if (!WEBKIT_IS_ACCESSIBLE(object))
        return 0;

    return core(WEBKIT_ACCESSIBLE(object));
}

static AccessibilityObject* core(AtkText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkTable* table)
{
    return core(ATK_OBJECT(table));
}

static AccessibilityObject* core(AtkValue* value)
{
    return core(ATK_OBJECT(value));
}

static gchar* webkit_accessible_text_get_text(AtkText*, gint startOffset, gint endOffset);

static const gchar* webkit_accessible_get_name(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    if (!coreObject->isAccessibilityRenderObject())
        return returnString(coreObject->stringValue());

    if (coreObject->isControl()) {
        AccessibilityObject* label = coreObject->correspondingLabelForControlElement();
        if (label) {
            AtkObject* atkObject = label->wrapper();
            if (ATK_IS_TEXT(atkObject))
                return webkit_accessible_text_get_text(ATK_TEXT(atkObject), 0, -1);
        }

        // Try text under the node.
        String textUnder = coreObject->textUnderElement();
        if (textUnder.length())
            return returnString(textUnder);
    }

    if (coreObject->isImage() || coreObject->isInputImage()) {
        Node* node = coreObject->node();
        if (node && node->isHTMLElement()) {
            // Get the attribute rather than altText String so as not to fall back on title.
            String alt = toHTMLElement(node)->getAttribute(HTMLNames::altAttr);
            if (!alt.isEmpty())
                return returnString(alt);
        }
    }

    // Fallback for the webArea object: just return the document's title.
    if (coreObject->isWebArea()) {
        Document* document = coreObject->document();
        if (document)
            return returnString(document->title());
    }

    // Nothing worked so far, try with the AccessibilityObject's
    // title() before going ahead with stringValue().
    String axTitle = coreObject->title();
    if (!axTitle.isEmpty())
        return returnString(axTitle);

    return returnString(coreObject->stringValue());
}

static const gchar* webkit_accessible_get_description(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    Node* node = 0;
    if (coreObject->isAccessibilityRenderObject())
        node = coreObject->node();
    if (!node || !node->isHTMLElement() || coreObject->ariaRoleAttribute() != UnknownRole)
        return returnString(coreObject->accessibilityDescription());

    // atk_table_get_summary returns an AtkObject. We have no summary object, so expose summary here.
    if (coreObject->roleValue() == TableRole) {
        String summary = static_cast<HTMLTableElement*>(node)->summary();
        if (!summary.isEmpty())
            return returnString(summary);
    }

    // The title attribute should be reliably available as the object's descripton.
    // We do not want to fall back on other attributes in its absence. See bug 25524.
    String title = toHTMLElement(node)->title();
    if (!title.isEmpty())
        return returnString(title);

    return returnString(coreObject->accessibilityDescription());
}

static void setAtkRelationSetFromCoreObject(AccessibilityObject* coreObject, AtkRelationSet* relationSet)
{
    if (coreObject->isControl()) {
        AccessibilityObject* label = coreObject->correspondingLabelForControlElement();
        if (label)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
    } else {
        AccessibilityObject* control = coreObject->correspondingControlForLabelElement();
        if (control)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, control->wrapper());
    }
}

static gpointer webkit_accessible_parent_class = 0;

static bool isRootObject(AccessibilityObject* coreObject)
{
    // The root accessible object in WebCore is always an object with
    // the ScrolledArea role with one child with the WebArea role.
    if (!coreObject || !coreObject->isScrollView())
        return false;

    AccessibilityObject* firstChild = coreObject->firstChild();
    if (!firstChild || !firstChild->isWebArea())
        return false;

    return true;
}

static AtkObject* atkParentOfRootObject(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* coreParent = coreObject->parentObjectUnignored();

    // The top level object claims to not have a parent. This makes it
    // impossible for assistive technologies to ascend the accessible
    // hierarchy all the way to the application. (Bug 30489)
    if (!coreParent && isRootObject(coreObject)) {
        Document* document = coreObject->document();
        if (!document)
            return 0;

        HostWindow* hostWindow = document->view()->hostWindow();
        if (hostWindow) {
            PlatformPageClient scrollView = hostWindow->platformPageClient();
            if (scrollView) {
                GtkWidget* scrollViewParent = gtk_widget_get_parent(scrollView);
                if (scrollViewParent)
                    return gtk_widget_get_accessible(scrollViewParent);
            }
        }
    }

    if (!coreParent)
        return 0;

    return coreParent->wrapper();
}

static AtkObject* webkit_accessible_get_parent(AtkObject* object)
{
    // Check first if the parent has been already set.
    AtkObject* accessibleParent = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->get_parent(object);
    if (accessibleParent)
        return accessibleParent;

    // Parent not set yet, so try to find it in the hierarchy.
    AccessibilityObject* coreObject = core(object);
    if (!coreObject)
        return 0;

    AccessibilityObject* coreParent = coreObject->parentObjectUnignored();

    if (!coreParent && isRootObject(coreObject))
        return atkParentOfRootObject(object);

    if (!coreParent)
        return 0;

    // GTK doesn't expose table rows to Assistive technologies, but we
    // need to have them anyway in the hierarchy from WebCore to
    // properly perform coordinates calculations when requested.
    if (coreParent->isTableRow() && coreObject->isTableCell())
        coreParent = coreParent->parentObjectUnignored();

    return coreParent->wrapper();
}

static gint getNChildrenForTable(AccessibilityObject* coreObject)
{
    AccessibilityObject::AccessibilityChildrenVector tableChildren = coreObject->children();
    size_t tableChildrenCount = tableChildren.size();
    size_t cellsCount = 0;

    // Look for the actual index of the cell inside the table.
    for (unsigned i = 0; i < tableChildrenCount; ++i) {
        if (tableChildren[i]->isTableRow()) {
            AccessibilityObject::AccessibilityChildrenVector rowChildren = tableChildren[i]->children();
            cellsCount += rowChildren.size();
        } else
            cellsCount++;
    }

    return cellsCount;
}

static gint webkit_accessible_get_n_children(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);

    // Tables should be treated in a different way because rows should
    // be bypassed for GTK when exposing the accessible hierarchy.
    if (coreObject->isAccessibilityTable())
        return getNChildrenForTable(coreObject);

    return coreObject->children().size();
}

static AccessibilityObject* getChildForTable(AccessibilityObject* coreObject, gint index)
{
    AccessibilityObject::AccessibilityChildrenVector tableChildren = coreObject->children();
    size_t tableChildrenCount = tableChildren.size();
    size_t cellsCount = 0;

    // Look for the actual index of the cell inside the table.
    size_t current = static_cast<size_t>(index);
    for (unsigned i = 0; i < tableChildrenCount; ++i) {
        if (tableChildren[i]->isTableRow()) {
            AccessibilityObject::AccessibilityChildrenVector rowChildren = tableChildren[i]->children();
            size_t rowChildrenCount = rowChildren.size();
            if (current < cellsCount + rowChildrenCount)
                return rowChildren.at(current - cellsCount).get();
            cellsCount += rowChildrenCount;
        } else if (cellsCount == current)
            return tableChildren[i].get();
        else
            cellsCount++;
    }

    // Shouldn't reach if the child was found.
    return 0;
}

static AtkObject* webkit_accessible_ref_child(AtkObject* object, gint index)
{
    if (index < 0)
        return 0;

    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* coreChild = 0;

    // Tables are special cases in GTK because rows should be
    // bypassed, but still taking their cells into account.
    if (coreObject->isAccessibilityTable())
        coreChild = getChildForTable(coreObject, index);
    else {
        AccessibilityObject::AccessibilityChildrenVector children = coreObject->children();
        if (static_cast<unsigned>(index) >= children.size())
            return 0;
        coreChild = children.at(index).get();
    }

    if (!coreChild)
        return 0;

    AtkObject* child = coreChild->wrapper();
    atk_object_set_parent(child, object);
    g_object_ref(child);

    return child;
}

static gint getIndexInParentForCellInRow(AccessibilityObject* coreObject)
{
    AccessibilityObject* parent = coreObject->parentObjectUnignored();
    if (!parent)
        return -1;

    AccessibilityObject* grandParent = parent->parentObjectUnignored();
    if (!grandParent)
        return -1;

    AccessibilityObject::AccessibilityChildrenVector rows = grandParent->children();
    size_t rowsCount = rows.size();
    size_t previousCellsCount = 0;

    // Look for the actual index of the cell inside the table.
    for (unsigned i = 0; i < rowsCount; ++i) {
        if (!rows[i]->isTableRow())
            continue;

        AccessibilityObject::AccessibilityChildrenVector cells = rows[i]->children();
        size_t cellsCount = cells.size();

        if (rows[i] == parent) {
            for (unsigned j = 0; j < cellsCount; ++j) {
                if (cells[j] == coreObject)
                    return previousCellsCount + j;
            }
        }

        previousCellsCount += cellsCount;
    }

    return -1;
}


static gint webkit_accessible_get_index_in_parent(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    AccessibilityObject* parent = coreObject->parentObjectUnignored();

    if (!parent && isRootObject(coreObject)) {
        AtkObject* atkParent = atkParentOfRootObject(object);
        if (!atkParent)
            return -1;

        unsigned count = atk_object_get_n_accessible_children(atkParent);
        for (unsigned i = 0; i < count; ++i) {
            AtkObject* child = atk_object_ref_accessible_child(atkParent, i);
            bool childIsObject = child == object;
            g_object_unref(child);
            if (childIsObject)
                return i;
        }
    }

    // Need to calculate the index of the cell in the table, as
    // rows won't be exposed to assistive technologies in GTK.
    if (parent && parent->isTableRow() && coreObject->isTableCell())
        return getIndexInParentForCellInRow(coreObject);

    AccessibilityObject::AccessibilityChildrenVector children = parent->children();
    unsigned count = children.size();
    for (unsigned i = 0; i < count; ++i) {
        if (children[i] == coreObject)
            return i;
    }

    return -1;
}

static AtkAttributeSet* webkit_accessible_get_attributes(AtkObject* object)
{
    AtkAttributeSet* attributeSet = 0;
    attributeSet = addToAtkAttributeSet(attributeSet, "toolkit", "WebKitGtk");

    AccessibilityObject* coreObject = core(object);
    if (!coreObject)
        return attributeSet;

    int headingLevel = coreObject->headingLevel();
    if (headingLevel) {
        String value = String::number(headingLevel);
        attributeSet = addToAtkAttributeSet(attributeSet, "level", value.utf8().data());
    }

    // Set the 'layout-guess' attribute to help Assistive
    // Technologies know when an exposed table is not data table.
    if (coreObject->isAccessibilityTable() && !coreObject->isDataTable())
        attributeSet = addToAtkAttributeSet(attributeSet, "layout-guess", "true");

    return attributeSet;
}

static AtkRole atkRole(AccessibilityRole role)
{
    switch (role) {
    case UnknownRole:
        return ATK_ROLE_UNKNOWN;
    case ButtonRole:
        return ATK_ROLE_PUSH_BUTTON;
    case RadioButtonRole:
        return ATK_ROLE_RADIO_BUTTON;
    case CheckBoxRole:
        return ATK_ROLE_CHECK_BOX;
    case SliderRole:
        return ATK_ROLE_SLIDER;
    case TabGroupRole:
        return ATK_ROLE_PAGE_TAB_LIST;
    case TextFieldRole:
    case TextAreaRole:
        return ATK_ROLE_ENTRY;
    case StaticTextRole:
        return ATK_ROLE_TEXT;
    case OutlineRole:
        return ATK_ROLE_TREE;
    case MenuBarRole:
        return ATK_ROLE_MENU_BAR;
    case MenuListPopupRole:
    case MenuRole:
        return ATK_ROLE_MENU;
    case MenuListOptionRole:
    case MenuItemRole:
        return ATK_ROLE_MENU_ITEM;
    case ColumnRole:
        // return ATK_ROLE_TABLE_COLUMN_HEADER; // Is this right?
        return ATK_ROLE_UNKNOWN; // Matches Mozilla
    case RowRole:
        // return ATK_ROLE_TABLE_ROW_HEADER; // Is this right?
        return ATK_ROLE_LIST_ITEM; // Matches Mozilla
    case ToolbarRole:
        return ATK_ROLE_TOOL_BAR;
    case BusyIndicatorRole:
        return ATK_ROLE_PROGRESS_BAR; // Is this right?
    case ProgressIndicatorRole:
        // return ATK_ROLE_SPIN_BUTTON; // Some confusion about this role in AccessibilityRenderObject.cpp
        return ATK_ROLE_PROGRESS_BAR;
    case WindowRole:
        return ATK_ROLE_WINDOW;
    case PopUpButtonRole:
    case ComboBoxRole:
        return ATK_ROLE_COMBO_BOX;
    case SplitGroupRole:
        return ATK_ROLE_SPLIT_PANE;
    case SplitterRole:
        return ATK_ROLE_SEPARATOR;
    case ColorWellRole:
        return ATK_ROLE_COLOR_CHOOSER;
    case ListRole:
        return ATK_ROLE_LIST;
    case ScrollBarRole:
        return ATK_ROLE_SCROLL_BAR;
    case ScrollAreaRole:
        return ATK_ROLE_SCROLL_PANE;
    case GridRole: // Is this right?
    case TableRole:
        return ATK_ROLE_TABLE;
    case ApplicationRole:
        return ATK_ROLE_APPLICATION;
    case GroupRole:
    case RadioGroupRole:
        return ATK_ROLE_PANEL;
    case RowHeaderRole: // Row headers are cells after all.
    case ColumnHeaderRole: // Column headers are cells after all.
    case CellRole:
        return ATK_ROLE_TABLE_CELL;
    case LinkRole:
    case WebCoreLinkRole:
    case ImageMapLinkRole:
        return ATK_ROLE_LINK;
    case ImageMapRole:
    case ImageRole:
        return ATK_ROLE_IMAGE;
    case ListMarkerRole:
        return ATK_ROLE_TEXT;
    case WebAreaRole:
        // return ATK_ROLE_HTML_CONTAINER; // Is this right?
        return ATK_ROLE_DOCUMENT_FRAME;
    case HeadingRole:
        return ATK_ROLE_HEADING;
    case ListBoxRole:
        return ATK_ROLE_LIST;
    case ListItemRole:
    case ListBoxOptionRole:
        return ATK_ROLE_LIST_ITEM;
    case ParagraphRole:
        return ATK_ROLE_PARAGRAPH;
    case LabelRole:
        return ATK_ROLE_LABEL;
    case DivRole:
        return ATK_ROLE_SECTION;
    case FormRole:
        return ATK_ROLE_FORM;
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkit_accessible_get_role(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);

    if (!coreObject)
        return ATK_ROLE_UNKNOWN;

    // Note: Why doesn't WebCore have a password field for this
    if (coreObject->isPasswordField())
        return ATK_ROLE_PASSWORD_TEXT;

    return atkRole(coreObject->roleValue());
}

static bool selectionBelongsToObject(AccessibilityObject* coreObject, VisibleSelection& selection)
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

static bool isTextWithCaret(AccessibilityObject* coreObject)
{
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return false;

    Document* document = coreObject->document();
    if (!document)
        return false;

    Frame* frame = document->frame();
    if (!frame)
        return false;

    Settings* settings = frame->settings();
    if (!settings || !settings->caretBrowsingEnabled())
        return false;

    // Check text objects and paragraphs only.
    AtkObject* axObject = coreObject->wrapper();
    AtkRole role = axObject ? atk_object_get_role(axObject) : ATK_ROLE_INVALID;
    if (role != ATK_ROLE_TEXT && role != ATK_ROLE_PARAGRAPH)
        return false;

    // Finally, check whether the caret is set in the current object.
    VisibleSelection selection = coreObject->selection();
    if (!selection.isCaret())
        return false;

    return selectionBelongsToObject(coreObject, selection);
}

static void setAtkStateSetFromCoreObject(AccessibilityObject* coreObject, AtkStateSet* stateSet)
{
    AccessibilityObject* parent = coreObject->parentObject();
    bool isListBoxOption = parent && parent->isListBox();

    // Please keep the state list in alphabetical order
    if (coreObject->isChecked())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKED);

    // FIXME: isReadOnly does not seem to do the right thing for
    // controls, so check explicitly for them. In addition, because
    // isReadOnly is false for listBoxOptions, we need to add one
    // more check so that we do not present them as being "editable".
    if ((!coreObject->isReadOnly()
         || (coreObject->isControl() && coreObject->canSetValueAttribute()))
        && !isListBoxOption)
        atk_state_set_add_state(stateSet, ATK_STATE_EDITABLE);

    // FIXME: Put both ENABLED and SENSITIVE together here for now
    if (coreObject->isEnabled()) {
        atk_state_set_add_state(stateSet, ATK_STATE_ENABLED);
        atk_state_set_add_state(stateSet, ATK_STATE_SENSITIVE);
    }

    if (coreObject->canSetExpandedAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_EXPANDABLE);

    if (coreObject->isExpanded())
        atk_state_set_add_state(stateSet, ATK_STATE_EXPANDED);

    if (coreObject->canSetFocusAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    if (coreObject->isFocused() || isTextWithCaret(coreObject))
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);

    // TODO: ATK_STATE_HORIZONTAL

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);

    if (coreObject->isMultiSelectable())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

    // TODO: ATK_STATE_SELECTABLE_TEXT

    if (coreObject->canSetSelectedAttribute()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTABLE);
        // Items in focusable lists in Gtk have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on the
        // former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);
    }

    if (coreObject->isSelected()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTED);
        // Items in focusable lists in Gtk have both STATE_SELECT{ABLE,ED}
        // and STATE_FOCUS{ABLE,ED}. We'll fake the latter based on the
        // former.
        if (isListBoxOption)
            atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);
    }

    // FIXME: Group both SHOWING and VISIBLE here for now
    // Not sure how to handle this in WebKit, see bug
    // http://bugzilla.gnome.org/show_bug.cgi?id=509650 for other
    // issues with SHOWING vs VISIBLE within GTK+
    if (!coreObject->isOffScreen()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
        atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
    }

    // Mutually exclusive, so we group these two
    if (coreObject->roleValue() == TextFieldRole)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);
    else if (coreObject->roleValue() == TextAreaRole)
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);

    // TODO: ATK_STATE_SENSITIVE

    // TODO: ATK_STATE_VERTICAL

    if (coreObject->isVisited())
        atk_state_set_add_state(stateSet, ATK_STATE_VISITED);
}

static AtkStateSet* webkit_accessible_ref_state_set(AtkObject* object)
{
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_state_set(object);
    AccessibilityObject* coreObject = core(object);

    if (coreObject == fallbackObject()) {
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    // Text objects must be focusable.
    AtkRole role = atk_object_get_role(object);
    if (role == ATK_ROLE_TEXT || role == ATK_ROLE_PARAGRAPH)
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    setAtkStateSetFromCoreObject(coreObject, stateSet);
    return stateSet;
}

static AtkRelationSet* webkit_accessible_ref_relation_set(AtkObject* object)
{
    AtkRelationSet* relationSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_relation_set(object);
    AccessibilityObject* coreObject = core(object);

    setAtkRelationSetFromCoreObject(coreObject, relationSet);

    return relationSet;
}

static void webkit_accessible_init(AtkObject* object, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize)
        ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize(object, data);

    WEBKIT_ACCESSIBLE(object)->m_object = reinterpret_cast<AccessibilityObject*>(data);
}

static void webkit_accessible_finalize(GObject* object)
{
    // This is a good time to clear the return buffer.
    returnString(String());

    G_OBJECT_CLASS(webkit_accessible_parent_class)->finalize(object);
}

static void webkit_accessible_class_init(AtkObjectClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    webkit_accessible_parent_class = g_type_class_peek_parent(klass);

    gobjectClass->finalize = webkit_accessible_finalize;

    klass->initialize = webkit_accessible_init;
    klass->get_name = webkit_accessible_get_name;
    klass->get_description = webkit_accessible_get_description;
    klass->get_parent = webkit_accessible_get_parent;
    klass->get_n_children = webkit_accessible_get_n_children;
    klass->ref_child = webkit_accessible_ref_child;
    klass->get_role = webkit_accessible_get_role;
    klass->ref_state_set = webkit_accessible_ref_state_set;
    klass->get_index_in_parent = webkit_accessible_get_index_in_parent;
    klass->get_attributes = webkit_accessible_get_attributes;
    klass->ref_relation_set = webkit_accessible_ref_relation_set;
}

GType
webkit_accessible_get_type(void)
{
    static volatile gsize type_volatile = 0;

    if (g_once_init_enter(&type_volatile)) {
        static const GTypeInfo tinfo = {
            sizeof(WebKitAccessibleClass),
            (GBaseInitFunc) 0,
            (GBaseFinalizeFunc) 0,
            (GClassInitFunc) webkit_accessible_class_init,
            (GClassFinalizeFunc) 0,
            0, /* class data */
            sizeof(WebKitAccessible), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc) 0,
            0 /* value table */
        };

        GType type = g_type_register_static(ATK_TYPE_OBJECT,
                                            "WebKitAccessible", &tinfo, GTypeFlags(0));
        g_once_init_leave(&type_volatile, type);
    }

    return type_volatile;
}

// Text

static gchar* utf8Substr(const gchar* string, gint start, gint end)
{
    ASSERT(string);
    glong strLen = g_utf8_strlen(string, -1);
    if (start > strLen || end > strLen)
        return 0;
    gchar* startPtr = g_utf8_offset_to_pointer(string, start);
    gsize lenInBytes = g_utf8_offset_to_pointer(string, end + 1) -  startPtr;
    gchar* output = static_cast<gchar*>(g_malloc0(lenInBytes + 1));
    return g_utf8_strncpy(output, startPtr, end - start + 1);
}

// This function is not completely general, is it's tied to the
// internals of WebCore's text presentation.
static gchar* convertUniCharToUTF8(const UChar* characters, gint length, int from, int to)
{
    CString stringUTF8 = UTF8Encoding().encode(characters, length, QuestionMarksForUnencodables);
    gchar* utf8String = utf8Substr(stringUTF8.data(), from, to);
    if (!g_utf8_validate(utf8String, -1, 0)) {
        g_free(utf8String);
        return 0;
    }
    gsize len = strlen(utf8String);
    GString* ret = g_string_new_len(0, len);
    gchar* ptr = utf8String;

    // WebCore introduces line breaks in the text that do not reflect
    // the layout you see on the screen, replace them with spaces
    while (len > 0) {
        gint index, start;
        pango_find_paragraph_boundary(ptr, len, &index, &start);
        g_string_append_len(ret, ptr, index);
        if (index == start)
            break;
        g_string_append_c(ret, ' ');
        ptr += start;
        len -= start;
    }

    g_free(utf8String);
    return g_string_free(ret, FALSE);
}

gchar* textForRenderer(RenderObject* renderer)
{
    GString* resultText = g_string_new(0);

    if (!renderer)
        return g_string_free(resultText, FALSE);

    // For RenderBlocks, piece together the text from the RenderText objects they contain.
    for (RenderObject* object = renderer->firstChild(); object; object = object->nextSibling()) {
        if (object->isBR()) {
            g_string_append(resultText, "\n");
            continue;
        }

        RenderText* renderText;
        if (object->isText())
            renderText = toRenderText(object);
        else {
            if (object->isReplaced())
                g_string_append_unichar(resultText, objectReplacementCharacter);

            // We need to check children, if any, to consider when
            // current object is not a text object but some of its
            // children are, in order not to miss those portions of
            // text by not properly handling those situations
            if (object->firstChild())
                g_string_append(resultText, textForRenderer(object));

            continue;
        }

        InlineTextBox* box = renderText ? renderText->firstTextBox() : 0;
        while (box) {
            gchar* text = convertUniCharToUTF8(renderText->characters(), renderText->textLength(), box->start(), box->end());
            g_string_append(resultText, text);
            // Newline chars in the source result in separate text boxes, so check
            // before adding a newline in the layout. See bug 25415 comment #78.
            // If the next sibling is a BR, we'll add the newline when we examine that child.
            if (!box->nextOnLineExists() && (!object->nextSibling() || !object->nextSibling()->isBR()))
                g_string_append(resultText, "\n");
            box = box->nextTextBox();
        }
    }

    // Insert the text of the marker for list item in the right place, if present
    if (renderer->isListItem()) {
        String markerText = toRenderListItem(renderer)->markerTextWithSuffix();
        if (renderer->style()->direction() == LTR)
            g_string_prepend(resultText, markerText.utf8().data());
        else
            g_string_append(resultText, markerText.utf8().data());
    }

    return g_string_free(resultText, FALSE);
}

gchar* textForObject(AccessibilityObject* coreObject)
{
    GString* str = g_string_new(0);

    // For text controls, we can get the text line by line.
    if (coreObject->isTextControl()) {
        unsigned textLength = coreObject->textLength();
        int lineNumber = 0;
        PlainTextRange range = coreObject->doAXRangeForLine(lineNumber);
        while (range.length) {
            // When a line of text wraps in a text area, the final space is removed.
            if (range.start + range.length < textLength)
                range.length -= 1;
            String lineText = coreObject->doAXStringForRange(range);
            g_string_append(str, lineText.utf8().data());
            g_string_append(str, "\n");
            range = coreObject->doAXRangeForLine(++lineNumber);
        }
    } else if (coreObject->isAccessibilityRenderObject()) {
        GOwnPtr<gchar> rendererText(textForRenderer(coreObject->renderer()));
        g_string_append(str, rendererText.get());
    }

    return g_string_free(str, FALSE);
}

static gchar* webkit_accessible_text_get_text(AtkText* text, gint startOffset, gint endOffset)
{
    AccessibilityObject* coreObject = core(text);

    int end = endOffset;
    if (endOffset == -1) {
        end = coreObject->stringValue().length();
        if (!end)
            end = coreObject->textUnderElement().length();
    }

    String ret;
    if (coreObject->isTextControl())
        ret = coreObject->doAXStringForRange(PlainTextRange(0, endOffset));
    else {
        ret = coreObject->stringValue();
        if (!ret)
            ret = coreObject->textUnderElement();
    }

    if (!ret.length()) {
        // This can happen at least with anonymous RenderBlocks (e.g. body text amongst paragraphs)
        ret = String(textForObject(coreObject));
        if (!end)
            end = ret.length();
    }

    // Prefix a item number/bullet if needed
    if (coreObject->roleValue() == ListItemRole) {
        RenderObject* objRenderer = coreObject->renderer();
        if (objRenderer && objRenderer->isListItem()) {
            String markerText = toRenderListItem(objRenderer)->markerTextWithSuffix();
            ret = objRenderer->style()->direction() == LTR ? markerText + ret : ret + markerText;
            if (endOffset == -1)
                end += markerText.length();
        }
    }

    ret = ret.substring(startOffset, end - startOffset);
    return g_strdup(ret.utf8().data());
}

static GailTextUtil* getGailTextUtilForAtk(AtkText* textObject)
{
    GailTextUtil* gailTextUtil = gail_text_util_new();
    gail_text_util_text_setup(gailTextUtil, webkit_accessible_text_get_text(textObject, 0, -1));
    return gailTextUtil;
}

static PangoLayout* getPangoLayoutForAtk(AtkText* textObject)
{
    AccessibilityObject* coreObject = core(textObject);

    Document* document = coreObject->document();
    if (!document)
        return 0;

    HostWindow* hostWindow = document->view()->hostWindow();
    if (!hostWindow)
        return 0;
    PlatformPageClient webView = hostWindow->platformPageClient();
    if (!webView)
        return 0;

    // Create a string with the layout as it appears on the screen
    PangoLayout* layout = gtk_widget_create_pango_layout(static_cast<GtkWidget*>(webView), textForObject(coreObject));
    return layout;
}

static gchar* webkit_accessible_text_get_text_after_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AFTER_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_at_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AT_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_before_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_BEFORE_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gunichar webkit_accessible_text_get_character_at_offset(AtkText* text, gint offset)
{
    notImplemented();
    return 0;
}

static gint webkit_accessible_text_get_caret_offset(AtkText* text)
{
    // coreObject is the unignored object whose offset the caller is requesting.
    // focusedObject is the object with the caret. It is likely ignored -- unless it's a link.
    AccessibilityObject* coreObject = core(text);
    if (!coreObject->isAccessibilityRenderObject())
        return 0;

    // We need to make sure we pass a valid object as reference.
    if (coreObject->accessibilityIsIgnored())
        coreObject = coreObject->parentObjectUnignored();
    if (!coreObject)
        return 0;

    int offset;
    if (!objectFocusedAndCaretOffsetUnignored(coreObject, offset))
        return 0;

    RenderObject* renderer = coreObject->renderer();
    if (renderer && renderer->isListItem()) {
        String markerText = toRenderListItem(renderer)->markerTextWithSuffix();

        // We need to adjust the offset for the list item marker.
        offset += markerText.length();
    }

    // TODO: Verify this for RTL text.
    return offset;
}

static int baselinePositionForRenderObject(RenderObject* renderObject)
{
    // FIXME: This implementation of baselinePosition originates from RenderObject.cpp and was
    // removed in r70072. The implementation looks incorrect though, because this is not the
    // baseline of the underlying RenderObject, but of the AccessibilityRenderObject.
    const FontMetrics& fontMetrics = renderObject->firstLineStyle()->fontMetrics();
    return fontMetrics.ascent() + (renderObject->firstLineStyle()->computedLineHeight() - fontMetrics.height()) / 2;
}

static AtkAttributeSet* getAttributeSetForAccessibilityObject(const AccessibilityObject* object)
{
    if (!object->isAccessibilityRenderObject())
        return 0;

    RenderObject* renderer = object->renderer();
    RenderStyle* style = renderer->style();

    AtkAttributeSet* result = 0;
    GOwnPtr<gchar> buffer(g_strdup_printf("%i", style->fontSize()));
    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_SIZE), buffer.get());

    Color bgColor = style->visitedDependentColor(CSSPropertyBackgroundColor);
    if (bgColor.isValid()) {
        buffer.set(g_strdup_printf("%i,%i,%i",
                                   bgColor.red(), bgColor.green(), bgColor.blue()));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_BG_COLOR), buffer.get());
    }

    Color fgColor = style->visitedDependentColor(CSSPropertyColor);
    if (fgColor.isValid()) {
        buffer.set(g_strdup_printf("%i,%i,%i",
                                   fgColor.red(), fgColor.green(), fgColor.blue()));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_FG_COLOR), buffer.get());
    }

    int baselinePosition;
    bool includeRise = true;
    switch (style->verticalAlign()) {
    case SUB:
        baselinePosition = -1 * baselinePositionForRenderObject(renderer);
        break;
    case SUPER:
        baselinePosition = baselinePositionForRenderObject(renderer);
        break;
    case BASELINE:
        baselinePosition = 0;
        break;
    default:
        includeRise = false;
        break;
    }

    if (includeRise) {
        buffer.set(g_strdup_printf("%i", baselinePosition));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_RISE), buffer.get());
    }

    if (!style->textIndent().isUndefined()) {
        int indentation = style->textIndent().calcValue(object->size().width());
        buffer.set(g_strdup_printf("%i", indentation));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_INDENT), buffer.get());
    }

    String fontFamilyName = style->font().family().family().string();
    if (fontFamilyName.left(8) == "-webkit-")
        fontFamilyName = fontFamilyName.substring(8);

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_FAMILY_NAME), fontFamilyName.utf8().data());

    int fontWeight = -1;
    switch (style->font().weight()) {
    case FontWeight100:
        fontWeight = 100;
        break;
    case FontWeight200:
        fontWeight = 200;
        break;
    case FontWeight300:
        fontWeight = 300;
        break;
    case FontWeight400:
        fontWeight = 400;
        break;
    case FontWeight500:
        fontWeight = 500;
        break;
    case FontWeight600:
        fontWeight = 600;
        break;
    case FontWeight700:
        fontWeight = 700;
        break;
    case FontWeight800:
        fontWeight = 800;
        break;
    case FontWeight900:
        fontWeight = 900;
    }
    if (fontWeight > 0) {
        buffer.set(g_strdup_printf("%i", fontWeight));
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_WEIGHT), buffer.get());
    }

    switch (style->textAlign()) {
    case TAAUTO:
    case TASTART:
    case TAEND:
        break;
    case LEFT:
    case WEBKIT_LEFT:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "left");
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "right");
        break;
    case CENTER:
    case WEBKIT_CENTER:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "center");
        break;
    case JUSTIFY:
        result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_JUSTIFICATION), "fill");
    }

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_UNDERLINE), (style->textDecoration() & UNDERLINE) ? "single" : "none");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_STYLE), style->font().italic() ? "italic" : "normal");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_STRIKETHROUGH), (style->textDecoration() & LINE_THROUGH) ? "true" : "false");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_INVISIBLE), (style->visibility() == HIDDEN) ? "true" : "false");

    result = addToAtkAttributeSet(result, atk_text_attribute_get_name(ATK_TEXT_ATTR_EDITABLE), object->isReadOnly() ? "false" : "true");

    return result;
}

static gint compareAttribute(const AtkAttribute* a, const AtkAttribute* b)
{
    return g_strcmp0(a->name, b->name) || g_strcmp0(a->value, b->value);
}

// Returns an AtkAttributeSet with the elements of a1 which are either
// not present or different in a2. Neither a1 nor a2 should be used
// after calling this function.
static AtkAttributeSet* attributeSetDifference(AtkAttributeSet* a1, AtkAttributeSet* a2)
{
    if (!a2)
        return a1;

    AtkAttributeSet* i = a1;
    AtkAttributeSet* found;
    AtkAttributeSet* toDelete = 0;

    while (i) {
        found = g_slist_find_custom(a2, i->data, (GCompareFunc)compareAttribute);
        if (found) {
            AtkAttributeSet* t = i->next;
            toDelete = g_slist_prepend(toDelete, i->data);
            a1 = g_slist_delete_link(a1, i);
            i = t;
        } else
            i = i->next;
    }

    atk_attribute_set_free(a2);
    atk_attribute_set_free(toDelete);
    return a1;
}

static guint accessibilityObjectLength(const AccessibilityObject* object)
{
    // Non render objects are not taken into account
    if (!object->isAccessibilityRenderObject())
        return 0;

    // For those objects implementing the AtkText interface we use the
    // well known API to always get the text in a consistent way
    AtkObject* atkObj = ATK_OBJECT(object->wrapper());
    if (ATK_IS_TEXT(atkObj)) {
        GOwnPtr<gchar> text(webkit_accessible_text_get_text(ATK_TEXT(atkObj), 0, -1));
        return g_utf8_strlen(text.get(), -1);
    }

    // Even if we don't expose list markers to Assistive
    // Technologies, we need to have a way to measure their length
    // for those cases when it's needed to take it into account
    // separately (as in getAccessibilityObjectForOffset)
    RenderObject* renderer = object->renderer();
    if (renderer && renderer->isListMarker()) {
        RenderListMarker* marker = toRenderListMarker(renderer);
        return marker->text().length() + marker->suffix().length();
    }

    return 0;
}

static const AccessibilityObject* getAccessibilityObjectForOffset(const AccessibilityObject* object, guint offset, gint* startOffset, gint* endOffset)
{
    const AccessibilityObject* result;
    guint length = accessibilityObjectLength(object);
    if (length > offset) {
        *startOffset = 0;
        *endOffset = length;
        result = object;
    } else {
        *startOffset = -1;
        *endOffset = -1;
        result = 0;
    }

    if (!object->firstChild())
        return result;

    AccessibilityObject* child = object->firstChild();
    guint currentOffset = 0;
    guint childPosition = 0;
    while (child && currentOffset <= offset) {
        guint childLength = accessibilityObjectLength(child);
        currentOffset = childLength + childPosition;
        if (currentOffset > offset) {
            gint childStartOffset;
            gint childEndOffset;
            const AccessibilityObject* grandChild = getAccessibilityObjectForOffset(child, offset-childPosition,  &childStartOffset, &childEndOffset);
            if (childStartOffset >= 0) {
                *startOffset = childStartOffset + childPosition;
                *endOffset = childEndOffset + childPosition;
                result = grandChild;
            }
        } else {
            childPosition += childLength;
            child = child->nextSibling();
        }
    }
    return result;
}

static AtkAttributeSet* getRunAttributesFromAccesibilityObject(const AccessibilityObject* element, gint offset, gint* startOffset, gint* endOffset)
{
    const AccessibilityObject *child = getAccessibilityObjectForOffset(element, offset, startOffset, endOffset);
    if (!child) {
        *startOffset = -1;
        *endOffset = -1;
        return 0;
    }

    AtkAttributeSet* defaultAttributes = getAttributeSetForAccessibilityObject(element);
    AtkAttributeSet* childAttributes = getAttributeSetForAccessibilityObject(child);

    return attributeSetDifference(childAttributes, defaultAttributes);
}

static AtkAttributeSet* webkit_accessible_text_get_run_attributes(AtkText* text, gint offset, gint* startOffset, gint* endOffset)
{
    AccessibilityObject* coreObject = core(text);
    AtkAttributeSet* result;

    if (!coreObject) {
        *startOffset = 0;
        *endOffset = atk_text_get_character_count(text);
        return 0;
    }

    if (offset == -1)
        offset = atk_text_get_caret_offset(text);

    result = getRunAttributesFromAccesibilityObject(coreObject, offset, startOffset, endOffset);

    if (*startOffset < 0) {
        *startOffset = offset;
        *endOffset = offset;
    }

    return result;
}

static AtkAttributeSet* webkit_accessible_text_get_default_attributes(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);
    if (!coreObject || !coreObject->isAccessibilityRenderObject())
        return 0;

    return getAttributeSetForAccessibilityObject(coreObject);
}

static IntRect textExtents(AtkText* text, gint startOffset, gint length, AtkCoordType coords)
{
    gchar* textContent = webkit_accessible_text_get_text(text, startOffset, -1);
    gint textLength = g_utf8_strlen(textContent, -1);

    // The first case (endOffset of -1) should work, but seems broken for all Gtk+ apps.
    gint rangeLength = length;
    if (rangeLength < 0 || rangeLength > textLength)
        rangeLength = textLength;
    AccessibilityObject* coreObject = core(text);

    IntRect extents = coreObject->doAXBoundsForRange(PlainTextRange(startOffset, rangeLength));
    switch (coords) {
    case ATK_XY_SCREEN:
        if (Document* document = coreObject->document())
            extents = document->view()->contentsToScreen(extents);
        break;
    case ATK_XY_WINDOW:
        // No-op
        break;
    }

    return extents;
}

static void webkit_accessible_text_get_character_extents(AtkText* text, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords)
{
    IntRect extents = textExtents(text, offset, 1, coords);
    *x = extents.x();
    *y = extents.y();
    *width = extents.width();
    *height = extents.height();
}

static void webkit_accessible_text_get_range_extents(AtkText* text, gint startOffset, gint endOffset, AtkCoordType coords, AtkTextRectangle* rect)
{
    IntRect extents = textExtents(text, startOffset, endOffset - startOffset, coords);
    rect->x = extents.x();
    rect->y = extents.y();
    rect->width = extents.width();
    rect->height = extents.height();
}

static gint webkit_accessible_text_get_character_count(AtkText* text)
{
    return accessibilityObjectLength(core(text));
}

static gint webkit_accessible_text_get_offset_at_point(AtkText* text, gint x, gint y, AtkCoordType coords)
{
    // FIXME: Use the AtkCoordType
    // TODO: Is it correct to ignore range.length?
    IntPoint pos(x, y);
    PlainTextRange range = core(text)->doAXRangeForPosition(pos);
    return range.start;
}

static void getSelectionOffsetsForObject(AccessibilityObject* coreObject, VisibleSelection& selection, gint& startOffset, gint& endOffset)
{
    if (!coreObject->isAccessibilityRenderObject())
        return;

    // Early return if the selection doesn't affect the selected node.
    if (!selectionBelongsToObject(coreObject, selection))
        return;

    // We need to find the exact start and end positions in the
    // selected node that intersects the selection, to later on get
    // the right values for the effective start and end offsets.
    ExceptionCode ec = 0;
    Position nodeRangeStart;
    Position nodeRangeEnd;
    Node* node = coreObject->node();
    RefPtr<Range> selRange = selection.toNormalizedRange();

    // If the selection affects the selected node and its first
    // possible position is also in the selection, we must set
    // nodeRangeStart to that position, otherwise to the selection's
    // start position (it would belong to the node anyway).
    Node* firstLeafNode = node->firstDescendant();
    if (selRange->isPointInRange(firstLeafNode, 0, ec))
        nodeRangeStart = firstPositionInOrBeforeNode(firstLeafNode);
    else
        nodeRangeStart = selRange->startPosition();

    // If the selection affects the selected node and its last
    // possible position is also in the selection, we must set
    // nodeRangeEnd to that position, otherwise to the selection's
    // end position (it would belong to the node anyway).
    Node* lastLeafNode = node->lastDescendant();
    if (selRange->isPointInRange(lastLeafNode, lastOffsetInNode(lastLeafNode), ec))
        nodeRangeEnd = lastPositionInOrAfterNode(lastLeafNode);
    else
        nodeRangeEnd = selRange->endPosition();

    // Calculate position of the selected range inside the object.
    Position parentFirstPosition = firstPositionInOrBeforeNode(node);
    RefPtr<Range> rangeInParent = Range::create(node->document(), parentFirstPosition, nodeRangeStart);

    // Set values for start and end offsets.
    startOffset = TextIterator::rangeLength(rangeInParent.get(), true);

    // We need to adjust the offsets for the list item marker.
    RenderObject* renderer = coreObject->renderer();
    if (renderer && renderer->isListItem()) {
        String markerText = toRenderListItem(renderer)->markerTextWithSuffix();
        startOffset += markerText.length();
    }

    RefPtr<Range> nodeRange = Range::create(node->document(), nodeRangeStart, nodeRangeEnd);
    endOffset = startOffset + TextIterator::rangeLength(nodeRange.get(), true);
}

static gint webkit_accessible_text_get_n_selections(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // Only range selections are needed for the purpose of this method
    if (!selection.isRange())
        return 0;

    // We don't support multiple selections for now, so there's only
    // two possibilities
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    return selectionBelongsToObject(coreObject, selection) ? 1 : 0;
}

static gchar* webkit_accessible_text_get_selection(AtkText* text, gint selectionNum, gint* startOffset, gint* endOffset)
{
    // Default values, unless the contrary is proved
    *startOffset = *endOffset = 0;

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return 0;

    // Get the offsets of the selection for the selected object
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();
    getSelectionOffsetsForObject(coreObject, selection, *startOffset, *endOffset);

    // Return 0 instead of "", as that's the expected result for
    // this AtkText method when there's no selection
    if (*startOffset == *endOffset)
        return 0;

    return webkit_accessible_text_get_text(text, *startOffset, *endOffset);
}

static gboolean webkit_accessible_text_add_selection(AtkText* text, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_selection(AtkText* text, gint selectionNum, gint startOffset, gint endOffset)
{
    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return FALSE;

    AccessibilityObject* coreObject = core(text);
    if (!coreObject->isAccessibilityRenderObject())
        return FALSE;

    // Consider -1 and out-of-bound values and correct them to length
    gint textCount = webkit_accessible_text_get_character_count(text);
    if (startOffset < 0 || startOffset > textCount)
        startOffset = textCount;
    if (endOffset < 0 || endOffset > textCount)
        endOffset = textCount;

    // We need to adjust the offsets for the list item marker.
    RenderObject* renderer = coreObject->renderer();
    if (renderer && renderer->isListItem()) {
        String markerText = toRenderListItem(renderer)->markerTextWithSuffix();
        int markerLength = markerText.length();
        if (startOffset < markerLength || endOffset < markerLength)
            return FALSE;

        startOffset -= markerLength;
        endOffset -= markerLength;
    }

    PlainTextRange textRange(startOffset, endOffset - startOffset);
    VisiblePositionRange range = coreObject->visiblePositionRangeForRange(textRange);
    if (range.isNull())
        return FALSE;

    coreObject->setSelectedVisiblePositionRange(range);
    return TRUE;
}

static gboolean webkit_accessible_text_remove_selection(AtkText* text, gint selectionNum)
{
    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    if (selectionNum)
        return FALSE;

    // Do nothing if current selection doesn't belong to the object
    if (!webkit_accessible_text_get_n_selections(text))
        return FALSE;

    // Set a new 0-sized selection to the caret position, in order
    // to simulate selection removal (GAIL style)
    gint caretOffset = webkit_accessible_text_get_caret_offset(text);
    return webkit_accessible_text_set_selection(text, selectionNum, caretOffset, caretOffset);
}

static gboolean webkit_accessible_text_set_caret_offset(AtkText* text, gint offset)
{
    AccessibilityObject* coreObject = core(text);

    if (!coreObject->isAccessibilityRenderObject())
        return FALSE;

    RenderObject* renderer = coreObject->renderer();
    if (renderer && renderer->isListItem()) {
        String markerText = toRenderListItem(renderer)->markerTextWithSuffix();
        int markerLength = markerText.length();
        if (offset < markerLength)
            return FALSE;

        // We need to adjust the offset for list items.
        offset -= markerLength;
    }

    PlainTextRange textRange(offset, 0);
    VisiblePositionRange range = coreObject->visiblePositionRangeForRange(textRange);
    if (range.isNull())
        return FALSE;

    coreObject->setSelectedVisiblePositionRange(range);
    return TRUE;
}

static void atk_text_interface_init(AtkTextIface* iface)
{
    iface->get_text = webkit_accessible_text_get_text;
    iface->get_text_after_offset = webkit_accessible_text_get_text_after_offset;
    iface->get_text_at_offset = webkit_accessible_text_get_text_at_offset;
    iface->get_character_at_offset = webkit_accessible_text_get_character_at_offset;
    iface->get_text_before_offset = webkit_accessible_text_get_text_before_offset;
    iface->get_caret_offset = webkit_accessible_text_get_caret_offset;
    iface->get_run_attributes = webkit_accessible_text_get_run_attributes;
    iface->get_default_attributes = webkit_accessible_text_get_default_attributes;
    iface->get_character_extents = webkit_accessible_text_get_character_extents;
    iface->get_range_extents = webkit_accessible_text_get_range_extents;
    iface->get_character_count = webkit_accessible_text_get_character_count;
    iface->get_offset_at_point = webkit_accessible_text_get_offset_at_point;
    iface->get_n_selections = webkit_accessible_text_get_n_selections;
    iface->get_selection = webkit_accessible_text_get_selection;

    // set methods
    iface->add_selection = webkit_accessible_text_add_selection;
    iface->remove_selection = webkit_accessible_text_remove_selection;
    iface->set_selection = webkit_accessible_text_set_selection;
    iface->set_caret_offset = webkit_accessible_text_set_caret_offset;
}

// Table

static AccessibilityTableCell* cell(AtkTable* table, guint row, guint column)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->cellForColumnAndRow(column, row);
    return 0;
}

static gint cellIndex(AccessibilityTableCell* axCell, AccessibilityTable* axTable)
{
    // Calculate the cell's index as if we had a traditional Gtk+ table in
    // which cells are all direct children of the table, arranged row-first.
    AccessibilityObject::AccessibilityChildrenVector allCells;
    axTable->cells(allCells);
    AccessibilityObject::AccessibilityChildrenVector::iterator position;
    position = std::find(allCells.begin(), allCells.end(), axCell);
    if (position == allCells.end())
        return -1;
    return position - allCells.begin();
}

static AccessibilityTableCell* cellAtIndex(AtkTable* table, gint index)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        AccessibilityObject::AccessibilityChildrenVector allCells;
        static_cast<AccessibilityTable*>(accTable)->cells(allCells);
        if (0 <= index && static_cast<unsigned>(index) < allCells.size()) {
            AccessibilityObject* accCell = allCells.at(index).get();
            return static_cast<AccessibilityTableCell*>(accCell);
        }
    }
    return 0;
}

static AtkObject* webkit_accessible_table_ref_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (!axCell)
        return 0;
    return axCell->wrapper();
}

static gint webkit_accessible_table_get_index_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    AccessibilityTable* axTable = static_cast<AccessibilityTable*>(core(table));
    return cellIndex(axCell, axTable);
}

static gint webkit_accessible_table_get_column_at_index(AtkTable* table, gint index)
{
    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell) {
        pair<int, int> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.first;
    }
    return -1;
}

static gint webkit_accessible_table_get_row_at_index(AtkTable* table, gint index)
{
    AccessibilityTableCell* axCell = cellAtIndex(table, index);
    if (axCell) {
        pair<int, int> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.first;
    }
    return -1;
}

static gint webkit_accessible_table_get_n_columns(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->columnCount();
    return 0;
}

static gint webkit_accessible_table_get_n_rows(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject())
        return static_cast<AccessibilityTable*>(accTable)->rowCount();
    return 0;
}

static gint webkit_accessible_table_get_column_extent_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        pair<int, int> columnRange;
        axCell->columnIndexRange(columnRange);
        return columnRange.second;
    }
    return 0;
}

static gint webkit_accessible_table_get_row_extent_at(AtkTable* table, gint row, gint column)
{
    AccessibilityTableCell* axCell = cell(table, row, column);
    if (axCell) {
        pair<int, int> rowRange;
        axCell->rowIndexRange(rowRange);
        return rowRange.second;
    }
    return 0;
}

static AtkObject* webkit_accessible_table_get_column_header(AtkTable* table, gint column)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        AccessibilityObject::AccessibilityChildrenVector allColumnHeaders;
        static_cast<AccessibilityTable*>(accTable)->columnHeaders(allColumnHeaders);
        unsigned columnCount = allColumnHeaders.size();
        for (unsigned k = 0; k < columnCount; ++k) {
            pair<int, int> columnRange;
            AccessibilityTableCell* cell = static_cast<AccessibilityTableCell*>(allColumnHeaders.at(k).get());
            cell->columnIndexRange(columnRange);
            if (columnRange.first <= column && column < columnRange.first + columnRange.second)
                return allColumnHeaders[k]->wrapper();
        }
    }
    return 0;
}

static AtkObject* webkit_accessible_table_get_row_header(AtkTable* table, gint row)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        AccessibilityObject::AccessibilityChildrenVector allRowHeaders;
        static_cast<AccessibilityTable*>(accTable)->rowHeaders(allRowHeaders);
        unsigned rowCount = allRowHeaders.size();
        for (unsigned k = 0; k < rowCount; ++k) {
            pair<int, int> rowRange;
            AccessibilityTableCell* cell = static_cast<AccessibilityTableCell*>(allRowHeaders.at(k).get());
            cell->rowIndexRange(rowRange);
            if (rowRange.first <= row && row < rowRange.first + rowRange.second)
                return allRowHeaders[k]->wrapper();
        }
    }
    return 0;
}

static AtkObject* webkit_accessible_table_get_caption(AtkTable* table)
{
    AccessibilityObject* accTable = core(table);
    if (accTable->isAccessibilityRenderObject()) {
        Node* node = accTable->node();
        if (node && node->hasTagName(HTMLNames::tableTag)) {
            HTMLTableCaptionElement* caption = static_cast<HTMLTableElement*>(node)->caption();
            if (caption)
                return AccessibilityObject::firstAccessibleObjectFromNode(caption->renderer()->node())->wrapper();
        }
    }
    return 0;
}

static const gchar* webkit_accessible_table_get_column_description(AtkTable* table, gint column)
{
    AtkObject* columnHeader = atk_table_get_column_header(table, column);
    if (columnHeader && ATK_IS_TEXT(columnHeader))
        return webkit_accessible_text_get_text(ATK_TEXT(columnHeader), 0, -1);

    return 0;
}

static const gchar* webkit_accessible_table_get_row_description(AtkTable* table, gint row)
{
    AtkObject* rowHeader = atk_table_get_row_header(table, row);
    if (rowHeader && ATK_IS_TEXT(rowHeader))
        return webkit_accessible_text_get_text(ATK_TEXT(rowHeader), 0, -1);

    return 0;
}

static void atk_table_interface_init(AtkTableIface* iface)
{
    iface->ref_at = webkit_accessible_table_ref_at;
    iface->get_index_at = webkit_accessible_table_get_index_at;
    iface->get_column_at_index = webkit_accessible_table_get_column_at_index;
    iface->get_row_at_index = webkit_accessible_table_get_row_at_index;
    iface->get_n_columns = webkit_accessible_table_get_n_columns;
    iface->get_n_rows = webkit_accessible_table_get_n_rows;
    iface->get_column_extent_at = webkit_accessible_table_get_column_extent_at;
    iface->get_row_extent_at = webkit_accessible_table_get_row_extent_at;
    iface->get_column_header = webkit_accessible_table_get_column_header;
    iface->get_row_header = webkit_accessible_table_get_row_header;
    iface->get_caption = webkit_accessible_table_get_caption;
    iface->get_column_description = webkit_accessible_table_get_column_description;
    iface->get_row_description = webkit_accessible_table_get_row_description;
}

static void webkitAccessibleValueGetCurrentValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->valueForRange());
}

static void webkitAccessibleValueGetMaximumValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->maxValueForRange());
}

static void webkitAccessibleValueGetMinimumValue(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);
    g_value_set_double(gValue, core(value)->minValueForRange());
}

static gboolean webkitAccessibleValueSetCurrentValue(AtkValue* value, const GValue* gValue)
{
    if (!G_VALUE_HOLDS_DOUBLE(gValue) && !G_VALUE_HOLDS_INT(gValue))
        return FALSE;

    AccessibilityObject* coreObject = core(value);
    if (!coreObject->canSetValueAttribute())
        return FALSE;

    if (G_VALUE_HOLDS_DOUBLE(gValue))
        coreObject->setValue(String::number(g_value_get_double(gValue)));
    else
        coreObject->setValue(String::number(g_value_get_int(gValue)));

    return TRUE;
}

static void webkitAccessibleValueGetMinimumIncrement(AtkValue* value, GValue* gValue)
{
    memset(gValue,  0, sizeof(GValue));
    g_value_init(gValue, G_TYPE_DOUBLE);

    // There's not such a thing in the WAI-ARIA specification, thus return zero.
    g_value_set_double(gValue, 0.0);
}

static void atkValueInterfaceInit(AtkValueIface* iface)
{
    iface->get_current_value = webkitAccessibleValueGetCurrentValue;
    iface->get_maximum_value = webkitAccessibleValueGetMaximumValue;
    iface->get_minimum_value = webkitAccessibleValueGetMinimumValue;
    iface->set_current_value = webkitAccessibleValueSetCurrentValue;
    iface->get_minimum_increment = webkitAccessibleValueGetMinimumIncrement;
}

static const GInterfaceInfo AtkInterfacesInitFunctions[] = {
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleActionInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleSelectionInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleEditableTextInterfaceInit), 0, 0},
    {(GInterfaceInitFunc)atk_text_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleComponentInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleImageInterfaceInit), 0, 0},
    {(GInterfaceInitFunc)atk_table_interface_init,
     (GInterfaceFinalizeFunc) 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleHypertextInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleHyperlinkImplInterfaceInit), 0, 0},
    {reinterpret_cast<GInterfaceInitFunc>(webkitAccessibleDocumentInterfaceInit), 0, 0},
    {(GInterfaceInitFunc)atkValueInterfaceInit,
     (GInterfaceFinalizeFunc) 0, 0}
};

enum WAIType {
    WAI_ACTION,
    WAI_SELECTION,
    WAI_EDITABLE_TEXT,
    WAI_TEXT,
    WAI_COMPONENT,
    WAI_IMAGE,
    WAI_TABLE,
    WAI_HYPERTEXT,
    WAI_HYPERLINK,
    WAI_DOCUMENT,
    WAI_VALUE,
};

static GType GetAtkInterfaceTypeFromWAIType(WAIType type)
{
    switch (type) {
    case WAI_ACTION:
        return ATK_TYPE_ACTION;
    case WAI_SELECTION:
        return ATK_TYPE_SELECTION;
    case WAI_EDITABLE_TEXT:
        return ATK_TYPE_EDITABLE_TEXT;
    case WAI_TEXT:
        return ATK_TYPE_TEXT;
    case WAI_COMPONENT:
        return ATK_TYPE_COMPONENT;
    case WAI_IMAGE:
        return ATK_TYPE_IMAGE;
    case WAI_TABLE:
        return ATK_TYPE_TABLE;
    case WAI_HYPERTEXT:
        return ATK_TYPE_HYPERTEXT;
    case WAI_HYPERLINK:
        return ATK_TYPE_HYPERLINK_IMPL;
    case WAI_DOCUMENT:
        return ATK_TYPE_DOCUMENT;
    case WAI_VALUE:
        return ATK_TYPE_VALUE;
    }

    return G_TYPE_INVALID;
}

static guint16 getInterfaceMaskFromObject(AccessibilityObject* coreObject)
{
    guint16 interfaceMask = 0;

    // Component interface is always supported
    interfaceMask |= 1 << WAI_COMPONENT;

    AccessibilityRole role = coreObject->roleValue();

    // Action
    // As the implementation of the AtkAction interface is a very
    // basic one (just relays in executing the default action for each
    // object, and only supports having one action per object), it is
    // better just to implement this interface for every instance of
    // the WebKitAccessible class and let WebCore decide what to do.
    interfaceMask |= 1 << WAI_ACTION;

    // Selection
    if (coreObject->isListBox() || coreObject->isMenuList())
        interfaceMask |= 1 << WAI_SELECTION;

    // Get renderer if available.
    RenderObject* renderer = 0;
    if (coreObject->isAccessibilityRenderObject())
        renderer = coreObject->renderer();

    // Hyperlink (links and embedded objects).
    if (coreObject->isLink() || (renderer && renderer->isReplaced()))
        interfaceMask |= 1 << WAI_HYPERLINK;

    // Text & Editable Text
    if (role == StaticTextRole || coreObject->isMenuListOption())
        interfaceMask |= 1 << WAI_TEXT;
    else {
        if (coreObject->isTextControl()) {
            interfaceMask |= 1 << WAI_TEXT;
            if (!coreObject->isReadOnly())
                interfaceMask |= 1 << WAI_EDITABLE_TEXT;
        } else {
            if (role != TableRole) {
                interfaceMask |= 1 << WAI_HYPERTEXT;
                if (renderer && renderer->childrenInline())
                    interfaceMask |= 1 << WAI_TEXT;
            }

            // Add the TEXT interface for list items whose
            // first accessible child has a text renderer
            if (role == ListItemRole) {
                AccessibilityObject::AccessibilityChildrenVector children = coreObject->children();
                if (children.size()) {
                    AccessibilityObject* axRenderChild = children.at(0).get();
                    interfaceMask |= getInterfaceMaskFromObject(axRenderChild);
                }
            }
        }
    }

    // Image
    if (coreObject->isImage())
        interfaceMask |= 1 << WAI_IMAGE;

    // Table
    if (role == TableRole)
        interfaceMask |= 1 << WAI_TABLE;

    // Document
    if (role == WebAreaRole)
        interfaceMask |= 1 << WAI_DOCUMENT;

    // Value
    if (role == SliderRole)
        interfaceMask |= 1 << WAI_VALUE;

    return interfaceMask;
}

static const char* getUniqueAccessibilityTypeName(guint16 interfaceMask)
{
#define WAI_TYPE_NAME_LEN (30) /* Enough for prefix + 5 hex characters (max) */
    static char name[WAI_TYPE_NAME_LEN + 1];

    g_sprintf(name, "WAIType%x", interfaceMask);
    name[WAI_TYPE_NAME_LEN] = '\0';

    return name;
}

static GType getAccessibilityTypeFromObject(AccessibilityObject* coreObject)
{
    static const GTypeInfo typeInfo = {
        sizeof(WebKitAccessibleClass),
        (GBaseInitFunc) 0,
        (GBaseFinalizeFunc) 0,
        (GClassInitFunc) 0,
        (GClassFinalizeFunc) 0,
        0, /* class data */
        sizeof(WebKitAccessible), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) 0,
        0 /* value table */
    };

    guint16 interfaceMask = getInterfaceMaskFromObject(coreObject);
    const char* atkTypeName = getUniqueAccessibilityTypeName(interfaceMask);
    GType type = g_type_from_name(atkTypeName);
    if (type)
        return type;

    type = g_type_register_static(WEBKIT_TYPE_ACCESSIBLE,
                                  atkTypeName,
                                  &typeInfo, GTypeFlags(0));
    for (guint i = 0; i < G_N_ELEMENTS(AtkInterfacesInitFunctions); i++) {
        if (interfaceMask & (1 << i))
            g_type_add_interface_static(type,
                                        GetAtkInterfaceTypeFromWAIType(static_cast<WAIType>(i)),
                                        &AtkInterfacesInitFunctions[i]);
    }

    return type;
}

WebKitAccessible* webkitAccessibleNew(AccessibilityObject* coreObject)
{
    GType type = getAccessibilityTypeFromObject(coreObject);
    AtkObject* object = static_cast<AtkObject*>(g_object_new(type, 0));

    atk_object_initialize(object, coreObject);

    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject* webkitAccessibleGetAccessibilityObject(WebKitAccessible* accessible)
{
    return accessible->m_object;
}

void webkitAccessibleDetach(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);

    if (core(accessible)->roleValue() == WebAreaRole)
        g_signal_emit_by_name(accessible, "state-change", "defunct", true);

    // We replace the WebCore AccessibilityObject with a fallback object that
    // provides default implementations to avoid repetitive null-checking after
    // detachment.
    accessible->m_object = fallbackObject();
}

AtkObject* webkitAccessibleGetFocusedElement(WebKitAccessible* accessible)
{
    if (!accessible->m_object)
        return 0;

    RefPtr<AccessibilityObject> focusedObj = accessible->m_object->focusedUIElement();
    if (!focusedObj)
        return 0;

    return focusedObj->wrapper();
}

AccessibilityObject* objectFocusedAndCaretOffsetUnignored(AccessibilityObject* referenceObject, int& offset)
{
    // Indication that something bogus has transpired.
    offset = -1;

    Document* document = referenceObject->document();
    if (!document)
        return 0;

    Node* focusedNode = referenceObject->selection().end().containerNode();
    if (!focusedNode)
        return 0;

    RenderObject* focusedRenderer = focusedNode->renderer();
    if (!focusedRenderer)
        return 0;

    AccessibilityObject* focusedObject = document->axObjectCache()->getOrCreate(focusedRenderer);
    if (!focusedObject)
        return 0;

    // Look for the actual (not ignoring accessibility) selected object.
    AccessibilityObject* firstUnignoredParent = focusedObject;
    if (firstUnignoredParent->accessibilityIsIgnored())
        firstUnignoredParent = firstUnignoredParent->parentObjectUnignored();
    if (!firstUnignoredParent)
        return 0;

    // Don't ignore links if the offset is being requested for a link.
    if (!referenceObject->isLink() && firstUnignoredParent->isLink())
        firstUnignoredParent = firstUnignoredParent->parentObjectUnignored();
    if (!firstUnignoredParent)
        return 0;

    // The reference object must either coincide with the focused
    // object being considered, or be a descendant of it.
    if (referenceObject->isDescendantOfObject(firstUnignoredParent))
        referenceObject = firstUnignoredParent;

    Node* startNode = 0;
    if (firstUnignoredParent != referenceObject || firstUnignoredParent->isTextControl()) {
        // We need to use the first child's node of the reference
        // object as the start point to calculate the caret offset
        // because we want it to be relative to the object of
        // reference, not just to the focused object (which could have
        // previous siblings which should be taken into account too).
        AccessibilityObject* axFirstChild = referenceObject->firstChild();
        if (axFirstChild)
            startNode = axFirstChild->node();
    }
    if (!startNode)
        startNode = firstUnignoredParent->node();

    VisiblePosition startPosition = VisiblePosition(positionBeforeNode(startNode), DOWNSTREAM);
    VisiblePosition endPosition = firstUnignoredParent->selection().visibleEnd();

    if (startPosition == endPosition)
        offset = 0;
    else if (!isStartOfLine(endPosition)) {
        RefPtr<Range> range = makeRange(startPosition, endPosition.previous());
        offset = TextIterator::rangeLength(range.get(), true) + 1;
    } else {
        RefPtr<Range> range = makeRange(startPosition, endPosition);
        offset = TextIterator::rangeLength(range.get(), true);
    }

    return firstUnignoredParent;
}

#endif // HAVE(ACCESSIBILITY)
