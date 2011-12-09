/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPopupMenuProxyQtDesktop.h"

#include "PlatformPopupMenuData.h"
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QtQuick/QQuickCanvas>
#include <QtQuick/QQuickItem>
#include <QMouseEvent>
#include <QStandardItemModel>
#include "WebPopupItem.h"
#include <wtf/CurrentTime.h>

using namespace WebCore;

namespace WebKit {

WebPopupMenuProxyQtDesktop::WebPopupMenuProxyQtDesktop(WebPopupMenuProxy::Client* client, QQuickItem* webViewItem)
    : WebPopupMenuProxy(client)
    , m_webViewItem(webViewItem)
    , m_selectedIndex(-1)
{
    window()->winId(); // Ensure that the combobox has a window
    Q_ASSERT(window()->windowHandle());
    window()->windowHandle()->setTransientParent(m_webViewItem->canvas());

    connect(this, SIGNAL(activated(int)), SLOT(setSelectedIndex(int)));
    // Install an event filter on the view inside the combo box popup to make sure we know
    // when the popup got closed. E.g. QComboBox::hidePopup() won't be called when the popup
    // is closed by a mouse wheel event outside its window.
    view()->installEventFilter(this);
}

WebPopupMenuProxyQtDesktop::~WebPopupMenuProxyQtDesktop()
{
}

void WebPopupMenuProxyQtDesktop::showPopupMenu(const IntRect& rect, WebCore::TextDirection, double, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    m_selectedIndex = selectedIndex;
    populate(items);
    setCurrentIndex(selectedIndex);
    setGeometry(m_webViewItem->mapRectToScene(QRect(rect)).toRect());

    QMouseEvent event(QEvent::MouseButtonPress, QCursor::pos(), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    event.setTimestamp(static_cast<qint64>(WTF::currentTimeMS()));
    QCoreApplication::sendEvent(this, &event);
}

void WebPopupMenuProxyQtDesktop::hidePopupMenu()
{
    hidePopup();
}

bool WebPopupMenuProxyQtDesktop::eventFilter(QObject *watched, QEvent *event)
{
    Q_ASSERT(watched == view());
    if (event->type() == QEvent::Hide) {
        if (m_client)
            m_client->valueChangedForPopupMenu(this, m_selectedIndex);
    }
    return false;
}

void WebPopupMenuProxyQtDesktop::setSelectedIndex(int index)
{
    m_selectedIndex = index;
}

void WebPopupMenuProxyQtDesktop::populate(const Vector<WebPopupItem>& items)
{
    clear();

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(this->model());
    Q_ASSERT(model);

    for (size_t i = 0; i < items.size(); ++i) {
        const WebPopupItem& item = items.at(i);
        if (item.m_type == WebPopupItem::Separator) {
            insertSeparator(i);
            continue;
        }
        insertItem(i, item.m_text);
        model->item(i)->setToolTip(item.m_toolTip);
        model->item(i)->setEnabled(item.m_isEnabled);
    }
}

#include "moc_WebPopupMenuProxyQtDesktop.cpp"

} // namespace WebKit
