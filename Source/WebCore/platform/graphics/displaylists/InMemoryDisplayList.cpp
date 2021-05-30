/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InMemoryDisplayList.h"

#include "DisplayListIterator.h"

namespace WebCore {
namespace DisplayList {

std::optional<std::size_t> InMemoryDisplayList::WritingClient::requiredSizeForItem(const DisplayListItem& displayListItem) const
{
    return paddedSizeOfTypeAndItemInBytes(displayListItem);
}

void InMemoryDisplayList::WritingClient::encodeItemInline(const DisplayListItem& displayListItem, uint8_t* location) const
{
    safeCopy({ location }, displayListItem);
}

std::optional<ItemHandle> WARN_UNUSED_RETURN InMemoryDisplayList::ReadingClient::decodeItem(const uint8_t* data, size_t dataLength, ItemType type, uint8_t* handleLocation)
{
    const ItemHandle dataHandle { const_cast<uint8_t*>(data) };
    ASSERT_UNUSED(dataLength, dataLength >= paddedSizeOfTypeAndItemInBytes(dataHandle.type()));
    ItemHandle result { handleLocation };
    if (dataHandle.safeCopy(type, result))
        return result;
    return std::nullopt;
}

InMemoryDisplayList::InMemoryDisplayList()
    : m_writingClient(WTF::makeUnique<WritingClient>())
    , m_readingClient(WTF::makeUnique<ReadingClient>())
{
    setItemBufferWritingClient(m_writingClient.get());
    setItemBufferReadingClient(m_readingClient.get());
}

InMemoryDisplayList::~InMemoryDisplayList()
{
    auto end = this->end();
    for (auto displayListItem : *this) {
        auto item = displayListItem->item;
        ASSERT(item);
        if (!item)
            break;
        item.destroy();
    }
}

}
}
