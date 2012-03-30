/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "WebPopupMenuProxyQt.h"

#include "PlatformPopupMenuData.h"
#include "WebPopupItem.h"
#include "qquickwebview_p.h"
#include "qquickwebview_p_p.h"
#include <QtCore/QAbstractListModel>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

using namespace WebCore;

namespace WebKit {

static QHash<int, QByteArray> createRoleNamesHash();

class PopupMenuItemModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        GroupRole = Qt::UserRole,
        EnabledRole = Qt::UserRole + 1,
        SelectedRole = Qt::UserRole + 2,
        IsSeparatorRole = Qt::UserRole + 3
    };

    PopupMenuItemModel(const Vector<WebPopupItem>&, int selectedOriginalIndex);
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const { return m_items.size(); }
    virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;

    Q_INVOKABLE void select(int);

    int selectedOriginalIndex() const;

private:
    struct Item {
        Item(const WebPopupItem& webPopupItem, const QString& group, int originalIndex, bool selected)
            : text(webPopupItem.m_text)
            , toolTip(webPopupItem.m_toolTip)
            , group(group)
            , originalIndex(originalIndex)
            , enabled(webPopupItem.m_isEnabled)
            , selected(selected)
            , isSeparator(webPopupItem.m_type == WebPopupItem::Separator)
        { }

        QString text;
        QString toolTip;
        QString group;
        // Keep track of originalIndex because we don't add the label (group) items to our vector.
        int originalIndex;
        bool enabled;
        bool selected;
        bool isSeparator;
    };

    void buildItems(const Vector<WebPopupItem>& webPopupItems, int selectedOriginalIndex);

    Vector<Item> m_items;
    int m_selectedModelIndex;
};

class ItemSelectorContextObject : public QObject {
    Q_OBJECT
    Q_PROPERTY(QRect elementRect READ elementRect CONSTANT FINAL)
    Q_PROPERTY(QObject* items READ items CONSTANT FINAL)

public:
    ItemSelectorContextObject(const IntRect& elementRect, const Vector<WebPopupItem>&, int selectedIndex);

    QRect elementRect() const { return m_elementRect; }
    PopupMenuItemModel* items() { return &m_items; }

    Q_INVOKABLE void accept(int index = -1);
    Q_INVOKABLE void reject() { emit rejected(); }

Q_SIGNALS:
    void acceptedWithOriginalIndex(int);
    void rejected();

private:
    QRect m_elementRect;
    PopupMenuItemModel m_items;
};

ItemSelectorContextObject::ItemSelectorContextObject(const IntRect& elementRect, const Vector<WebPopupItem>& webPopupItems, int selectedIndex)
    : m_elementRect(elementRect)
    , m_items(webPopupItems, selectedIndex)
{
}

void ItemSelectorContextObject::accept(int index)
{
    if (index != -1)
        m_items.select(index);
    int originalIndex = m_items.selectedOriginalIndex();
    emit acceptedWithOriginalIndex(originalIndex);
}

static QHash<int, QByteArray> createRoleNamesHash()
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "text";
    roles[Qt::ToolTipRole] = "tooltip";
    roles[PopupMenuItemModel::GroupRole] = "group";
    roles[PopupMenuItemModel::EnabledRole] = "enabled";
    roles[PopupMenuItemModel::SelectedRole] = "selected";
    roles[PopupMenuItemModel::IsSeparatorRole] = "isSeparator";
    return roles;
}

PopupMenuItemModel::PopupMenuItemModel(const Vector<WebPopupItem>& webPopupItems, int selectedOriginalIndex)
    : m_selectedModelIndex(-1)
{
    static QHash<int, QByteArray> roles = createRoleNamesHash();
    setRoleNames(roles);
    buildItems(webPopupItems, selectedOriginalIndex);
}

QVariant PopupMenuItemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return QVariant();

    const Item& item = m_items[index.row()];
    if (item.isSeparator) {
        if (role == IsSeparatorRole)
            return true;
        return QVariant();
    }

    switch (role) {
    case Qt::DisplayRole:
        return item.text;
    case Qt::ToolTipRole:
        return item.toolTip;
    case GroupRole:
        return item.group;
    case EnabledRole:
        return item.enabled;
    case SelectedRole:
        return item.selected;
    case IsSeparatorRole:
        return false;
    }

    return QVariant();
}

void PopupMenuItemModel::select(int index)
{
    int oldIndex = m_selectedModelIndex;
    if (index == oldIndex)
        return;
    if (index < 0 || index >= m_items.size())
        return;
    Item& item = m_items[index];
    if (!item.enabled)
        return;

    Item& oldItem = m_items[oldIndex];
    oldItem.selected = false;
    item.selected = true;
    m_selectedModelIndex = index;

    emit dataChanged(this->index(oldIndex), this->index(oldIndex));
    emit dataChanged(this->index(index), this->index(index));
}

