/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (c) 2012, Intel Corporation
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
#include "PlatformContextCairo.h"

#if USE(CAIRO)

#include "CairoUtilities.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "Pattern.h"
#include <cairo.h>

namespace WebCore {

// In Cairo image masking is immediate, so to emulate image clipping we must save masking
// details as part of the context state and apply them during platform restore.
class ImageMaskInformation {
public:
    void update(cairo_surface_t* maskSurface, const FloatRect& maskRect)
    {
        m_maskSurface = maskSurface;
        m_maskRect = maskRect;
    }

    bool isValid() const { return m_maskSurface; }
    cairo_surface_t* maskSurface() const { return m_maskSurface.get(); }
    const FloatRect& maskRect() const { return m_maskRect; }

private:
    RefPtr<cairo_surface_t> m_maskSurface;
    FloatRect m_maskRect;
};


// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class PlatformContextCairo::State {
public:
    State() = default;

    ImageMaskInformation m_imageMaskInformation;
};

PlatformContextCairo::PlatformContextCairo(cairo_t* cr)
    : m_cr(cr)
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

void PlatformContextCairo::restore()
{
    const ImageMaskInformation& maskInformation = m_state->m_imageMaskInformation;
    if (maskInformation.isValid()) {
        const FloatRect& maskRect = maskInformation.maskRect();
        cairo_pop_group_to_source(m_cr.get());
        cairo_mask_surface(m_cr.get(), maskInformation.maskSurface(), maskRect.x(), maskRect.y());
    }

    m_stateStack.removeLast();
    ASSERT(!m_stateStack.isEmpty());
    m_state = &m_stateStack.last();

    cairo_restore(m_cr.get());
}

PlatformContextCairo::~PlatformContextCairo() = default;

void PlatformContextCairo::save()
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();

    cairo_save(m_cr.get());
}

void PlatformContextCairo::pushImageMask(cairo_surface_t* surface, const FloatRect& rect)
{
    // We must call savePlatformState at least once before we can use image masking,
    // since we actually apply the mask in restorePlatformState.
    ASSERT(!m_stateStack.isEmpty());
    m_state->m_imageMaskInformation.update(surface, rect);

    // Cairo doesn't support the notion of an image clip, so we push a group here
    // and then paint it to the surface with an image mask (which is an immediate
    // operation) during restorePlatformState.

    // We want to allow the clipped elements to composite with the surface as it
    // is now, but they are isolated in another group. To make this work, we're
    // going to blit the current surface contents onto the new group once we push it.
    cairo_surface_t* currentTarget = cairo_get_target(m_cr.get());
    cairo_surface_flush(currentTarget);

    // Pushing a new group ensures that only things painted after this point are clipped.
    cairo_push_group(m_cr.get());
    cairo_set_operator(m_cr.get(), CAIRO_OPERATOR_SOURCE);

    // To avoid the limit of Pixman backend, we need to reduce the size of pattern matrix
    // See https://bugs.webkit.org/show_bug.cgi?id=154283
    cairo_set_source_surface(m_cr.get(), currentTarget, rect.x(), rect.y());
    cairo_translate(m_cr.get(), rect.x(), rect.y());
    cairo_rectangle(m_cr.get(), 0, 0, rect.width(), rect.height());
    cairo_fill(m_cr.get());
    cairo_translate(m_cr.get(), -rect.x(), -rect.y());
}

} // namespace WebCore

#endif // USE(CAIRO)
