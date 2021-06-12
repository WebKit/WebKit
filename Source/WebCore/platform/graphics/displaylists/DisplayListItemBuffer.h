/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DisplayListItemBufferIdentifier.h"
#include "DisplayListItemType.h"
#include "DisplayListItems.h"
#include "SharedBuffer.h"
#include <wtf/FastMalloc.h>
#include <wtf/ObjectIdentifier.h>

namespace WebCore {

class GraphicsContext;

namespace DisplayList {

class DisplayList;

// An ItemBufferHandle wraps a pointer to a buffer that contains display list item data.
struct ItemBufferHandle {
    ItemBufferIdentifier identifier;
    uint8_t* data { nullptr };
    size_t capacity { 0 };

    operator bool() const { return !!(*this); }
    bool operator!() const { return !data; }
};

using ItemBufferHandles = Vector<ItemBufferHandle, 2>;

// An ItemHandle wraps a pointer to an ItemType followed immediately by an item of that type.
// Each item handle data pointer is aligned to 8 bytes, and the item itself is also aligned
// to 8 bytes. To ensure this, the item type consists of 8 bytes (1 byte for the type and 7
// bytes of padding).
struct ItemHandle {
    uint8_t* data { nullptr };

    void apply(GraphicsContext&);
    WEBCORE_EXPORT void destroy();
    bool isDrawingItem() const { return WebCore::DisplayList::isDrawingItem(type()); }

    operator bool() const { return !!(*this); }
    bool operator!() const { return !data; }

#if !defined(NDEBUG) || !LOG_DISABLED
    CString description() const;
#endif

    ItemType type() const { return static_cast<ItemType>(data[0]); }

    template<typename T> bool is() const { return type() == T::itemType; }
    template<typename T> T& get() const
    {
        ASSERT(is<T>());
        return *reinterpret_cast<T*>(data + sizeof(uint64_t));
    }

    bool safeCopy(ItemType, ItemHandle destination) const;
};

bool safeCopy(ItemHandle destination, const DisplayListItem& source);

enum class DidChangeItemBuffer : bool { No, Yes };

class ItemBufferWritingClient {
public:
    virtual ~ItemBufferWritingClient()
    {
    }

    virtual ItemBufferHandle createItemBuffer(size_t)
    {
        return { };
    }

    virtual std::optional<std::size_t> requiredSizeForItem(const DisplayListItem&) const
    {
        return std::nullopt;
    }

    virtual RefPtr<SharedBuffer> encodeItemOutOfLine(const DisplayListItem&) const
    {
        return nullptr;
    }

    virtual void encodeItemInline(const DisplayListItem&, uint8_t*) const
    {
    }

    virtual void didAppendData(const ItemBufferHandle&, size_t numberOfBytes, DidChangeItemBuffer)
    {
        UNUSED_PARAM(numberOfBytes);
    }
};

class ItemBufferReadingClient {
public:
    virtual ~ItemBufferReadingClient()
    {
    }

    virtual std::optional<ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t* data, size_t dataLength, ItemType, uint8_t* handleLocation) = 0;
};

// An ItemBuffer contains display list item data, and consists of a readwrite ItemBufferHandle (to which display
// list items are appended) as well as a number of readonly ItemBufferHandles. Items are appended to the readwrite
// buffer until all available capacity is exhausted, at which point we will move this writable handle to the list
// of readonly handles.
class ItemBuffer {
    WTF_MAKE_NONCOPYABLE(ItemBuffer); WTF_MAKE_FAST_ALLOCATED;
public:
    friend class DisplayList;

    WEBCORE_EXPORT ItemBuffer();
    WEBCORE_EXPORT ~ItemBuffer();
    ItemBuffer(ItemBufferHandles&&);

    WEBCORE_EXPORT ItemBuffer(ItemBuffer&&);
    WEBCORE_EXPORT ItemBuffer& operator=(ItemBuffer&&);

    size_t sizeInBytes() const
    {
        size_t result = 0;
        for (auto buffer : m_readOnlyBuffers)
            result += buffer.capacity;
        result += m_writtenNumberOfBytes;
        return result;
    }

    void clear();
    void shrinkToFit();

    bool isEmpty() const
    {
        return !m_writtenNumberOfBytes && m_readOnlyBuffers.isEmpty();
    }

    WEBCORE_EXPORT ItemBufferHandle createItemBuffer(size_t);

    // ItemBuffer::append<T>(...) appends a display list item of type T to the end of the current
    // writable buffer handle; if remaining buffer capacity is insufficient to store the item, a
    // new buffer will be allocated (either by the ItemBufferWritingClient, if set, or by the item
    // buffer itself if there is no client). Items are placed back-to-back in these item buffers,
    // with padding between each item to ensure that all items are aligned to 8 bytes.
    //
    // If a writing client is present and requires custom encoding for the given item type T, the
    // item buffer will ask the client for an opaque SharedBuffer containing encoded data for the
    // item. This encoded data is then appended to the item buffer, with padding to ensure that
    // the start and end of this data are aligned to 8 bytes, if necessary. When consuming encoded
    // item data, a corresponding ItemBufferReadingClient will be required to convert this encoded
    // data back into an item of type T.
    template<typename T, class... Args> void append(Args&&... args)
    {
        static_assert(std::is_trivially_destructible<T>::value == T::isInlineItem);

        if constexpr (!T::isInlineItem) {
            RELEASE_ASSERT(m_writingClient);
            append(DisplayListItem(T(std::forward<Args>(args)...)));
            return;
        }

        auto bufferChanged = swapWritableBufferIfNeeded(paddedSizeOfTypeAndItemInBytes(T::itemType));

        uncheckedAppend<T>(bufferChanged, std::forward<Args>(args)...);
    }

    void setClient(ItemBufferWritingClient* client) { m_writingClient = client; }
    void setClient(ItemBufferReadingClient* client) { m_readingClient = client; }
    void prepareToAppend(ItemBufferHandle&&);

private:
    const ItemBufferHandles& readOnlyBuffers() const { return m_readOnlyBuffers; }
    void forEachItemBuffer(Function<void(const ItemBufferHandle&)>&&) const;
    WEBCORE_EXPORT void didAppendData(size_t numberOfBytes, DidChangeItemBuffer);

    WEBCORE_EXPORT DidChangeItemBuffer swapWritableBufferIfNeeded(size_t numberOfBytes);
    WEBCORE_EXPORT void append(const DisplayListItem&);
    template<typename T, class... Args> void uncheckedAppend(DidChangeItemBuffer didChangeItemBuffer, Args&&... args)
    {
        auto* startOfItem = &m_writableBuffer.data[m_writtenNumberOfBytes];
        startOfItem[0] = static_cast<uint8_t>(T::itemType);
        new (startOfItem + sizeof(uint64_t)) T(std::forward<Args>(args)...);
        didAppendData(paddedSizeOfTypeAndItemInBytes(T::itemType), didChangeItemBuffer);
    }

    ItemBufferReadingClient* m_readingClient { nullptr };
    ItemBufferWritingClient* m_writingClient { nullptr };
    Vector<uint8_t*> m_allocatedBuffers;
    ItemBufferHandles m_readOnlyBuffers;
    ItemBufferHandle m_writableBuffer;
    size_t m_writtenNumberOfBytes { 0 };
};

} // namespace DisplayList
} // namespace WebCore
