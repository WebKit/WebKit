/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef GraphicsLayerTextureMapper_h
#define GraphicsLayerTextureMapper_h

#if USE(TEXTURE_MAPPER) && !USE(COORDINATED_GRAPHICS)

#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "Image.h"
#include "TextureMapperLayer.h"
#include "TextureMapperPlatformLayer.h"
#include "TextureMapperTiledBackingStore.h"
#include "Timer.h"

namespace WebCore {

class GraphicsLayerTextureMapper final : public GraphicsLayer, TextureMapperPlatformLayer::Client {
public:
    explicit GraphicsLayerTextureMapper(Type, GraphicsLayerClient&);
    virtual ~GraphicsLayerTextureMapper();

    void setScrollClient(TextureMapperLayer::ScrollingClient* client) { m_layer.setScrollClient(client); }
    void setID(uint32_t id) { m_layer.setID(id); }

    // GraphicsLayer
    virtual bool setChildren(const Vector<GraphicsLayer*>&) override;
    virtual void addChild(GraphicsLayer*) override;
    virtual void addChildAtIndex(GraphicsLayer*, int index) override;
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling) override;
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling) override;
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild) override;

    virtual void setMaskLayer(GraphicsLayer*) override;
    virtual void setReplicatedByLayer(GraphicsLayer*) override;
    virtual void setPosition(const FloatPoint&) override;
    virtual void setAnchorPoint(const FloatPoint3D&) override;
    virtual void setSize(const FloatSize&) override;
    virtual void setTransform(const TransformationMatrix&) override;
    virtual void setChildrenTransform(const TransformationMatrix&) override;
    virtual void setPreserves3D(bool) override;
    virtual void setMasksToBounds(bool) override;
    virtual void setDrawsContent(bool) override;
    virtual void setContentsVisible(bool) override;
    virtual void setContentsOpaque(bool) override;
    virtual void setBackfaceVisibility(bool) override;
    virtual void setOpacity(float) override;
    virtual bool setFilters(const FilterOperations&) override;

    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer = ClipToLayer) override;
    virtual void setContentsNeedsDisplay() override;
    virtual void setContentsRect(const FloatRect&) override;

    virtual bool addAnimation(const KeyframeValueList&, const FloatSize&, const Animation*, const String&, double) override;
    virtual void pauseAnimation(const String&, double) override;
    virtual void removeAnimation(const String&) override;

    virtual void setContentsToImage(Image*) override;
    virtual void setContentsToSolidColor(const Color&) override;
    virtual void setContentsToPlatformLayer(PlatformLayer*, ContentsLayerPurpose) override;
    virtual bool usesContentsLayer() const override { return m_contentsLayer; }
    virtual PlatformLayer* platformLayer() const override { return m_contentsLayer; }

    virtual void setShowDebugBorder(bool) override;
    virtual void setDebugBorder(const Color&, float width) override;
    virtual void setShowRepaintCounter(bool) override;

    virtual void flushCompositingState(const FloatRect&, bool) override;
    virtual void flushCompositingStateForThisLayerOnly(bool) override;

    void updateBackingStoreIncludingSubLayers(const FloatRect&);

    TextureMapperLayer& layer() { return m_layer; }

    void didCommitScrollOffset(const IntSize&);
    void setIsScrollable(bool);
    bool isScrollable() const { return m_isScrollable; }

    void setFixedToViewport(bool);
    bool fixedToViewport() const { return m_fixedToViewport; }

    Color debugBorderColor() const { return m_debugBorderColor; }
    float debugBorderWidth() const { return m_debugBorderWidth; }
    void setRepaintCount(int);

    void setAnimations(const TextureMapperAnimations&);
    void setAsNonCompositingLayer() { m_isNonCompositingLayer = true; }

private:
    // GraphicsLayer
    virtual bool isGraphicsLayerTextureMapper() const override { return true; }

    // TextureMapperPlatformLayer::Client
    virtual void platformLayerWillBeDestroyed() override { setContentsToPlatformLayer(0, NoContentsLayer); }
    virtual void setPlatformLayerNeedsDisplay() override { setContentsNeedsDisplay(); }

    void commitLayerChanges();
    void updateDebugBorderAndRepaintCount();
    void updateBackingStoreIfNeeded(const FloatRect&);
    void prepareBackingStoreIfNeeded();
    bool shouldHaveBackingStore() const;

    bool selfOrAncestorHasActiveTransformAnimation() const;
    void computeTransformedVisibleRect();
    void markVisibleRectAsDirty();
    FloatRect transformedVisibleRect(const FloatRect&);

    // This set of flags help us defer which properties of the layer have been
    // modified by the compositor, so we can know what to look for in the next flush.
    enum ChangeMask {
        NoChanges =                 0,

        ChildrenChange =            (1L << 1),
        MaskLayerChange =           (1L << 2),
        ReplicaLayerChange =        (1L << 3),

        ContentChange =             (1L << 4),
        ContentsRectChange =        (1L << 5),
        ContentsVisibleChange =     (1L << 6),
        ContentsOpaqueChange =      (1L << 7),

        PositionChange =            (1L << 8),
        AnchorPointChange =         (1L << 9),
        SizeChange =                (1L << 10),
        TransformChange =           (1L << 11),
        ChildrenTransformChange =   (1L << 12),
        Preserves3DChange =         (1L << 13),

        MasksToBoundsChange =       (1L << 14),
        DrawsContentChange =        (1L << 15),
        OpacityChange =             (1L << 16),
        BackfaceVisibilityChange =  (1L << 17),

        BackingStoreChange =        (1L << 18),
        DisplayChange =             (1L << 19),
        ContentsDisplayChange =     (1L << 20),
        BackgroundColorChange =     (1L << 21),

        AnimationChange =           (1L << 22),
        FilterChange =              (1L << 23),

        DebugVisualsChange =        (1L << 24),
        RepaintCountChange =        (1L << 25),

        FixedToViewporChange =      (1L << 26),
        AnimationStarted =          (1L << 27),

        CommittedScrollOffsetChange =     (1L << 28),
        IsScrollableChange =              (1L << 29)
    };
    void notifyChange(ChangeMask);

    TextureMapperLayer m_layer;
    RefPtr<TextureMapperTiledBackingStore> m_compositedImage;
    NativeImagePtr m_compositedNativeImagePtr;
    RefPtr<TextureMapperBackingStore> m_backingStore;

    int m_changeMask;
    bool m_needsDisplay;
    bool m_fixedToViewport;
    Color m_solidColor;

    Color m_debugBorderColor;
    float m_debugBorderWidth;

    TextureMapperPlatformLayer* m_contentsLayer;
    FloatRect m_needsDisplayRect;
    TextureMapperAnimations m_animations;
    double m_animationStartTime;

    IntSize m_committedScrollOffset;
    bool m_isScrollable;

    bool m_isNonCompositingLayer;
    bool m_isVisibleRectDirty;
    GraphicsLayerTransform m_layerTransform;
    TransformationMatrix m_cachedInverseTransform;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_GRAPHICSLAYER(WebCore::GraphicsLayerTextureMapper, isGraphicsLayerTextureMapper())

#endif

#endif // GraphicsLayerTextureMapper_h
