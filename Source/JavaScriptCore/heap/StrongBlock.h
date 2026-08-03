/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/HandleTypes.h>
#include <JavaScriptCore/MarkedBlock.h>
#include <limits>
#include <wtf/SentinelLinkedList.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class StrongSet;

// Holds the slots for Strong<> handles. The block is blockSize-aligned, so masking
// a slot address yields its block. Free slots are threaded into an intrusive list
// through the slots themselves; a link is tagged so that it can never read as a
// cell, which is what lets marking walk slots with no allocated-bits side table.
class StrongBlock {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(StrongBlock);
public:
    // Shared with MarkedBlock so that every block-aligned allocation in the heap
    // has one granularity. MarkedBlock already asserts it is a power of two, which
    // is what s_blockMask needs.
    static constexpr size_t blockSize = MarkedBlock::blockSize;

    class BlockListNode : public BasicRawSentinelNode<BlockListNode> {
    public:
        StrongBlock& block() { return *blockContaining(this); }
    };

    using BlockList = SentinelLinkedList<BlockListNode, BasicRawSentinelNode<BlockListNode>>;

    struct Header {
        uint16_t m_usedCount { 0 };
        // The used count at or below which a free must notify the owning
        // StrongSet. Zero means "notify only on empty".
        uint16_t m_freeNotifyThreshold { 0 };
        bool m_isCurrent { false };
        StrongSet* m_strongSet { nullptr };
        BlockListNode m_blocksNode;
        BlockListNode m_availableNode;
        HandleSlot m_freeListHead { nullptr };
    };

    static constexpr size_t headerSize = sizeof(Header);
    static constexpr unsigned capacity = static_cast<unsigned>((blockSize - headerSize) / sizeof(JSValue));

    static_assert(!(headerSize % sizeof(JSValue)), "The payload must begin slot-aligned and leave no slack.");
    static_assert(headerSize + capacity * sizeof(JSValue) == blockSize);
    static_assert(capacity <= std::numeric_limits<uint16_t>::max(), "m_usedCount must hold capacity inclusive.");

    JS_EXPORT_PRIVATE static StrongBlock* create(StrongSet*);

    ~StrongBlock();

    static StrongBlock* blockFor(HandleSlot);

    StrongSet* strongSet() const { return m_header.m_strongSet; }

    HandleSlot payload() const;
    HandleSlot payloadEnd() const;

    unsigned indexOf(HandleSlot) const;
    HandleSlot slotAtIndex(unsigned) const;

    unsigned usedCount() const { return m_header.m_usedCount; }
    bool isEmpty() const { return !m_header.m_usedCount; }
    bool isFull() const { return m_header.m_usedCount == capacity; }

    bool isAvailable() const { return m_header.m_availableNode.isOnList(); }

    bool isCurrent() const { return m_header.m_isCurrent; }

    HandleSlot freeListHead() const { return m_header.m_freeListHead; }

    unsigned freeNotifyThreshold() const { return m_header.m_freeNotifyThreshold; }

    static JSValue encodeFreeListEntry(HandleSlot next);
    static HandleSlot decodeFreeListEntry(JSValue);

private:
    // Every mutator below can break a cross-object invariant (the hoisted free
    // list, the availability chain) if called from anywhere but the owning
    // StrongSet, so they are kept private.
    friend class StrongSet;

    explicit StrongBlock(StrongSet*);

    BlockListNode& blocksNode() { return m_header.m_blocksNode; }
    BlockListNode& availableNode() { return m_header.m_availableNode; }
    void setCurrent(bool isCurrent) { m_header.m_isCurrent = isCurrent; }

    void setFreeListHead(HandleSlot);

    void pushFreeSlot(HandleSlot slot)
    {
        *slot = encodeFreeListEntry(m_header.m_freeListHead);
        m_header.m_freeListHead = slot;
    }

    void setFreeNotifyThreshold(unsigned threshold)
    {
        ASSERT(threshold <= capacity);
        m_header.m_freeNotifyThreshold = static_cast<uint16_t>(threshold);
    }

    void incrementUsedCount();
    unsigned decrementUsedCount();

    void resetToBumpMode();

