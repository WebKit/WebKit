/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2008, 2009, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Coypright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "PopupMenu.h"

#include "Chrome.h"
#include "ChromeClientQt.h"
#include "FrameView.h"
#include "PopupMenuClient.h"
#include "QWebPageClient.h"
#include "QtAbstractWebPopup.h"

namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
    , m_popup(0)
{
}

PopupMenu::~PopupMenu()
{
    delete m_popup;
}

void PopupMenu::show(const IntRect& rect, FrameView* view, int index)
{
    ChromeClientQt* chromeClient = static_cast<ChromeClientQt*>(
        view->frame()->page()->chrome()->client());
    ASSERT(chromeClient);

    if (!m_popup)
        m_popup = chromeClient->createSelectPopup();

    m_popup->m_popupClient = m_popupClient;
    m_popup->m_currentIndex = index;
    m_popup->m_pageClient = chromeClient->platformPageClient();

    QRect geometry(rect);
    geometry.moveTopLeft(view->contentsToWindow(rect.topLeft()));
    m_popup->m_geometry = geometry;

    m_popup->show();

}

void PopupMenu::hide()
{
    m_popup->hide();
}

void PopupMenu::updateFromElement()
{
    client()->setTextFromItem(m_popupClient->selectedIndex());
}

bool PopupMenu::itemWritingDirectionIsNatural()
{
    return false;
}

}

// vim: ts=4 sw=4 et
