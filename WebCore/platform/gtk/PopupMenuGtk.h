/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef PopupMenuGtk_h
#define PopupMenuGtk_h

#include "GRefPtrGtk.h"
#include "IntRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include <glib.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class FrameView;
class Scrollbar;

class PopupMenuGtk : public PopupMenu {
public:
    PopupMenuGtk(PopupMenuClient*);
    ~PopupMenuGtk();

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

private:
    PopupMenuClient* client() const { return m_popupClient; }

    static void menuItemActivated(GtkMenuItem* item, PopupMenuGtk*);
    static void menuUnmapped(GtkWidget*, PopupMenuGtk*);
    static void menuPositionFunction(GtkMenu*, gint*, gint*, gboolean*, PopupMenuGtk*);
    static void menuRemoveItem(GtkWidget*, PopupMenuGtk*);

    PopupMenuClient* m_popupClient;
    IntPoint m_menuPosition;
    PlatformRefPtr<GtkMenu> m_popup;
    HashMap<GtkWidget*, int> m_indexMap;
};

}

#endif // PopupMenuGtk_h
