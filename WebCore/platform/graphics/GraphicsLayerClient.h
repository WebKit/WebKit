/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsLayerClient_h
#define GraphicsLayerClient_h

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

class GraphicsContext;
class GraphicsLayer;
class IntPoint;
class IntRect;
class FloatPoint;

enum GraphicsLayerPaintingPhase {
    GraphicsLayerPaintBackgroundMask = (1 << 0),
    GraphicsLayerPaintForegroundMask = (1 << 1),
    GraphicsLayerPaintAllMask = (GraphicsLayerPaintBackgroundMask | GraphicsLayerPaintForegroundMask)
};

enum AnimatedPropertyID {
    AnimatedPropertyInvalid,
    AnimatedPropertyWebkitTransform,
    AnimatedPropertyOpacity,
    AnimatedPropertyBackgroundColor
};

class GraphicsLayerClient {
public:
    virtual ~GraphicsLayerClient() {}

    // Callbacks for when hardware-accelerated transitions and animation started
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) = 0;
    
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) = 0;

    // Return a rect for the "contents" of the graphics layer, i.e. video or image content, in GraphicsLayer coordinates.
    virtual IntRect contentsBox(const GraphicsLayer*) = 0;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerClient_h
