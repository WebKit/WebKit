/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef WebLayerTreeInfo_h
#define WebLayerTreeInfo_h

#if USE(COORDINATED_GRAPHICS)

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsLayer.h"

namespace WebKit {

typedef uint32_t WebLayerID;
enum { InvalidWebLayerID = 0 };

typedef uint64_t CoordinatedImageBackingID;
enum { InvalidCoordinatedImageBackingID = 0 };

// NOTE: WebLayerInfo should only use POD types, as to make serialization faster.
struct WebLayerInfo {
    WebLayerInfo()
        : replica(InvalidWebLayerID)
        , mask(InvalidWebLayerID)
        , imageID(InvalidCoordinatedImageBackingID)
        , opacity(0)
        , flags(0) { }

    WebLayerID replica;
    WebLayerID mask;
    CoordinatedImageBackingID imageID;

    WebCore::FloatPoint pos;
    WebCore::FloatPoint3D anchorPoint;
    WebCore::FloatSize size;
    WebCore::TransformationMatrix transform;
    WebCore::TransformationMatrix childrenTransform;
    WebCore::IntRect contentsRect;
    float opacity;
    WebCore::Color backgroundColor;

    union {
        struct {
            bool contentsOpaque : 1;
            bool drawsContent : 1;
            bool contentsVisible : 1;
            bool backfaceVisible : 1;
            bool masksToBounds : 1;
            bool preserves3D : 1;
            bool isRootLayer: 1;
            bool fixedToViewport : 1;
        };
        unsigned int flags;
    };

    void encode(CoreIPC::ArgumentEncoder&) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebLayerInfo&);
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WebLayerTreeInfo_h
