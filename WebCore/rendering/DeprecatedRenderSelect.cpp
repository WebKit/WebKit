/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "DeprecatedRenderSelect.h"

#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "PopUpButton.h"

using std::min;

namespace WebCore {

using namespace HTMLNames;

DeprecatedRenderSelect::DeprecatedRenderSelect(HTMLSelectElement* element)
    : RenderFormElement(element)
    , m_size(element->size())
    , m_multiple(element->multiple())
    , m_selectionChanged(true)
    , m_ignoreSelectEvents(false)
    , m_optionsChanged(true)
{
    setWidget(createListBox());
}

void DeprecatedRenderSelect::setWidgetWritingDirection()
{
    TextDirection d = style()->direction() == RTL ? RTL : LTR;
    static_cast<ListBox*>(m_widget)->setWritingDirection(d);
}

void DeprecatedRenderSelect::setStyle(RenderStyle* s)
{
    RenderFormElement::setStyle(s);
    setWidgetWritingDirection();
}

void DeprecatedRenderSelect::updateFromElement()
{
    m_ignoreSelectEvents = true;

    // change widget type
    bool oldMultiple = m_multiple;
    m_multiple = static_cast<HTMLSelectElement*>(node())->multiple();

    if (oldMultiple != m_multiple) {
        static_cast<ListBox*>(m_widget)->setSelectionMode(m_multiple ? ListBox::Extended : ListBox::Single);
        m_selectionChanged = true;
        m_optionsChanged = true;
    }

    // update contents listbox/combobox based on options in m_element
    if (m_optionsChanged) {
        static_cast<HTMLSelectElement*>(node())->recalcListItems();
        const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->listItems();
        int listIndex;

        static_cast<ListBox*>(m_widget)->clear();
        
        bool groupEnabled = true;
        for (listIndex = 0; listIndex < int(listItems.size()); listIndex++) {
            if (listItems[listIndex]->hasTagName(optgroupTag)) {
                HTMLOptGroupElement* optgroupElement = static_cast<HTMLOptGroupElement*>(listItems[listIndex]);
                DeprecatedString label = optgroupElement->getAttribute(labelAttr).deprecatedString();
                label.replace('\\', backslashAsCurrencySymbol());
                
                // In WinIE, an optgroup can't start or end with whitespace (other than the indent
                // we give it).  We match this behavior.
                label = label.stripWhiteSpace();
                // We want to collapse our whitespace too.  This will match other browsers.
                label = label.simplifyWhiteSpace();

                groupEnabled = optgroupElement->isEnabled();
                
                static_cast<ListBox*>(m_widget)->appendGroupLabel(label, groupEnabled);

            } else if (listItems[listIndex]->hasTagName(optionTag)) {
                HTMLOptionElement* optionElement = static_cast<HTMLOptionElement*>(listItems[listIndex]);
                DeprecatedString itemText = optionElement->text().deprecatedString();
                if (itemText.isEmpty())
                    itemText = optionElement->getAttribute(labelAttr).deprecatedString();
                
                itemText.replace('\\', backslashAsCurrencySymbol());

                // In WinIE, leading and trailing whitespace is ignored in options. We match this behavior.
                itemText = itemText.stripWhiteSpace();
                // We want to collapse our whitespace too.  This will match other browsers.
                itemText = itemText.simplifyWhiteSpace();
                
                if (listItems[listIndex]->parentNode()->hasTagName(optgroupTag))
                    itemText.prepend("    ");

                static_cast<ListBox*>(m_widget)->appendItem(itemText, groupEnabled && optionElement->isEnabled());
            } else
                ASSERT(false);
            m_selectionChanged = true;
        }
        static_cast<ListBox*>(m_widget)->doneAppendingItems();
        setNeedsLayoutAndMinMaxRecalc();
        m_optionsChanged = false;
    }

    // update selection
    if (m_selectionChanged)
        updateSelection();

    m_ignoreSelectEvents = false;

    RenderFormElement::updateFromElement();
}

short DeprecatedRenderSelect::baselinePosition(bool f, bool isRootLineBox) const
{
    // FIXME: Should get the hardcoded constant of 7 by calling a ListBox function,
    // as we do for other widget classes.
    return RenderWidget::baselinePosition(f, isRootLineBox) - 7;
}

void DeprecatedRenderSelect::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());

    if (m_optionsChanged)
        updateFromElement();

    // ### ugly HACK FIXME!!!
    setMinMaxKnown();
    layoutIfNeeded();
    setNeedsLayoutAndMinMaxRecalc();
    // ### end FIXME

    RenderFormElement::calcMinMaxWidth();
}

void DeprecatedRenderSelect::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    // ### maintain selection properly between type/size changes, and work
    // out how to handle multiselect->singleselect (probably just select
    // first selected one)

    // calculate size
    ListBox* w = static_cast<ListBox*>(m_widget);


    int size = m_size;
    // check if multiple and size was not given or invalid
    // Internet Exploder sets size to min(number of elements, 4)
    // Netscape seems to simply set it to "number of elements"
    // the average of that is IMHO min(number of elements, 10)
    // so I did that ;-)
    if (size < 1)
        size = min(static_cast<ListBox*>(m_widget)->count(), 10U);

    // Let the widget tell us how big it wants to be.
    IntSize s(w->sizeForNumberOfLines(size));
    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());

    RenderFormElement::layout();

    // and now disable the widget in case there is no <option> given
    const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->listItems();

    bool foundOption = false;
    for (unsigned i = 0; i < listItems.size() && !foundOption; i++)
        foundOption = (listItems[i]->hasTagName(optionTag));

    m_widget->setEnabled(foundOption && ! static_cast<HTMLSelectElement*>(node())->disabled());
}

void DeprecatedRenderSelect::selectionChanged(Widget*)
{
    if (m_ignoreSelectEvents)
        return;

    // don't use listItems() here as we have to avoid recalculations - changing the
    // option list will make use update options not in the way the user expects them
    const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->m_listItems;
    int j = 0;
    unsigned size = listItems.size();
    for (unsigned i = 0; i < size; i++) {
        // don't use setSelected() here because it will cause us to be called
        // again with updateSelection.
        if (listItems[i]->hasTagName(optionTag))
            static_cast<HTMLOptionElement*>(listItems[i])
                ->m_selected = static_cast<ListBox*>(m_widget)->isSelected(j);
        if (listItems[i]->hasTagName(optionTag) || listItems[i]->hasTagName(optgroupTag))
            ++j;
    }
    static_cast<HTMLSelectElement*>(node())->onChange();
}

void DeprecatedRenderSelect::setOptionsChanged(bool _optionsChanged)
{
    m_optionsChanged = _optionsChanged;
}

ListBox* DeprecatedRenderSelect::createListBox()
{
    ListBox *lb = new ListBox();
    lb->setSelectionMode(m_multiple ? ListBox::Extended : ListBox::Single);
    m_ignoreSelectEvents = false;
    return lb;
}

void DeprecatedRenderSelect::updateSelection()
{
    const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->listItems();
    int i;
    // if multi-select, we select only the new selected index
    ListBox *listBox = static_cast<ListBox*>(m_widget);
    int j = 0;
    for (i = 0; i < int(listItems.size()); i++) {
        listBox->setSelected(j, listItems[i]->hasTagName(optionTag) &&
                            static_cast<HTMLOptionElement*>(listItems[i])->selected());
        if (listItems[i]->hasTagName(optionTag) || listItems[i]->hasTagName(optgroupTag))
            ++j;
        
    }

    m_selectionChanged = false;
}

} // namespace WebCore
