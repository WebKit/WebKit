/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "BumpArena.h"

#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>

#if DEBUG_BUMPARENA
#include <notify.h>
#endif

namespace WTF {

class BumpArena::Block {
public:
    static const size_t blockSize = 4096;
    static const size_t blockMask = ~(blockSize - 1);

    static constexpr size_t defaultCapacity() { return Block::blockSize - sizeof(Block); }

    static Ref<Block> create(BumpArena&, size_t capacity);
    ~Block();

    void ref();
    void deref();

    BumpArena& arena() { return m_arena.get(); }

    static Block& blockFor(const void*);

    char* payloadStart() { return reinterpret_cast<char*>(&m_data); }

#if DEBUG_BUMPARENA
    void dump();
#endif

private:
    friend class BumpArena;
    explicit Block(BumpArena&);

    Ref<BumpArena> m_arena;

    unsigned m_refCount { 1 };

#if DEBUG_BUMPARENA
    size_t m_bytesAvailable { 0 };
#endif

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif
    char* m_data[0];
#if COMPILER(MSVC)
#pragma warning(pop)
#endif
};

static BumpArena& globalArena()
{
    static BumpArena& arena = BumpArena::create().leakRef();
    return arena;
}

#if DEBUG_BUMPARENA
static HashSet<BumpArena*>& arenas()
{
    static NeverDestroyed<HashSet<BumpArena*>> set;
    return set;
}
#endif

BumpArena::Block::Block(BumpArena& arena)
    : m_arena(arena)
{
#if DEBUG_BUMPARENA
    m_arena->m_liveBlocks.add(this);
#endif
}

BumpArena::Block::~Block()
{
#if DEBUG_BUMPARENA
    m_arena->m_liveBlocks.remove(this);
#endif
}

void BumpArena::Block::ref()
{
    ++m_refCount;
}

void BumpArena::Block::deref()
{
    --m_refCount;

    if (!m_refCount) {
        this->~Block();
        fastAlignedFree(this);
    } else if (m_refCount == 1 && m_arena->m_currentBlock == this) {
        m_arena->m_currentPayloadEnd = nullptr;
        m_arena->m_currentRemaining = 0;
        // Break the ref-cycle between BumpArena and this Block.
        m_arena->m_currentBlock = nullptr;
    }
}

Ref<BumpArena::Block> BumpArena::Block::create(BumpArena& arena, size_t capacity)
{
    return adoptRef(*new (NotNull, fastAlignedMalloc(blockSize, sizeof(Block) + capacity)) Block(arena));
}

#if DEBUG_BUMPARENA
void BumpArena::Block::dump()
{
    WTFLogAlways("  %10s %p - %3u, %8lu bytes available\n", m_arena->m_currentBlock == this ? "[current]" : "", this, m_refCount, m_bytesAvailable);
}

void BumpArena::dump()
{
    WTFLogAlways("%sBumpArena{%p}", &globalArena() == this ? "Global " : "", this);
    WTFLogAlways("  refCount: %u\n", refCount());
    WTFLogAlways("  Blocks (%d)\n", m_liveBlocks.size());
    for (Block* block : m_liveBlocks) {
        if (block == m_currentBlock)
            continue;
        block->dump();
    }
    if (m_currentBlock)
        m_currentBlock->dump();
}
#endif

Ref<BumpArena> BumpArena::create()
{
    return adoptRef(*new BumpArena);
}

BumpArena::BumpArena()
{
#if DEBUG_BUMPARENA
    arenas().add(this);
    static std::once_flag once;
    std::call_once(once, [] {
        int d;
        notify_register_dispatch("com.apple.WebKit.BumpArena.Dump", &d, dispatch_get_main_queue(), ^(int) {
            for (BumpArena* arena : arenas()) {
                if (&globalArena() == arena)
                    continue;
                arena->dump();
            }
            globalArena().dump();
        });
    });
#endif
}

BumpArena::~BumpArena()
{
#if DEBUG_BUMPARENA
    arenas().remove(this);
#endif
}

void* BumpArena::allocateSlow(size_t size)
{
    if (size >= Block::defaultCapacity()) {
        // leakRef() balanced by API deallocation of region inside block.
        Block& oversizeBlock = Block::create(*this, size).leakRef();
        return oversizeBlock.payloadStart();
    }

    m_currentBlock = Block::create(*this, Block::defaultCapacity());
    m_currentPayloadEnd = m_currentBlock->payloadStart() + Block::defaultCapacity();
    m_currentRemaining = Block::defaultCapacity() - size;
#if DEBUG_BUMPARENA
    m_currentBlock->m_bytesAvailable = m_currentRemaining;
#endif
    m_currentBlock->ref();
    return m_currentPayloadEnd - m_currentRemaining - size;
}

void* BumpArena::allocate(size_t size)
{
    size_t currentRemaining = m_currentRemaining;
    if (size > currentRemaining)
        return allocateSlow(size);
    currentRemaining -= size;
    m_currentRemaining = currentRemaining;
#if DEBUG_BUMPARENA
    m_currentBlock->m_bytesAvailable = m_currentRemaining;
#endif
    m_currentBlock->ref();
    return m_currentPayloadEnd - currentRemaining - size;
}

void BumpArena::deallocate(void* p)
{
    if (!p)
        return;
    Block::blockFor(p).deref();
}

BumpArena::Block& BumpArena::Block::blockFor(const void* p)
{
    return *reinterpret_cast<Block*>(reinterpret_cast<uintptr_t>(p) & blockMask);
}

BumpArena& BumpArena::arenaFor(const void* p)
{
    ASSERT(p);
    return Block::blockFor(p).arena();
}

void* BumpArena::allocate(BumpArena* arena, size_t size)
{
    if (arena)
        return arena->allocate(size);
    return globalArena().allocate(size);
}

} // namespace WTF
