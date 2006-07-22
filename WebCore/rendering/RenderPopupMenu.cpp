/**
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
 */

#include "config.h"
#include "RenderPopupMenu.h"

#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

using namespace HTMLNames;

RenderPopupMenu::RenderPopupMenu(Node* element, RenderMenuList* menuList)
    : RenderBlock(element)
    , m_menuList(menuList)
{
}

void RenderPopupMenu::populate()
{
    ASSERT(menuList());
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(menuList()->node());
    if (!select)
        return;
    const Vector<HTMLElement*>& items = select->listItems();
    size_t size = items.size();
    for (size_t i = 0; i < size; ++i) {
        HTMLElement* element = items[i];
        if (element->hasTagName(optionTag))
            addOption(static_cast<HTMLOptionElement*>(element));
        else if (element->hasTagName(optgroupTag))
            addGroupLabel(static_cast<HTMLOptGroupElement*>(element));
        else if (element->hasTagName(hrTag))
            addSeparator();
        else
            ASSERT(0);
    }
}

}
