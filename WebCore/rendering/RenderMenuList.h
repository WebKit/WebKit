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

#include "RenderFlexibleBox.h"

namespace WebCore {

class HTMLSelectElement;
class RenderPopupMenu;

class RenderMenuList : public RenderFlexibleBox {
public:
    RenderMenuList(HTMLSelectElement*);
    ~RenderMenuList();

    virtual bool isMenuList() const { return true; }

    virtual void addChild(RenderObject* newChild, RenderObject *beforeChild = 0);
    virtual void removeChild(RenderObject*);
    virtual bool createsAnonymousWrapper() const { return true; }
    virtual bool canHaveChildren() const { return false; }

    virtual void setStyle(RenderStyle*);
    virtual void updateFromElement();

    virtual void paintObject(PaintInfo&, int tx, int ty);

    virtual const char* renderName() const { return "RenderMenuList"; }

    virtual void calcMinMaxWidth();

    RenderPopupMenu* popup() const { return m_popup; }
    bool popupIsVisible() const { return m_popupIsVisible; }
    void showPopup();
    void hidePopup();

    void setOptionsChanged(bool c) { m_optionsChanged = c; }
    void valueChanged(unsigned listIndex);

    String text();

protected:
    virtual bool hasLineIfEmpty() const { return true; }

private:
    void createInnerBlock();
    void setText(const String&);

    RenderText* m_buttonText;
    RenderBlock* m_innerBlock;

    bool m_optionsChanged;
    int m_optionsWidth;

    RenderPopupMenu* m_popup;
    bool m_popupIsVisible;
};

}

#endif
