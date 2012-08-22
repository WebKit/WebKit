/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Jan Michael Alonzo
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "AccessibilityUIElement.h"

#include "WebCoreSupport/DumpRenderTreeSupportGtk.h"
#include <JavaScriptCore/JSStringRef.h>
#include <atk/atk.h>
#include <gtk/gtk.h>
#include <wtf/Assertions.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
    g_object_ref(m_element);
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : m_element(other.m_element)
{
    g_object_ref(m_element);
}

AccessibilityUIElement::~AccessibilityUIElement()
{
    g_object_unref(m_element);
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elements)
{
    // FIXME: implement
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>&)
{
    // FIXME: implement
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& children)
{
    int count = childrenCount();
    for (int i = 0; i < count; i++) {
        AtkObject* child = atk_object_ref_accessible_child(ATK_OBJECT(m_element), i);
        children.append(AccessibilityUIElement(child));
    }
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned start, unsigned end)
{
    for (unsigned i = start; i < end; i++) {
        AtkObject* child = atk_object_ref_accessible_child(ATK_OBJECT(m_element), i);
        elementVector.append(AccessibilityUIElement(child));
    }
}

int AccessibilityUIElement::rowCount()
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_TABLE(m_element));

    return atk_table_get_n_rows(ATK_TABLE(m_element));
}

int AccessibilityUIElement::columnCount()
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_TABLE(m_element));

    return atk_table_get_n_columns(ATK_TABLE(m_element));
}

int AccessibilityUIElement::childrenCount()
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_OBJECT(m_element));

    return atk_object_get_n_accessible_children(ATK_OBJECT(m_element));
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, index + 1);

    if (children.size() == 1)
        return children.at(0);

    return 0;
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{ 
    // FIXME: implement
    return 0;
}

gchar* attributeSetToString(AtkAttributeSet* attributeSet)
{
    GString* str = g_string_new(0);
    for (GSList* attributes = attributeSet; attributes; attributes = attributes->next) {
        AtkAttribute* attribute = static_cast<AtkAttribute*>(attributes->data);
        GOwnPtr<gchar> attributeData(g_strconcat(attribute->name, ":", attribute->value, NULL));
        g_string_append(str, attributeData.get());
        if (attributes->next)
            g_string_append(str, ", ");
    }

    return g_string_free(str, FALSE);
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    if (!m_element)
        return JSStringCreateWithCharacters(0, 0);

    ASSERT(ATK_IS_OBJECT(m_element));
    GOwnPtr<gchar> attributeData(attributeSetToString(atk_object_get_attributes(ATK_OBJECT(m_element))));
    return JSStringCreateWithUTF8CString(attributeData.get());
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfDocumentLinks()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_OBJECT(m_element));

    AtkObject* parent =  atk_object_get_parent(ATK_OBJECT(m_element));
    return parent ? AccessibilityUIElement(parent) : 0;
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::parameterizedAttributeNames()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::role()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));

    if (!role)
        return JSStringCreateWithCharacters(0, 0);

    const gchar* roleName = atk_role_get_name(role);
    GOwnPtr<gchar> axRole(g_strdup_printf("AXRole: %s", roleName));

    return JSStringCreateWithUTF8CString(axRole.get());
}

JSStringRef AccessibilityUIElement::subrole()
{
    return 0;
}

JSStringRef AccessibilityUIElement::roleDescription()
{
    return 0;
}

JSStringRef AccessibilityUIElement::title()
{
    const gchar* name = atk_object_get_name(ATK_OBJECT(m_element));

    if (!name)
        return JSStringCreateWithCharacters(0, 0);

    GOwnPtr<gchar> axTitle(g_strdup_printf("AXTitle: %s", name));

    return JSStringCreateWithUTF8CString(axTitle.get());
}

JSStringRef AccessibilityUIElement::description()
{
    const gchar* description = atk_object_get_description(ATK_OBJECT(m_element));

    if (!description)
        return JSStringCreateWithCharacters(0, 0);

    GOwnPtr<gchar> axDesc(g_strdup_printf("AXDescription: %s", description));

    return JSStringCreateWithUTF8CString(axDesc.get());
}

JSStringRef AccessibilityUIElement::stringValue()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::language()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::helpText() const
{
    if (!m_element)
        return JSStringCreateWithCharacters(0, 0);

    ASSERT(ATK_IS_OBJECT(m_element));

    CString helpText = DumpRenderTreeSupportGtk::accessibilityHelpText(ATK_OBJECT(m_element));
    GOwnPtr<gchar> axHelpText(g_strdup_printf("AXHelp: %s", helpText.data()));
    return JSStringCreateWithUTF8CString(axHelpText.get());
}

