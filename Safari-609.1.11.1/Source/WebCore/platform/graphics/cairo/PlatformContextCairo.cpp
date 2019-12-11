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

#include <cairo.h>

namespace WebCore {

// Encapsulates the additional painting state information we store for each
// pushed graphics state.
class PlatformContextCairo::State {
public:
    State() = default;

    struct {
        RefPtr<cairo_pattern_t> pattern;
        cairo_matrix_t matrix;
    } m_mask;
};

PlatformContextCairo::PlatformContextCairo(cairo_t* cr)
    : m_cr(cr)
{
    m_stateStack.append(State());
    m_state = &m_stateStack.last();
}

void PlatformContextCairo::restore()
{
    if (m_state->m_mask.pattern) {
        cairo_pop_group_to_source(m_cr.get());

        cairo_matrix_t matrix;
        cairo_get_matrix(m_cr.get(), &matrix);
        cairo_set_matrix(m_cr.get(), &m_state->m_mask.matrix);
        cairo_mask(m_cr.get(), m_state->m_mask.pattern.get());
        cairo_set_matrix(m_cr.get(), &matrix);
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
    m_state->m_mask.pattern = adoptRef(cairo_pattern_create_for_surface(surface));
    cairo_get_matrix(m_cr.get(), &m_state->m_mask.matrix);

    cairo_matrix_t matrix;
    cairo_matrix_init_translate(&matrix, -rect.x(), -rect.y());
    cairo_pattern_set_matrix(m_state->m_mask.pattern.get(), &matrix);

    cairo_push_group(m_cr.get());
}

} // namespace WebCore

#endif // USE(CAIRO)
