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

#include <QFont>
#include <QList>
#include <QRect>

namespace WebCore {

class PopupMenuClient;

class QtAbstractWebPopup {
public:
    struct Item {
        enum { Option, Group, Separator } type;
        QString text;
        QString toolTip;
        bool enabled;
    };

    QtAbstractWebPopup();
    virtual ~QtAbstractWebPopup();

    virtual void show(const QRect& geometry, int selectedIndex) = 0;
    virtual void hide() = 0;
    virtual void populate(const QFont& font, const QList<Item>& items) = 0;
    virtual void setParent(QWidget* parent) = 0;

protected:
    void popupDidHide(bool acceptSuggestions);
    void valueChanged(int index);

private:
    friend class PopupMenu;
    PopupMenuClient* m_client;
};

}

#endif // QtAbstractWebPopup_h