double AccessibilityUIElement::x()
{
    int x, y;

    atk_component_get_position(ATK_COMPONENT(m_element), &x, &y, ATK_XY_SCREEN);

    return x;
}

double AccessibilityUIElement::y()
{
    int x, y;

    atk_component_get_position(ATK_COMPONENT(m_element), &x, &y, ATK_XY_SCREEN);

    return y;
}

double AccessibilityUIElement::width()
{
    int width, height;

    atk_component_get_size(ATK_COMPONENT(m_element), &width, &height);

    return width;
}

double AccessibilityUIElement::height()
{
    int width, height;

    atk_component_get_size(ATK_COMPONENT(m_element), &width, &height);

    return height;
}

double AccessibilityUIElement::clickPointX()
{
    return 0.f;
}

double AccessibilityUIElement::clickPointY()
{
    return 0.f;
}

JSStringRef AccessibilityUIElement::orientation() const
{
    return 0;
}

double AccessibilityUIElement::intValue() const
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_current_value(ATK_VALUE(m_element), &value);
    if (!G_VALUE_HOLDS_FLOAT(&value))
        return 0.0f;
    return g_value_get_float(&value);
}

double AccessibilityUIElement::minValue()
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_minimum_value(ATK_VALUE(m_element), &value);
    if (!G_VALUE_HOLDS_FLOAT(&value))
        return 0.0f;
    return g_value_get_float(&value);
}

double AccessibilityUIElement::maxValue()
{
    GValue value = { 0, { { 0 } } };

    if (!ATK_IS_VALUE(m_element))
        return 0.0f;

    atk_value_get_maximum_value(ATK_VALUE(m_element), &value);
    if (!G_VALUE_HOLDS_FLOAT(&value))
        return 0.0f;
    return g_value_get_float(&value);
}

JSStringRef AccessibilityUIElement::valueDescription()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

static bool checkElementState(PlatformUIElement element, AtkStateType stateType)
{
    if (!ATK_IS_OBJECT(element))
         return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(element)));
    return atk_state_set_contains_state(stateSet.get(), stateType);
}

bool AccessibilityUIElement::isEnabled()
{
    return checkElementState(m_element, ATK_STATE_ENABLED);
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    // FIXME: implement
    return 0;
}

bool AccessibilityUIElement::isActionSupported(JSStringRef action)
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    if (!ATK_IS_OBJECT(m_element))
        return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(m_element)));
    gboolean isFocused = atk_state_set_contains_state(stateSet.get(), ATK_STATE_FOCUSED);

    return isFocused;
}

bool AccessibilityUIElement::isSelected() const
{
    return checkElementState(m_element, ATK_STATE_SELECTED);
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    // FIXME: implement
    return 0;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return false;
}
 
JSStringRef AccessibilityUIElement::ariaDropEffects() const
{   
    return 0; 
}

bool AccessibilityUIElement::isExpanded() const
{
    if (!ATK_IS_OBJECT(m_element))
        return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(m_element)));
    gboolean isExpanded = atk_state_set_contains_state(stateSet.get(), ATK_STATE_EXPANDED);

    return isExpanded;
}

bool AccessibilityUIElement::isChecked() const
{
    if (!ATK_IS_OBJECT(m_element))
        return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(m_element)));
    gboolean isChecked = atk_state_set_contains_state(stateSet.get(), ATK_STATE_CHECKED);

    return isChecked;
}

JSStringRef AccessibilityUIElement::attributesOfColumnHeaders()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRowHeaders()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfColumns()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRows()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfVisibleCells()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfHeader()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::indexInTable()
{
    // FIXME: implement
    return 0;
}

static JSStringRef indexRangeInTable(PlatformUIElement element, bool isRowRange)
{
    GOwnPtr<gchar> rangeString(g_strdup("{0, 0}"));

    if (!element)
        return JSStringCreateWithUTF8CString(rangeString.get());

    ASSERT(ATK_IS_OBJECT(element));

    AtkObject* axTable = atk_object_get_parent(ATK_OBJECT(element));
    if (!axTable || !ATK_IS_TABLE(axTable))
        return JSStringCreateWithUTF8CString(rangeString.get());

    // Look for the cell in the table.
    gint indexInParent = atk_object_get_index_in_parent(ATK_OBJECT(element));
    if (indexInParent == -1)
        return JSStringCreateWithUTF8CString(rangeString.get());

    int row = -1;
    int column = -1;
    row = atk_table_get_row_at_index(ATK_TABLE(axTable), indexInParent);
    column = atk_table_get_column_at_index(ATK_TABLE(axTable), indexInParent);

    // Get the actual values, if row and columns are valid values.
    if (row != -1 && column != -1) {
        int base = 0;
        int length = 0;
        if (isRowRange) {
            base = row;
            length = atk_table_get_row_extent_at(ATK_TABLE(axTable), row, column);
        } else {
            base = column;
            length = atk_table_get_column_extent_at(ATK_TABLE(axTable), row, column);
        }
        rangeString.set(g_strdup_printf("{%d, %d}", base, length));
    }

    return JSStringCreateWithUTF8CString(rangeString.get());
}

