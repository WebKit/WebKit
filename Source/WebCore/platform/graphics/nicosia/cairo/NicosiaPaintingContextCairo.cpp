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

#include "config.h"
#include "NicosiaPaintingContextCairo.h"

#if USE(CAIRO)

#include "GraphicsContext.h"
#include "GraphicsContextImplCairo.h"
#include "NicosiaBuffer.h"
#include "NicosiaCairoOperationRecorder.h"
#include "NicosiaPaintingOperationReplayCairo.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <utility>

namespace Nicosia {

PaintingContextCairo::ForPainting::ForPainting(Buffer& buffer)
{
    // Balanced by the deref in the s_bufferKey user data destroy callback.
    buffer.ref();

    m_cairo.surface = adoptRef(cairo_image_surface_create_for_data(buffer.data(),
        CAIRO_FORMAT_ARGB32, buffer.size().width(), buffer.size().height(), buffer.stride()));

    static cairo_user_data_key_t s_bufferKey;
    cairo_surface_set_user_data(m_cairo.surface.get(), &s_bufferKey,
        new std::pair<Buffer*, ForPainting*> { &buffer, this },
        [](void* data)
        {
            auto* userData = static_cast<std::pair<Buffer*, ForPainting*>*>(data);

            // Deref the Buffer object.
            userData->first->deref();
#if ASSERT_ENABLED
            // Mark the deletion of the cairo_surface_t object associated with this
            // PaintingContextCairo as complete. This way we check that the cairo_surface_t
            // object doesn't outlive the PaintingContextCairo through which it was used.
            userData->second->m_deletionComplete = true;
#endif
            delete userData;
        });

    m_cairo.context = adoptRef(cairo_create(m_cairo.surface.get()));
    m_platformContext = makeUnique<WebCore::PlatformContextCairo>(m_cairo.context.get());
    m_graphicsContext = makeUnique<WebCore::GraphicsContext>(WebCore::GraphicsContextImplCairo::createFactory(*m_platformContext));
}

PaintingContextCairo::ForPainting::~ForPainting()
{
    cairo_surface_flush(m_cairo.surface.get());

    m_graphicsContext = nullptr;
    m_platformContext = nullptr;
    m_cairo.context = nullptr;
    m_cairo.surface = nullptr;

    // With all the Cairo references purged, the cairo_surface_t object should be destroyed
    // as well. This is checked by asserting that m_deletionComplete is true, which should
    // be the case if the s_bufferKey user data destroy callback has been invoked upon the
    // cairo_surface_t destruction.
    ASSERT(m_deletionComplete);
}

WebCore::GraphicsContext& PaintingContextCairo::ForPainting::graphicsContext()
{
    return *m_graphicsContext;
}

void PaintingContextCairo::ForPainting::replay(const PaintingOperations& paintingOperations)
{
    PaintingOperationReplayCairo operationReplay { *m_platformContext };
    for (auto& operation : paintingOperations)
        operation->execute(operationReplay);
}

PaintingContextCairo::ForRecording::ForRecording(PaintingOperations& paintingOperations)
{
    m_graphicsContext = makeUnique<WebCore::GraphicsContext>(
        [&paintingOperations](WebCore::GraphicsContext& context)
        {
            return makeUnique<CairoOperationRecorder>(context, paintingOperations);
        });
}

PaintingContextCairo::ForRecording::~ForRecording() = default;

WebCore::GraphicsContext& PaintingContextCairo::ForRecording::graphicsContext()
{
    return *m_graphicsContext;
}

void PaintingContextCairo::ForRecording::replay(const PaintingOperations&)
{
    ASSERT_NOT_REACHED();
}

} // namespace Nicosia

#endif // USE(CAIRO)
