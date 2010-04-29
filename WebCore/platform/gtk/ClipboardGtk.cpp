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
#include "Editor.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "Image.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "PasteboardHelper.h"
#include "RenderImage.h"
#include "StringHash.h"
#include "markup.h"
#include <wtf/text/CString.h>
#include <gtk/gtk.h>

namespace WebCore {

enum ClipboardType {
    ClipboardTypeText,
    ClipboardTypeMarkup,
    ClipboardTypeURIList,
    ClipboardTypeURL,
    ClipboardTypeImage,
    ClipboardTypeUnknown
};

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy)
{
    return ClipboardGtk::create(policy, gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD), false);
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, GtkClipboard* clipboard)
    : Clipboard(policy, false)
    , m_dataObject(DataObjectGtk::forClipboard(clipboard))
    , m_clipboard(clipboard)
    , m_helper(Pasteboard::generalPasteboard()->helper())
{
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, PassRefPtr<DataObjectGtk> dataObject, bool forDragging)
    : Clipboard(policy, forDragging)
    , m_dataObject(dataObject)
    , m_clipboard(0)
    , m_helper(Pasteboard::generalPasteboard()->helper())
{
}

ClipboardGtk::~ClipboardGtk()
{
}

static ClipboardType dataObjectTypeFromHTMLClipboardType(const String& rawType)
{
    String type(rawType.stripWhiteSpace());

    // Two special cases for IE compatibility
    if (type == "Text")
        return ClipboardTypeText;
    if (type == "URL")
        return ClipboardTypeURL;

    // From the Mac port: Ignore any trailing charset - JS strings are
    // Unicode, which encapsulates the charset issue.
    if (type == "text/plain" || type.startsWith("text/plain;"))
        return ClipboardTypeText;
    if (type == "text/html" || type.startsWith("text/html;"))
        return ClipboardTypeMarkup;
    if (type == "Files" || type == "text/uri-list" || type.startsWith("text/uri-list;"))
        return ClipboardTypeURIList;

    // Not a known type, so just default to using the text portion.
    return ClipboardTypeUnknown;
}

void ClipboardGtk::clearData(const String& typeString)
{
    if (policy() != ClipboardWritable)
        return;

    ClipboardType type = dataObjectTypeFromHTMLClipboardType(typeString);
    switch (type) {
    case ClipboardTypeURIList:
    case ClipboardTypeURL:
        m_dataObject->clearURIList();
        break;
    case ClipboardTypeMarkup:
        m_dataObject->clearMarkup();
        break;
    case ClipboardTypeText:
        m_dataObject->clearText();
        break;
    case ClipboardTypeUnknown:
    default:
        m_dataObject->clear();
    }

    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}


void ClipboardGtk::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    m_dataObject->clear();

    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}

static String joinURIList(Vector<KURL> uriList)
{
    if (uriList.isEmpty())
        return String();

    String joined(uriList[0].string());
    for (size_t i = 1; i < uriList.size(); i++) {
        joined.append("\r\n");
        joined.append(uriList[i].string());
    }

    return joined;
}

String ClipboardGtk::getData(const String& typeString, bool& success) const
{
    success = false; // Pessimism.
    if (policy() != ClipboardReadable || !m_dataObject)
        return String();

    if (m_clipboard)
        m_helper->getClipboardContents(m_clipboard);

    ClipboardType type = dataObjectTypeFromHTMLClipboardType(typeString);
    if (type == ClipboardTypeURIList) {
        if (!m_dataObject->hasURIList())
            return String();
        success = true;
        return joinURIList(m_dataObject->uriList());
    }

    if (type == ClipboardTypeURL) {
        if (!m_dataObject->hasURL())
            return String();
        success = true;
        return m_dataObject->url();
    }

    if (type == ClipboardTypeMarkup) {
        if (!m_dataObject->hasMarkup())
            return String();
        success = true;
        return m_dataObject->markup();
    }

    if (type == ClipboardTypeText) {
        if (!m_dataObject->hasText())
                return String();
        success = true;
        return m_dataObject->text();
    }

    return String();
}

bool ClipboardGtk::setData(const String& typeString, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;

    bool success = false;
    ClipboardType type = dataObjectTypeFromHTMLClipboardType(typeString);
    if (type == ClipboardTypeURIList || type == ClipboardTypeURL) {
        Vector<KURL> uriList;
        gchar** uris = g_uri_list_extract_uris(data.utf8().data());
        if (uris) {
            gchar** currentURI = uris;
            while (*currentURI) {
                uriList.append(KURL(KURL(), *currentURI));
                currentURI++;
            }
            g_strfreev(uris);
            m_dataObject->setURIList(uriList);
            success = true;
        }
    } else if (type == ClipboardTypeMarkup) {
        m_dataObject->setMarkup(data);
        success = true;
    } else if (type == ClipboardTypeText) {
        m_dataObject->setText(data);
        success = true;
    }

    if (success && m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);

    return success;
}

HashSet<String> ClipboardGtk::types() const
{
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return HashSet<String>();

    if (m_clipboard)
        m_helper->getClipboardContents(m_clipboard);

    HashSet<String> types;
    if (m_dataObject->hasText()) {
        types.add("text/plain");
        types.add("Text");
    }

    if (m_dataObject->hasMarkup())
        types.add("text/html");

    if (m_dataObject->hasURIList()) {
        types.add("text/uri-list");
        types.add("URL");
        types.add("Files");
    }

    return types;
}

PassRefPtr<FileList> ClipboardGtk::files() const
{
    if (policy() != ClipboardReadable)
        return FileList::create();

    if (m_clipboard)
        m_helper->getClipboardContents(m_clipboard);

    RefPtr<FileList> fileList = FileList::create();
    Vector<String> fileVector(m_dataObject->files());

    for (size_t i = 0; i < fileVector.size(); i++)
        fileList->append(File::create(fileVector[i]));

    return fileList.release();
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
    String actualLabel(label);
    if (actualLabel.isEmpty())
        actualLabel = url;

    m_dataObject->setText(url.string());

    Vector<UChar> markup;
    append(markup, "<a href=\"");
    append(markup, url.string());
    append(markup, "\">");
    append(markup, label);
    append(markup, "</a>");
    m_dataObject->setMarkup(String::adopt(markup));

    Vector<KURL> uriList;
    uriList.append(url);
    m_dataObject->setURIList(uriList);

    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}

void ClipboardGtk::writeRange(Range* range, Frame* frame)
{
    ASSERT(range);

    m_dataObject->setText(frame->selectedText());
    m_dataObject->setMarkup(createMarkup(range, 0, AnnotateForInterchange));

    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}

void ClipboardGtk::writePlainText(const String& text)
{
    m_dataObject->setText(text);

    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}

bool ClipboardGtk::hasData()
{
    if (m_clipboard)
        m_helper->getClipboardContents(m_clipboard);

    return m_dataObject->hasText() || m_dataObject->hasMarkup()
        || m_dataObject->hasURIList() || m_dataObject->hasImage();
}

}
