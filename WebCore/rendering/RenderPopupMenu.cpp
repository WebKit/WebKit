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

namespace WebCore {

using namespace HTMLNames;

RenderPopupMenu::RenderPopupMenu(Node* element)
    : RenderBlock(element)
{
}

void RenderPopupMenu::populate()
{
    RenderMenuList* select = getRenderMenuList();
    ASSERT(select);
    if (!select->node())
        return;
    //FIXME: Maybe we should just iterate through the select element's list items?
    for (Node* n = select->node()->firstChild(); n; n = n->traverseNextNode(select->node())) {
        if (n->hasTagName(optionTag))
            addOption(static_cast<HTMLOptionElement*>(n));
        else if (n->hasTagName(optgroupTag))
            addGroupLabel(static_cast<HTMLOptGroupElement*>(n));
        else if (n->hasTagName(hrTag))
            addSeparator();
    }
}

}
