/*
 *  Copyright (C) 2016 Red Hat Inc.
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

#include "config.h"
#include "PlatformPasteboard.h"

#include "Color.h"
#include "PasteboardHelper.h"
#include "SelectionData.h"
#include "SharedBuffer.h"
#include <gtk/gtk.h>
#include <wtf/URL.h>

namespace WebCore {

#if USE(GTK4)
PlatformPasteboard::PlatformPasteboard(const String&)
{
}
#else
PlatformPasteboard::PlatformPasteboard(const String& pasteboardName)
    : m_clipboard(gtk_clipboard_get(gdk_atom_intern(pasteboardName.utf8().data(), TRUE)))
{
    ASSERT(m_clipboard);
}
#endif

void PlatformPasteboard::writeToClipboard(const SelectionData& selection, WTF::Function<void()>&& primarySelectionCleared)
{
#if !USE(GTK4)
    PasteboardHelper::singleton().writeClipboardContents(m_clipboard, selection, gtk_clipboard_get(GDK_SELECTION_PRIMARY) == m_clipboard ? WTFMove(primarySelectionCleared) : nullptr);
#endif
}

Ref<SelectionData> PlatformPasteboard::readFromClipboard()
{
    Ref<SelectionData> selection(SelectionData::create());
#if !USE(GTK4)
    PasteboardHelper::singleton().getClipboardContents(m_clipboard, selection.get());
#endif
    return selection;
}

Vector<String> PlatformPasteboard::typesSafeForDOMToReadAndWrite(const String&) const
{
    return { };
}

int64_t PlatformPasteboard::write(const PasteboardCustomData&)
{
    return 0;
}

int64_t PlatformPasteboard::write(const Vector<PasteboardCustomData>&)
{
    return 0;
}

}
