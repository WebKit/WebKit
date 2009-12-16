/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
#include "QtFallbackWebPopup.h"

#include "HostWindow.h"
#include "QWebPageClient.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QInputContext>
#include <QMouseEvent>
#include <QStandardItemModel>

namespace WebCore {

// QtFallbackWebPopup

QtFallbackWebPopup::QtFallbackWebPopup(PopupMenuClient* client)
    : QtAbstractWebPopup(client)
    , m_popupVisible(false)
{
    setFont(QtAbstractWebPopup::client()->menuStyle().font().font());
    connect(this, SIGNAL(activated(int)),
            SLOT(activeChanged(int)), Qt::QueuedConnection);
}


void QtFallbackWebPopup::show(const QRect& geometry, int selectedIndex)
{
    populate();
    setCurrentIndex(selectedIndex);

    QWidget* parent = 0;
    if (client()->hostWindow() && client()->hostWindow()->platformPageClient())
       parent = client()->hostWindow()->platformPageClient()->ownerWidget();

    setParent(parent);
    setGeometry(QRect(geometry.left(), geometry.top(), geometry.width(), sizeHint().height()));

    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(this, &event);
}

void QtFallbackWebPopup::populate()
{
    clear();

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(QComboBox::model());
    Q_ASSERT(model);

    int size = client()->listSize();
    for (int i = 0; i < size; i++) {
        if (client()->itemIsSeparator(i))
            insertSeparator(i);
        else {
            insertItem(i, client()->itemText(i));

            if (model && !client()->itemIsEnabled(i))
                model->item(i)->setEnabled(false);
        }
    }
}

void QtFallbackWebPopup::showPopup()
{
    QComboBox::showPopup();
    m_popupVisible = true;
}

void QtFallbackWebPopup::hidePopup()
{
    QWidget* activeFocus = QApplication::focusWidget();
    if (activeFocus && activeFocus == view()
        && activeFocus->testAttribute(Qt::WA_InputMethodEnabled)) {
        QInputContext* qic = activeFocus->inputContext();
        if (qic) {
            qic->reset();
            qic->setFocusWidget(0);
        }
    }

    QComboBox::hidePopup();
    if (!m_popupVisible)
        return;

    m_popupVisible = false;
    client()->popupDidHide();
}

void QtFallbackWebPopup::activeChanged(int index)
{
    if (index < 0)
        return;

    client()->valueChanged(index);
}

}
