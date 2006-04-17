/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2005 Apple Computer
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

#ifndef render_button_h
#define render_button_h

#include "RenderFlexibleBox.h"

namespace WebCore {

// RenderButtons are just like normal flexboxes except that they will generate an anonymous block child.
// For inputs, they will also generate an anonymous RenderText and keep its style and content up
// to date as the button changes.
class RenderButton : public RenderFlexibleBox
{
public:
    RenderButton(Node*);

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject* oldChild);
    virtual void removeLeftoverAnonymousBoxes() {}
    virtual bool allowsReusingAnonymousChild() const { return false; }

    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();

    virtual void paintObject(PaintInfo&, int tx, int ty);

    virtual const char *renderName() const { return "RenderButton"; }

protected:
    RenderText* m_buttonText;
    RenderBlock* m_inner;
};

}

#endif
