/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DataTransfer.h"

#include "CachedImage.h"
#include "CachedImageClient.h"
#include "CommonAtomStrings.h"
#include "DataTransferItem.h"
#include "DataTransferItemList.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentFragment.h"
#include "DocumentInlines.h"
#include "DragData.h"
#include "Editor.h"
#include "FileList.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLImageElement.h"
#include "Image.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PagePasteboardContext.h"
#include "Pasteboard.h"
#include "Quirks.h"
#include "Settings.h"
#include "StaticPasteboard.h"
#include "WebContentReader.h"
#include "WebCorePasteboardFileReader.h"
#include "markup.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URLParser.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

#if ENABLE(DRAG_SUPPORT)

class DragImageLoader final : public CachedImageClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(DragImageLoader);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DragImageLoader);
    WTF_MAKE_NONCOPYABLE(DragImageLoader);
public:
    explicit DragImageLoader(DataTransfer&);
    void startLoading(CachedResourceHandle<CachedImage>&);
    void stopLoading(CachedResourceHandle<CachedImage>&);
    void moveToDataTransfer(DataTransfer&);

private:
    void imageChanged(CachedImage*, const IntRect*) override;
    WeakRef<DataTransfer> m_dataTransfer;
};

#endif

DataTransfer::DataTransfer(StoreMode mode, std::unique_ptr<Pasteboard> pasteboard, Type type, String&& effectAllowed)
    : m_storeMode(mode)
    , m_pasteboard(WTFMove(pasteboard))
#if ENABLE(DRAG_SUPPORT)
    , m_type(type)
    , m_dropEffect("uninitialized"_s)
    , m_effectAllowed(WTFMove(effectAllowed))
    , m_shouldUpdateDragImage(false)
#endif
{
#if !ENABLE(DRAG_SUPPORT)
    UNUSED_PARAM(effectAllowed);
    ASSERT_UNUSED(type, type != Type::DragAndDropData && type != Type::DragAndDropFiles);
#endif
}

Ref<DataTransfer> DataTransfer::createForCopyAndPaste(const Document& document, StoreMode storeMode, std::unique_ptr<Pasteboard>&& pasteboard)
{
    auto dataTransfer = adoptRef(*new DataTransfer(storeMode, WTFMove(pasteboard)));
    dataTransfer->m_originIdentifier = document.originIdentifierForPasteboard();
    return dataTransfer;
}

Ref<DataTransfer> DataTransfer::create()
{
    return adoptRef(*new DataTransfer(StoreMode::ReadWrite, makeUnique<StaticPasteboard>(), Type::CopyAndPaste, "none"_s));
}

DataTransfer::~DataTransfer()
{
#if ENABLE(DRAG_SUPPORT)
    if (m_dragImageLoader && m_dragImage)
        m_dragImageLoader->stopLoading(m_dragImage);
#endif
}

bool DataTransfer::canReadTypes() const
{
    return m_storeMode == StoreMode::Readonly || m_storeMode == StoreMode::Protected || m_storeMode == StoreMode::ReadWrite;
}

bool DataTransfer::canReadData() const
{
    return m_storeMode == StoreMode::Readonly || m_storeMode == StoreMode::ReadWrite;
}

bool DataTransfer::canWriteData() const
{
    return m_storeMode == StoreMode::ReadWrite;
}

static String normalizeType(const String& type)
{
    if (type.isNull())
        return type;

    auto lowercaseType = type.trim(isASCIIWhitespace).convertToASCIILowercase();
    if (lowercaseType == "text"_s || lowercaseType.startsWith(textPlainContentTypeAtom()))
        return textPlainContentTypeAtom();
    if (lowercaseType == "url"_s || lowercaseType.startsWith("text/uri-list;"_s))
        return "text/uri-list"_s;
    if (lowercaseType.startsWith("text/html;"_s))
        return textHTMLContentTypeAtom();

    return lowercaseType;
}

void DataTransfer::clearData(const String& type)
{
    if (!canWriteData())
        return;

    String normalizedType = normalizeType(type);
    if (normalizedType.isNull())
        m_pasteboard->clear();
    else
        m_pasteboard->clear(normalizedType);
    if (m_itemList)
        m_itemList->didClearStringData(normalizedType);
}

