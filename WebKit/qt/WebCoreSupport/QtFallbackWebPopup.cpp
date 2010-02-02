/*
 * Copyright (C) 2010 Girish Ramakrishnan <girish@forwardbias.in>
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
#include "PopupMenuClient.h"
#include "qgraphicswebview.h"
#include "QWebPageClient.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QInputContext>
#include <QMouseEvent>
#include <QStandardItemModel>

namespace WebCore {

QtFallbackWebPopupCombo::QtFallbackWebPopupCombo(QtFallbackWebPopup& ownerPopup)
    : m_ownerPopup(ownerPopup)
{
}

void QtFallbackWebPopupCombo::showPopup()
{
    QComboBox::showPopup();
    m_ownerPopup.m_popupVisible = true;
}

void QtFallbackWebPopupCombo::hidePopup()
{
    QWidget* activeFocus = QApplication::focusWidget();
    if (activeFocus && activeFocus == QComboBox::view()
        && activeFocus->testAttribute(Qt::WA_InputMethodEnabled)) {
        QInputContext* qic = activeFocus->inputContext();
        if (qic) {
            qic->reset();
            qic->setFocusWidget(0);
        }
    }

    QComboBox::hidePopup();

    if (QGraphicsProxyWidget* proxy = graphicsProxyWidget())
        proxy->setVisible(false);

    if (!m_ownerPopup.m_popupVisible)
        return;

    m_ownerPopup.m_popupVisible = false;
    m_ownerPopup.popupDidHide();
}

// QtFallbackWebPopup

QtFallbackWebPopup::QtFallbackWebPopup()
    : QtAbstractWebPopup()
    , m_popupVisible(false)
    , m_combo(new QtFallbackWebPopupCombo(*this))
    , m_proxy(0)
{
    connect(m_combo, SIGNAL(activated(int)),
            SLOT(activeChanged(int)), Qt::QueuedConnection);
}

QtFallbackWebPopup::~QtFallbackWebPopup()
{
    // If we create a proxy, then the deletion of the proxy and the
    // combo will be done by the proxy's parent (QGraphicsWebView)
    if (!m_proxy)
        delete m_combo;
}

void QtFallbackWebPopup::show()
{
    populate();
    m_combo->setCurrentIndex(currentIndex());

#if defined(Q_WS_MAEMO_5)
    // Comboboxes with Qt on Maemo 5 come up in their full width on the screen, so neither
    // the proxy widget, nor the coordinates are needed.
    m_combo->setParent(pageClient()->ownerWidget());
    m_combo->showPopup();
    return;
#endif

    QRect rect = geometry();
    if (QGraphicsWebView *webView = qobject_cast<QGraphicsWebView*>(pageClient()->pluginParent())) {
        if (!m_proxy) {
            m_proxy = new QGraphicsProxyWidget(webView);
            m_proxy->setWidget(m_combo);
        } else
            m_proxy->setVisible(true);
        m_proxy->setGeometry(rect);
    } else {
        m_combo->setParent(pageClient()->ownerWidget());
        m_combo->setGeometry(QRect(rect.left(), rect.top(),
                               rect.width(), m_combo->sizeHint().height()));

    }

    // QCursor::pos() is not a great idea for a touch screen, but as Maemo 5 is handled
    // separately above, this should be okay.
    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_combo, &event);
}

void QtFallbackWebPopup::hide()
{
    m_combo->hidePopup();
}

void QtFallbackWebPopup::populate()
{
    m_combo->clear();

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_combo->model());
    Q_ASSERT(model);

#if !defined(Q_WS_S60) && !defined(Q_WS_MAEMO_5)
    m_combo->setFont(font());
#endif
    for (int i = 0; i < itemCount(); ++i) {
        switch (itemType(i)) {
        case Separator:
            m_combo->insertSeparator(i);
            break;
        case Group:
            m_combo->insertItem(i, itemText(i));
            model->item(i)->setEnabled(false);
            break;
        case Option:
            m_combo->insertItem(i, itemText(i));
            model->item(i)->setEnabled(itemIsEnabled(i));
            break;
        }
    }
}

void QtFallbackWebPopup::activeChanged(int index)
{
    if (index < 0)
        return;

    valueChanged(index);
}

}