int PopupMenuItemModel::selectedOriginalIndex() const
{
    if (m_selectedModelIndex == -1)
        return -1;
    return m_items[m_selectedModelIndex].originalIndex;
}

void PopupMenuItemModel::buildItems(const Vector<WebPopupItem>& webPopupItems, int selectedOriginalIndex)
{
    QString currentGroup;
    m_items.reserveInitialCapacity(webPopupItems.size());
    for (int i = 0; i < webPopupItems.size(); i++) {
        const WebPopupItem& webPopupItem = webPopupItems[i];
        if (webPopupItem.m_isLabel) {
            currentGroup = webPopupItem.m_text;
            continue;
        }
        const bool selected = i == selectedOriginalIndex;
        if (selected)
            m_selectedModelIndex = m_items.size();
        m_items.append(Item(webPopupItem, currentGroup, i, selected));
    }
}

WebPopupMenuProxyQt::WebPopupMenuProxyQt(WebPopupMenuProxy::Client* client, QQuickWebView* webView)
    : WebPopupMenuProxy(client)
    , m_webView(webView)
{
}

WebPopupMenuProxyQt::~WebPopupMenuProxyQt()
{
}

void WebPopupMenuProxyQt::showPopupMenu(const IntRect& rect, WebCore::TextDirection, double, const Vector<WebPopupItem>& items, const PlatformPopupMenuData&, int32_t selectedIndex)
{
    m_selectedIndex = selectedIndex;

    ItemSelectorContextObject* contextObject = new ItemSelectorContextObject(rect, items, m_selectedIndex);
    createItem(contextObject);
    if (!m_itemSelector) {
        notifyValueChanged();
        return;
    }
    QQuickWebViewPrivate::get(m_webView)->setDialogActive(true);
}

void WebPopupMenuProxyQt::hidePopupMenu()
{
    m_itemSelector.clear();
    QQuickWebViewPrivate::get(m_webView)->setDialogActive(false);
    m_context.clear();
    notifyValueChanged();
}

void WebPopupMenuProxyQt::selectIndex(int index)
{
    m_selectedIndex = index;
}

void WebPopupMenuProxyQt::createItem(QObject* contextObject)
{
    QQmlComponent* component = m_webView->experimental()->itemSelector();
    if (!component) {
        delete contextObject;
        return;
    }

    createContext(component, contextObject);
    QObject* object = component->beginCreate(m_context.get());
    if (!object) {
        m_context.clear();
        return;
    }

    m_itemSelector = adoptPtr(qobject_cast<QQuickItem*>(object));
    if (!m_itemSelector) {
        m_context.clear();
        m_itemSelector.clear();
        return;
    }

    connect(contextObject, SIGNAL(acceptedWithOriginalIndex(int)), SLOT(selectIndex(int)));

    // We enqueue these because they are triggered by m_itemSelector and will lead to its destruction.
    connect(contextObject, SIGNAL(acceptedWithOriginalIndex(int)), SLOT(hidePopupMenu()), Qt::QueuedConnection);
    connect(contextObject, SIGNAL(rejected()), SLOT(hidePopupMenu()), Qt::QueuedConnection);

    QQuickWebViewPrivate::get(m_webView)->setViewInAttachedProperties(m_itemSelector.get());
    component->completeCreate();

    m_itemSelector->setParentItem(m_webView);
}

void WebPopupMenuProxyQt::createContext(QQmlComponent* component, QObject* contextObject)
{
    QQmlContext* baseContext = component->creationContext();
    if (!baseContext)
        baseContext = QQmlEngine::contextForObject(m_webView);
    m_context = adoptPtr(new QQmlContext(baseContext));

    contextObject->setParent(m_context.get());
    m_context->setContextProperty(QLatin1String("model"), contextObject);
    m_context->setContextObject(contextObject);
}

void WebPopupMenuProxyQt::notifyValueChanged()
{
    if (m_client) {
        m_client->valueChangedForPopupMenu(this, m_selectedIndex);
        invalidate();
    }
}

} // namespace WebKit

// Since we define QObjects in WebPopupMenuProxyQt.cpp, this will trigger moc to run on .cpp.
#include "WebPopupMenuProxyQt.moc"

// And we can't compile the moc for WebPopupMenuProxyQt.h by itself, since it doesn't include "config.h"
#include "moc_WebPopupMenuProxyQt.cpp"
