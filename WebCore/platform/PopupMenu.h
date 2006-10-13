/*
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
 *
 */

#ifndef POPUPMENU_H
#define POPUPMENU_H

#include "Shared.h"
#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSPopUpButtonCell;
#else
class NSPopUpButtonCell;
#endif
#elif PLATFORM(WIN)
typedef struct HWND__* HWND;
#endif

namespace WebCore {

class FrameView;
class IntRect;
class HTMLOptionElement;
class HTMLOptGroupElement;
class RenderMenuList;

class PopupMenu : public Shared<PopupMenu> {
public:
    static PassRefPtr<PopupMenu> create(RenderMenuList* menuList);
    ~PopupMenu();
    
    void menuListIsDetaching() { m_menuList = 0; }

    void clear();
    void populate();
    void show(const IntRect&, FrameView*, int index);
    void hide();
    
    bool up();
    bool down();
    
    bool wasClicked() const { return m_wasClicked; }
    void setWasClicked(bool b) { m_wasClicked = b; }

    int focusedIndex() const;
    
    RenderMenuList* menuList() { return m_menuList; }

#if PLATFORM(WIN)
    HWND popupHandle() const { return m_popup; }
#endif

protected:
    void addItems();
    void addSeparator();
    void addGroupLabel(HTMLOptGroupElement*);
    void addOption(HTMLOptionElement*);

    void setPositionAndSize(const IntRect&, FrameView*);
    
 private:
    PopupMenu(RenderMenuList* menuList);
    
    RenderMenuList* m_menuList;
    bool m_wasClicked;
    
#if PLATFORM(MAC)
    NSPopUpButtonCell* popup;
#elif PLATFORM(WIN)
    HWND m_popup;
    HWND m_container;
#endif
};

}

#endif