static String readURLsFromPasteboardAsString(Page* page, Pasteboard& pasteboard, Function<bool(const String&)>&& shouldIncludeURL)
{
    StringBuilder urlList;
    auto urlStrings = pasteboard.readAllStrings("text/uri-list"_s);
    if (page) {
        urlStrings = urlStrings.map([&](auto& string) {
            return page->applyLinkDecorationFiltering(string, LinkDecorationFilteringTrigger::Paste);
        });
    }

    urlStrings.removeAllMatching([&](auto& string) {
        return !shouldIncludeURL(string);
    });

    return makeStringByJoining(urlStrings, "\n"_s);
}

String DataTransfer::getDataForItem(Document& document, const String& type) const
{
    if (!canReadData())
        return { };

    auto lowercaseType = type.trim(isASCIIWhitespace).convertToASCIILowercase();
    if (shouldSuppressGetAndSetDataToAvoidExposingFilePaths()) {
        if (lowercaseType == "text/uri-list"_s) {
            return readURLsFromPasteboardAsString(document.page(), *m_pasteboard, [] (auto& urlString) {
                return Pasteboard::canExposeURLToDOMWhenPasteboardContainsFiles(urlString);
            });
        }

        if (lowercaseType == textHTMLContentTypeAtom() && DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
            // If the pasteboard contains files and the page requests 'text/html', we only read from rich text types to prevent file
            // paths from leaking (e.g. from plain text data on the pasteboard) since we sanitize cross-origin markup. However, if
            // custom pasteboard data is disabled, then we can't ensure that the markup we deliver is sanitized, so we fall back to
            // current behavior and return an empty string.
            return readStringFromPasteboard(document, lowercaseType, WebContentReadingPolicy::OnlyRichTextTypes);
        }

        return { };
    }

    return readStringFromPasteboard(document, lowercaseType, WebContentReadingPolicy::AnyType);
}

String DataTransfer::readStringFromPasteboard(Document& document, const String& lowercaseType, WebContentReadingPolicy policy) const
{
    if (!DeprecatedGlobalSettings::customPasteboardDataEnabled())
        return m_pasteboard->readString(lowercaseType);

    // StaticPasteboard is only used to stage data written by websites before being committed to the system pasteboard.
    bool isSameOrigin = is<StaticPasteboard>(*m_pasteboard) || (!m_originIdentifier.isNull() && m_originIdentifier == m_pasteboard->readOrigin());
    if (isSameOrigin) {
        String value = m_pasteboard->readStringInCustomData(lowercaseType);
        if (!value.isNull())
            return value;
    }
    if (!Pasteboard::isSafeTypeForDOMToReadAndWrite(lowercaseType))
        return { };

    if (!is<StaticPasteboard>(*m_pasteboard) && lowercaseType == textHTMLContentTypeAtom()) {
        if (!document.frame())
            return { };
        WebContentMarkupReader reader { document.protectedFrame().releaseNonNull() };
        m_pasteboard->read(reader, policy);
        return reader.takeMarkup();
    }

    if (!is<StaticPasteboard>(*m_pasteboard) && lowercaseType == "text/uri-list"_s) {
        return readURLsFromPasteboardAsString(document.protectedPage().get(), *m_pasteboard, [] (auto&) {
            return true;
        });
    }

    auto string = m_pasteboard->readString(lowercaseType);
    if (RefPtr page = document.page())
        return page->applyLinkDecorationFiltering(string, LinkDecorationFilteringTrigger::Paste);

    return string;
}

String DataTransfer::getData(Document& document, const String& type) const
{
    return getDataForItem(document, normalizeType(type));
}

bool DataTransfer::shouldSuppressGetAndSetDataToAvoidExposingFilePaths() const
{
    if (!forFileDrag() && !DeprecatedGlobalSettings::customPasteboardDataEnabled())
        return false;
    return m_pasteboard->fileContentState() == Pasteboard::FileContentState::MayContainFilePaths;
}

void DataTransfer::setData(Document& document, const String& type, const String& data)
{
    if (!canWriteData())
        return;

    if (shouldSuppressGetAndSetDataToAvoidExposingFilePaths())
        return;

    auto normalizedType = normalizeType(type);
    setDataFromItemList(document, normalizedType, data);
    if (m_itemList)
        m_itemList->didSetStringData(normalizedType);
}

