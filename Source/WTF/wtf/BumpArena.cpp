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
#include <wtf/Vector.h>

#if OS(UNIX)
#define USE_BUMPARENA 1
#include <sys/mman.h>
#else
#define USE_BUMPARENA 0
#endif

#if OS(DARWIN)
#include <mach/vm_statistics.h>
// FIXME: Figure out which VM tag to use.
#define BUMPARENA_VM_TAG VM_MAKE_TAG(VM_MEMORY_APPLICATION_SPECIFIC_1)
#else
#define BUMPARENA_VM_TAG -1
#endif

#if DEBUG_BUMPARENA
#include <notify.h>
#endif

namespace WTF {

#if USE(BUMPARENA)

static const size_t blockSize = 4096;
static const size_t blockMask = ~(blockSize - 1);

class BumpArena::Block {
public:
    static constexpr size_t capacity() { return blockSize - sizeof(Block); }

    static RefPtr<Block> create(BumpArena&);
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

struct BlockAllocator {
    static const size_t vmSize = 128 * MB;

    BlockAllocator();

    void* allocateBlock();
    void deallocateBlock(void*);

    bool isAllocation(const void* p) const;

    char* vmBase;
    char* vmEnd;
    char* nextBlock;
    Vector<void*> freeList;
};

BlockAllocator::BlockAllocator()
{
    vmBase = reinterpret_cast<char*>(mmap(0, vmSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, BUMPARENA_VM_TAG, 0));
    RELEASE_ASSERT(vmBase != MAP_FAILED);
    vmEnd = vmBase + vmSize;
    nextBlock = reinterpret_cast<char*>(roundUpToMultipleOf<blockSize>(reinterpret_cast<uintptr_t>(vmBase)));
}

bool BlockAllocator::isAllocation(const void* p) const
{
    return p >= vmBase && p <= vmEnd;
}

static BlockAllocator& blockAllocator()
{
    static NeverDestroyed<BlockAllocator> allocator;
    return allocator;
}

void* BlockAllocator::allocateBlock()
{
    if (!freeList.isEmpty()) {
        void* block = freeList.takeLast();
#if OS(DARWIN)
        while (madvise(block, blockSize, MADV_FREE_REUSE) == -1 && errno == EAGAIN) { }
#else
        while (madvise(block, blockSize, MADV_NORMAL) == -1 && errno == EAGAIN) { }
#endif
        return block;
    }
    char* newNextBlock = nextBlock + blockSize;
    if (newNextBlock >= vmEnd)
        return nullptr;
    void* block = nextBlock;
    nextBlock += blockSize;
    return block;
}

void BlockAllocator::deallocateBlock(void* block)
{
#if OS(DARWIN)
    while (madvise(block, blockSize, MADV_FREE_REUSABLE) == -1 && errno == EAGAIN) { }
#else
    while (madvise(block, blockSize, MADV_DONTNEED) == -1 && errno == EAGAIN) { }
#endif
    freeList.append(block);
}

void BumpArena::Block::deref()
{
    --m_refCount;

    if (!m_refCount) {
        this->~Block();
        blockAllocator().deallocateBlock(this);
    } else if (m_refCount == 1 && m_arena->m_currentBlock == this) {
        m_arena->m_currentPayloadEnd = nullptr;
        m_arena->m_currentRemaining = 0;
        // Break the ref-cycle between BumpArena and this Block.
        m_arena->m_currentBlock = nullptr;
    }
}

RefPtr<BumpArena::Block> BumpArena::Block::create(BumpArena& arena)
{
    void* allocation = blockAllocator().allocateBlock();
    if (!allocation)
        return nullptr;
    return adoptRef(*new (NotNull, allocation) Block(arena));
}

#if DEBUG_BUMPARENA
void BumpArena::Block::dump()
{
    WTFLogAlways("  %10s %p - %3u, %8lu bytes available\n", m_arena->m_currentBlock == this ? "[current]" : "", this, m_refCount, m_bytesAvailable);
}

void BumpArena::dump()
{
    WTFLogAlways("BumpArena{%p}", this);
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
            for (BumpArena* arena : arenas())
                arena->dump();
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
    if (size > Block::capacity())
        return fastMalloc(size);

    m_currentBlock = Block::create(*this);
    if (!m_currentBlock) {
        m_currentPayloadEnd = nullptr;
        m_currentRemaining = 0;
        return fastMalloc(size);
    }

    m_currentPayloadEnd = m_currentBlock->payloadStart() + Block::capacity();
    m_currentRemaining = Block::capacity() - size;
#if DEBUG_BUMPARENA
    m_currentBlock->m_bytesAvailable = m_currentRemaining;
#endif
    m_currentBlock->ref();
    return m_currentPayloadEnd - m_currentRemaining - size;
}

void* BumpArena::allocate(size_t size)
{
    size = roundUpToMultipleOf<8>(size);
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
    if (blockAllocator().isAllocation(p))
        Block::blockFor(p).deref();
    else
        fastFree(p);
}

BumpArena::Block& BumpArena::Block::blockFor(const void* p)
{
    return *reinterpret_cast<Block*>(reinterpret_cast<uintptr_t>(p) & blockMask);
}

BumpArena* BumpArena::arenaFor(const void* p)
{
    ASSERT(p);
    if (!blockAllocator().isAllocation(p))
        return nullptr;
    return &Block::blockFor(p).arena();
}

void* BumpArena::allocate(BumpArena* arena, size_t size)
{
    if (arena)
        return arena->allocate(size);
    return fastMalloc(size);
}

#else // !USE(BUMPARENA) below

class BumpArena::Block {
public:
    void ref() { }
    void deref() { }
};

Ref<BumpArena> BumpArena::create()
{
    return adoptRef(*new BumpArena);
}

BumpArena::BumpArena()
{
}

BumpArena::~BumpArena()
{
}

BumpArena* BumpArena::arenaFor(const void*)
{
    return nullptr;
}

void* BumpArena::allocate(BumpArena*, size_t size)
{
    return fastMalloc(size);
}

void BumpArena::deallocate(void* p)
{
    fastFree(p);
}

#endif

} // namespace WTF
