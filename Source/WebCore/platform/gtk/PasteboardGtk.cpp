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
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "URL.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

enum ClipboardDataType {
    ClipboardDataTypeText,
    ClipboardDataTypeMarkup,
    ClipboardDataTypeURIList,
    ClipboardDataTypeURL,
    ClipboardDataTypeImage,
    ClipboardDataTypeUnknown
};

std::unique_ptr<Pasteboard> Pasteboard::createForCopyAndPaste()
{
    return std::make_unique<Pasteboard>("CLIPBOARD");
}

std::unique_ptr<Pasteboard> Pasteboard::createForGlobalSelection()
{
    return std::make_unique<Pasteboard>("PRIMARY");
}

std::unique_ptr<Pasteboard> Pasteboard::createPrivate()
{
    return std::make_unique<Pasteboard>(DataObjectGtk::create());
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return std::make_unique<Pasteboard>(DataObjectGtk::create());
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    return std::make_unique<Pasteboard>(dragData.platformData());
}
#endif

// Making this non-inline so that WebKit 2's decoding doesn't have to include Image.h.
PasteboardImage::PasteboardImage()
{
}

PasteboardImage::~PasteboardImage()
{
}

Pasteboard::Pasteboard(RefPtr<DataObjectGtk>&& dataObject)
    : m_dataObject(WTFMove(dataObject))
{
    ASSERT(m_dataObject);
}

Pasteboard::Pasteboard(const String& name)
    : m_dataObject(DataObjectGtk::create())
    , m_name(name)
{
}

Pasteboard::~Pasteboard()
{
}

const DataObjectGtk& Pasteboard::dataObject() const
{
    return *m_dataObject;
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

void Pasteboard::writeToClipboard(ShouldIncludeSmartPaste shouldIncludeSmartPaste)
{
    if (m_name.isNull())
        return;
    m_dataObject->setCanSmartReplace(shouldIncludeSmartPaste == ShouldIncludeSmartPaste::Yes);
    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, m_dataObject);
}

void Pasteboard::readFromClipboard()
{
    if (m_name.isNull())
        return;
    m_dataObject = platformStrategies()->pasteboardStrategy()->readFromClipboard(m_name);
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

    writeToClipboard(smartReplaceOption == CanSmartReplace ? ShouldIncludeSmartPaste::Yes : ShouldIncludeSmartPaste::No);
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());

    m_dataObject->clearAll();
    m_dataObject->setURL(pasteboardURL.url, pasteboardURL.title);

    writeToClipboard();
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

    writeToClipboard();
}

void Pasteboard::write(const PasteboardWebContent& pasteboardContent)
{
    m_dataObject->clearAll();
    m_dataObject->setText(pasteboardContent.text);
    m_dataObject->setMarkup(pasteboardContent.markup);

    writeToClipboard(pasteboardContent.canSmartCopyOrDelete ? ShouldIncludeSmartPaste::Yes : ShouldIncludeSmartPaste::No);
}

void Pasteboard::writePasteboard(const Pasteboard& sourcePasteboard)
{
    const auto& sourceDataObject = sourcePasteboard.dataObject();
    m_dataObject->clearAll();

    if (sourceDataObject.hasText())
        m_dataObject->setText(sourceDataObject.text());
    if (sourceDataObject.hasMarkup())
        m_dataObject->setMarkup(sourceDataObject.markup());
    if (sourceDataObject.hasURL())
        m_dataObject->setURL(sourceDataObject.url(), sourceDataObject.urlLabel());
    if (sourceDataObject.hasURIList())
        m_dataObject->setURIList(sourceDataObject.uriList());
    if (sourceDataObject.hasImage())
        m_dataObject->setImage(sourceDataObject.image());
    if (sourceDataObject.hasUnknownTypeData()) {
        for (auto& it : sourceDataObject.unknownTypes())
            m_dataObject->setUnknownTypeData(it.key, it.value);
    }

    writeToClipboard();
}

void Pasteboard::clear()
{
    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would
    // still contain the "Files" string if any files were included in the drag)."
    m_dataObject->clearAllExceptFilenames();
    writeToClipboard();
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

    writeToClipboard();
}

bool Pasteboard::canSmartReplace()
{
    readFromClipboard();
    return m_dataObject->canSmartReplace();
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImageRef, const IntPoint&)
{
}
#endif

void Pasteboard::read(PasteboardPlainText& text)
{
    readFromClipboard();
    text.text = m_dataObject->text();
}

bool Pasteboard::hasData()
{
    readFromClipboard();
    return m_dataObject->hasText() || m_dataObject->hasMarkup() || m_dataObject->hasURIList() || m_dataObject->hasImage() || m_dataObject->hasUnknownTypeData();
}

Vector<String> Pasteboard::types()
{
    readFromClipboard();

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

    for (auto& key : m_dataObject->unknownTypes().keys())
        types.append(key);

    return types;
}

String Pasteboard::readString(const String& type)
{
    readFromClipboard();

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
    readFromClipboard();
    return m_dataObject->filenames();
}

}
