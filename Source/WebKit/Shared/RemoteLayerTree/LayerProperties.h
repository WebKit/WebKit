/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "PlatformCAAnimationRemote.h"
#include <WebCore/PlatformCALayer.h>

namespace WebKit {

class RemoteLayerBackingStore;
class RemoteLayerBackingStoreProperties;

enum class LayerChangeIndex : size_t {
    EventRegionChanged = 39,
#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeIDChanged,
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    SeparatedChanged,
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    SeparatedPortalChanged,
    DescendentOfSeparatedPortalChanged,
#endif
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    VisibleRectChanged,
#endif
};

enum class LayerChange : uint64_t {
    NameChanged                         = 1LLU << 0,
    TransformChanged                    = 1LLU << 1,
    SublayerTransformChanged            = 1LLU << 2,
    ShapeRoundedRectChanged             = 1LLU << 3,
    ChildrenChanged                     = 1LLU << 4,
    AnimationsChanged                   = 1LLU << 5,
    PositionChanged                     = 1LLU << 6,
    AnchorPointChanged                  = 1LLU << 7,
    BoundsChanged                       = 1LLU << 8,
    ContentsRectChanged                 = 1LLU << 9,
    BackingStoreChanged                 = 1LLU << 10,
    FiltersChanged                      = 1LLU << 11,
    ShapePathChanged                    = 1LLU << 12,
    MaskLayerChanged                    = 1LLU << 13,
    ClonedContentsChanged               = 1LLU << 14,
    TimeOffsetChanged                   = 1LLU << 15,
    SpeedChanged                        = 1LLU << 16,
    ContentsScaleChanged                = 1LLU << 17,
    CornerRadiusChanged                 = 1LLU << 18,
    BorderWidthChanged                  = 1LLU << 19,
    OpacityChanged                      = 1LLU << 20,
    BackgroundColorChanged              = 1LLU << 21,
    BorderColorChanged                  = 1LLU << 22,
    CustomAppearanceChanged             = 1LLU << 23,
    MinificationFilterChanged           = 1LLU << 24,
    MagnificationFilterChanged          = 1LLU << 25,
    BlendModeChanged                    = 1LLU << 26,
    WindRuleChanged                     = 1LLU << 27,
    VideoGravityChanged                 = 1LLU << 28,
    AntialiasesEdgesChanged             = 1LLU << 29,
    HiddenChanged                       = 1LLU << 30,
    BackingStoreAttachmentChanged       = 1LLU << 31,
    GeometryFlippedChanged              = 1LLU << 32,
    DoubleSidedChanged                  = 1LLU << 33,
    MasksToBoundsChanged                = 1LLU << 34,
    OpaqueChanged                       = 1LLU << 35,
    ContentsHiddenChanged               = 1LLU << 36,
    UserInteractionEnabledChanged       = 1LLU << 37,
    BackdropRootChanged                 = 1LLU << 38,
    EventRegionChanged                  = 1LLU << static_cast<size_t>(LayerChangeIndex::EventRegionChanged),
#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeIDChanged              = 1LLU << static_cast<size_t>(LayerChangeIndex::ScrollingNodeIDChanged),
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    SeparatedChanged                    = 1LLU << static_cast<size_t>(LayerChangeIndex::SeparatedChanged),
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    SeparatedPortalChanged              = 1LLU << static_cast<size_t>(LayerChangeIndex::SeparatedPortalChanged),
    DescendentOfSeparatedPortalChanged  = 1LLU << static_cast<size_t>(LayerChangeIndex::DescendentOfSeparatedPortalChanged),
#endif
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    VisibleRectChanged                  = 1LLU << static_cast<size_t>(LayerChangeIndex::VisibleRectChanged),
#endif
};

struct RemoteLayerBackingStoreOrProperties {
    RemoteLayerBackingStoreOrProperties() = default;
    ~RemoteLayerBackingStoreOrProperties();
    RemoteLayerBackingStoreOrProperties(RemoteLayerBackingStoreOrProperties&&) = default;
    RemoteLayerBackingStoreOrProperties& operator=(RemoteLayerBackingStoreOrProperties&&) = default;
    RemoteLayerBackingStoreOrProperties(std::unique_ptr<RemoteLayerBackingStoreProperties>&& properties)
        : properties(WTFMove(properties)) { }