void DataTransfer::setDataFromItemList(Document& document, const String& type, const String& data)
{
    ASSERT(canWriteData());

    auto& pasteboard = downcast<StaticPasteboard>(*m_pasteboard);
    if (!DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
        pasteboard.writeString(type, data);
        return;
    }

    String sanitizedData;
    if (type == "text/html"_s)
        sanitizedData = sanitizeMarkup(data);
    else if (type == "text/uri-list"_s) {
        auto url = URL({ }, data);
        if (url.isValid())
            sanitizedData = url.string();
    } else if (type == textPlainContentTypeAtom())
        sanitizedData = data; // Nothing to sanitize.

    if (type == "text/uri-list"_s || type == textPlainContentTypeAtom()) {
        if (RefPtr page = document.page())
            sanitizedData = page->applyLinkDecorationFiltering(sanitizedData, LinkDecorationFilteringTrigger::Copy);
    }

    if (sanitizedData != data)
        pasteboard.writeStringInCustomData(type, data);

    if (Pasteboard::isSafeTypeForDOMToReadAndWrite(type) && !sanitizedData.isNull())
        pasteboard.writeString(type, sanitizedData);
}

void DataTransfer::updateFileList(ScriptExecutionContext* context)
{
    ASSERT(canWriteData());

    m_fileList->m_files = filesFromPasteboardAndItemList(context);
}

void DataTransfer::didAddFileToItemList()
{
    ASSERT(canWriteData());
    if (!m_fileList)
        return;

    auto& newItem = m_itemList->items().last();
    ASSERT(newItem->isFile());
    m_fileList->append(*newItem->file());
}

DataTransferItemList& DataTransfer::items(Document& document)
{
    if (!m_itemList)
        m_itemList = makeUniqueWithoutRefCountedCheck<DataTransferItemList>(document, *this);
    return *m_itemList;
}

Vector<String> DataTransfer::types(Document& document) const
{
    return types(document, AddFilesType::Yes);
}

Vector<String> DataTransfer::typesForItemList(Document& document) const
{
    return types(document, AddFilesType::No);
}

Vector<String> DataTransfer::types(Document& document, AddFilesType addFilesType) const
{
    if (!canReadTypes())
        return { };
    
    bool hideFilesType = !canWriteData() && !allowsFileAccess();
    if (!DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
        auto types = m_pasteboard->typesForLegacyUnsafeBindings();
        ASSERT(!types.contains("Files"_s));
        if (!hideFilesType && m_pasteboard->fileContentState() != Pasteboard::FileContentState::NoFileOrImageData && addFilesType == AddFilesType::Yes)
            types.append("Files"_s);
        return types;
    }

    auto safeTypes = m_pasteboard->typesSafeForBindings(m_originIdentifier);
    bool hasFileBackedItem = m_itemList && m_itemList->hasItems() && notFound != m_itemList->items().findIf([] (const auto& item) {
        return item->isFile();
    });

    auto fileContentState = m_pasteboard->fileContentState();
    if (hasFileBackedItem || fileContentState != Pasteboard::FileContentState::NoFileOrImageData) {
        Vector<String> types;
        if (!hideFilesType && addFilesType == AddFilesType::Yes) {
            types.append("Files"_s);
            if (document.quirks().needsMozillaFileTypeForDataTransfer())
                types.append("application/x-moz-file"_s);
        }

        if (fileContentState != Pasteboard::FileContentState::MayContainFilePaths) {
            types.appendVector(WTFMove(safeTypes));
            return types;
        }

        if (safeTypes.contains("text/uri-list"_s))
            types.append("text/uri-list"_s);
        if (safeTypes.contains(textHTMLContentTypeAtom()) && DeprecatedGlobalSettings::customPasteboardDataEnabled())
            types.append(textHTMLContentTypeAtom());
        return types;
    }

    ASSERT(!safeTypes.contains("Files"_s));
    return safeTypes;
}

Vector<Ref<File>> DataTransfer::filesFromPasteboardAndItemList(ScriptExecutionContext* context) const
{
    bool addedFilesFromPasteboard = false;
    Vector<Ref<File>> files;
    if (allowsFileAccess() && m_pasteboard->fileContentState() != Pasteboard::FileContentState::NoFileOrImageData) {
        WebCorePasteboardFileReader reader(context);
        m_pasteboard->read(reader);
        files = WTFMove(reader.files);
        addedFilesFromPasteboard = !files.isEmpty();
    }

    bool itemListContainsItems = false;
    if (m_itemList && m_itemList->hasItems()) {
        for (auto& item : m_itemList->items()) {
            if (auto file = item->file())
                files.append(file.releaseNonNull());
        }
        itemListContainsItems = true;
    }

    bool containsItemsAndFiles = itemListContainsItems && addedFilesFromPasteboard;
    ASSERT_UNUSED(containsItemsAndFiles, !containsItemsAndFiles);
    return files;
}

