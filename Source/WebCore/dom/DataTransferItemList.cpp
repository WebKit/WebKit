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

namespace WebCore {

unsigned DataTransferItemList::length() const
{
    return ensureItems().size();
}

RefPtr<DataTransferItem> DataTransferItemList::item(unsigned index)
{
    auto& items = ensureItems();
    if (items.size() <= index)
        return nullptr;
    return items[index].get();
}

ExceptionOr<void> DataTransferItemList::add(const String&, const String&)
{
    return { };
}

void DataTransferItemList::add(RefPtr<File>&&)
{
}

void DataTransferItemList::remove(unsigned)
{
}

void DataTransferItemList::clear()
{
}

// FIXME: DataTransfer should filter types itself.
static bool isSupportedType(const String& type)
{
    return equalIgnoringASCIICase(type, "text/plain");
}

Vector<std::unique_ptr<DataTransferItem>>& DataTransferItemList::ensureItems() const
{
    if (m_items)
        return *m_items;

    Vector<std::unique_ptr<DataTransferItem>> items;
    for (String& type : m_dataTransfer.types()) {
        if (isSupportedType(type))
            items.append(std::make_unique<DataTransferItem>(m_dataTransfer, type));
    }

    FileList& files = m_dataTransfer.files();
    for (unsigned i = 0, length = files.length(); i < length; ++i) {
        File& file = *files.item(i);
        String type = File::contentTypeForFile(file.path());
        if (isSupportedType(type))
            items.append(std::make_unique<DataTransferItem>(m_dataTransfer, type, file));
    }


    m_items = WTFMove(items);

    return *m_items;
}

}