    // Used in the WebContent process.
    std::unique_ptr<RemoteLayerBackingStore> store;
    // Used in the UI process.
    std::unique_ptr<RemoteLayerBackingStoreProperties> properties;
};

struct LayerProperties {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    void notePropertiesChanged(OptionSet<LayerChange> changeFlags)
    {
        changedProperties.add(changeFlags);
        everChangedProperties.add(changeFlags);
    }

    void resetChangedProperties()
    {
        changedProperties = { };
    }

    OptionSet<LayerChange> changedProperties;
    OptionSet<LayerChange> everChangedProperties;

    String name;
    std::unique_ptr<WebCore::TransformationMatrix> transform;
    std::unique_ptr<WebCore::TransformationMatrix> sublayerTransform;
    std::unique_ptr<WebCore::FloatRoundedRect> shapeRoundedRect;

    Vector<WebCore::PlatformLayerIdentifier> children;

    struct AnimationChanges {
        Vector<std::pair<String, PlatformCAAnimationRemote::Properties>> addedAnimations;
        HashSet<String> keysOfAnimationsToRemove;
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        Vector<Ref<WebCore::AcceleratedEffect>> effects;
        WebCore::AcceleratedEffectValues baseValues;
#endif
    } animationChanges;

    WebCore::FloatPoint3D position;
    WebCore::FloatPoint3D anchorPoint { 0.5, 0.5, 0 };
    WebCore::FloatRect bounds;
    WebCore::FloatRect contentsRect { 0, 0, 1, 1 };
    RemoteLayerBackingStoreOrProperties backingStoreOrProperties;
    std::unique_ptr<WebCore::FilterOperations> filters;
    WebCore::Path shapePath;
    Markable<WebCore::PlatformLayerIdentifier> maskLayerID;
    Markable<WebCore::PlatformLayerIdentifier> clonedLayerID;
    double timeOffset { 0 };
    float speed { 1 };
    float contentsScale { 1 };
    float cornerRadius { 0 };
    float borderWidth { 0 };
    float opacity { 1 };
    WebCore::Color backgroundColor { WebCore::Color::transparentBlack };
    WebCore::Color borderColor { WebCore::Color::black };
    WebCore::GraphicsLayer::CustomAppearance customAppearance { WebCore::GraphicsLayer::CustomAppearance::None };
    WebCore::PlatformCALayer::FilterType minificationFilter { WebCore::PlatformCALayer::FilterType::Linear };
    WebCore::PlatformCALayer::FilterType magnificationFilter { WebCore::PlatformCALayer::FilterType::Linear };
    WebCore::BlendMode blendMode { WebCore::BlendMode::Normal };
    WebCore::WindRule windRule { WebCore::WindRule::NonZero };
    WebCore::MediaPlayerVideoGravity videoGravity { WebCore::MediaPlayerVideoGravity::ResizeAspect };
    bool antialiasesEdges { true };
    bool hidden { false };
    bool backingStoreAttached { true };
    bool geometryFlipped { false };
    bool doubleSided { false };
    bool masksToBounds { false };
    bool opaque { false };
    bool contentsHidden { false };
    bool userInteractionEnabled { true };
    bool backdropRoot { false };
    WebCore::EventRegion eventRegion;

#if ENABLE(SCROLLING_THREAD)
    WebCore::ScrollingNodeID scrollingNodeID { 0 };
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    bool isSeparated { false };
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    bool isSeparatedPortal { false };
    bool isDescendentOfSeparatedPortal { false };
#endif
#endif
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    WebCore::FloatRect visibleRect;
#endif
};

}
