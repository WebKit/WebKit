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
#include "WebPopupItem.h"
#include <QtDeclarative/QSGCanvas>
#include <QtDeclarative/QSGItem>
#include <QtGui/QStandardItemModel>

using namespace WebCore;

namespace WebKit {

WebPopupMenuProxyQtDesktop::WebPopupMenuProxyQtDesktop(WebPopupMenuProxy::Client* client, QSGItem* webViewItem)
    : QObject()
    , WebPopupMenuProxy(client)
    , m_comboBox(new QtWebComboBox)
    , m_webViewItem(webViewItem)
    , m_selectedIndex(-1)
{
    QtWebComboBox* comboBox = m_comboBox.data();
    comboBox->setParent(m_webViewItem->canvas());
    connect(comboBox, SIGNAL(activated(int)), SLOT(setSelectedIndex(int)));
    connect(comboBox, SIGNAL(didHide()), SLOT(onPopupMenuHidden()), Qt::QueuedConnection);
}

WebPopupMenuProxyQtDesktop::~WebPopupMenuProxyQtDesktop()
{
    delete m_comboBox.data();
}

void WebPopupMenuProxyQtDesktop::showPopupMenu(const IntRect& rect, WebCore::TextDirection, double, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    QtWebComboBox* comboBox = m_comboBox.data();
    m_selectedIndex = selectedIndex;
    populate(items);
    comboBox->setCurrentIndex(selectedIndex);
    comboBox->setGeometry(m_webViewItem->mapRectToScene(QRect(rect)).toRect());
    comboBox->showPopupAtCursorPosition();
}

void WebPopupMenuProxyQtDesktop::hidePopupMenu()
{
    if (m_comboBox)
        m_comboBox.data()->hidePopup();
}

void WebPopupMenuProxyQtDesktop::setSelectedIndex(int index)
{
    m_selectedIndex = index;
}

void WebPopupMenuProxyQtDesktop::onPopupMenuHidden()
{
    if (m_client)
        m_client->valueChangedForPopupMenu(this, m_selectedIndex);
}

void WebPopupMenuProxyQtDesktop::populate(const Vector<WebPopupItem>& items)
{
    QtWebComboBox* comboBox = m_comboBox.data();
    Q_ASSERT(comboBox);

    comboBox->clear();

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(comboBox->model());
    Q_ASSERT(model);

    for (size_t i = 0; i < items.size(); ++i) {
        const WebPopupItem& item = items.at(i);
        if (item.m_type == WebPopupItem::Separator) {
            comboBox->insertSeparator(i);
            continue;
        }
        comboBox->insertItem(i, item.m_text);
        model->item(i)->setToolTip(item.m_toolTip);
        model->item(i)->setEnabled(item.m_isEnabled);
    }
}

#include "moc_WebPopupMenuProxyQtDesktop.cpp"

} // namespace WebKit
