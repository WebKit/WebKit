/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef PopupMenuHaiku_h
#define PopupMenuHaiku_h

#include "IntRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class FrameView;
class HaikuPopup;
class Scrollbar;

class PopupMenuHaiku : public PopupMenu {
public:
    PopupMenuHaiku(PopupMenuClient*);
    ~PopupMenuHaiku();

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

private:
    PopupMenuClient* client() const { return m_popupClient; }

    PopupMenuClient* m_popupClient;
    HaikuPopup* m_menu;
};

}

#endif // PopupMenuHaiku_h
