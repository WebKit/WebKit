/*
 * Copyright (C) 2026 Ron Masas <ronmasas@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <bmalloc/TZoneHeapManager.h>
#include <wtf/TZoneMallocInlines.h>

namespace TestWebKitAPI {

class TZoneMallocObject {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TZoneMallocObject);
public:
    explicit TZoneMallocObject(unsigned value)
        : m_value(value)
    {
    }

    unsigned value() const { return m_value; }

private:
    unsigned m_value;
};

TEST(WTF_TZoneMalloc, AllocateAndFree)
{
#if USE(TZONE_MALLOC)
    static_assert(WTF::usesTZoneHeap<TZoneMallocObject>());
#else
    static_assert(!WTF::usesTZoneHeap<TZoneMallocObject>());
#endif

    for (unsigned value = 0; value < 1000; ++value) {
        TZoneMallocObject* object = new TZoneMallocObject(value);
        EXPECT_EQ(value, object->value());
        delete object;
    }

#if USE(TZONE_MALLOC)
    EXPECT_TRUE(bmalloc::api::TZoneHeapManager::isReady());
#endif
}

} // namespace TestWebKitAPI