    // Any address inside the block masks back to it, which is what lets both a
    // payload slot and an embedded header field name their block.
    static StrongBlock* blockContaining(const void*);

    static constexpr size_t s_blockMask = ~(blockSize - 1);

    Header m_header;
};

static_assert(sizeof(StrongBlock) == StrongBlock::headerSize, "A member outside m_header would eat into the payload.");

inline StrongBlock::~StrongBlock()
{
    if (m_header.m_blocksNode.isOnList())
        m_header.m_blocksNode.remove();
    if (m_header.m_availableNode.isOnList())
        m_header.m_availableNode.remove();
}

inline StrongBlock* StrongBlock::blockContaining(const void* address)
{
    return std::bit_cast<StrongBlock*>(std::bit_cast<uintptr_t>(address) & s_blockMask);
}

inline StrongBlock* StrongBlock::blockFor(HandleSlot slot)
{
    return blockContaining(slot);
}

inline HandleSlot StrongBlock::payload() const
{
    return std::bit_cast<HandleSlot>(std::bit_cast<uintptr_t>(this) + headerSize);
}

inline HandleSlot StrongBlock::payloadEnd() const
{
    return payload() + capacity;
}

inline unsigned StrongBlock::indexOf(HandleSlot slot) const
{
    ASSERT(blockFor(slot) == this);
    return static_cast<unsigned>(slot - payload());
}

inline HandleSlot StrongBlock::slotAtIndex(unsigned index) const
{
    ASSERT(index < capacity);
    return payload() + index;
}

#if USE(JSVALUE32_64)

// A pointer fits exactly in an int32 JSValue's payload, and Int32Tag != CellTag
// gives the same non-cell property as the 64-bit encoding above.
inline JSValue StrongBlock::encodeFreeListEntry(HandleSlot next)
{
    static_assert(sizeof(HandleSlot) == sizeof(uint32_t));
    return jsNumber(static_cast<int32_t>(std::bit_cast<uintptr_t>(next)));
}

inline HandleSlot StrongBlock::decodeFreeListEntry(JSValue value)
{
    ASSERT(value.isInt32());
    return std::bit_cast<HandleSlot>(static_cast<uintptr_t>(static_cast<uint32_t>(value.asInt32())));
}

#else

// A slot address is at most 48 significant bits on every supported 64-bit
// target and NumberTag occupies bits 49 through 63, so the tag strips back off
// exactly. isCell() is !(bits & NotCellMask) and NotCellMask includes NumberTag,
// so a tagged link can never be read as a cell; a bare pointer would be.
inline JSValue StrongBlock::encodeFreeListEntry(HandleSlot next)
{
    uint64_t bits = static_cast<uint64_t>(std::bit_cast<uintptr_t>(next));
    RELEASE_ASSERT(!(bits & static_cast<uint64_t>(JSValue::NumberTag)));
    return JSValue::decode(static_cast<EncodedJSValue>(bits | static_cast<uint64_t>(JSValue::NumberTag)));
}

inline HandleSlot StrongBlock::decodeFreeListEntry(JSValue value)
{
    uint64_t bits = static_cast<uint64_t>(JSValue::encode(value)) & ~static_cast<uint64_t>(JSValue::NumberTag);
    return std::bit_cast<HandleSlot>(static_cast<uintptr_t>(bits));
}

#endif

inline void StrongBlock::setFreeListHead(HandleSlot freeListHead)
{
    ASSERT(!freeListHead || blockFor(freeListHead) == this);
    m_header.m_freeListHead = freeListHead;
}

inline void StrongBlock::incrementUsedCount()
{
    ASSERT(m_header.m_usedCount < capacity);
    ++m_header.m_usedCount;
}

inline unsigned StrongBlock::decrementUsedCount()
{
    // Release-checked: a double-deallocated handle would underflow this to zero
    // while handles into the block are still live, and StrongSet would then
    // fastFree the block out from under them.
    RELEASE_ASSERT(m_header.m_usedCount);
    return --m_header.m_usedCount;
}

inline void StrongBlock::resetToBumpMode()
{
    ASSERT(isEmpty());
    m_header.m_freeListHead = nullptr;
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
