/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#if USE(TEXTURE_MAPPER)

#include "CoordinatedBackingStore.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace Nicosia {

class Buffer;

class ImageBackingStore : public ThreadSafeRefCounted<ImageBackingStore> {
public:
    ImageBackingStore();
    ~ImageBackingStore();

    struct BackingStoreState {
        RefPtr<Nicosia::Buffer> buffer;
    };
    BackingStoreState& backingStoreState() { return m_backingStoreState; }

    struct BackingStoreContainer : public ThreadSafeRefCounted<BackingStoreContainer> {
        RefPtr<WebCore::CoordinatedBackingStore> backingStore;
    };

    struct CompositionState {
        CompositionState()
            : backingStoreContainer(adoptRef(*new BackingStoreContainer))
        { }

        Ref<BackingStoreContainer> backingStoreContainer;
    };
    CompositionState& compositionState() { return m_compositionState; }

private:
    BackingStoreState m_backingStoreState;
    CompositionState m_compositionState;
};

} // namespace Nicosia

#endif // USE(TEXTURE_MAPPER)
