/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Company 100, Inc. All rights reserved.
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

#ifndef CoordinatedGraphicsState_h
#define CoordinatedGraphicsState_h

#if USE(COORDINATED_GRAPHICS)

#include "Color.h"
#include "FilterOperations.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "IntRect.h"
#include "IntSize.h"
#include "SurfaceUpdateInfo.h"
#include "TextureMapperAnimation.h"
#include "TransformationMatrix.h"

#if USE(COORDINATED_GRAPHICS_THREADED)
#include "TextureMapperPlatformLayerProxy.h"
#endif

namespace WebCore {

class CoordinatedSurface;

typedef uint32_t CoordinatedLayerID;
enum { InvalidCoordinatedLayerID = 0 };

typedef uint64_t CoordinatedImageBackingID;
enum { InvalidCoordinatedImageBackingID = 0 };

struct TileUpdateInfo {
    uint32_t tileID;
    IntRect tileRect;
    WebCore::SurfaceUpdateInfo updateInfo;
};

struct TileCreationInfo {
    uint32_t tileID;
    float scale;
};

struct CoordinatedGraphicsLayerState {
    union {
        struct {
            bool positionChanged: 1;
            bool anchorPointChanged: 1;
            bool sizeChanged: 1;
            bool transformChanged: 1;
            bool childrenTransformChanged: 1;
            bool contentsRectChanged: 1;
            bool opacityChanged: 1;
            bool solidColorChanged: 1;
            bool debugBorderColorChanged: 1;
            bool debugBorderWidthChanged: 1;
            bool replicaChanged: 1;
            bool maskChanged: 1;
            bool imageChanged: 1;
            bool flagsChanged: 1;
            bool animationsChanged: 1;
            bool filtersChanged: 1;
            bool childrenChanged: 1;
            bool repaintCountChanged : 1;
            bool platformLayerChanged: 1;
            bool platformLayerShouldSwapBuffers: 1;
            bool isScrollableChanged: 1;
            bool committedScrollOffsetChanged: 1;
            bool contentsTilingChanged: 1;
        };
        unsigned changeMask;
    };
    union {
        struct {
            bool contentsOpaque : 1;
            bool drawsContent : 1;
            bool contentsVisible : 1;
            bool backfaceVisible : 1;
            bool masksToBounds : 1;
            bool preserves3D : 1;
            bool fixedToViewport : 1;
            bool showDebugBorders : 1;
            bool showRepaintCounter : 1;
            bool isScrollable: 1;
        };
        unsigned flags;
    };

    CoordinatedGraphicsLayerState()
        : changeMask(0)
        , contentsOpaque(false)
        , drawsContent(false)
        , contentsVisible(true)
        , backfaceVisible(true)
        , masksToBounds(false)
        , preserves3D(false)
        , fixedToViewport(false)
        , showDebugBorders(false)
        , showRepaintCounter(false)
        , isScrollable(false)
        , opacity(0)
        , debugBorderWidth(0)
        , replica(InvalidCoordinatedLayerID)
        , mask(InvalidCoordinatedLayerID)
        , imageID(InvalidCoordinatedImageBackingID)
        , repaintCount(0)
#if USE(COORDINATED_GRAPHICS_THREADED)
        , platformLayerProxy(0)
#endif
    {
    }

    FloatPoint pos;
    FloatPoint3D anchorPoint;
    FloatSize size;
    TransformationMatrix transform;
    TransformationMatrix childrenTransform;
    FloatRect contentsRect;
    FloatSize contentsTilePhase;
    FloatSize contentsTileSize;
    float opacity;
    Color solidColor;
    Color debugBorderColor;
    float debugBorderWidth;
    FilterOperations filters;
    TextureMapperAnimations animations;
    Vector<uint32_t> children;
    Vector<TileCreationInfo> tilesToCreate;
    Vector<uint32_t> tilesToRemove;
    CoordinatedLayerID replica;
    CoordinatedLayerID mask;
    CoordinatedImageBackingID imageID;

    unsigned repaintCount;
    Vector<TileUpdateInfo> tilesToUpdate;

#if USE(COORDINATED_GRAPHICS_THREADED)
    RefPtr<TextureMapperPlatformLayerProxy> platformLayerProxy;
#endif

    IntSize committedScrollOffset;

    bool hasPendingChanges() const
    {
        return changeMask || tilesToUpdate.size() || tilesToRemove.size() || tilesToCreate.size();
    }
};

struct CoordinatedGraphicsState {
    uint32_t rootCompositingLayer;
    FloatPoint scrollPosition;
    IntSize contentsSize;
    IntRect coveredRect;

    Vector<CoordinatedLayerID> layersToCreate;
    Vector<std::pair<CoordinatedLayerID, CoordinatedGraphicsLayerState> > layersToUpdate;
    Vector<CoordinatedLayerID> layersToRemove;

    Vector<CoordinatedImageBackingID> imagesToCreate;
    Vector<CoordinatedImageBackingID> imagesToRemove;
    Vector<std::pair<CoordinatedImageBackingID, RefPtr<CoordinatedSurface> > > imagesToUpdate;
    Vector<CoordinatedImageBackingID> imagesToClear;

    Vector<std::pair<uint32_t /* atlasID */, RefPtr<CoordinatedSurface> > > updateAtlasesToCreate;
    Vector<uint32_t /* atlasID */> updateAtlasesToRemove;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedGraphicsState_h
