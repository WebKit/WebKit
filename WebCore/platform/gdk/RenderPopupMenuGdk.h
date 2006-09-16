/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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

#ifndef RENDER_POPUPMENU_GDK_H
#define RENDER_POPUPMENU_GDK_H

#include "RenderPopupMenu.h"

namespace WebCore {

class HTMLOptionElement;
class HTMLOptGroupElement;

class RenderPopupMenuGdk : public RenderPopupMenu {
public:
    RenderPopupMenuGdk(Node*,  RenderMenuList*);
    ~RenderPopupMenuGdk();

    virtual void clear();
    virtual void populate();
    virtual void showPopup(const IntRect&, FrameView*, int index);
    virtual void hidePopup();
    
protected:
    virtual void addSeparator();
    virtual void addGroupLabel(HTMLOptGroupElement*);
    virtual void addOption(HTMLOptionElement*);

};

}

#endif
