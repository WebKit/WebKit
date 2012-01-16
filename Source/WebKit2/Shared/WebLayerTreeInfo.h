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

#if USE(ACCELERATED_COMPOSITING)

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsLayer.h"
#include "ShareableBitmap.h"

namespace WebKit {

typedef uint32_t WebLayerID;
enum { InvalidWebLayerID = 0 };

struct WebLayerUpdateInfo {
    WebLayerUpdateInfo() { }
    WebLayerUpdateInfo(const WebCore::IntRect& r)
        : layerID(InvalidWebLayerID)
        , rect(r) { }

    WebLayerID layerID;
    WebCore::IntRect rect;
    ShareableBitmap::Handle bitmapHandle;

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebLayerUpdateInfo&);
};

struct WebLayerAnimation {
    WebLayerAnimation()
        : operation(InvalidAnimation)
        , keyframeList(WebCore::AnimatedPropertyInvalid)
        , startTime(0) { }
    WebLayerAnimation(const WebCore::KeyframeValueList& valueList)
        : operation(InvalidAnimation)
        , keyframeList(valueList)
        , startTime(0) { }

    String name;
    enum Operation {
        AddAnimation,
        RemoveAnimation,
        PauseAnimation,
        InvalidAnimation
    } operation;
    WebCore::IntSize boxSize;
    RefPtr<WebCore::Animation> animation;
    WebCore::KeyframeValueList keyframeList;
    double startTime;

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebLayerAnimation&);
};

struct WebLayerInfo {
    WebLayerInfo()
        : id(InvalidWebLayerID)
        , parent(InvalidWebLayerID)
        , replica(InvalidWebLayerID)
        , mask(InvalidWebLayerID)
        , imageBackingStoreID(0)
        , opacity(0)
        , flags(0) { }

    String name;
    WebLayerID id;
    WebLayerID parent;
    WebLayerID replica;
    WebLayerID mask;
    int64_t imageBackingStoreID;

    WebCore::FloatPoint pos;
    WebCore::FloatPoint3D anchorPoint;
    WebCore::FloatSize size;
    WebCore::TransformationMatrix transform;
    WebCore::TransformationMatrix childrenTransform;
    WebCore::IntRect contentsRect;
    float opacity;

    union {
        struct {
            bool contentsOpaque : 1;
            bool drawsContent : 1;
            bool backfaceVisible : 1;
            bool masksToBounds : 1;
            bool preserves3D : 1;
            bool contentNeedsDisplay : 1;
            bool imageIsUpdated: 1;
            bool isRootLayer: 1;
        };
        unsigned int flags;
    };
    Vector<WebLayerID> children;
    Vector<WebLayerAnimation> animations;
    RefPtr<ShareableBitmap> imageBackingStore;

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebLayerInfo&);
};

struct WebLayerTreeInfo {
    WebLayerTreeInfo()
        : rootLayerID(InvalidWebLayerID)
        , contentScale(0) { }

    Vector<WebLayerInfo> layers;
    Vector<WebLayerID> deletedLayerIDs;
    WebLayerID rootLayerID;
    float contentScale;

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, WebLayerTreeInfo&);
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WebLayerTreeInfo_h
