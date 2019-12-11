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

#include "Color.h"
#include "DragData.h"
#include "Image.h"
#include "NotImplemented.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SelectionData.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Optional.h>
#include <wtf/URL.h>

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
    return makeUnique<Pasteboard>("CLIPBOARD");
}

std::unique_ptr<Pasteboard> Pasteboard::createForGlobalSelection()
{
    return makeUnique<Pasteboard>("PRIMARY");
}

#if ENABLE(DRAG_SUPPORT)
std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop()
{
    return makeUnique<Pasteboard>(SelectionData::create());
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    ASSERT(dragData.platformData());
    return makeUnique<Pasteboard>(*dragData.platformData());
}
#endif

Pasteboard::Pasteboard(SelectionData& selectionData)
    : m_selectionData(selectionData)
{
}

Pasteboard::Pasteboard(const String& name)
    : m_selectionData(SelectionData::create())
    , m_name(name)
{
}

Pasteboard::Pasteboard()
    : m_selectionData(SelectionData::create())
{
}

Pasteboard::~Pasteboard() = default;

const SelectionData& Pasteboard::selectionData() const
{
    return m_selectionData.get();
}

static ClipboardDataType selectionDataTypeFromHTMLClipboardType(const String& type)
{
    // From the Mac port: Ignore any trailing charset - JS strings are
    // Unicode, which encapsulates the charset issue.
    if (type == "text/plain")
        return ClipboardDataTypeText;
    if (type == "text/html")
        return ClipboardDataTypeMarkup;
    if (type == "Files" || type == "text/uri-list")
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
        break;
    case ClipboardDataTypeMarkup:
        m_selectionData->setMarkup(data);
        break;
    case ClipboardDataTypeText:
        m_selectionData->setText(data);
        break;
    case ClipboardDataTypeUnknown:
        m_selectionData->setUnknownTypeData(type, data);
        break;
    case ClipboardDataTypeImage:
        break;
    }
    writeToClipboard();
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

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
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
void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
}
#endif

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, Optional<size_t>)
{
    readFromClipboard();
    text.text = m_selectionData->text();
}

void Pasteboard::read(PasteboardWebContentReader&, WebContentReadingPolicy, Optional<size_t>)
{
}

void Pasteboard::read(PasteboardFileReader& reader)
{
    readFromClipboard();
    for (auto& filename : m_selectionData->filenames())
        reader.readFilename(filename);
}

bool Pasteboard::hasData()
{
    readFromClipboard();
    return m_selectionData->hasText() || m_selectionData->hasMarkup() || m_selectionData->hasURIList() || m_selectionData->hasImage() || m_selectionData->hasUnknownTypeData();
}

Vector<String> Pasteboard::typesSafeForBindings(const String&)
{
    notImplemented(); // webkit.org/b/177633: [GTK] Move to new Pasteboard API
    return { };
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    readFromClipboard();

    Vector<String> types;
    if (m_selectionData->hasText()) {
        types.append("text/plain"_s);
        types.append("Text"_s);
        types.append("text"_s);
    }

    if (m_selectionData->hasMarkup())
        types.append("text/html"_s);

    if (m_selectionData->hasURIList()) {
        types.append("text/uri-list"_s);
        types.append("URL"_s);
    }

    for (auto& key : m_selectionData->unknownTypes().keys())
        types.append(key);

    return types;
}

String Pasteboard::readOrigin()
{
    notImplemented(); // webkit.org/b/177633: [GTK] Move to new Pasteboard API
    return { };
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

String Pasteboard::readStringInCustomData(const String&)
{
    notImplemented(); // webkit.org/b/177633: [GTK] Move to new Pasteboard API
    return { };
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    readFromClipboard();
    return m_selectionData->filenames().isEmpty() ? FileContentState::NoFileOrImageData : FileContentState::MayContainFilePaths;
}

void Pasteboard::writeMarkup(const String&)
{
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>&)
{
}

void Pasteboard::write(const Color&)
{
}

}
