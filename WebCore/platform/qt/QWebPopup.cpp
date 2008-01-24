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
#include "config.h"
#include "QWebPopup.h"
#include "RenderStyle.h"

#include <QCoreApplication>
#include <QMouseEvent>

namespace WebCore {

QWebPopup::QWebPopup(PopupMenuClient* client)
    : m_client(client)
    , m_popupVisible(false)
{
    Q_ASSERT(m_client);

    setFont(m_client->clientStyle()->font().font());
    connect(this, SIGNAL(activated(int)),
            SLOT(activeChanged(int)));
}


void QWebPopup::exec()
{
    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(this, &event);
}

void QWebPopup::showPopup()
{
    QComboBox::showPopup();
    m_popupVisible = true;
}

void QWebPopup::hidePopup()
{
    QComboBox::hidePopup();
    if (!m_popupVisible)
        return;

    m_popupVisible = false;
    m_client->hidePopup();
}

void QWebPopup::activeChanged(int index)
{
    if (index < 0)
        return;

    m_client->valueChanged(index);
}

}
