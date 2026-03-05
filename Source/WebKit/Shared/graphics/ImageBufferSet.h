/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "ImageBufferSetIdentifier.h"
#include "PrepareBackingStoreBuffersData.h"
#include <WebCore/FloatRect.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/Region.h>
#include <wtf/Identified.h>
#include <wtf/Vector.h>

namespace WebKit {

// An ImageBufferSet is a set of three ImageBuffers (front, back, secondary back),
// for the purpose of drawing successive (layer) frames.
// It handles picking an existing buffer to be the new front buffer, and then copying
// forward the previous pixels, clipping to the dirty area and clearing that subset.
// FIXME: This class should also have common code for buffer allocation, setting volatility,
// and accessing the GraphicsContext.
// Splitting out a virtual base class so that RemoteImageBufferProxy can implement the interface
// would also be nice.
class ImageBufferSet {
public:
    using PaintRectList = Vector<WebCore::FloatRect, 5>;

    ImageBufferSet() = default;
    ImageBufferSet(ImageBufferSetIdentifier identifier)
        : m_identifier(identifier)
    { }

    // Tries to swap one of the existing back buffers to the new front buffer, if any are
    // not currently in-use.
    SwapBuffersDisplayRequirement swapBuffersForDisplay(bool hasEmptyDirtyRegion, bool supportsPartialRepaint);

    // Initializes the contents of the new front buffer using the previous
    // frames pixels(if applicable), clips to the dirty region, and clears the pixels
    // to be drawn (unless drawing will be opaque).
    void prepareBufferForDisplay(const WebCore::FloatRect& layerBounds, const WebCore::Region& dirtyRegion, const PaintRectList& paintingRects, bool requiresClearedPixels);


    static PaintRectList computePaintingRects(const WebCore::Region& dirtyRegion, float resolutionScale);

    void clearBuffers();

    ImageBufferSetIdentifier identifier() const { return m_identifier; }

    const ImageBufferSetIdentifier m_identifier { ImageBufferSetIdentifier::generate() };
    RefPtr<WebCore::ImageBuffer> m_frontBuffer;
    RefPtr<WebCore::ImageBuffer> m_backBuffer;
    RefPtr<WebCore::ImageBuffer> m_secondaryBackBuffer;

    std::optional<WebCore::IntRect> m_previouslyPaintedRect;
    bool m_frontBufferIsCleared { false };
};

} // namespace WebKit

#endif