FileList& DataTransfer::files(Document* document) const
{
    if (!canReadData()) {
        if (m_fileList)
            m_fileList->clear();
        else
            m_fileList = FileList::create();
        return *m_fileList;
    }

    if (!m_fileList)
        m_fileList = FileList::create(filesFromPasteboardAndItemList(document));

    return *m_fileList;
}

FileList& DataTransfer::files(Document& document) const
{
    return files(&document);
}

struct PasteboardFileTypeReader final : PasteboardFileReader {
    void readFilename(const String& filename)
    {
        types.add(File::contentTypeForFile(filename));
    }

    void readBuffer(const String&, const String& type, Ref<SharedBuffer>&&)
    {
        types.add(type);
    }

    HashSet<String, ASCIICaseInsensitiveHash> types;
};

bool DataTransfer::hasFileOfType(const String& type)
{
    ASSERT_WITH_SECURITY_IMPLICATION(canReadTypes());
    PasteboardFileTypeReader reader;
    m_pasteboard->read(reader);
    return reader.types.contains(type);
}

bool DataTransfer::hasStringOfType(Document& document, const String& type)
{
    ASSERT_WITH_SECURITY_IMPLICATION(canReadTypes());

    return !type.isNull() && types(document).contains(type);
}

Ref<DataTransfer> DataTransfer::createForInputEvent(const String& plainText, const String& htmlText)
{
    auto pasteboard = makeUnique<StaticPasteboard>();
    pasteboard->writeString(textPlainContentTypeAtom(), plainText);
    pasteboard->writeString(textHTMLContentTypeAtom(), htmlText);
    return adoptRef(*new DataTransfer(StoreMode::Readonly, WTFMove(pasteboard), Type::InputEvent));
}

void DataTransfer::commitToPasteboard(Pasteboard& nativePasteboard)
{
    ASSERT(!is<StaticPasteboard>(nativePasteboard));
    auto& staticPasteboard = downcast<StaticPasteboard>(*m_pasteboard);
    if (!staticPasteboard.hasNonDefaultData()) {
        // We clear the platform pasteboard here to ensure that the pasteboard doesn't contain any data
        // that may have been written before starting the drag or copying, and after ending the last
        // drag session or paste. After pushing the static pasteboard's contents to the platform, the
        // pasteboard should only contain data that was in the static pasteboard.
        nativePasteboard.clear();
        return;
    }

    auto customData = staticPasteboard.takeCustomData();
    if (DeprecatedGlobalSettings::customPasteboardDataEnabled()) {
        customData.setOrigin(m_originIdentifier);
        nativePasteboard.writeCustomData({ customData });
        return;
    }

    nativePasteboard.clear();
    customData.forEachPlatformString([&] (auto& type, auto& string) {
        nativePasteboard.writeString(type, string);
    });

    customData.forEachCustomString([&] (auto& type, auto& string) {
        nativePasteboard.writeString(type, string);
    });
}

#if !ENABLE(DRAG_SUPPORT)

String DataTransfer::dropEffect() const
{
    return noneAtom();
}

void DataTransfer::setDropEffect(const String&)
{
}

String DataTransfer::effectAllowed() const
{
    return "uninitialized"_s;
}

void DataTransfer::setEffectAllowed(const String&)
{
}

void DataTransfer::setDragImage(Ref<Element>&&, int, int)
{
}

#else

Ref<DataTransfer> DataTransfer::createForDrag(const Document& document)
{
    return adoptRef(*new DataTransfer(StoreMode::ReadWrite, Pasteboard::createForDragAndDrop(PagePasteboardContext::create(document.pageID())), Type::DragAndDropData));
}

Ref<DataTransfer> DataTransfer::createForDragStartEvent(const Document& document)
{
    auto dataTransfer = adoptRef(*new DataTransfer(StoreMode::ReadWrite, makeUnique<StaticPasteboard>(), Type::DragAndDropData));
    dataTransfer->m_originIdentifier = document.originIdentifierForPasteboard();
    return dataTransfer;
}

Ref<DataTransfer> DataTransfer::createForDrop(const Document& document, std::unique_ptr<Pasteboard>&& pasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    auto dataTransfer = adoptRef(*new DataTransfer(DataTransfer::StoreMode::Readonly, WTFMove(pasteboard), draggingFiles ? Type::DragAndDropFiles : Type::DragAndDropData));
    dataTransfer->setSourceOperationMask(sourceOperationMask);
    dataTransfer->m_originIdentifier = document.originIdentifierForPasteboard();
    return dataTransfer;
}

