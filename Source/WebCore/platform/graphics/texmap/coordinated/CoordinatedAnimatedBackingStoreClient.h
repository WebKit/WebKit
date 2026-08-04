/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS)
#include "FloatRect.h"
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class GraphicsLayer;
class TransformationMatrix;

class CoordinatedAnimatedBackingStoreClient final : public ThreadSafeRefCounted<CoordinatedAnimatedBackingStoreClient> {
public:
    static Ref<CoordinatedAnimatedBackingStoreClient> create();
    ~CoordinatedAnimatedBackingStoreClient() = default;

    void invalidate();
    void update(GraphicsLayer*, const FloatRect& visibleRect, const FloatRect& coverRect, const FloatSize&, float contentsScale);
    void requestBackingStoreUpdateIfNeeded(const TransformationMatrix&) const;

private:
    CoordinatedAnimatedBackingStoreClient() = default;

    GraphicsLayer* m_layer WTF_GUARDED_BY_CAPABILITY(mainThread) { nullptr };
    mutable Lock m_lock;
    FloatRect m_visibleRect WTF_GUARDED_BY_LOCK(m_lock);
    FloatRect m_coverRect WTF_GUARDED_BY_LOCK(m_lock);
    FloatSize m_size WTF_GUARDED_BY_LOCK(m_lock);
    float m_contentsScale WTF_GUARDED_BY_LOCK(m_lock) { 1 };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
