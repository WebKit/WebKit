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
#include "DragData.h"
#include "Image.h"
#include "URL.h"
#include "PasteboardHelper.h"
#include <gtk/gtk.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

enum ClipboardDataType {
    ClipboardDataTypeText,
    ClipboardDataTypeMarkup,
    ClipboardDataTypeURIList,
    ClipboardDataTypeURL,
    ClipboardDataTypeImage,
    ClipboardDataTypeUnknown
};

PassOwnPtr<Pasteboard> Pasteboard::create(GtkClipboard* gtkClipboard)
{
    return adoptPtr(new Pasteboard(gtkClipboard));
}

PassOwnPtr<Pasteboard> Pasteboard::create(PassRefPtr<DataObjectGtk> dataObject)
{
    return adoptPtr(new Pasteboard(dataObject));
}

PassOwnPtr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return create(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
}

PassOwnPtr<Pasteboard> Pasteboard::createForGlobalSelection()
{
    return create(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
}

PassOwnPtr<Pasteboard> Pasteboard::createPrivate()
{
    return create(DataObjectGtk::create());
}

#if ENABLE(DRAG_SUPPORT)
PassOwnPtr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return create(DataObjectGtk::create());
}

PassOwnPtr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return create(dragData.platformData());
}
#endif

// Making this non-inline so that WebKit 2's decoding doesn't have to include Image.h.
PasteboardImage::PasteboardImage()
{
}

PasteboardImage::~PasteboardImage()
{
}

Pasteboard::Pasteboard(PassRefPtr<DataObjectGtk> dataObject)
    : m_dataObject(dataObject)
    , m_gtkClipboard(0)
{
    ASSERT(m_dataObject);
}

Pasteboard::Pasteboard(GtkClipboard* gtkClipboard)
    : m_dataObject(DataObjectGtk::forClipboard(gtkClipboard))
    , m_gtkClipboard(gtkClipboard)
{
    ASSERT(m_dataObject);
}

Pasteboard::~Pasteboard()
{
}

PassRefPtr<DataObjectGtk> Pasteboard::dataObject() const
{
    return m_dataObject;
}

static ClipboardDataType dataObjectTypeFromHTMLClipboardType(const String& rawType)
{
    String type(rawType.stripWhiteSpace());

    // Two special cases for IE compatibility
    if (type == "Text" || type == "text")
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

void Pasteboard::writeString(const String& type, const String& data)
{
    switch (dataObjectTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_dataObject->setURIList(data);
        return;
    case ClipboardDataTypeMarkup:
        m_dataObject->setMarkup(data);
        return;
    case ClipboardDataTypeText:
        m_dataObject->setText(data);
        return;
    case ClipboardDataTypeUnknown:
        m_dataObject->setUnknownTypeData(type, data);
        return;
    case ClipboardDataTypeImage:
        break;
    }
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    m_dataObject->clearAll();
    m_dataObject->setText(text);

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard, (smartReplaceOption == CanSmartReplace) ? PasteboardHelper::IncludeSmartPaste : PasteboardHelper::DoNotIncludeSmartPaste);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());

    m_dataObject->clearAll();
    m_dataObject->setURL(pasteboardURL.url, pasteboardURL.title);

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    m_dataObject->clearAll();
    if (!pasteboardImage.url.url.isEmpty()) {
        m_dataObject->setURL(pasteboardImage.url.url, pasteboardImage.url.title);
        m_dataObject->setMarkup(pasteboardImage.url.markup);
    }

    GRefPtr<GdkPixbuf> pixbuf = adoptGRef(pasteboardImage.image->getGdkPixbuf());
    if (pixbuf)
        m_dataObject->setImage(pixbuf.get());

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::write(const PasteboardWebContent& pasteboardContent)
{
    m_dataObject->clearAll();
    m_dataObject->setText(pasteboardContent.text);
    m_dataObject->setMarkup(pasteboardContent.markup);

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard, pasteboardContent.canSmartCopyOrDelete ? PasteboardHelper::IncludeSmartPaste : PasteboardHelper::DoNotIncludeSmartPaste, pasteboardContent.callback.get());
}

