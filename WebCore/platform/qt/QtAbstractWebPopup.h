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
#ifndef QtAbstractWebPopup_h
#define QtAbstractWebPopup_h

#include "PopupMenuClient.h"

#include <QFont>
#include <QList>
#include <QRect>
#include <QWidget>

class QWebPageClient;

namespace WebCore {

class QtAbstractWebPopup {
public:
    enum ItemType { Option, Group, Separator };

    ItemType itemType(int) const;
    QString itemText(int idx) const { return m_popupClient->itemText(idx); }
    QString itemToolTip(int idx) const { return m_popupClient->itemToolTip(idx); }
    bool itemIsEnabled(int idx) const { return m_popupClient->itemIsEnabled(idx); }
    int itemCount() const { return m_popupClient->listSize(); }

    QWebPageClient* pageClient() const { return m_pageClient; }
    QRect geometry() const { return m_geometry; }
    int currentIndex() const { return m_currentIndex; }

    QtAbstractWebPopup();
    virtual ~QtAbstractWebPopup();

    virtual void show() = 0;
    virtual void hide() = 0;

    void popupDidHide();
    void valueChanged(int index);

    QFont font() { return m_popupClient->menuStyle().font().font(); }

private:
    friend class PopupMenu;
    PopupMenuClient* m_popupClient;
    QWebPageClient* m_pageClient;
    int m_currentIndex;
    QRect m_geometry;
};

}

#endif // QtAbstractWebPopup_h
