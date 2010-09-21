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
#include "DragData.h"
#include "Editor.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Image.h"
#include "NotImplemented.h"
#include "Pasteboard.h"
#include "PasteboardHelper.h"
#include "RenderImage.h"
#include "ScriptExecutionContext.h"
#include "markup.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <gtk/gtk.h>

namespace WebCore {

enum ClipboardDataType {
    ClipboardDataTypeText,
    ClipboardDataTypeMarkup,
    ClipboardDataTypeURIList,
    ClipboardDataTypeURL,
    ClipboardDataTypeImage,
    ClipboardDataTypeUnknown
};

PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy, Frame* frame)
{
    return ClipboardGtk::create(policy, gtk_clipboard_get_for_display(gdk_display_get_default(), GDK_SELECTION_CLIPBOARD), frame);
}

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardGtk::create(policy, dragData->platformData(), DragAndDrop, frame);
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, GtkClipboard* clipboard, Frame* frame)
    : Clipboard(policy, CopyAndPaste)
    , m_dataObject(DataObjectGtk::forClipboard(clipboard))
    , m_clipboard(clipboard)
    , m_helper(Pasteboard::generalPasteboard()->helper())
    , m_frame(frame)
{
}

ClipboardGtk::ClipboardGtk(ClipboardAccessPolicy policy, PassRefPtr<DataObjectGtk> dataObject, ClipboardType clipboardType, Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(dataObject)
    , m_clipboard(0)
    , m_helper(Pasteboard::generalPasteboard()->helper())
    , m_frame(frame)
{
}

ClipboardGtk::~ClipboardGtk()
{
}

static ClipboardDataType dataObjectTypeFromHTMLClipboardType(const String& rawType)
{
    String type(rawType.stripWhiteSpace());

    // Two special cases for IE compatibility
    if (type == "Text")
        return ClipboardDataTypeText;
    if (type == "URL")
        return ClipboardDataTypeURL;

    // From the Mac port: Ignore any trailing charset - JS strings are
    // Unicode, which encapsulates the charset issue.
    if (type == "text/plain" || type.startsWith("text/plain;"))
        return ClipboardDataTypeText;
    if (type == "text/html" || type.startsWith("text/html;"))
        return ClipboardDataTypeMarkup;
    if (type == "Files" || type == "text/uri-list" || type.startsWith("text/uri-list;"))
        return ClipboardDataTypeURIList;

    // Not a known type, so just default to using the text portion.
    return ClipboardDataTypeUnknown;
}

void ClipboardGtk::clearData(const String& typeString)
{
    if (policy() != ClipboardWritable)
        return;

    ClipboardDataType type = dataObjectTypeFromHTMLClipboardType(typeString);
    switch (type) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_dataObject->clearURIList();
        break;
    case ClipboardDataTypeMarkup:
        m_dataObject->clearMarkup();
        break;
    case ClipboardDataTypeText:
        m_dataObject->clearText();
        break;
    case ClipboardDataTypeUnknown:
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

    ClipboardDataType type = dataObjectTypeFromHTMLClipboardType(typeString);
    if (type == ClipboardDataTypeURIList) {
        if (!m_dataObject->hasURIList())
            return String();
        success = true;
        return joinURIList(m_dataObject->uriList());
    }

    if (type == ClipboardDataTypeURL) {
        if (!m_dataObject->hasURL())
            return String();
        success = true;
        return m_dataObject->url();
    }

    if (type == ClipboardDataTypeMarkup) {
        if (!m_dataObject->hasMarkup())
            return String();
        success = true;
        return m_dataObject->markup();
    }

    if (type == ClipboardDataTypeText) {
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
    ClipboardDataType type = dataObjectTypeFromHTMLClipboardType(typeString);
    if (type == ClipboardDataTypeURIList || type == ClipboardDataTypeURL) {
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
    } else if (type == ClipboardDataTypeMarkup) {
        m_dataObject->setMarkup(data);
        success = true;
    } else if (type == ClipboardDataTypeText) {
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

void ClipboardGtk::setDragImage(CachedImage* image, const IntPoint& location)
{
    setDragImage(image, 0, location);
}

void ClipboardGtk::setDragImageElement(Node* element, const IntPoint& location)
{
    setDragImage(0, element, location);
}

void ClipboardGtk::setDragImage(CachedImage* image, Node* element, const IntPoint& location)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable)
        return;

    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = location;
    m_dragImageElement = element;
}

DragImageRef ClipboardGtk::createDragImage(IntPoint& location) const
{
    location = m_dragLoc;
    if (!m_dragImage)
        return 0;

    return createDragImageFromImage(m_dragImage->image());
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

void ClipboardGtk::declareAndWriteDragImage(Element* element, const KURL& url, const String& label, Frame* frame)
{
    m_dataObject->setURL(url, label);
    m_dataObject->setMarkup(createMarkup(element, IncludeNode, 0, AbsoluteURLs));

    CachedImage* image = getCachedImage(element);
    if (!image || !image->isLoaded())
        return;

    PlatformRefPtr<GdkPixbuf> pixbuf = adoptPlatformRef(image->image()->getGdkPixbuf());
    if (!pixbuf)
        return;

    m_dataObject->setImage(pixbuf.get());
}

void ClipboardGtk::writeURL(const KURL& url, const String& label, Frame*)
{
    m_dataObject->setURL(url, label);
    if (m_clipboard)
        m_helper->writeClipboardContents(m_clipboard);
}

void ClipboardGtk::writeRange(Range* range, Frame* frame)
{
    ASSERT(range);

    m_dataObject->setText(frame->editor()->selectedText());
    m_dataObject->setMarkup(createMarkup(range, 0, AnnotateForInterchange, false, AbsoluteURLs));

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
