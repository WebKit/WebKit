/*
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "SelectElement.h"

#include "Attribute.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Element.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "OptionElement.h"
#include "OptionGroupElement.h"
#include "Page.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "SpatialNavigation.h"
#include <wtf/Assertions.h>
#include <wtf/unicode/CharacterNames.h>

using std::min;
using std::max;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

// SelectElementData
SelectElementData::SelectElementData()
    : m_lastCharTime(0)
    , m_repeatingChar(0)
    , m_size(0)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_userDrivenChange(false)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_recalcListItems(false)
{
}

SelectElementData::~SelectElementData()
{
}

void SelectElementData::checkListItems(const Element* element) const
{
#if !ASSERT_DISABLED
    Vector<Element*> items = m_listItems;
    HTMLSelectElement::recalcListItems(*const_cast<SelectElementData*>(this), element, false);
    ASSERT(items == m_listItems);
#else
    UNUSED_PARAM(element);
#endif
}

Vector<Element*>& SelectElementData::listItems(const Element* element)
{
    if (m_recalcListItems)
        HTMLSelectElement::recalcListItems(*this, element);
    else
        checkListItems(element);

    return m_listItems;
}

const Vector<Element*>& SelectElementData::listItems(const Element* element) const
{
    if (m_recalcListItems)
        HTMLSelectElement::recalcListItems(*const_cast<SelectElementData*>(this), element);
    else
        checkListItems(element);

    return m_listItems;
}

}
