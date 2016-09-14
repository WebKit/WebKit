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
#include "DataObjectGtk.h"
#include "PasteboardHelper.h"
#include "SharedBuffer.h"
#include "URL.h"
#include <gtk/gtk.h>

namespace WebCore {

PlatformPasteboard::PlatformPasteboard(const String& pasteboardName)
    : m_clipboard(gtk_clipboard_get(gdk_atom_intern(pasteboardName.utf8().data(), TRUE)))
{
    ASSERT(m_clipboard);
}

void PlatformPasteboard::writeToClipboard(const RefPtr<DataObjectGtk>& dataObject, std::function<void()>&& primarySelectionCleared)
{
    PasteboardHelper::singleton().writeClipboardContents(m_clipboard, *dataObject, gtk_clipboard_get(GDK_SELECTION_PRIMARY) == m_clipboard ? WTFMove(primarySelectionCleared) : nullptr);
}

RefPtr<DataObjectGtk> PlatformPasteboard::readFromClipboard()
{
    RefPtr<DataObjectGtk> dataObject = DataObjectGtk::create();
    PasteboardHelper::singleton().getClipboardContents(m_clipboard, *dataObject);
    return dataObject;
}

}
