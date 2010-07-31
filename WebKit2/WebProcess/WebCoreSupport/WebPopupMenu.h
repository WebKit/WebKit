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

#ifndef WebPopupMenu_h
#define WebPopupMenu_h

#include <WebCore/PopupMenu.h>

namespace WebCore {
class PopupMenuClient;
}

namespace WebKit {

class WebPopupMenu : public WebCore::PopupMenu {
public:
    WebPopupMenu(WebCore::PopupMenuClient*);
    ~WebPopupMenu();

    virtual void show(const WebCore::IntRect&, WebCore::FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

private:
    WebCore::PopupMenuClient* m_popupClient;
};

}

#endif // WebPopupMenu_h
