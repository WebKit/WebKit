/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef RemoteLayerTreeTransaction_h
#define RemoteLayerTreeTransaction_h

#include "RemoteLayerBackingStore.h"
#include <WebCore/Color.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatSize.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerTreeTransaction {
public:
    typedef uint64_t LayerID;

    enum LayerChange {
        NoChange = 0,
        NameChanged = 1 << 1,
        ChildrenChanged = 1 << 2,
        PositionChanged = 1 << 3,
        SizeChanged = 1 << 4,
        BackgroundColorChanged = 1 << 5,
        AnchorPointChanged = 1 << 6,
        BorderWidthChanged = 1 << 7,
        BorderColorChanged = 1 << 8,
        OpacityChanged = 1 << 9,
        TransformChanged = 1 << 10,
        SublayerTransformChanged = 1 << 11,
        HiddenChanged = 1 << 12,
        GeometryFlippedChanged = 1 << 13,
        DoubleSidedChanged = 1 << 14,
        MasksToBoundsChanged = 1 << 15,
        OpaqueChanged = 1 << 16,
        MaskLayerChanged = 1 << 17,
        ContentsRectChanged = 1 << 18,
        ContentsScaleChanged = 1 << 19,
        MinificationFilterChanged = 1 << 20,
        MagnificationFilterChanged = 1 << 21,
        SpeedChanged = 1 << 22,
        TimeOffsetChanged = 1 << 23,
        BackingStoreChanged = 1 << 24,
        FiltersChanged = 1 << 25
    };

    struct LayerCreationProperties {
        LayerCreationProperties();

        void encode(CoreIPC::ArgumentEncoder&) const;
        static bool decode(CoreIPC::ArgumentDecoder&, LayerCreationProperties&);

        LayerID layerID;
        WebCore::PlatformCALayer::LayerType type;
    };

    struct LayerProperties {
        LayerProperties();

        void encode(CoreIPC::ArgumentEncoder&) const;
        static bool decode(CoreIPC::ArgumentDecoder&, LayerProperties&);

        void notePropertiesChanged(LayerChange layerChanges) { changedProperties = static_cast<LayerChange>(changedProperties | layerChanges); }

        LayerChange changedProperties;

        String name;
        Vector<LayerID> children;
        WebCore::FloatPoint3D position;
        WebCore::FloatSize size;
        WebCore::Color backgroundColor;
        WebCore::FloatPoint3D anchorPoint;
        float borderWidth;
        WebCore::Color borderColor;
        float opacity;
        WebCore::TransformationMatrix transform;
        WebCore::TransformationMatrix sublayerTransform;
        bool hidden;
        bool geometryFlipped;
        bool doubleSided;
        bool masksToBounds;
        bool opaque;
        LayerID maskLayer;
        WebCore::FloatRect contentsRect;
        float contentsScale;
        WebCore::PlatformCALayer::FilterType minificationFilter;
        WebCore::PlatformCALayer::FilterType magnificationFilter;
        float speed;
        double timeOffset;
        RemoteLayerBackingStore backingStore;
        WebCore::FilterOperations filters;
    };

    explicit RemoteLayerTreeTransaction();
    ~RemoteLayerTreeTransaction();

    void encode(CoreIPC::ArgumentEncoder&) const;
    static bool decode(CoreIPC::ArgumentDecoder&, RemoteLayerTreeTransaction&);

    LayerID rootLayerID() const { return m_rootLayerID; }
    void setRootLayerID(LayerID rootLayerID);
    void layerPropertiesChanged(PlatformCALayerRemote*, LayerProperties&);
    void setCreatedLayers(Vector<LayerCreationProperties>);
    void setDestroyedLayerIDs(Vector<LayerID>);

#if !defined(NDEBUG) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    void dump() const;
#endif

    Vector<LayerCreationProperties> createdLayers() const { return m_createdLayers; }
    HashMap<LayerID, LayerProperties> changedLayers() const { return m_changedLayerProperties; }
    Vector<LayerID> destroyedLayers() const { return m_destroyedLayerIDs; }

private:
    LayerID m_rootLayerID;
    HashMap<LayerID, LayerProperties> m_changedLayerProperties;
    Vector<LayerCreationProperties> m_createdLayers;
    Vector<LayerID> m_destroyedLayerIDs;
};

} // namespace WebKit

#endif // RemoteLayerTreeTransaction_h
