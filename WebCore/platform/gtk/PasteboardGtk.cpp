/*
 *  Copyright (C) 2007 Holger Hans Peter Freyther
 *  Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Pasteboard.h"

#include "DataObjectGtk.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "TextResourceDecoder.h"
#include "Image.h"
#include "RenderImage.h"
#include "KURL.h"
#include "markup.h"
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

#include <gtk/gtk.h>

namespace WebCore {

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
    notImplemented();
}

Pasteboard::~Pasteboard()
{
    delete m_helper;
}

PasteboardHelper* Pasteboard::helper()
{
    return m_helper;
}

void Pasteboard::setHelper(PasteboardHelper* helper)
{
    m_helper = helper;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    GtkClipboard* clipboard = m_helper->getClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->setText(frame->editor()->selectedText());
    dataObject->setMarkup(createMarkup(selectedRange, 0, AnnotateForInterchange, false, AbsoluteURLs));
    m_helper->writeClipboardContents(clipboard);
}

void Pasteboard::writePlainText(const String& text)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->setText(text);
    m_helper->writeClipboardContents(clipboard);
}

void Pasteboard::writeURL(const KURL& url, const String& label, Frame* frame)
{
    if (url.isEmpty())
        return;

    GtkClipboard* clipboard = m_helper->getClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->setURL(url, label);
    m_helper->writeClipboardContents(clipboard);
}

void Pasteboard::writeImage(Node* node, const KURL&, const String&)
{
    GtkClipboard* clipboard = gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

    ASSERT(node && node->renderer() && node->renderer()->isImage());
    RenderImage* renderer = toRenderImage(node->renderer());
    CachedImage* cachedImage = renderer->cachedImage();
    if (!cachedImage || cachedImage->errorOccurred())
        return;
    Image* image = cachedImage->image();
    ASSERT(image);

    PlatformRefPtr<GdkPixbuf> pixbuf = adoptPlatformRef(image->getGdkPixbuf());
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    dataObject->setImage(pixbuf.get());
    m_helper->writeClipboardContents(clipboard);
}

void Pasteboard::clear()
{
    gtk_clipboard_clear(gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD));
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    GtkClipboard* clipboard = m_helper->getCurrentClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);
    m_helper->getClipboardContents(clipboard);

    chosePlainText = false;

    if (dataObject->hasMarkup()) {
        RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), dataObject->markup(), "", FragmentScriptingNotAllowed);
        if (fragment)
            return fragment.release();
    }

    if (!allowPlainText)
        return 0;

    if (dataObject->hasText()) {
        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), dataObject->text());
        if (fragment)
            return fragment.release();
    }

    return 0;
}

String Pasteboard::plainText(Frame* frame)
{
    GtkClipboard* clipboard = m_helper->getCurrentClipboard(frame);
    DataObjectGtk* dataObject = DataObjectGtk::forClipboard(clipboard);

    m_helper->getClipboardContents(clipboard);
    return dataObject->text();
}

}
