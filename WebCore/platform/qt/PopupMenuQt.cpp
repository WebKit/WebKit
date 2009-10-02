/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "PopupMenuClient.h"
#include "QWebPageClient.h"
#include "QWebPopup.h"

#include <QAction>
#include <QDebug>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPoint>
#include <QStandardItemModel>
#include <QWidgetAction>

namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
{
    m_popup = new QWebPopup(client);
}

PopupMenu::~PopupMenu()
{
    delete m_popup;
}

void PopupMenu::clear()
{
    m_popup->clear();
}

void PopupMenu::populate(const IntRect&)
{
    clear();
    Q_ASSERT(client());

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_popup->model());
    Q_ASSERT(model);

    int size = client()->listSize();
    for (int i = 0; i < size; i++) {
        if (client()->itemIsSeparator(i))
            m_popup->insertSeparator(i);
        else {
            m_popup->insertItem(i, client()->itemText(i));

            if (model && !client()->itemIsEnabled(i))
                model->item(i)->setEnabled(false);

            if (client()->itemIsSelected(i))
                m_popup->setCurrentIndex(i);
        }
    }
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    QWebPageClient* client = v->hostWindow()->platformPageClient();
    populate(r);
    QRect rect = r;
    rect.moveTopLeft(v->contentsToWindow(r.topLeft()));
    rect.setHeight(m_popup->sizeHint().height());

    m_popup->setParent(QWidget::find(client->winId()));
    m_popup->setGeometry(rect);
    m_popup->setCurrentIndex(index);
    m_popup->exec();
}

void PopupMenu::hide()
{
    m_popup->hidePopup();
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
