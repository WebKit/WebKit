/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "OpaqueRectTrackingContentLayerDelegate.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "PlatformContextSkia.h"
#include <public/WebFloatRect.h>
#include <public/WebRect.h>

using WebKit::WebFloatRect;
using WebKit::WebRect;

namespace WebCore {

OpaqueRectTrackingContentLayerDelegate::OpaqueRectTrackingContentLayerDelegate(GraphicsContextPainter* painter)
    : m_painter(painter)
    , m_opaque(false)
{
}

OpaqueRectTrackingContentLayerDelegate::~OpaqueRectTrackingContentLayerDelegate()
{
}

void OpaqueRectTrackingContentLayerDelegate::paintContents(SkCanvas* canvas, const WebRect& clip, bool canPaintLCDText, WebFloatRect& opaque)
{
    PlatformContextSkia platformContext(canvas);
#if defined(SK_SUPPORT_HINTING_SCALE_FACTOR)
    if (canvas)
        platformContext.setHintingScaleFactor(canvas->getTotalMatrix().getScaleX());
#endif
    platformContext.setTrackOpaqueRegion(!m_opaque);
    GraphicsContext context(&platformContext);
    context.setShouldSmoothFonts(canPaintLCDText && m_opaque);

    // Record transform prior to painting, as all opaque tracking will be
    // relative to this current value.
    AffineTransform canvasToContentTransform = context.getCTM().inverse();

    m_painter->paint(context, clip);

    // Transform tracked opaque paints back to our layer's content space.
    ASSERT(canvasToContentTransform.isInvertible());
    ASSERT(canvasToContentTransform.preservesAxisAlignment());
    opaque = canvasToContentTransform.mapRect(platformContext.opaqueRegion().asRect());
}

}
