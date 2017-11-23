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

#if USE(COORDINATED_GRAPHICS)

#include "IntSize.h"
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class GraphicsContext;
class Image;
class ImageBuffer;
class IntRect;

class CoordinatedBuffer : public ThreadSafeRefCounted<CoordinatedBuffer> {
public:
    enum Flag {
        NoFlags = 0,
        SupportsAlpha = 1 << 0,
    };
    using Flags = unsigned;

    class Client {
    public:
        virtual ~Client() = default;
        virtual void paintToSurfaceContext(GraphicsContext&) = 0;
    };

    static Ref<CoordinatedBuffer> create(const IntSize&, Flags);
    ~CoordinatedBuffer();

    bool supportsAlpha() const { return m_flags & SupportsAlpha; }
    const IntSize& size() const { return m_size; }

    void paintToSurface(const IntRect&, Client&);
    RefPtr<Image> uploadImage();

private:
    CoordinatedBuffer(const IntSize&, Flags);

    std::unique_ptr<ImageBuffer> m_imageBuffer;
    IntSize m_size;
    Flags m_flags;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
