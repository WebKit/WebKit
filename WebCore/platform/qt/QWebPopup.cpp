/*
 *
 * Copyright (C) 2007 Trolltech ASA
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
#include "QWebPopup.h"

#include <QCoreApplication>
#include <QMouseEvent>

namespace WebCore {

QWebPopup::QWebPopup(PopupMenuClient* client)
{
    m_client = client;
    connect(this, SIGNAL(activated(int)),
            SLOT(activeChanged(int)));
}


void QWebPopup::exec()
{
    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(this, &event);
}

void QWebPopup::hideEvent(QHideEvent* e)
{
    QComboBox::hideEvent(e);
    if (m_client)
        m_client->hidePopup();
}

void QWebPopup::activeChanged(int index)
{
    if (m_client) {
        if (index >= 0)
            m_client->valueChanged(index);
        m_client->hidePopup();
    }
}

}
