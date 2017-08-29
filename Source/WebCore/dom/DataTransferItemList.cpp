/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "DataTransferItemList.h"

#include "DataTransferItem.h"
#include "FileList.h"
#include "Pasteboard.h"

namespace WebCore {

// FIXME: DataTransfer should filter types itself.
static bool isSupportedType(const String& type)
{
    return type == "text/plain";
}

DataTransferItemList::DataTransferItemList(DataTransfer& dataTransfer)
    : m_weakPtrFactory(this)
    , m_dataTransfer(dataTransfer)
{
}

DataTransferItemList::~DataTransferItemList()
{
}

unsigned DataTransferItemList::length() const
{
    return ensureItems().size();
}

RefPtr<DataTransferItem> DataTransferItemList::item(unsigned index)
{
    auto& items = ensureItems();
    if (items.size() <= index)
        return nullptr;
    return items[index].copyRef();
}

ExceptionOr<RefPtr<DataTransferItem>> DataTransferItemList::add(const String& data, const String& type)
{
    if (!m_dataTransfer.canWriteData())
        return nullptr;

    for (auto& item : ensureItems()) {
        if (!item->isFile() && equalIgnoringASCIICase(item->type(), type))
            return Exception { NotSupportedError };
    }

    String lowercasedType = type.convertToASCIILowercase();

    // FIXME: Allow writing & reading of any types to clipboard / drag data store.
    if (!isSupportedType(lowercasedType))
        return nullptr;

    m_dataTransfer.pasteboard().writeString(lowercasedType, data);
    ASSERT(m_items);
    m_items->append(DataTransferItem::create(m_weakPtrFactory.createWeakPtr(), type));
    return RefPtr<DataTransferItem> { m_items->last().copyRef() };
}

RefPtr<DataTransferItem> DataTransferItemList::add(Ref<File>&&)
{
    return nullptr;
}

ExceptionOr<void> DataTransferItemList::remove(unsigned index)
{
    if (!m_dataTransfer.canWriteData())
        return Exception { InvalidStateError };

    auto& items = ensureItems();
    if (items.size() <= index)
        return Exception { IndexSizeError }; // Matches Gecko. See https://github.com/whatwg/html/issues/2925

    // FIXME: Handle the removal of files once we added the support for writing a File.
    ASSERT(!items[index]->isFile());

    auto& removedItem = items[index].get();
    m_dataTransfer.pasteboard().clear(removedItem.type());
    removedItem.clearListAndPutIntoDisabledMode();
    items.remove(index);

    return { };
}

void DataTransferItemList::clear()
{
    m_dataTransfer.pasteboard().clear();
    if (m_items) {
        for (auto& item : *m_items)
            item->clearListAndPutIntoDisabledMode();
        m_items->clear();
    }
}

Vector<Ref<DataTransferItem>>& DataTransferItemList::ensureItems() const
{
    if (m_items)
        return *m_items;

    Vector<Ref<DataTransferItem>> items;
    for (String& type : m_dataTransfer.types()) {
        String lowercasedType = type.convertToASCIILowercase();
        if (isSupportedType(lowercasedType))
            items.append(DataTransferItem::create(m_weakPtrFactory.createWeakPtr(), lowercasedType));
    }

    FileList& files = m_dataTransfer.files();
    for (unsigned i = 0, length = files.length(); i < length; ++i) {
        File& file = *files.item(i);
        String type = File::contentTypeForFile(file.path()).convertToASCIILowercase();
        if (isSupportedType(type) || file.isDirectory())
            items.append(DataTransferItem::create(m_weakPtrFactory.createWeakPtr(), type, file));
    }

    m_items = WTFMove(items);

    return *m_items;
}

static void removeStringItemOfLowercasedType(Vector<Ref<DataTransferItem>>& items, const String& lowercasedType)
{
    auto index = items.findMatching([lowercasedType](auto& item) {
        return !item->isFile() && item->type() == lowercasedType;
    });
    if (index == notFound)
        return;
    items[index]->clearListAndPutIntoDisabledMode();
    items.remove(index);
}

void DataTransferItemList::didClearStringData(const String& type)
{
    if (!m_items)
        return;

    auto& items = *m_items;
    if (!type.isNull())
        return removeStringItemOfLowercasedType(items, type.convertToASCIILowercase());

    for (auto& item : items) {
        if (!item->isFile())
            item->clearListAndPutIntoDisabledMode();
    }
    items.removeAllMatching([](auto& item) {
        return !item->isFile();
    });
}

// https://html.spec.whatwg.org/multipage/dnd.html#dom-datatransfer-setdata
void DataTransferItemList::didSetStringData(const String& type)
{
    if (!m_items)
        return;

    String lowercasedType = type.convertToASCIILowercase();
    removeStringItemOfLowercasedType(*m_items, type.convertToASCIILowercase());

    m_items->append(DataTransferItem::create(m_weakPtrFactory.createWeakPtr(), lowercasedType));
}

}

