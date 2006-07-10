/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006 Apple Computer
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

#ifndef RenderMenuList_H
#define RenderMenuList_H

#include "RenderButton.h"
#include "HTMLSelectElement.h"

namespace WebCore {

class RenderPopupMenu;

class RenderMenuList : public RenderFlexibleBox
{
public:
    RenderMenuList(HTMLSelectElement*);

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject* oldChild);
    virtual void removeLeftoverAnonymousBoxes() {}
    virtual bool createsAnonymousWrapper() const { return true; }
    virtual bool canHaveChildren() const { return false; }

    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();

    virtual void paintObject(PaintInfo&, int tx, int ty);

    virtual const char* renderName() const { return "RenderMenuList"; }
    
    RenderStyle* createInnerStyle(RenderStyle*);
    virtual void calcMinMaxWidth();
    virtual void layout();

    void setOptionsChanged(bool o) { m_optionsChanged = o; }

    bool selectionChanged() { return m_selectionChanged; }
    void setSelectionChanged(bool selectionChanged) { m_selectionChanged = selectionChanged; }
    void updateSelection();
    
    void showPopup();
    void valueChanged(unsigned index);
    bool hasPopupMenu() { return m_popupMenu; }

private:
    void createInnerBlock();
    void setText(const String&);

    RenderText* m_buttonText;
    RenderBlock* m_inner;
    RenderPopupMenu* m_popupMenu;

    unsigned m_size;
    bool m_selectionChanged;
    bool m_optionsChanged;
    float m_longestWidth;
    int m_selectedIndex;
};

}

#endif