Ref<DataTransfer> DataTransfer::createForUpdatingDropTarget(const Document& document, std::unique_ptr<Pasteboard>&& pasteboard, OptionSet<DragOperation> sourceOperationMask, bool draggingFiles)
{
    auto dataTransfer = adoptRef(*new DataTransfer(DataTransfer::StoreMode::Protected, WTFMove(pasteboard), draggingFiles ? Type::DragAndDropFiles : Type::DragAndDropData));
    dataTransfer->setSourceOperationMask(sourceOperationMask);
    dataTransfer->m_originIdentifier = document.originIdentifierForPasteboard();
    return dataTransfer;
}

void DataTransfer::setDragImage(Ref<Element>&& element, int x, int y)
{
    if (!forDrag() || !canWriteData())
        return;

    CachedResourceHandle<CachedImage> image;
    if (auto* imageElement = dynamicDowncast<HTMLImageElement>(element.get()); imageElement && !imageElement->isConnected())
        image = imageElement->cachedImage();

    m_dragLocation = IntPoint(x, y);

    if (m_dragImageLoader && m_dragImage)
        m_dragImageLoader->stopLoading(m_dragImage);
    m_dragImage = image;
    if (m_dragImage) {
        if (!m_dragImageLoader)
            m_dragImageLoader = makeUnique<DragImageLoader>(*this);
        m_dragImageLoader->startLoading(m_dragImage);
    }

    if (image)
        m_dragImageElement = nullptr;
    else
        m_dragImageElement = WTFMove(element);

    updateDragImage();
}

void DataTransfer::updateDragImage()
{
    // Don't allow setting the image if we haven't started dragging yet; we'll rely on the dragging code
    // to install this drag image as part of getting the drag kicked off.
    if (!m_shouldUpdateDragImage)
        return;

    IntPoint computedHotSpot;
    auto computedImage = DragImage { createDragImage(computedHotSpot) };
    if (!computedImage)
        return;

    m_pasteboard->setDragImage(WTFMove(computedImage), computedHotSpot);
}

RefPtr<Element> DataTransfer::dragImageElement() const
{
    return m_dragImageElement;
}

#if !PLATFORM(MAC)

DragImageRef DataTransfer::createDragImage(IntPoint& location) const
{
    location = m_dragLocation;

    if (m_dragImage)
        return createDragImageFromImage(m_dragImage->protectedImage().get(), ImageOrientation::Orientation::None);

    if (m_dragImageElement) {
        if (RefPtr frame = m_dragImageElement->document().frame())
            return createDragImageForNode(*frame, dragImageElement().releaseNonNull());
    }

    // We do not have enough information to create a drag image, use the default icon.
    return nullptr;
}

#endif

DragImageLoader::DragImageLoader(DataTransfer& dataTransfer)
    : m_dataTransfer(dataTransfer)
{
}

void DragImageLoader::moveToDataTransfer(DataTransfer& newDataTransfer)
{
    m_dataTransfer = newDataTransfer;
}

void DragImageLoader::startLoading(CachedResourceHandle<WebCore::CachedImage>& image)
{
    // FIXME: Does this really trigger a load? Does it need to?
    image->addClient(*this);
}

void DragImageLoader::stopLoading(CachedResourceHandle<WebCore::CachedImage>& image)
{
    image->removeClient(*this);
}

void DragImageLoader::imageChanged(CachedImage*, const IntRect*)
{
    m_dataTransfer->updateDragImage();
}

static OptionSet<DragOperation> dragOpFromIEOp(const String& operation)
{
    if (operation == "uninitialized"_s)
        return anyDragOperation();
    if (operation == "none"_s)
        return { };
    if (operation == "copy"_s)
        return { DragOperation::Copy };
    if (operation == "link"_s)
        return { DragOperation::Link };
    if (operation == "move"_s)
        return { DragOperation::Generic, DragOperation::Move };
    if (operation == "copyLink"_s)
        return { DragOperation::Copy, DragOperation::Link };
    if (operation == "copyMove"_s)
        return { DragOperation::Copy, DragOperation::Generic, DragOperation::Move };
    if (operation == "linkMove"_s)
        return { DragOperation::Link, DragOperation::Generic, DragOperation::Move };
    if (operation == "all"_s)
        return anyDragOperation();
    return { DragOperation::Private }; // Really a marker for "no conversion".
}

