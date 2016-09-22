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

#include "DragData.h"
#include "Image.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SelectionData.h"
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
    return std::make_unique<Pasteboard>(SelectionData::create());
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return std::make_unique<Pasteboard>(SelectionData::create());
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    ASSERT(dragData.platformData());
    return std::make_unique<Pasteboard>(*dragData.platformData());
}
#endif

// Making this non-inline so that WebKit 2's decoding doesn't have to include Image.h.
PasteboardImage::PasteboardImage()
{
}

PasteboardImage::~PasteboardImage()
{
}

Pasteboard::Pasteboard(SelectionData& selectionData)
    : m_selectionData(selectionData)
{
}

Pasteboard::Pasteboard(const String& name)
    : m_selectionData(SelectionData::create())
    , m_name(name)
{
}

Pasteboard::~Pasteboard()
{
}

const SelectionData& Pasteboard::selectionData() const
{
    return m_selectionData.get();
}

static ClipboardDataType selectionDataTypeFromHTMLClipboardType(const String& rawType)
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

void Pasteboard::writeToClipboard()
{
    if (m_name.isNull())
        return;

    platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, m_selectionData);
}

void Pasteboard::readFromClipboard()
{
    if (m_name.isNull())
        return;
    m_selectionData = platformStrategies()->pasteboardStrategy()->readFromClipboard(m_name);
}

void Pasteboard::writeString(const String& type, const String& data)
{
    switch (selectionDataTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_selectionData->setURIList(data);
        return;
    case ClipboardDataTypeMarkup:
        m_selectionData->setMarkup(data);
        return;
    case ClipboardDataTypeText:
        m_selectionData->setText(data);
        return;
    case ClipboardDataTypeUnknown:
        m_selectionData->setUnknownTypeData(type, data);
        return;
    case ClipboardDataTypeImage:
        break;
    }
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    m_selectionData->clearAll();
    m_selectionData->setText(text);
    m_selectionData->setCanSmartReplace(smartReplaceOption == CanSmartReplace);

    writeToClipboard();
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());

    m_selectionData->clearAll();
    m_selectionData->setURL(pasteboardURL.url, pasteboardURL.title);

    writeToClipboard();
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    m_selectionData->clearAll();
    if (!pasteboardImage.url.url.isEmpty()) {
        m_selectionData->setURL(pasteboardImage.url.url, pasteboardImage.url.title);
        m_selectionData->setMarkup(pasteboardImage.url.markup);
    }
    m_selectionData->setImage(pasteboardImage.image.get());

    writeToClipboard();
}

void Pasteboard::write(const PasteboardWebContent& pasteboardContent)
{
    m_selectionData->clearAll();
    m_selectionData->setText(pasteboardContent.text);
    m_selectionData->setMarkup(pasteboardContent.markup);
    m_selectionData->setCanSmartReplace(pasteboardContent.canSmartCopyOrDelete);

    writeToClipboard();
}

void Pasteboard::writePasteboard(const Pasteboard& sourcePasteboard)
{
    const auto& sourceDataObject = sourcePasteboard.selectionData();
    m_selectionData->clearAll();

    if (sourceDataObject.hasText())
        m_selectionData->setText(sourceDataObject.text());
    if (sourceDataObject.hasMarkup())
        m_selectionData->setMarkup(sourceDataObject.markup());
    if (sourceDataObject.hasURL())
        m_selectionData->setURL(sourceDataObject.url(), sourceDataObject.urlLabel());
    if (sourceDataObject.hasURIList())
        m_selectionData->setURIList(sourceDataObject.uriList());
    if (sourceDataObject.hasImage())
        m_selectionData->setImage(sourceDataObject.image());
    if (sourceDataObject.hasUnknownTypeData()) {
        for (auto& it : sourceDataObject.unknownTypes())
            m_selectionData->setUnknownTypeData(it.key, it.value);
    }

    writeToClipboard();
}

void Pasteboard::clear()
{
    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would
    // still contain the "Files" string if any files were included in the drag)."
    m_selectionData->clearAllExceptFilenames();
    writeToClipboard();
}

void Pasteboard::clear(const String& type)
{
    switch (selectionDataTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
    case ClipboardDataTypeURL:
        m_selectionData->clearURIList();
        break;
    case ClipboardDataTypeMarkup:
        m_selectionData->clearMarkup();
        break;
    case ClipboardDataTypeText:
        m_selectionData->clearText();
        break;
    case ClipboardDataTypeImage:
        m_selectionData->clearImage();
        break;
    case ClipboardDataTypeUnknown:
        m_selectionData->clearAll();
        break;
    }

    writeToClipboard();
}

bool Pasteboard::canSmartReplace()
{
    readFromClipboard();
    return m_selectionData->canSmartReplace();
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImageRef, const IntPoint&)
{
}
#endif

void Pasteboard::read(PasteboardPlainText& text)
{
    readFromClipboard();
    text.text = m_selectionData->text();
}

bool Pasteboard::hasData()
{
    readFromClipboard();
    return m_selectionData->hasText() || m_selectionData->hasMarkup() || m_selectionData->hasURIList() || m_selectionData->hasImage() || m_selectionData->hasUnknownTypeData();
}

Vector<String> Pasteboard::types()
{
    readFromClipboard();

    Vector<String> types;
    if (m_selectionData->hasText()) {
        types.append(ASCIILiteral("text/plain"));
        types.append(ASCIILiteral("Text"));
        types.append(ASCIILiteral("text"));
    }

    if (m_selectionData->hasMarkup())
        types.append(ASCIILiteral("text/html"));

    if (m_selectionData->hasURIList()) {
        types.append(ASCIILiteral("text/uri-list"));
        types.append(ASCIILiteral("URL"));
    }

    if (m_selectionData->hasFilenames())
        types.append(ASCIILiteral("Files"));

    for (auto& key : m_selectionData->unknownTypes().keys())
        types.append(key);

    return types;
}

String Pasteboard::readString(const String& type)
{
    readFromClipboard();

    switch (selectionDataTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
        return m_selectionData->uriList();
    case ClipboardDataTypeURL:
        return m_selectionData->url();
    case ClipboardDataTypeMarkup:
        return m_selectionData->markup();
    case ClipboardDataTypeText:
        return m_selectionData->text();
    case ClipboardDataTypeUnknown:
        return m_selectionData->unknownTypeData(type);
    case ClipboardDataTypeImage:
        break;
    }

    return String();
}

Vector<String> Pasteboard::readFilenames()
{
    readFromClipboard();
    return m_selectionData->filenames();
}

}
