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

#include <QRect>

namespace WebCore {

class QtAbstractWebPopup;
class PopupMenuClient;

class QtAbstractWebPopupFactory {
public:
    virtual QtAbstractWebPopup* create(PopupMenuClient* client) = 0;
};

class QtAbstractWebPopup {
public:
    QtAbstractWebPopup(PopupMenuClient* client);
    virtual ~QtAbstractWebPopup();

    virtual void show(const QRect& geometry, int selectedIndex) = 0;
    virtual void hide() = 0;

    static void setFactory(QtAbstractWebPopupFactory* factory);
    static QtAbstractWebPopup* create(PopupMenuClient* client);

protected:
    PopupMenuClient* client();

private:
    PopupMenuClient* m_client;
    static QtAbstractWebPopupFactory* m_factory;
};

}

#endif // QtAbstractWebPopup_h
