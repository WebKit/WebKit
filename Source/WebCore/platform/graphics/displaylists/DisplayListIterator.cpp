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
#include "DisplayListIterator.h"

namespace WebCore {
namespace DisplayList {

bool DisplayList::Iterator::atEnd() const
{
    if (m_displayList.isEmpty() || !m_isValid)
        return true;

    auto& items = *itemBuffer();
    auto endCursor = items.m_writableBuffer.data + items.m_writtenNumberOfBytes;
    return m_cursor == endCursor;
}

auto DisplayList::Iterator::updateCurrentDrawingItemExtent(ItemType itemType) -> ExtentUpdateResult
{
    auto& extents = m_displayList.m_drawingItemExtents;
    if (extents.isEmpty())
        return ExtentUpdateResult::Success;

    if (!isDrawingItem(itemType)) {
        m_currentExtent = std::nullopt;
        return ExtentUpdateResult::Success;
    }

    if (m_drawingItemIndex >= extents.size())
        return ExtentUpdateResult::Failure;

    m_currentExtent = extents[m_drawingItemIndex];
    m_drawingItemIndex++;
    return ExtentUpdateResult::Success;
}

void DisplayList::Iterator::updateCurrentItem()
{
    clearCurrentItem();

    if (atEnd())
        return;

    auto& items = *itemBuffer();
    auto rawItemTypeValue = m_cursor[0];
    if (UNLIKELY(!isValidEnum<ItemType>(rawItemTypeValue))) {
        m_isValid = false;
        return;
    }

    auto itemType = static_cast<ItemType>(rawItemTypeValue);
    if (UNLIKELY(updateCurrentDrawingItemExtent(itemType) == ExtentUpdateResult::Failure)) {
        m_isValid = false;
        return;
    }

    auto remainingCapacityInBuffer = static_cast<uint64_t>(m_currentEndOfBuffer - m_cursor);
    auto paddedSizeOfTypeAndItem = paddedSizeOfTypeAndItemInBytes(itemType);
    m_currentBufferForItem = paddedSizeOfTypeAndItem <= sizeOfFixedBufferForCurrentItem ? m_fixedBufferForCurrentItem : static_cast<uint8_t*>(fastMalloc(paddedSizeOfTypeAndItem));
    if (isInlineItem(itemType)) {
        if (UNLIKELY(remainingCapacityInBuffer < paddedSizeOfTypeAndItem)) {
            m_isValid = false;
            return;
        }

        if (UNLIKELY(!ItemHandle { m_cursor }.safeCopy(itemType, { m_currentBufferForItem }))) {
            m_isValid = false;
            return;
        }

        m_currentItemSizeInBuffer = paddedSizeOfTypeAndItem;
    } else {
        auto* client = items.m_readingClient;
        RELEASE_ASSERT(client);
        constexpr auto sizeOfTypeAndDataLength = 2 * sizeof(uint64_t);

        if (UNLIKELY(remainingCapacityInBuffer < sizeOfTypeAndDataLength)) {
            m_isValid = false;
            return;
        }

        auto dataLength = reinterpret_cast<uint64_t*>(m_cursor)[1];
        if (UNLIKELY(dataLength >= std::numeric_limits<uint32_t>::max() - alignof(uint64_t) - sizeOfTypeAndDataLength)) {
            m_isValid = false;
            return;
        }

        auto itemSizeInBuffer = roundUpToMultipleOf<alignof(uint64_t)>(dataLength) + sizeOfTypeAndDataLength;
        if (UNLIKELY(remainingCapacityInBuffer < itemSizeInBuffer)) {
            m_isValid = false;
            return;
        }

        auto* startOfData = m_cursor + sizeOfTypeAndDataLength;
        auto decodedItemHandle = client->decodeItem(startOfData, dataLength, itemType, m_currentBufferForItem);
        if (UNLIKELY(!decodedItemHandle)) {
            m_isValid = false;
            return;
        }

        m_currentBufferForItem[0] = static_cast<uint8_t>(itemType);
        m_currentItemSizeInBuffer = itemSizeInBuffer;
    }
}

void DisplayList::Iterator::advance()
{
    if (atEnd())
        return;

    m_cursor += m_currentItemSizeInBuffer;

    if (m_cursor == m_currentEndOfBuffer && m_readOnlyBufferIndex < itemBuffer()->m_readOnlyBuffers.size()) {
        m_readOnlyBufferIndex++;
        moveCursorToStartOfCurrentBuffer();
    }

    updateCurrentItem();
}

void DisplayList::Iterator::clearCurrentItem()
{
    auto items = itemBuffer();
    if (items && items->m_readingClient && m_currentBufferForItem) {
        if (LIKELY(m_isValid))
            ItemHandle { m_currentBufferForItem }.destroy();

        if (UNLIKELY(m_currentBufferForItem != m_fixedBufferForCurrentItem))
            fastFree(m_currentBufferForItem);
    }

    m_currentItemSizeInBuffer = 0;
    m_currentBufferForItem = nullptr;
}

void DisplayList::Iterator::moveToEnd()
{
    if (auto items = itemBuffer()) {
        m_cursor = items->m_writableBuffer.data + items->m_writtenNumberOfBytes;
        m_currentEndOfBuffer = m_cursor;
        m_readOnlyBufferIndex = items->m_readOnlyBuffers.size();
    }
}

void DisplayList::Iterator::moveCursorToStartOfCurrentBuffer()
{
    auto items = itemBuffer();
    if (!items)
        return;

    auto numberOfReadOnlyBuffers = items->m_readOnlyBuffers.size();
    if (m_readOnlyBufferIndex < numberOfReadOnlyBuffers) {
        auto& nextBufferHandle = items->m_readOnlyBuffers[m_readOnlyBufferIndex];
        m_cursor = nextBufferHandle.data;
        m_currentEndOfBuffer = m_cursor + nextBufferHandle.capacity;
    } else if (m_readOnlyBufferIndex == numberOfReadOnlyBuffers) {
        m_cursor = items->m_writableBuffer.data;
        m_currentEndOfBuffer = m_cursor + items->m_writtenNumberOfBytes;
    }
}

}
}
