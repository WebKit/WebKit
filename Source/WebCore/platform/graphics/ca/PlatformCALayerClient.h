/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef PlatformCALayerClient_h
#define PlatformCALayerClient_h

#include "GraphicsLayer.h"

namespace WebCore {

class FloatRect;
class GraphicsContext;
class PlatformCALayer;

class PlatformCALayerClient {
public:
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*) { }
    virtual bool platformCALayerRespondsToLayoutChanges() const { return false; }

    virtual void platformCALayerAnimationStarted(CFTimeInterval) { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const { return GraphicsLayer::CompositingCoordinatesTopDown; }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect& inClip) = 0;
    virtual bool platformCALayerShowDebugBorders() const { return false; }
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const { return false; }
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) { return 0; }
    
    virtual bool platformCALayerContentsOpaque() const = 0;
    virtual bool platformCALayerDrawsContent() const = 0;
    virtual void platformCALayerLayerDidDisplay(PlatformLayer*)  { }

    virtual void platformCALayerSetNeedsToRevalidateTiles() { }
    virtual float platformCALayerDeviceScaleFactor() const = 0;
    virtual float platformCALayerContentsScaleMultiplierForNewTiles(PlatformCALayer*) const { return 1; }

    virtual bool platformCALayerShouldAggressivelyRetainTiles(PlatformCALayer*) const { return false; }
    virtual bool platformCALayerShouldTemporarilyRetainTileCohorts(PlatformCALayer*) const { return true; }

    virtual bool isCommittingChanges() const { return false; }

protected:
    virtual ~PlatformCALayerClient() {}
};

}

#endif // PlatformCALayerClient_h
