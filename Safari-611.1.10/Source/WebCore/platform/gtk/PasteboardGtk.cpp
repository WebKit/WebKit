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
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "SharedBuffer.h"
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
    return makeUnique<Pasteboard>(SelectionData());
}

std::unique_ptr<Pasteboard> Pasteboard::createForDragAndDrop(const DragData& dragData)
{
    ASSERT(dragData.platformData());
    return makeUnique<Pasteboard>(*dragData.platformData());
}

Pasteboard::Pasteboard(SelectionData&& selectionData)
    : m_selectionData(WTFMove(selectionData))
{
}
#endif

Pasteboard::Pasteboard(SelectionData& selectionData)
    : m_selectionData(selectionData)
{
}

Pasteboard::Pasteboard(const String& name)
    : m_name(name)
{
}

Pasteboard::Pasteboard() = default;
Pasteboard::~Pasteboard() = default;

const SelectionData& Pasteboard::selectionData() const
{
    ASSERT(m_selectionData);
    return *m_selectionData;
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

void Pasteboard::writeString(const String& type, const String& data)
{
    ASSERT(m_selectionData);
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
    case ClipboardDataTypeImage:
        break;
    }
}

void Pasteboard::writePlainText(const String& text, SmartReplaceOption smartReplaceOption)
{
    if (m_selectionData) {
        m_selectionData->clearAll();
        m_selectionData->setText(text);
        m_selectionData->setCanSmartReplace(smartReplaceOption == CanSmartReplace);
    } else {
        SelectionData data;
        data.setText(text);
        data.setCanSmartReplace(smartReplaceOption == CanSmartReplace);
        platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
    }
}

void Pasteboard::write(const PasteboardURL& pasteboardURL)
{
    ASSERT(!pasteboardURL.url.isEmpty());
    if (m_selectionData) {
        m_selectionData->clearAll();
        m_selectionData->setURL(pasteboardURL.url, pasteboardURL.title);
    } else {
        SelectionData data;
        data.setURL(pasteboardURL.url, pasteboardURL.title);
        platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
    }
}

void Pasteboard::writeTrustworthyWebURLsPboardType(const PasteboardURL&)
{
    notImplemented();
}

void Pasteboard::write(const PasteboardImage& pasteboardImage)
{
    if (m_selectionData) {
        m_selectionData->clearAll();
        if (!pasteboardImage.url.url.isEmpty()) {
            m_selectionData->setURL(pasteboardImage.url.url, pasteboardImage.url.title);
            m_selectionData->setMarkup(pasteboardImage.url.markup);
        }
        m_selectionData->setImage(pasteboardImage.image.get());
    } else {
        SelectionData data;
        if (!pasteboardImage.url.url.isEmpty()) {
            data.setURL(pasteboardImage.url.url, pasteboardImage.url.title);
            data.setMarkup(pasteboardImage.url.markup);
        }
        data.setImage(pasteboardImage.image.get());
        platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
    }
}

void Pasteboard::write(const PasteboardWebContent& pasteboardContent)
{
    if (m_selectionData) {
        m_selectionData->clearAll();
        m_selectionData->setText(pasteboardContent.text);
        m_selectionData->setMarkup(pasteboardContent.markup);
        m_selectionData->setCanSmartReplace(pasteboardContent.canSmartCopyOrDelete);
        PasteboardCustomData customData;
        customData.setOrigin(pasteboardContent.contentOrigin);
        m_selectionData->setCustomData(customData.createSharedBuffer());
    } else {
        SelectionData data;
        data.setText(pasteboardContent.text);
        data.setMarkup(pasteboardContent.markup);
        data.setCanSmartReplace(pasteboardContent.canSmartCopyOrDelete);
        PasteboardCustomData customData;
        customData.setOrigin(pasteboardContent.contentOrigin);
        data.setCustomData(customData.createSharedBuffer());
        platformStrategies()->pasteboardStrategy()->writeToClipboard(m_name, WTFMove(data));
    }
}

