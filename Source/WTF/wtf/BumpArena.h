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

#ifndef BumpArena_h
#define BumpArena_h

#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#define DEBUG_BUMPARENA 0

namespace WTF {

class BumpArena : public RefCounted<BumpArena> {
public:
    WTF_EXPORT_PRIVATE static Ref<BumpArena> create();
    WTF_EXPORT_PRIVATE ~BumpArena();

    WTF_EXPORT_PRIVATE static void* allocate(BumpArena*, size_t);
    WTF_EXPORT_PRIVATE static void deallocate(void*);

    WTF_EXPORT_PRIVATE static BumpArena& arenaFor(const void*);

private:
    BumpArena();
    void* allocate(size_t);
    void* allocateSlow(size_t);

    class Block;
    friend class Block;
    RefPtr<Block> m_currentBlock;

    char* m_currentPayloadEnd { nullptr };
    size_t m_currentRemaining { 0 };

#if DEBUG_BUMPARENA
    void dump();
    HashSet<Block*> m_liveBlocks;
#endif
};

#define WTF_MAKE_BUMPARENA_ALLOCATED \
public: \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        return ::WTF::BumpArena::allocate(nullptr, size); \
    } \
    void* operator new(size_t size, BumpArena* arena) \
    { \
        return ::WTF::BumpArena::allocate(arena, size); \
    } \
    void operator delete(void* p) \
    { \
        ::WTF::BumpArena::deallocate(p); \
    } \
    \
    void* operator new[](size_t size) \
    { \
        return ::WTF::BumpArena::allocate(nullptr, size); \
    } \
    \
    void operator delete[](void* p) \
    { \
        ::WTF::BumpArena::deallocate(p); \
    } \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
private: \
typedef int __thisIsHereToForceASemicolonAfterThisMacro

} // namespace WTF

using WTF::BumpArena;

#endif
