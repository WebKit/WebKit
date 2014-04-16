/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "DragData.h"
#include "Editor.h"
#include "FileList.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLImageElement.h"
#include "Image.h"
#include "Pasteboard.h"

namespace WebCore {

#if ENABLE(DRAG_SUPPORT)

class DragImageLoader final : private CachedImageClient {
    WTF_MAKE_NONCOPYABLE(DragImageLoader); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DragImageLoader(DataTransfer*);
    void startLoading(CachedResourceHandle<CachedImage>&);
    void stopLoading(CachedResourceHandle<CachedImage>&);

private:
    virtual void imageChanged(CachedImage*, const IntRect*) override;
    DataTransfer* m_dataTransfer;
};

#endif

DataTransfer::DataTransfer(DataTransferAccessPolicy policy, PassOwnPtr<Pasteboard> pasteboard, Type type, bool forFileDrag)
    : m_policy(policy)
    , m_pasteboard(pasteboard)
#if ENABLE(DRAG_SUPPORT)
    , m_forDrag(type != CopyAndPaste)
    , m_forFileDrag(forFileDrag)
    , m_dropEffect(ASCIILiteral("uninitialized"))
    , m_effectAllowed(ASCIILiteral("uninitialized"))
    , m_shouldUpdateDragImage(false)
#endif
{
#if !ENABLE(DRAG_SUPPORT)
    ASSERT_UNUSED(type, type == CopyAndPaste);
    ASSERT_UNUSED(forFileDrag, !forFileDrag);
#endif
}

PassRefPtr<DataTransfer> DataTransfer::createForCopyAndPaste(DataTransferAccessPolicy policy)
{
    return adoptRef(new DataTransfer(policy, policy == DataTransferAccessPolicy::Writable ? Pasteboard::createPrivate() : Pasteboard::createForCopyAndPaste()));
}

DataTransfer::~DataTransfer()
{
#if ENABLE(DRAG_SUPPORT)
    if (m_dragImageLoader && m_dragImage)
        m_dragImageLoader->stopLoading(m_dragImage);
#endif
}
    
void DataTransfer::setAccessPolicy(DataTransferAccessPolicy policy)
{
    // Once the dataTransfer goes numb, it can never go back.
    ASSERT(m_policy != DataTransferAccessPolicy::Numb || policy == DataTransferAccessPolicy::Numb);
    m_policy = policy;
}

bool DataTransfer::canReadTypes() const
{
    return m_policy == DataTransferAccessPolicy::Readable || m_policy == DataTransferAccessPolicy::TypesReadable || m_policy == DataTransferAccessPolicy::Writable;
}

bool DataTransfer::canReadData() const
{
    return m_policy == DataTransferAccessPolicy::Readable || m_policy == DataTransferAccessPolicy::Writable;
}

bool DataTransfer::canWriteData() const
{
    return m_policy == DataTransferAccessPolicy::Writable;
}

void DataTransfer::clearData(const String& type)
{
    if (!canWriteData())
        return;

    m_pasteboard->clear(type);
}

void DataTransfer::clearData()
{
    if (!canWriteData())
        return;

    m_pasteboard->clear();
}

String DataTransfer::getData(const String& type) const
{
    if (!canReadData())
        return String();

#if ENABLE(DRAG_SUPPORT)
    if (m_forFileDrag)
        return String();
#endif

    return m_pasteboard->readString(type);
}

bool DataTransfer::setData(const String& type, const String& data)
{
    if (!canWriteData())
        return false;

#if ENABLE(DRAG_SUPPORT)
    if (m_forFileDrag)
        return false;
#endif

    return m_pasteboard->writeString(type, data);
}

Vector<String> DataTransfer::types() const
{
    // FIXME: Per HTML5, types should be a live array, and the DOM attribute should always return the same object.

    if (!canReadTypes())
        return Vector<String>();

    return m_pasteboard->types();
}

FileList* DataTransfer::files() const
{
    bool newlyCreatedFileList = !m_fileList;
    if (!m_fileList)
        m_fileList = FileList::create();

    if (!canReadData()) {
        m_fileList->clear();
        return m_fileList.get();
    }

#if ENABLE(DRAG_SUPPORT)
    if (m_forDrag && !m_forFileDrag) {
        ASSERT(m_fileList->isEmpty());
        return m_fileList.get();
    }
#endif

    if (newlyCreatedFileList) {
        for (const String& filename : m_pasteboard->readFilenames())
            m_fileList->append(File::create(filename, File::AllContentTypes));
    }
    return m_fileList.get();
}

#if !ENABLE(DRAG_SUPPORT)

String DataTransfer::dropEffect() const
{
    return ASCIILiteral("none");
}

void DataTransfer::setDropEffect(const String&)
{
}

String DataTransfer::effectAllowed() const
{
    return ASCIILiteral("uninitialized");
}

void DataTransfer::setEffectAllowed(const String&)
{
}

void DataTransfer::setDragImage(Element*, int, int)
{
}

#else

PassRefPtr<DataTransfer> DataTransfer::createForDragAndDrop()
{
    return adoptRef(new DataTransfer(DataTransferAccessPolicy::Writable, Pasteboard::createForDragAndDrop(), DragAndDrop));
}

PassRefPtr<DataTransfer> DataTransfer::createForDragAndDrop(DataTransferAccessPolicy policy, const DragData& dragData)
{
    return adoptRef(new DataTransfer(policy, Pasteboard::createForDragAndDrop(dragData), DragAndDrop, dragData.containsFiles()));
}

bool DataTransfer::canSetDragImage() const
{
    // Note that the spec doesn't actually allow drag image modification outside the dragstart
    // event. This capability is maintained for backwards compatiblity for ports that have
    // supported this in the past. On many ports, attempting to set a drag image outside the
    // dragstart operation is a no-op anyway.
    return m_forDrag && (m_policy == DataTransferAccessPolicy::ImageWritable || m_policy == DataTransferAccessPolicy::Writable);
}

void DataTransfer::setDragImage(Element* element, int x, int y)
{
    if (!canSetDragImage())
        return;

    CachedImage* image;
    if (element && isHTMLImageElement(element) && !element->inDocument())
        image = toHTMLImageElement(element)->cachedImage();
    else
        image = 0;

    m_dragLocation = IntPoint(x, y);

    if (m_dragImageLoader && m_dragImage)
        m_dragImageLoader->stopLoading(m_dragImage);
    m_dragImage = image;
    if (m_dragImage) {
        if (!m_dragImageLoader)
            m_dragImageLoader = std::make_unique<DragImageLoader>(this);
        m_dragImageLoader->startLoading(m_dragImage);
    }

    m_dragImageElement = image ? 0 : element;

    updateDragImage();
}

void DataTransfer::updateDragImage()
{
    // Don't allow setting the image if we haven't started dragging yet; we'll rely on the dragging code
    // to install this drag image as part of getting the drag kicked off.
    if (!m_shouldUpdateDragImage)
        return;

    IntPoint computedHotSpot;
    DragImageRef computedImage = createDragImage(computedHotSpot);
    if (!computedImage)
        return;

    m_pasteboard->setDragImage(computedImage, computedHotSpot);
}

#if !PLATFORM(COCOA)

DragImageRef DataTransfer::createDragImage(IntPoint& location) const
{
    location = m_dragLocation;

    if (m_dragImage)
        return createDragImageFromImage(m_dragImage->image(), ImageOrientationDescription());

    if (m_dragImageElement) {
        if (Frame* frame = m_dragImageElement->document().frame())
            return createDragImageForNode(*frame, *m_dragImageElement);
    }

    // We do not have enough information to create a drag image, use the default icon.
    return nullptr;
}

#endif

DragImageLoader::DragImageLoader(DataTransfer* dataTransfer)
    : m_dataTransfer(dataTransfer)
{
}

void DragImageLoader::startLoading(CachedResourceHandle<WebCore::CachedImage>& image)
{
    // FIXME: Does this really trigger a load? Does it need to?
    image->addClient(this);
}

void DragImageLoader::stopLoading(CachedResourceHandle<WebCore::CachedImage>& image)
{
    image->removeClient(this);
}

void DragImageLoader::imageChanged(CachedImage*, const IntRect*)
{
    m_dataTransfer->updateDragImage();
}

static DragOperation dragOpFromIEOp(const String& operation)
{
    if (operation == "uninitialized")
        return DragOperationEvery;
    if (operation == "none")
        return DragOperationNone;
    if (operation == "copy")
        return DragOperationCopy;
    if (operation == "link")
        return DragOperationLink;
    if (operation == "move")
        return (DragOperation)(DragOperationGeneric | DragOperationMove);
    if (operation == "copyLink")
        return (DragOperation)(DragOperationCopy | DragOperationLink);
    if (operation == "copyMove")
        return (DragOperation)(DragOperationCopy | DragOperationGeneric | DragOperationMove);
    if (operation == "linkMove")
        return (DragOperation)(DragOperationLink | DragOperationGeneric | DragOperationMove);
    if (operation == "all")
        return DragOperationEvery;
    return DragOperationPrivate; // really a marker for "no conversion"
}

static const char* IEOpFromDragOp(DragOperation operation)
{
    bool isGenericMove = operation & (DragOperationGeneric | DragOperationMove);

    if ((isGenericMove && (operation & DragOperationCopy) && (operation & DragOperationLink)) || operation == DragOperationEvery)
        return "all";
    if (isGenericMove && (operation & DragOperationCopy))
        return "copyMove";
    if (isGenericMove && (operation & DragOperationLink))
        return "linkMove";
    if ((operation & DragOperationCopy) && (operation & DragOperationLink))
        return "copyLink";
    if (isGenericMove)
        return "move";
    if (operation & DragOperationCopy)
        return "copy";
    if (operation & DragOperationLink)
        return "link";
    return "none";
}

DragOperation DataTransfer::sourceOperation() const
{
    DragOperation operation = dragOpFromIEOp(m_effectAllowed);
    ASSERT(operation != DragOperationPrivate);
    return operation;
}

DragOperation DataTransfer::destinationOperation() const
{
    DragOperation operation = dragOpFromIEOp(m_dropEffect);
    ASSERT(operation == DragOperationCopy || operation == DragOperationNone || operation == DragOperationLink || operation == (DragOperation)(DragOperationGeneric | DragOperationMove) || operation == DragOperationEvery);
    return operation;
}

void DataTransfer::setSourceOperation(DragOperation operation)
{
    ASSERT_ARG(operation, operation != DragOperationPrivate);
    m_effectAllowed = IEOpFromDragOp(operation);
}

void DataTransfer::setDestinationOperation(DragOperation operation)
{
    ASSERT_ARG(operation, operation == DragOperationCopy || operation == DragOperationNone || operation == DragOperationLink || operation == DragOperationGeneric || operation == DragOperationMove || operation == (DragOperation)(DragOperationGeneric | DragOperationMove));
    m_dropEffect = IEOpFromDragOp(operation);
}

String DataTransfer::dropEffect() const
{
    return m_dropEffect == "uninitialized" ? ASCIILiteral("none") : m_dropEffect;
}

void DataTransfer::setDropEffect(const String& effect)
{
    if (!m_forDrag)
        return;

    if (effect != "none" && effect != "copy" && effect != "link" && effect != "move")
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
    if (!m_forDrag)
        return;

    // Ignore any attempts to set it to an unknown value.
    if (dragOpFromIEOp(effect) == DragOperationPrivate)
        return;

    if (!canWriteData())
        return;

    m_effectAllowed = effect;
}

#endif // ENABLE(DRAG_SUPPORT)

} // namespace WebCore
