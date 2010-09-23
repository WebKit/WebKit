/*
 * Copyright (C) 2010 Sencha, Inc.
 * Copyright (C) 2010 Igalia S.L.
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContextShadow.h"

#include "CairoUtilities.h"
#include "Timer.h"
#include <cairo.h>

namespace WebCore {

static cairo_surface_t* scratchBuffer = 0;
static void purgeScratchBuffer()
{
    cairo_surface_destroy(scratchBuffer);
    scratchBuffer = 0;
}

// ContextShadow needs a scratch image as the buffer for the blur filter.
// Instead of creating and destroying the buffer for every operation,
// we create a buffer which will be automatically purged via a timer.
class PurgeScratchBufferTimer : public TimerBase {
private:
    virtual void fired() { purgeScratchBuffer(); }
};
static PurgeScratchBufferTimer purgeScratchBufferTimer;
static void scheduleScratchBufferPurge()
{
    if (purgeScratchBufferTimer.isActive())
        purgeScratchBufferTimer.stop();
    purgeScratchBufferTimer.startOneShot(2);
}

static cairo_surface_t* getScratchBuffer(const IntSize& size)
{
    int width = size.width();
    int height = size.height();
    int scratchWidth = scratchBuffer ? cairo_image_surface_get_width(scratchBuffer) : 0;
    int scratchHeight = scratchBuffer ? cairo_image_surface_get_height(scratchBuffer) : 0;

    // We do not need to recreate the buffer if the current buffer is large enough.
    if (scratchBuffer && scratchWidth >= width && scratchHeight >= height)
        return scratchBuffer;

    purgeScratchBuffer();

    // Round to the nearest 32 pixels so we do not grow the buffer for similar sized requests.
    width = (1 + (width >> 5)) << 5;
    height = (1 + (height >> 5)) << 5;
    scratchBuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    return scratchBuffer;
}

PlatformContext ContextShadow::beginShadowLayer(PlatformContext context, const FloatRect& layerArea)
{
    double x1, x2, y1, y2;
    cairo_clip_extents(context, &x1, &y1, &x2, &y2);
    calculateLayerBoundingRect(layerArea, IntRect(x1, y1, x2 - x1, y2 - y1));

    // Don't paint if we are totally outside the clip region.
    if (m_layerRect.isEmpty())
        return 0;

    m_layerImage = getScratchBuffer(m_layerRect.size());
    m_layerContext = cairo_create(m_layerImage);

    // Always clear the surface first.
    cairo_set_operator(m_layerContext, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_layerContext);
    cairo_set_operator(m_layerContext, CAIRO_OPERATOR_OVER);

    cairo_translate(m_layerContext, m_offset.width(), m_offset.height());
    cairo_translate(m_layerContext, -m_layerRect.x(), -m_layerRect.y());
    return m_layerContext;
}

void ContextShadow::endShadowLayer(cairo_t* cr)
{
    cairo_destroy(m_layerContext);
    m_layerContext = 0;

    if (m_type == BlurShadow)
        blurLayerImage(cairo_image_surface_get_data(m_layerImage),
                       IntSize(cairo_image_surface_get_width(m_layerImage), cairo_image_surface_get_height(m_layerImage)),
                       cairo_image_surface_get_stride(m_layerImage));

    cairo_save(cr);
    setSourceRGBAFromColor(cr, m_color);
    cairo_mask_surface(cr, m_layerImage, m_layerRect.x(), m_layerRect.y());
    cairo_restore(cr);

    // Schedule a purge of the scratch buffer. We do not need to destroy the surface.
    scheduleScratchBufferPurge();
}

}
