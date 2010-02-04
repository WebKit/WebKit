/*
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
#include "ClipboardGtk.h"

#include "CachedImage.h"
#include "CString.h"
#include "Editor.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "markup.h"
#include "NotImplemented.h"
#include "RenderImage.h"
#include "StringHash.h"

#include <gtk/gtk.h>

namespace WebCore {

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return ClipboardGtk::create(policy, false);
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
}

ClipboardGtk::~ClipboardGtk()
{
}

void ClipboardGtk::clearData(const String&)
{
    notImplemented();
}

void ClipboardGtk::clearAllData()
{
    notImplemented();
}

String ClipboardGtk::getData(const String&, bool &success) const
{
    notImplemented();
    success = false;
    return String();
}

bool ClipboardGtk::setData(const String&, const String&)
{
    notImplemented();
    return false;
}

HashSet<String> ClipboardGtk::types() const
{
    notImplemented();
    return HashSet<String>();
}

PassRefPtr<FileList> ClipboardGtk::files() const
{
    notImplemented();
    return 0;
}

IntPoint ClipboardGtk::dragLocation() const
{
    notImplemented();
    return IntPoint(0, 0);
}

CachedImage* ClipboardGtk::dragImage() const
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImage(CachedImage*, const IntPoint&)
{
    notImplemented();
}

Node* ClipboardGtk::dragImageElement()
{
    notImplemented();
    return 0;
}

void ClipboardGtk::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardGtk::createDragImage(IntPoint&) const
{
    notImplemented();
    return 0;
}

static CachedImage* getCachedImage(Element* element)
{
    // Attempt to pull CachedImage from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = static_cast<RenderImage*>(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

void ClipboardGtk::declareAndWriteDragImage(Element* element, const KURL& url, const String& label, Frame*)
{
    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->isLoaded())
        return;

    GdkPixbuf* pixbuf = cachedImage->image()->getGdkPixbuf();
    if (!pixbuf)
        return;

    GtkClipboard* imageClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardImage"));
    gtk_clipboard_clear(imageClipboard);

    gtk_clipboard_set_image(imageClipboard, pixbuf);
    g_object_unref(pixbuf);

    writeURL(url, label, 0);
}

void ClipboardGtk::writeURL(const KURL& url, const String& label, Frame*)
{
    GtkClipboard* textClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardText"));
    GtkClipboard* urlClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardUrl"));
    GtkClipboard* urlLabelClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardUrlLabel"));

    gtk_clipboard_clear(textClipboard);
    gtk_clipboard_clear(urlClipboard);
    gtk_clipboard_clear(urlLabelClipboard);

    gtk_clipboard_set_text(textClipboard, url.string().utf8().data(), -1);
    gtk_clipboard_set_text(urlClipboard, url.string().utf8().data(), -1);
    gtk_clipboard_set_text(urlLabelClipboard, label.utf8().data(), -1);
}

void ClipboardGtk::writeRange(Range* range, Frame* frame)
{
    GtkClipboard* textClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardText"));
    GtkClipboard* htmlClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardHtml"));

    gtk_clipboard_clear(textClipboard);
    gtk_clipboard_clear(htmlClipboard);

    gtk_clipboard_set_text(textClipboard, frame->selectedText().utf8().data(), -1);
    gtk_clipboard_set_text(htmlClipboard, createMarkup(range, 0, AnnotateForInterchange).utf8().data(), -1);
}

void ClipboardGtk::writePlainText(const String& text)
{
    GtkClipboard* textClipboard = gtk_clipboard_get(gdk_atom_intern_static_string("WebKitClipboardText"));
    
    gtk_clipboard_clear(textClipboard);
    
    gtk_clipboard_set_text(textClipboard, text.utf8().data(), -1);
}
    
bool ClipboardGtk::hasData()
{
    notImplemented();
    return false;
}

}
