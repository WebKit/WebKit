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
#if USE(ACCELERATED_COMPOSITING)
#include "LayerBackedDrawingArea.h"
#endif

#if ENABLE(TILED_BACKING_STORE)
#include "TiledDrawingArea.h"
#endif

namespace WebKit {

PassRefPtr<DrawingArea> DrawingArea::create(Type type, DrawingAreaID identifier, WebPage* webPage)
{
    switch (type) {
        case None:
            ASSERT_NOT_REACHED();
            break;

        case ChunkedUpdateDrawingAreaType:
            return adoptRef(new ChunkedUpdateDrawingArea(identifier, webPage));

#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
        case LayerBackedDrawingAreaType:
            return adoptRef(new LayerBackedDrawingArea(identifier, webPage));
#endif
#if ENABLE(TILED_BACKING_STORE)
        case TiledDrawingAreaType:
            return adoptRef(new TiledDrawingArea(identifier, webPage));
#endif
    }

    return 0;
}

DrawingArea::DrawingArea(Type type, DrawingAreaID identifier, WebPage* webPage)
    : DrawingAreaBase(type, identifier)
    , m_webPage(webPage)
{
}

DrawingArea::~DrawingArea()
{
}

} // namespace WebKit