static ASCIILiteral IEOpFromDragOp(OptionSet<DragOperation> operationMask)
{
    bool isGenericMove = operationMask.containsAny({ DragOperation::Generic, DragOperation::Move });

    if ((isGenericMove && operationMask.containsAll({ DragOperation::Copy, DragOperation::Link })) || operationMask.containsAll({ DragOperation::Copy, DragOperation::Link, DragOperation::Generic, DragOperation::Private, DragOperation::Move, DragOperation::Delete }))
        return "all"_s;
    if (isGenericMove && operationMask.contains(DragOperation::Copy))
        return "copyMove"_s;
    if (isGenericMove && operationMask.contains(DragOperation::Link))
        return "linkMove"_s;
    if (operationMask.containsAll({ DragOperation::Copy, DragOperation::Link }))
        return "copyLink"_s;
    if (isGenericMove)
        return "move"_s;
    if (operationMask.contains(DragOperation::Copy))
        return "copy"_s;
    if (operationMask.contains(DragOperation::Link))
        return "link"_s;
    return "none"_s;
}

OptionSet<DragOperation> DataTransfer::sourceOperationMask() const
{
    auto operationMask = dragOpFromIEOp(m_effectAllowed);
    ASSERT(operationMask != DragOperation::Private);
    return operationMask;
}

OptionSet<DragOperation> DataTransfer::destinationOperationMask() const
{
    auto operationMask = dragOpFromIEOp(m_dropEffect);
    ASSERT(operationMask == DragOperation::Copy || operationMask.isEmpty() || operationMask == DragOperation::Link || operationMask == OptionSet<DragOperation>({ DragOperation::Generic, DragOperation::Move }) || operationMask.containsAll({ DragOperation::Copy, DragOperation::Link, DragOperation::Generic, DragOperation::Private, DragOperation::Move, DragOperation::Delete }));
    return operationMask;
}

void DataTransfer::setSourceOperationMask(OptionSet<DragOperation> operationMask)
{
    ASSERT_ARG(operationMask, operationMask != DragOperation::Private);
    m_effectAllowed = IEOpFromDragOp(operationMask);
}

void DataTransfer::setDestinationOperationMask(OptionSet<DragOperation> operationMask)
{
    ASSERT_ARG(operationMask, operationMask == DragOperation::Copy || operationMask.isEmpty() || operationMask == DragOperation::Link || operationMask == DragOperation::Generic || operationMask == DragOperation::Move || operationMask == OptionSet<DragOperation>({ DragOperation::Generic, DragOperation::Move }));
    m_dropEffect = IEOpFromDragOp(operationMask);
}

String DataTransfer::dropEffect() const
{
    return m_dropEffect == "uninitialized"_s ? "none"_s : m_dropEffect;
}

void DataTransfer::setDropEffect(const String& effect)
{
    if (!forDrag())
        return;

    if (effect != "none"_s && effect != "copy"_s && effect != "link"_s && effect != "move"_s)
        return;

    // FIXME: The spec allows this in all circumstances. There is probably no value
    // in ignoring attempts to change it.
    if (!canReadTypes())
        return;

    m_dropEffect = effect;
}

String DataTransfer::effectAllowed() const
{
    return m_effectAllowed;
}

void DataTransfer::setEffectAllowed(const String& effect)
{
    if (!forDrag())
        return;

    // Ignore any attempts to set it to an unknown value.
    if (dragOpFromIEOp(effect) == DragOperation::Private)
        return;

    if (!canWriteData())
        return;

    m_effectAllowed = effect;
}

void DataTransfer::moveDragState(Ref<DataTransfer>&& other)
{
    RELEASE_ASSERT(is<StaticPasteboard>(other->pasteboard()));
    other->commitToPasteboard(*m_pasteboard);

    m_dropEffect = other->m_dropEffect;
    m_effectAllowed = other->m_effectAllowed;
    m_dragLocation = other->m_dragLocation;
    m_dragImage = other->m_dragImage;
    m_dragImageElement = WTFMove(other->m_dragImageElement);
    m_dragImageLoader = WTFMove(other->m_dragImageLoader);
    if (m_dragImageLoader)
        m_dragImageLoader->moveToDataTransfer(*this);
    m_fileList = WTFMove(other->m_fileList);
}

bool DataTransfer::hasDragImage() const
{
    return m_dragImage || m_dragImageElement;
}

#endif // ENABLE(DRAG_SUPPORT)

} // namespace WebCore
