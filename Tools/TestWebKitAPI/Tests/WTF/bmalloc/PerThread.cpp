/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#include "config.h"

#if !USE(SYSTEM_MALLOC)

#include <bmalloc/bmalloc.h>

#if !BUSE(LIBPAS)

#include <bmalloc/Cache.h>
#include <unordered_set>
#include <wtf/Threading.h>

using namespace bmalloc;
using namespace bmalloc::api;

TEST(bmalloc_PerThread, EachCacheHasDifferentCache)
{
    std::mutex mutex;
    std::unordered_set<void*> signatures;

    WTF::Vector<Ref<Thread>> threads;
    for (unsigned i = 0; i < 10; ++i) {
        threads.append(Thread::create("PerThreadTest", [&] {
            void* signature = Cache::getSignatureForTesting();

            std::scoped_lock<std::mutex> lock { mutex };
            EXPECT_FALSE(signatures.contains(signature));
            signatures.insert(signature);
        }));
    }

    for (auto& thread : threads)
        thread->waitForCompletion();
}

#endif

#endif
