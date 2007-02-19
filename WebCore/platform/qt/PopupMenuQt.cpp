/*
 * This file is part of the popup menu implementation for <select> elements in WebCore.
 *
 * Copyright (C) 2007 Trolltech ASA
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "PopupMenu.h"

#include "Frame.h"
#include "FrameView.h"
#include "PopupMenuClient.h"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QPoint>
#include <QListWidget>
#include <QListWidgetItem>
#include <QWidgetAction>
#include <qglobal.h>

#define notImplemented() qDebug("FIXME: UNIMPLEMENTED: %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__)

namespace WebCore {

PopupMenu::PopupMenu(PopupMenuClient* client)
    : m_popupClient(client)
{
    m_popup = new QMenu();
}

PopupMenu::~PopupMenu()
{
    delete m_popup;
}

void PopupMenu::clear()
{
    m_popup->clear();
    m_actions.clear();
}

void PopupMenu::populate(const IntRect& r)
{
    clear();
    Q_ASSERT(client());
    int size = client()->listSize();

    QListWidget* listWidget = new QListWidget(m_popup);

    for (int i = 0; i < size; i++) {
        QListWidgetItem* item = 0;
        if (client()->itemIsSeparator(i)) {
            //FIXME: better seperator item
            item = new QListWidgetItem("---");
            item->setFlags(0);
        }
        else {
            RenderStyle* style = client()->itemStyle(i);
            item = new QListWidgetItem(client()->itemText(i));
            m_actions.insert(item, i);
            if (style->font() != Font())
                item->setFont(style->font());

            Qt::ItemFlags flags = Qt::ItemIsSelectable;
            if (client()->itemIsEnabled(i))
                flags |= Qt::ItemIsEnabled;
            item->setFlags(flags);
        }
        if (item)
            listWidget->addItem(item);
    }
    QWidgetAction* action = new QWidgetAction(m_popup);
    action->setDefaultWidget(listWidget);
    m_popup->addAction(action);
    m_popup->setDefaultAction(action);
    m_popup->resize(r.width(), r.width());
    QObject::connect(listWidget, SIGNAL(itemActivated(QListWidgetItem*)),
                     m_popup, SLOT(hide()));
}

void PopupMenu::show(const IntRect& r, FrameView* v, int index)
{
    populate(r);
    //m_popup->setParent(v->qwidget());

    RefPtr<PopupMenu> protector(this);
    QAction* action = m_popup->exec(v->qwidget()->mapToGlobal(QPoint(r.x(), r.y())));
    if (!action)
        action = m_popup->defaultAction();

    if (client()) {
        QWidgetAction* wa = qobject_cast<QWidgetAction*>(action);

        client()->hidePopup();

        if (!wa)
            return;
        QListWidget* lw = qobject_cast<QListWidget*>(wa->defaultWidget());
        if (!lw)
            return;
        QListWidgetItem* item = lw->currentItem();
        if (!item)
            return;
        int newIndex = m_actions[item];
        if (index != newIndex && newIndex >= 0)
            client()->valueChanged(newIndex);
    }
}

void PopupMenu::hide()
{
    m_popup->hide();
}

void PopupMenu::updateFromElement()
{
}

}

// vim: ts=4 sw=4 et
