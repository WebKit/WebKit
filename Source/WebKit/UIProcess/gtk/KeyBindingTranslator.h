/*
 * Copyright (C) 2010, 2011, 2020 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GtkWidget GtkWidget;

#if USE(GTK4)
typedef struct _GtkEventControllerKey GtkEventControllerKey;
#else
typedef struct _GdkEventKey GdkEventKey;
#endif

namespace WebKit {

class KeyBindingTranslator {
public:
    KeyBindingTranslator();
    ~KeyBindingTranslator();

    GtkWidget* widget() const { return m_nativeWidget.get(); }
    void invalidate() { m_nativeWidget = nullptr; }

#if USE(GTK4)
    Vector<String> commandsForKeyEvent(GtkEventControllerKey*);
#else
    Vector<String> commandsForKeyEvent(GdkEventKey*);
#endif
    Vector<String> commandsForKeyval(unsigned keyval, unsigned modifiers);

    void addPendingEditorCommand(const char* command) { m_pendingEditorCommands.append(String::fromLatin1(command)); }

private:
    GRefPtr<GtkWidget> m_nativeWidget;
    Vector<String> m_pendingEditorCommands;
};

} // namespace WebKit

