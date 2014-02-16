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

#include "config.h"
#include "DrawingArea.h"
#include <wtf/Functional.h>

// Subclasses
#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingArea.h"
#include "TiledCoreAnimationDrawingArea.h"
#else
#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedDrawingArea.h"
#else
#include "DrawingAreaImpl.h"
#endif
#endif

#include "WebPageCreationParameters.h"

namespace WebKit {

std::unique_ptr<DrawingArea> DrawingArea::create(WebPage* webPage, const WebPageCreationParameters& parameters)
{
    switch (parameters.drawingAreaType) {
#if PLATFORM(COCOA)
#if !PLATFORM(IOS)
    case DrawingAreaTypeTiledCoreAnimation:
        return std::make_unique<TiledCoreAnimationDrawingArea>(webPage, parameters);
#endif
    case DrawingAreaTypeRemoteLayerTree:
        return std::make_unique<RemoteLayerTreeDrawingArea>(webPage, parameters);
#else
#if USE(COORDINATED_GRAPHICS)
    case DrawingAreaTypeCoordinated:
        return std::make_unique<CoordinatedDrawingArea>(webPage, parameters);
#else
    case DrawingAreaTypeImpl:
        return std::make_unique<DrawingAreaImpl>(webPage, parameters);
#endif
#endif
    }

    return nullptr;
}

DrawingArea::DrawingArea(DrawingAreaType type, WebPage* webPage)
    : m_type(type)
    , m_webPage(webPage)
{
}

DrawingArea::~DrawingArea()
{
}

void DrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(std::function<void ()> function)
{
    // Scroll position updates are synchronous by default so we can just call the function right away here.
    function();
}

} // namespace WebKit
