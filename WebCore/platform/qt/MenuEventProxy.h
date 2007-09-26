/*
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
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
 */


#ifndef MENUEVENTPROXY_H
#define MENUEVENTPROXY_H

#include "Platform.h"
#include <qobject.h>
#include <qmap.h>
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "ContextMenuController.h"

namespace WebCore {
class MenuEventProxy : public QObject {
    Q_OBJECT
    public:
        MenuEventProxy(WebCore::ContextMenu *m) : m_m(m) {}
        ~MenuEventProxy() {}

        void map(QAction* action, unsigned actionTag) { _map[action] = actionTag; }

    public slots:
        void trigger(QAction *action) {
            WebCore::ContextMenuItem item(WebCore::ActionType, static_cast<WebCore::ContextMenuAction>(_map[action]), WebCore::String()); 
            m_m->controller()->contextMenuItemSelected(&item);
        }

    private:
        WebCore::ContextMenu *m_m;
        QMap<QAction*, unsigned> _map;
};

}

#endif
// vim: ts=4 sw=4 et
