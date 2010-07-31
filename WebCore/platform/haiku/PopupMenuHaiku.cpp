/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "PopupMenuHaiku.h"

#include "FrameView.h"

#include "NotImplemented.h"
#include <Application.h>
#include <Handler.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <String.h>
#include <Window.h>
#include <support/Autolock.h>
#include <support/Locker.h>

namespace WebCore {

static const uint32 kPopupResult = 'pmrs';
static const uint32 kPopupHidden = 'pmhd';

// This BHandler is added to the application (main) thread, so that we
// invoke methods on the PopupMenuClient within the main thread.
class PopupMenuHandler : public BHandler {
public:
    PopupMenuHandler(PopupMenuClient* popupClient)
        : m_popupClient(popupClient)
    {
    }

    virtual void MessageReceived(BMessage* message)
    {
        switch (message->what) {
        case kPopupResult: {
            int32 index = 0;
            message->FindInt32("index", &index);
            m_popupClient->valueChanged(index);
            break;
        }
        case kPopupHidden:
            m_popupClient->popupDidHide();
            break;

        default:
            BHandler::MessageReceived(message);
        }
    }

private:
    PopupMenuClient* m_popupClient;
};

class HaikuPopup : public BPopUpMenu {
public:
    HaikuPopup(PopupMenuClient* popupClient)
        : BPopUpMenu("WebCore Popup", true, false)
        , m_popupClient(popupClient)
        , m_Handler(popupClient)
    {
        if (be_app->Lock()) {
            be_app->AddHandler(&m_Handler);
            be_app->Unlock();
        }
        SetAsyncAutoDestruct(false);
    }

    virtual ~HaikuPopup()
    {
        if (be_app->Lock()) {
            be_app->RemoveHandler(&m_Handler);
            be_app->Unlock();
        }
    }

    void show(const IntRect& rect, FrameView* view, int index)
    {
        // Clean out the menu first
        for (int32 i = CountItems() - 1; i >= 0; i--)
            delete RemoveItem(i);

        // Popuplate the menu from the client
        int itemCount = m_popupClient->listSize();
        for (int i = 0; i < itemCount; i++) {
            if (m_popupClient->itemIsSeparator(i))
                AddSeparatorItem();
            else {
                // NOTE: WebCore distinguishes between "Group" and "Label"
                // here, but both types of item (radio or check mark) currently
                // look the same on Haiku.
                BString label(m_popupClient->itemText(i));
                BMessage* message = new BMessage(kPopupResult);
                message->AddInt32("index", i);
                BMenuItem* item = new BMenuItem(label.String(), message);
                AddItem(item);
                item->SetTarget(BMessenger(&m_Handler));
                item->SetEnabled(m_popupClient->itemIsEnabled(i));
                item->SetMarked(i == index);
            }
        }

        // We need to force a layout now, or the item frames will not be
        // computed yet, so we cannot move the current item under the mouse.
        DoLayout();

        // Account for frame of menu field
        BRect screenRect(view->contentsToScreen(rect));
        screenRect.OffsetBy(2, 2);
        // Move currently selected item under the mouse.
        if (BMenuItem* item = ItemAt(index))
            screenRect.OffsetBy(0, -item->Frame().top);

        BRect openRect = Bounds().OffsetToSelf(screenRect.LeftTop());

        Go(screenRect.LeftTop(), true, true, openRect, true);
    }

    void hide()
    {
        if (!IsHidden())
            Hide();
    }

private:
    virtual void Hide()
    {
        BPopUpMenu::Hide();
        be_app->PostMessage(kPopupHidden, &m_Handler);
    }

    PopupMenuClient* m_popupClient;
    PopupMenuHandler m_Handler;
};

PopupMenuHaiku::PopupMenuHaiku(PopupMenuClient* client)
    : m_popupClient(client)
    , m_menu(new HaikuPopup(client))
{
    // We don't need additional references to the client, since we completely
    // control any sub-objects we create that need it as well.
}

PopupMenuHaiku::~PopupMenuHaiku()
{
    delete m_menu;
}

void PopupMenuHaiku::disconnectClient()
{
    m_popupClient = 0;
}

void PopupMenuHaiku::show(const IntRect& rect, FrameView* view, int index)
{
    // The menu will update itself from the PopupMenuClient before showing.
    m_menu->show(rect, view, index);
}

void PopupMenuHaiku::hide()
{
    m_menu->hide();
}

void PopupMenuHaiku::updateFromElement()
{
    client()->setTextFromItem(m_popupClient->selectedIndex());
}

} // namespace WebCore

