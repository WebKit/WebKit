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

#include "NicosiaPaintingContext.h"

#if USE(CAIRO)

#include <wtf/RefPtr.h>

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

namespace WebCore {
class GraphicsContext;
class PlatformContextCairo;
}

namespace Nicosia {

class PaintingContextCairo final {
public:
    class ForPainting final : public PaintingContext {
    public:
        explicit ForPainting(Buffer&);
        virtual ~ForPainting();

    private:
        WebCore::GraphicsContext& graphicsContext() override;
        void replay(const PaintingOperations&) override;

        struct {
            RefPtr<cairo_surface_t> surface;
            RefPtr<cairo_t> context;
        } m_cairo;
        std::unique_ptr<WebCore::PlatformContextCairo> m_platformContext;
        std::unique_ptr<WebCore::GraphicsContext> m_graphicsContext;

#ifndef NDEBUG
        bool m_deletionComplete { false };
#endif
    };

    class ForRecording final : public PaintingContext {
    public:
        ForRecording(PaintingOperations&);
        virtual ~ForRecording();

    private:
        WebCore::GraphicsContext& graphicsContext() override;
        void replay(const PaintingOperations&) override;

        std::unique_ptr<WebCore::GraphicsContext> m_graphicsContext;
    };
};

} // namespace Nicosia

#endif // USE(CAIRO)
