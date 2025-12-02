/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "HeapHelperPool.h"

#include <mutex>
#include <wtf/NeverDestroyed.h>
#include "Options.h"

namespace JSC {

ParallelHelperPool& heapHelperPool()
{
    static std::once_flag initializeHelperPoolOnceFlag;
    static LazyNeverDestroyed<Ref<ParallelHelperPool>> helperPool;
    std::call_once(
        initializeHelperPoolOnceFlag,
        [] {
#if OS(LINUX)
            constexpr auto threadName = "HeapHelper"_s;
#else
            constexpr auto threadName = "Heap Helper Thread"_s;
#endif
            Ref pool = ParallelHelperPool::create(threadName);
            pool->ensureThreads(Options::numberOfGCMarkers() - 1);
            helperPool.construct(WTFMove(pool));
        });
    return helperPool.get().get();
}

} // namespace JSC
