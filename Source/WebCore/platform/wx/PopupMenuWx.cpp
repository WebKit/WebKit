/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2008 Apple Computer, Inc.
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

#include "config.h"
#include "PopupMenuWx.h"

#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "PopupMenuClient.h"
#include "PlatformString.h"

#include <wx/defs.h>
#if __WXMSW__
#include <wx/msw/winundef.h>
#endif
#include <wx/event.h>
#include <wx/menu.h>

static int s_menuStartId = wxNewId();

namespace WebCore {

class PopupMenuEventHandler : public wxEvtHandler
{
public:
    PopupMenuEventHandler(PopupMenuClient* client) :
        m_client(client)
    {}

    void OnMenuItemSelected(wxCommandEvent& event)
    {
        if (m_client) {
            m_client->valueChanged(event.GetId() - s_menuStartId);
            m_client->popupDidHide();
        }
        // TODO: Do we need to call Disconnect here? Do we have a ref to the native window still?
    }

private:
    PopupMenuClient* m_client;

};

PopupMenuWx::PopupMenuWx(PopupMenuClient* client)
    : m_popupClient(client)
    , m_menu(0)
{
    m_popupHandler = new PopupMenuEventHandler(client);
}

PopupMenuWx::~PopupMenuWx()
{
    delete m_popupHandler;
    delete m_menu;
}

void PopupMenuWx::disconnectClient()
{
    m_popupClient = 0;
}

void PopupMenuWx::show(const IntRect& r, FrameView* v, int index)
{
    // just delete and recreate
    delete m_menu;
    ASSERT(client());
    ASSERT(v);

    wxWindow* nativeWin = v->hostWindow()->platformPageClient();

    if (nativeWin) {
        // construct the menu
        m_menu = new wxMenu();

        int size = client()->listSize();
        for (int i = 0; i < size; i++) {
            int id = s_menuStartId + i;
            
            if (client()->itemIsSeparator(i)) {
                m_menu->AppendSeparator();
            }
            else {
                // FIXME: appending a menu item with an empty label asserts in
                // wx. This needs to be fixed at wx level so that we can have
                // the concept of "no selection" in choice boxes, etc.
                if (!client()->itemText(i).isEmpty())
                    m_menu->Append(s_menuStartId + i, client()->itemText(i));
            }
        }
        nativeWin->Connect(s_menuStartId, s_menuStartId + (size-1), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(PopupMenuEventHandler::OnMenuItemSelected), 0, m_popupHandler);
        nativeWin->PopupMenu(m_menu, r.x() - v->scrollX(), r.y() - v->scrollY());
        nativeWin->Disconnect(s_menuStartId, s_menuStartId + (size-1), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(PopupMenuEventHandler::OnMenuItemSelected), 0, m_popupHandler);
    }
}

void PopupMenuWx::hide()
{
    // we don't need to do anything here, the native control only exists during the time
    // show is called
}

void PopupMenuWx::updateFromElement()
{
    client()->setTextFromItem(m_popupClient->selectedIndex());
}

}