void Pasteboard::writePasteboard(const Pasteboard& sourcePasteboard)
{
    RefPtr<DataObjectGtk> sourceDataObject = sourcePasteboard.dataObject();
    m_dataObject->clearAll();

    if (sourceDataObject->hasText())
        m_dataObject->setText(sourceDataObject->text());
    if (sourceDataObject->hasMarkup())
        m_dataObject->setMarkup(sourceDataObject->markup());
    if (sourceDataObject->hasURL())
        m_dataObject->setURL(sourceDataObject->url(), sourceDataObject->urlLabel());
    if (sourceDataObject->hasURIList())
        m_dataObject->setURIList(sourceDataObject->uriList());
    if (sourceDataObject->hasImage())
        m_dataObject->setImage(sourceDataObject->image());
    if (sourceDataObject->hasUnknownTypeData()) {
        auto types = m_dataObject->unknownTypes();
        auto end = types.end();
        for (auto it = types.begin(); it != end; ++it)
            m_dataObject->setUnknownTypeData(it->key, it->value);
    }

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::clear()
{
    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would
    // still contain the "Files" string if any files were included in the drag)."
    m_dataObject->clearAllExceptFilenames();

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

void Pasteboard::clear(const String& type)
{
    switch (dataObjectTypeFromHTMLClipboardType(type)) {
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
    case ClipboardDataTypeImage:
        m_dataObject->clearImage();
        break;
    case ClipboardDataTypeUnknown:
        m_dataObject->clearAll();
        break;
    }

    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->writeClipboardContents(m_gtkClipboard);
}

bool Pasteboard::canSmartReplace()
{
    return m_gtkClipboard && PasteboardHelper::defaultPasteboardHelper()->clipboardContentSupportsSmartReplace(m_gtkClipboard);
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImageRef, const IntPoint&)
{
}
#endif

void Pasteboard::read(PasteboardPlainText& text)
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);
    text.text = m_dataObject->text();
}

bool Pasteboard::hasData()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    return m_dataObject->hasText() || m_dataObject->hasMarkup() || m_dataObject->hasURIList() || m_dataObject->hasImage() || m_dataObject->hasUnknownTypeData();
}

Vector<String> Pasteboard::types()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    Vector<String> types;
    if (m_dataObject->hasText()) {
        types.append(ASCIILiteral("text/plain"));
        types.append(ASCIILiteral("Text"));
        types.append(ASCIILiteral("text"));
    }

    if (m_dataObject->hasMarkup())
        types.append(ASCIILiteral("text/html"));

    if (m_dataObject->hasURIList()) {
        types.append(ASCIILiteral("text/uri-list"));
        types.append(ASCIILiteral("URL"));
    }

    if (m_dataObject->hasFilenames())
        types.append(ASCIILiteral("Files"));

    auto unknownTypes = m_dataObject->unknownTypes();
    auto end = unknownTypes.end();
    for (auto it = unknownTypes.begin(); it != end; ++it)
        types.append(it->key);

    return types;
}

String Pasteboard::readString(const String& type)
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    switch (dataObjectTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
        return m_dataObject->uriList();
    case ClipboardDataTypeURL:
        return m_dataObject->url();
    case ClipboardDataTypeMarkup:
        return m_dataObject->markup();
    case ClipboardDataTypeText:
        return m_dataObject->text();
    case ClipboardDataTypeUnknown:
        return m_dataObject->unknownTypeData(type);
    case ClipboardDataTypeImage:
        break;
    }

    return String();
}

Vector<String> Pasteboard::readFilenames()
{
    if (m_gtkClipboard)
        PasteboardHelper::defaultPasteboardHelper()->getClipboardContents(m_gtkClipboard);

    return m_dataObject->filenames();
}

}