void Pasteboard::clear()
{
    if (!m_selectionData) {
        platformStrategies()->pasteboardStrategy()->clearClipboard(m_name);
        return;
    }

    // We do not clear filenames. According to the spec: "The clearData() method
    // does not affect whether any files were included in the drag, so the types
    // attribute's list might still not be empty after calling clearData() (it would
    // still contain the "Files" string if any files were included in the drag)."
    m_selectionData->clearAllExceptFilenames();
}

void Pasteboard::clear(const String& type)
{
    ASSERT(m_selectionData);
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
}

bool Pasteboard::canSmartReplace()
{
    if (m_selectionData)
        return m_selectionData->canSmartReplace();
    return platformStrategies()->pasteboardStrategy()->types(m_name).contains("application/vnd.webkitgtk.smartpaste"_s);
}

#if ENABLE(DRAG_SUPPORT)
void Pasteboard::setDragImage(DragImage, const IntPoint&)
{
}
#endif

void Pasteboard::read(PasteboardPlainText& text, PlainTextURLReadingPolicy, Optional<size_t>)
{
    text.text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name);
}

void Pasteboard::read(PasteboardWebContentReader& reader, WebContentReadingPolicy policy, Optional<size_t>)
{
    reader.contentOrigin = readOrigin();

    if (m_selectionData) {
        if (m_selectionData->hasMarkup() && reader.readHTML(m_selectionData->markup()))
            return;

        if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
            return;

        if (m_selectionData->hasFilenames() && reader.readFilePaths(m_selectionData->filenames()))
            return;

        if (m_selectionData->hasText() && reader.readPlainText(m_selectionData->text()))
            return;

        return;
    }

    auto types = platformStrategies()->pasteboardStrategy()->types(m_name);
    if (types.contains("text/html"_s)) {
        auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, "text/html"_s);
        if (buffer && reader.readHTML(String::fromUTF8(buffer->data(), buffer->size())))
            return;
    }

    if (policy == WebContentReadingPolicy::OnlyRichTextTypes)
        return;

    static const ASCIILiteral imageTypes[] = { "image/png"_s, "image/jpeg"_s, "image/gif"_s, "image/bmp"_s, "image/vnd.microsoft.icon"_s, "image/x-icon"_s };
    for (const auto& imageType : imageTypes) {
        if (types.contains(imageType)) {
            auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, imageType);
            if (!buffer->isEmpty() && reader.readImage(buffer.releaseNonNull(), imageType))
                return;
        }
    }

    if (types.contains("text/uri-list"_s)) {
        auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
        if (reader.readFilePaths(filePaths))
            return;
    }

    if (types.contains("text/plain"_s) || types.contains("text/plain;charset=utf-8"_s)) {
        auto text = platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name);
        if (!text.isNull() && reader.readPlainText(text))
            return;
    }
}

void Pasteboard::read(PasteboardFileReader& reader, Optional<size_t>)
{
    if (m_selectionData) {
        for (const auto& filePath : m_selectionData->filenames())
            reader.readFilename(filePath);
        return;
    }

    auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
    for (const auto& filePath : filePaths)
        reader.readFilename(filePath);
}

bool Pasteboard::hasData()
{
    if (m_selectionData)
        return m_selectionData->hasText() || m_selectionData->hasMarkup() || m_selectionData->hasURIList() || m_selectionData->hasImage() || m_selectionData->hasCustomData();
    return !platformStrategies()->pasteboardStrategy()->types(m_name).isEmpty();
}