JSStringRef AccessibilityUIElement::rowIndexRange()
{
    // Range in table for rows.
    return indexRangeInTable(m_element, true);
}

JSStringRef AccessibilityUIElement::columnIndexRange()
{
    // Range in table for columns.
    return indexRangeInTable(m_element, false);
}

int AccessibilityUIElement::lineForIndex(int)
{
    // FIXME: implement
    return 0;
}

JSStringRef AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::stringForRange(unsigned, unsigned) 
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
} 

JSStringRef AccessibilityUIElement::attributedStringForRange(unsigned, unsigned)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    // FIXME: implement
    return false;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(AccessibilityUIElement* startElement, bool isDirectionNext, JSStringRef searchKey, JSStringRef searchText)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned column, unsigned row)
{
    if (!m_element)
        return 0;

    ASSERT(ATK_IS_TABLE(m_element));

    AtkObject* foundCell = atk_table_ref_at(ATK_TABLE(m_element), row, column);
    return foundCell ? AccessibilityUIElement(foundCell) : 0;
}

JSStringRef AccessibilityUIElement::selectedTextRange()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    // FIXME: implement
}

JSStringRef AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return 0.0f;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

static void alterCurrentValue(PlatformUIElement element, int factor)
{
    if (!element)
        return;

    ASSERT(ATK_IS_VALUE(element));

    GValue currentValue = G_VALUE_INIT;
    atk_value_get_current_value(ATK_VALUE(element), &currentValue);

    GValue increment = G_VALUE_INIT;
    atk_value_get_minimum_increment(ATK_VALUE(element), &increment);

    GValue newValue = G_VALUE_INIT;
    g_value_init(&newValue, G_TYPE_FLOAT);

    g_value_set_float(&newValue, g_value_get_float(&currentValue) + factor * g_value_get_float(&increment));
    atk_value_set_current_value(ATK_VALUE(element), &newValue);

    g_value_unset(&newValue);
    g_value_unset(&increment);
    g_value_unset(&currentValue);
}

void AccessibilityUIElement::increment()
{
    alterCurrentValue(m_element, 1);
}

void AccessibilityUIElement::decrement()
{
    alterCurrentValue(m_element, -1);
}

void AccessibilityUIElement::press()
{
    if (!m_element)
        return;

    ASSERT(ATK_IS_OBJECT(m_element));

    if (!ATK_IS_ACTION(m_element))
        return;

    // Only one action per object is supported so far.
    atk_action_do_action(ATK_ACTION(m_element), 0);
}

void AccessibilityUIElement::showMenu()
{
    // FIXME: implement
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    return 0;
}

JSStringRef AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::documentEncoding()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));
    if (role != ATK_ROLE_DOCUMENT_FRAME)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element), "Encoding"));
}

JSStringRef AccessibilityUIElement::documentURI()
{
    AtkRole role = atk_object_get_role(ATK_OBJECT(m_element));
    if (role != ATK_ROLE_DOCUMENT_FRAME)
        return JSStringCreateWithCharacters(0, 0);

    return JSStringCreateWithUTF8CString(atk_document_get_attribute_value(ATK_DOCUMENT(m_element), "URI"));
}

JSStringRef AccessibilityUIElement::url()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    // FIXME: implement
    return false;
}

void AccessibilityUIElement::removeNotificationListener()
{
    // FIXME: implement
}

bool AccessibilityUIElement::isFocusable() const
{
    if (!ATK_IS_OBJECT(m_element))
        return false;

    GRefPtr<AtkStateSet> stateSet = adoptGRef(atk_object_ref_state_set(ATK_OBJECT(m_element)));
    gboolean isFocusable = atk_state_set_contains_state(stateSet.get(), ATK_STATE_FOCUSABLE);

    return isFocusable;
}

bool AccessibilityUIElement::isSelectable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    // FIXME: implement
    return false;
}

void AccessibilityUIElement::takeFocus()
{
    // FIXME: implement
}

void AccessibilityUIElement::takeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::addSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::removeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    // FIXME: implement
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    // FIXME: implement
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    // FIXME: implement
}
