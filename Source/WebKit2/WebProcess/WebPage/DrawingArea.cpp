/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DrawingArea.h"

// Subclasses
#include "ChunkedUpdateDrawingArea.h"

#ifdef __APPLE__
#include "DrawingAreaImpl.h"
#endif

#if USE(ACCELERATED_COMPOSITING)
#include "LayerBackedDrawingArea.h"
#endif

#if ENABLE(TILED_BACKING_STORE)
#include "TiledDrawingArea.h"
#endif

#include "WebPageCreationParameters.h"

namespace WebKit {

PassRefPtr<DrawingArea> DrawingArea::create(WebPage* webPage, const WebPageCreationParameters& parameters)
{
    switch (parameters.drawingAreaInfo.type) {
        case DrawingAreaInfo::None:
            ASSERT_NOT_REACHED();
            break;

        case DrawingAreaInfo::Impl:
#ifdef __APPLE__
            return DrawingAreaImpl::create(webPage, parameters);
#else
            return 0;
#endif
        case DrawingAreaInfo::ChunkedUpdate:
            return adoptRef(new ChunkedUpdateDrawingArea(parameters.drawingAreaInfo.identifier, webPage));

#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
        case DrawingAreaInfo::LayerBacked:
            return adoptRef(new LayerBackedDrawingArea(parameters.drawingAreaInfo.identifier, webPage));
#endif
#if ENABLE(TILED_BACKING_STORE)
        case DrawingAreaInfo::Tiled:
            return adoptRef(new TiledDrawingArea(parameters.drawingAreaInfo.identifier, webPage));
#endif
    }

    return 0;
}

DrawingArea::DrawingArea(DrawingAreaInfo::Type type, DrawingAreaInfo::Identifier identifier, WebPage* webPage)
    : m_info(type, identifier)
    , m_webPage(webPage)
{
}

DrawingArea::~DrawingArea()
{
}

} // namespace WebKit