Vector<String> Pasteboard::typesSafeForBindings(const String& origin)
{
    if (m_selectionData) {
        ListHashSet<String> types;
        if (auto* buffer = m_selectionData->customData()) {
            auto customData = PasteboardCustomData::fromSharedBuffer(*buffer);
            if (customData.origin() == origin) {
                for (auto& type : customData.orderedTypes())
                    types.add(type);
            }
        }

        if (m_selectionData->hasText())
            types.add("text/plain"_s);
        if (m_selectionData->hasMarkup())
            types.add("text/html"_s);
        if (m_selectionData->hasURIList())
            types.add("text/uri-list"_s);

        return copyToVector(types);
    }

    return platformStrategies()->pasteboardStrategy()->typesSafeForDOMToReadAndWrite(m_name, origin);
}

Vector<String> Pasteboard::typesForLegacyUnsafeBindings()
{
    if (!m_selectionData)
        return platformStrategies()->pasteboardStrategy()->types(m_name);

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

    return types;
}

String Pasteboard::readOrigin()
{
    if (m_selectionData) {
        if (auto* buffer = m_selectionData->customData())
            return PasteboardCustomData::fromSharedBuffer(*buffer).origin();

        return { };
    }

    // FIXME: cache custom data?
    if (auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, PasteboardCustomData::gtkType()))
        return PasteboardCustomData::fromSharedBuffer(*buffer).origin();

    return { };
}

String Pasteboard::readString(const String& type)
{
    if (!m_selectionData) {
        if (type.startsWith("text/plain"))
            return platformStrategies()->pasteboardStrategy()->readTextFromClipboard(m_name);

        auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, type);
        return buffer ? String::fromUTF8(buffer->data(), buffer->size()) : String();
    }

    switch (selectionDataTypeFromHTMLClipboardType(type)) {
    case ClipboardDataTypeURIList:
        return m_selectionData->uriList();
    case ClipboardDataTypeURL:
        return m_selectionData->url().string();
    case ClipboardDataTypeMarkup:
        return m_selectionData->markup();
    case ClipboardDataTypeText:
        return m_selectionData->text();
    case ClipboardDataTypeUnknown:
    case ClipboardDataTypeImage:
        break;
    }

    return { };
}

String Pasteboard::readStringInCustomData(const String& type)
{
    if (m_selectionData) {
        if (auto* buffer = m_selectionData->customData())
            return PasteboardCustomData::fromSharedBuffer(*buffer).readStringInCustomData(type);

        return { };
    }

    // FIXME: cache custom data?
    if (auto buffer = platformStrategies()->pasteboardStrategy()->readBufferFromClipboard(m_name, PasteboardCustomData::gtkType()))
        return PasteboardCustomData::fromSharedBuffer(*buffer).readStringInCustomData(type);

    return { };
}

Pasteboard::FileContentState Pasteboard::fileContentState()
{
    if (m_selectionData)
        return m_selectionData->filenames().isEmpty() ? FileContentState::NoFileOrImageData : FileContentState::MayContainFilePaths;

    auto types = platformStrategies()->pasteboardStrategy()->types(m_name);
    if (types.contains("text/uri-list"_s)) {
        auto filePaths = platformStrategies()->pasteboardStrategy()->readFilePathsFromClipboard(m_name);
        if (!filePaths.isEmpty())
            return FileContentState::MayContainFilePaths;
    }

    auto result = types.findMatching([](const String& type) {
        return MIMETypeRegistry::isSupportedImageMIMEType(type);
    });
    return result == notFound ? FileContentState::NoFileOrImageData : FileContentState::MayContainFilePaths;
}

void Pasteboard::writeMarkup(const String&)
{
}

void Pasteboard::writeCustomData(const Vector<PasteboardCustomData>& data)
{
    if (m_selectionData) {
        if (!data.isEmpty()) {
            const auto& customData = data[0];
            customData.forEachPlatformString([this] (auto& type, auto& string) {
                writeString(type, string);
            });
            if (customData.hasSameOriginCustomData() || !customData.origin().isEmpty())
                m_selectionData->setCustomData(customData.createSharedBuffer());
        }
        return;
    }

    platformStrategies()->pasteboardStrategy()->writeCustomData(data, m_name);
}

void Pasteboard::write(const Color&)
{
}

}
