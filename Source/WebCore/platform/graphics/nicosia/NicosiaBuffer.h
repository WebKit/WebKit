/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IntSize.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/MallocPtr.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace Nicosia {

class Buffer : public ThreadSafeRefCounted<Buffer> {
public:
    enum Flag {
        NoFlags = 0,
        SupportsAlpha = 1 << 0,
    };
    using Flags = unsigned;

    WEBCORE_EXPORT static Ref<Buffer> create(const WebCore::IntSize&, Flags);
    WEBCORE_EXPORT ~Buffer();

    bool supportsAlpha() const { return m_flags & SupportsAlpha; }
    const WebCore::IntSize& size() const { return m_size; }
    int stride() const { return m_size.width() * 4; }
    unsigned char* data() const { return m_data.get(); }

    void beginPainting();
    void completePainting();
    void waitUntilPaintingComplete();

private:
    Buffer(const WebCore::IntSize&, Flags);

    MallocPtr<unsigned char> m_data;
    WebCore::IntSize m_size;
    Flags m_flags;

    enum class PaintingState {
        InProgress,
        Complete
    };

    struct {
        Lock lock;
        Condition condition;
        PaintingState state { PaintingState::Complete };
    } m_painting;
};

} // namespace Nicosia
