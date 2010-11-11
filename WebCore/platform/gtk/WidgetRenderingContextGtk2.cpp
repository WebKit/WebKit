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

#ifdef GTK_API_VERSION_2

#include "config.h"
#include "WidgetRenderingContext.h"

#include "GraphicsContext.h"
#include "RefPtrCairo.h"
#include "RenderThemeGtk.h"
#include "Timer.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

namespace WebCore {

static GdkPixmap* gScratchBuffer = 0;
static void purgeScratchBuffer()
{
    if (gScratchBuffer)
        g_object_unref(gScratchBuffer);
    gScratchBuffer = 0;
}

// FIXME: Perhaps we can share some of this code with the ContextShadowCairo.
// Widget rendering needs a scratch image as the buffer for the intermediate
// render. Instead of creating and destroying the buffer for every operation,
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

static IntSize getExtraSpaceForWidget(RenderThemeGtk* theme)
{
    // Checkboxes and scrollbar thumbs often draw outside their boundaries. Here we figure out
    // the maximum amount of space we need for any type of widget and use that to increase the
    // size of the scratch buffer, while preserving the final widget position.

    // The checkbox extra space is calculated by looking at the widget style.
    int indicatorSize, indicatorSpacing;
    theme->getIndicatorMetrics(CheckboxPart, indicatorSize, indicatorSpacing);
    IntSize extraSpace(indicatorSpacing, indicatorSpacing);

    // Scrollbar thumbs need at least an extra pixel along their movement axis.
    return extraSpace.expandedTo(IntSize(1, 1));
}

WidgetRenderingContext::WidgetRenderingContext(GraphicsContext* graphicsContext, const IntRect& targetRect)
    : m_graphicsContext(graphicsContext)
    , m_targetRect(targetRect)
    , m_hadError(false)
{
    RenderThemeGtk* theme = static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get());

    // Fallback: We failed to create an RGBA colormap earlier, so we cannot properly paint 
    // to a temporary surface and preserve transparency. To ensure decent widget rendering, just
    // paint directly to the target drawable. This will not render CSS rotational transforms properly.
    if (!theme->m_themePartsHaveRGBAColormap && graphicsContext->gdkDrawable()) {
        m_paintRect = graphicsContext->getCTM().mapRect(targetRect);
        m_target = graphicsContext->gdkDrawable();
        return;
    }

    // Some widgets render outside their rectangles. We need to account for this.
    m_extraSpace = getExtraSpaceForWidget(theme);

    // Offset the target rectangle so that the extra space is within the boundaries of the scratch buffer.
    m_paintRect = IntRect(IntPoint(m_extraSpace.width(), m_extraSpace.height()),
                                   m_targetRect.size());

    int width = m_targetRect.width() + (m_extraSpace.width() * 2);
    int height = m_targetRect.height() + (m_extraSpace.height() * 2);
    int scratchWidth = 0;
    int scratchHeight = 0;
    if (gScratchBuffer)
        gdk_drawable_get_size(gScratchBuffer, &scratchWidth, &scratchHeight);

    // We do not need to recreate the buffer if the current buffer is large enough.
    if (!gScratchBuffer || scratchWidth < width || scratchHeight < height) {
        purgeScratchBuffer();
        // Round to the nearest 32 pixels so we do not grow the buffer for similar sized requests.
        width = (1 + (width >> 5)) << 5;
        height = (1 + (height >> 5)) << 5;

        gScratchBuffer = gdk_pixmap_new(0, width, height, gdk_colormap_get_visual(theme->m_themeParts.colormap)->depth);
        gdk_drawable_set_colormap(gScratchBuffer, theme->m_themeParts.colormap);
    }
    m_target = gScratchBuffer;

    // Clear the scratch buffer.
    RefPtr<cairo_t> scratchBufferContext = adoptRef(gdk_cairo_create(gScratchBuffer));
    cairo_set_operator(scratchBufferContext.get(), CAIRO_OPERATOR_CLEAR);
    cairo_paint(scratchBufferContext.get());
}

WidgetRenderingContext::~WidgetRenderingContext()
{
    // We do not need to blit back to the target in the fallback case. See above.
    RenderThemeGtk* theme = static_cast<RenderThemeGtk*>(RenderTheme::defaultTheme().get());
    if (!theme->m_themePartsHaveRGBAColormap && m_graphicsContext->gdkDrawable())
        return;

    // Don't paint the results back if there was an error.
    if (m_hadError) {
        scheduleScratchBufferPurge();
        return;
    }

    // FIXME: It's unclear if it is necessary to preserve the current source here.
    cairo_t* cairoContext = m_graphicsContext->platformContext();
    RefPtr<cairo_pattern_t> previousSource(cairo_get_source(cairoContext));

    // The blit rectangle is the original target rectangle adjusted for any extra space.
    IntRect fullTargetRect(m_targetRect);
    fullTargetRect.inflateX(m_extraSpace.width());
    fullTargetRect.inflateY(m_extraSpace.height());

    gdk_cairo_set_source_pixmap(cairoContext, gScratchBuffer, fullTargetRect.x(), fullTargetRect.y());
    cairo_rectangle(cairoContext, fullTargetRect.x(), fullTargetRect.y(), fullTargetRect.width(), fullTargetRect.height());
    cairo_fill(cairoContext);
    cairo_set_source(cairoContext, previousSource.get());
    scheduleScratchBufferPurge();
}

bool WidgetRenderingContext::paintMozillaWidget(GtkThemeWidgetType type, GtkWidgetState* state, int flags, GtkTextDirection textDirection)
{
    // Sometimes moz_gtk_widget_paint modifies the clipping rectangle, so we must use a copy.
    GdkRectangle clipRect = m_paintRect;
    m_hadError = moz_gtk_widget_paint(type, m_target, &clipRect, &m_paintRect,
        state, flags, textDirection) != MOZ_GTK_SUCCESS;
    return !m_hadError;
}

}

#endif // GTK_API_VERSION_2
