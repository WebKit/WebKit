/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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

#pragma once

#if USE(SKIA)

#include "IntRect.h"
#include "IntSize.h"

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkImage.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

// Stores atlas layout computed during recording.
// Shared (read-only) across all worker threads during replay.
// Does NOT hold GPU textures - those are created per-worker in SkiaReplayAtlas.
class SkiaImageAtlasLayout final : public ThreadSafeRefCounted<SkiaImageAtlasLayout> {
public:
    struct Entry {
        sk_sp<SkImage> rasterImage;
        IntRect atlasRect;
    };

    static Ref<SkiaImageAtlasLayout> create(const IntSize& atlasSize, Vector<Entry>&& entries);

    const IntSize& atlasSize() const { return m_atlasSize; }
    const Vector<Entry>& entries() const { return m_entries; }

private:
    SkiaImageAtlasLayout(const IntSize& atlasSize, Vector<Entry>&& entries);

    IntSize m_atlasSize;
    Vector<Entry> m_entries;
};

} // namespace WebCore

#endif // USE(SKIA)
