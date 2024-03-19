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

#import "config.h"
#import "ModelProcessModelPlayerProxy.h"

#if ENABLE(MODEL_PROCESS)

#import "LayerHostingContext.h"
#import "Logging.h"
#import "ModelConnectionToWebProcess.h"
#import "ModelProcessModelPlayerManagerProxy.h"
#import "ModelProcessModelPlayerMessages.h"
#import "WKModelProcessModelLayer.h"
#import <RealitySystemSupport/RealitySystemSupport.h>
#import <SurfBoardServices/SurfBoardServices.h>
#import <WebCore/Color.h>
#import <WebCore/LayerHostingContextIdentifier.h>
#import <WebCore/Model.h>
#import <WebCore/ResourceError.h>
#import <WebKitAdditions/REModel.h>
#import <WebKitAdditions/REModelLoader.h>
#import <WebKitAdditions/REPtr.h>
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#import <WebKitAdditions/WKREEngine.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MathExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/TextStream.h>

namespace WebKit {

Ref<ModelProcessModelPlayerProxy> ModelProcessModelPlayerProxy::create(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection)
{
    return adoptRef(*new ModelProcessModelPlayerProxy(manager, identifier, WTFMove(connection)));
}

ModelProcessModelPlayerProxy::ModelProcessModelPlayerProxy(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection)
    : m_id(identifier)
    , m_webProcessConnection(WTFMove(connection))
    , m_manager(manager)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy initialized id=%" PRIu64, this, identifier.toUInt64());
}

ModelProcessModelPlayerProxy::~ModelProcessModelPlayerProxy()
{
    if (m_loader)
        m_loader->cancel();

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy deallocated id=%" PRIu64, this, m_id.toUInt64());
}

void ModelProcessModelPlayerProxy::invalidate()
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy invalidated id=%" PRIu64, this, m_id.toUInt64());
}

template<typename T>
ALWAYS_INLINE void ModelProcessModelPlayerProxy::send(T&& message)
{
    m_webProcessConnection->send(std::forward<T>(message), m_id);
}

void ModelProcessModelPlayerProxy::createLayer()
{
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT(!m_layer);

    m_layer = adoptNS([[WKModelProcessModelLayer alloc] init]);
    [m_layer setName:@"WKModelProcessModelLayer"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.isPortal"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.transform"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.collider"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.mesh"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.material"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.texture"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.clippingPrimitive"];
    updateBackgroundColor();

    [m_layer setPlayer:RefPtr { this }];

    LayerHostingContextOptions contextOptions;
    m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess(contextOptions);
    m_layerHostingContext->setRootLayer(m_layer.get());

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy creating remote CA layer ctxID = %" PRIu64 " id=%" PRIu64, this, layerHostingContextIdentifier().value().toUInt64(), m_id.toUInt64());

    if (auto contextID = layerHostingContextIdentifier())
        send(Messages::ModelProcessModelPlayer::DidCreateLayer(contextID.value()));
}

void ModelProcessModelPlayerProxy::loadModel(Ref<WebCore::Model>&& model, WebCore::LayoutSize layoutSize)
{
    // FIXME: Change the IPC message to land on load() directly
    load(model, layoutSize);
}

static inline simd_float2 makeMeterSizeFromPointSize(CGSize pointSize, CGFloat pointsPerMeter)
{
    return simd_make_float2(pointSize.width / pointsPerMeter, pointSize.height / pointsPerMeter);
}

static simd_float3 computeExtents(simd_float2 boundsOfLayerInMeters, simd_float3 originalBoundingBoxExtents)
{
    if (simd_reduce_min(originalBoundingBoxExtents) - FLT_EPSILON > 0) {
        auto boundsScaleRatios = simd_make_float2(
            boundsOfLayerInMeters.x / originalBoundingBoxExtents.x,
            boundsOfLayerInMeters.y / originalBoundingBoxExtents.y
        );
        return simd_reduce_min(boundsScaleRatios) * originalBoundingBoxExtents;
    }

    return originalBoundingBoxExtents;
}

static RESRT computeSRT(CALayer *layer, simd_float3 originalBoundingBoxExtents, float pitch, float yaw, bool isPortal, CGFloat pointsPerMeter)
{
    auto boundsOfLayerInMeters = makeMeterSizeFromPointSize(layer.bounds.size, pointsPerMeter);
    auto extents = computeExtents(boundsOfLayerInMeters, originalBoundingBoxExtents);

    RESRT srt;
    srt.scale = simd_make_float3(extents.x / originalBoundingBoxExtents.x, extents.y / originalBoundingBoxExtents.y, extents.z / originalBoundingBoxExtents.z);

    // Must be normalized, but these obviously are.
    simd_float3 xAxis = simd_make_float3(1, 0, 0);
    simd_float3 yAxis = simd_make_float3(0, 1, 0);

    // FIXME: These should rotate around the center point of the model.
    simd_quatf pitchQuat = simd_quaternion(deg2rad(pitch), xAxis);
    simd_quatf yawQuat = simd_quaternion(deg2rad(yaw), yAxis);
    srt.rotation = simd_mul(pitchQuat, yawQuat);

    if (isPortal)
        srt.translation = simd_make_float3(0, -boundsOfLayerInMeters.y / 2.0f, -extents.z / 2.0f);
    else
        srt.translation = simd_make_float3(0, -boundsOfLayerInMeters.y / 2.0f, extents.z / 2.0f);

    return srt;
}

static CGFloat effectivePointsPerMeter(CALayer *caLayer)
{
    constexpr CGFloat defaultPointsPerMeter = 1360;

    CALayer *layer = caLayer;
    do {
        if (CGFloat pointsPerMeter = [[layer valueForKeyPath:@"separatedOptions.pointsPerMeter"] floatValue])
            return pointsPerMeter;
        layer = layer.superlayer;
    } while (layer);

    return defaultPointsPerMeter;
}

void ModelProcessModelPlayerProxy::updateBackgroundColor()
{
    if (!m_layer)
        return;

    if (m_backgroundColor.isValid())
        [m_layer setValue:(__bridge id)cachedCGColor(m_backgroundColor).get() forKeyPath:@"separatedOptions.material.clearColor"];
    else
        [m_layer setValue:(__bridge id)CGColorGetConstantColor(kCGColorWhite) forKeyPath:@"separatedOptions.material.clearColor"];
}

void ModelProcessModelPlayerProxy::updateTransform()
{
    if (!m_model || !m_layer)
        return;

    // FIXME: Use the value of the 'object-fit' property here to compute an appropriate SRT.
    RESRT newSRT = computeSRT(m_layer.get(), m_originalBoundingBoxExtents, m_pitch, m_yaw, true, effectivePointsPerMeter(m_layer.get()));

    auto transform = REEntityGetOrAddComponentByClass(m_model->rootEntity(), RETransformComponentGetComponentType());
    RETransformComponentSetLocalSRT(transform, newSRT);
    RENetworkMarkComponentDirty(transform);
}

void ModelProcessModelPlayerProxy::updateOpacity()
{
    if (!m_model || !m_layer)
        return;

    auto opacity = std::max(0.0f, [m_layer opacity]);

    if (opacity >= 1.0f) {
        // If the hosting layer is completely opaque, remove any fade component that might have been set
        // previously.
        REEntityRemoveComponentByClass(m_model->rootEntity(), REHierarchicalFadeComponentGetComponentType());
        // FIXME: Do we need to mark anything as dirty when removing a component?
        return;
    }

    auto hierarchicalFadeComponent = REEntityGetOrAddComponentByClass(m_model->rootEntity(), REHierarchicalFadeComponentGetComponentType());
    REHierarchicalFadeComponentSetOpacity(hierarchicalFadeComponent, opacity);
    RENetworkMarkComponentDirty(hierarchicalFadeComponent);
}

void ModelProcessModelPlayerProxy::startAnimating()
{
    if (!m_model || !m_layer)
        return;

    auto animationLibraryComponent = REEntityGetComponentByClass(m_model->rootEntity(), REAnimationLibraryComponentGetComponentType());
    if (!animationLibraryComponent)
        return;

    auto animationLibraryAsset = REAnimationLibraryComponentGetAnimationLibraryAsset(animationLibraryComponent);
    if (!animationLibraryAsset)
        return;

    auto engine = REEngineGetShared();
    auto serviceLocator = REEngineGetServiceLocator(engine);
    auto assetManager = REServiceLocatorGetAssetManager(serviceLocator);

    auto animationLibraryDefinition = adoptRE(REAnimationLibraryDefinitionCreateFromAnimationLibraryAsset(assetManager, animationLibraryAsset));
    if (!animationLibraryDefinition)
        return;

    // FIXME: Allow passing in the name of the animation to run, and then loop over all the
    // entries in the animationLibraryDefinition, extracting the root timelines and getting
    // their names via RETimelineDefinitionGetName().

    auto animationAsset = REAnimationLibraryDefinitionGetEntryAsset(animationLibraryDefinition.get(), 0);
    if (!animationAsset)
        return;

    auto extractRootTimelineDefinition = [] (auto animationAsset) -> REPtr<RETimelineDefinitionRef> {
        auto animationAssetType = REAssetHandleAssetType(animationAsset);
        if (animationAssetType == kREAssetTypeTimeline)
            return adoptRE(RETimelineDefinitionCreateFromTimeline(animationAsset));
        if (animationAssetType == kREAssetTypeAnimationScene) {
            auto rootTimelineAsset = REAnimationSceneAssetGetRootTimeline(animationAsset);
            return adoptRE(RETimelineDefinitionCreateFromTimeline(rootTimelineAsset));
        }
        return nullptr;
    };

    auto rootTimelineDefinition = extractRootTimelineDefinition(animationAsset);
    if (!rootTimelineDefinition) {
        RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy Could not extract root timeline from animation asset due to unknown asset type id=%" PRIu64 " type=%@", this, m_id.toUInt64(), (NSString *)REAssetGetType(animationAsset));
        return;
    }

    // FIXME: Allow passing in options to control looping behavior.

    // Wrap animation asset in an infinitely repeating clip.

    auto repeatingTimelineClipDefinition = adoptRE(RETimelineDefinitionCreateTimelineClip("Repeater", assetManager, rootTimelineDefinition.get()));
    RETimelineDefinitionSetClipLoopBehavior(repeatingTimelineClipDefinition.get(), kREAnimationLoopBehaviorRepeat);
    double duration = std::numeric_limits<double>::infinity();
    RETimelineDefinitionSetClipDuration(repeatingTimelineClipDefinition.get(), &duration);
    auto repeatingTimelineAsset = adoptRE(RETimelineDefinitionCreateTimelineAsset(repeatingTimelineClipDefinition.get(), assetManager));

    auto animationComponent = REEntityGetOrAddComponentByClass(m_model->rootEntity(), REAnimationComponentGetComponentType());
    auto animationHandoffDescription = REAnimationHandoffDefaultDescEx();
    m_animationPlaybackToken = REAnimationComponentPlay(animationComponent, repeatingTimelineAsset.get(), animationHandoffDescription, kREAnimationMarkComponentsDirty);
}

// MARK: - WebCore::RELoaderClient

static RECALayerService *webDefaultLayerService(void)
{
    return REServiceLocatorGetCALayerService(REEngineGetServiceLocator(REEngineGetShared()));
}

void ModelProcessModelPlayerProxy::didFinishLoading(WebCore::REModelLoader& loader, Ref<WebCore::REModel> model)
{
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT(&loader == m_loader.get());

    m_loader = nullptr;
    m_model = WTFMove(model);

    auto modelBoundingBox = REEntityComputeMeshBounds(m_model->rootEntity(), true, matrix_identity_float4x4, kREEntityStatusNone);
    m_originalBoundingBoxExtents = REAABBExtents(modelBoundingBox);

    REEntitySubtreeAddNetworkComponentRecursive(m_model->rootEntity());

    REPtr<REEntityRef> hostingEntity = adoptRE(REEntityCreate());
    REEntitySetName(hostingEntity.get(), "WebKit:EntityWithRootComponent");

    REPtr<REComponentRef> layerComponent = adoptRE(RECALayerServiceCreateRootComponent(webDefaultLayerService(), CALayerGetContext(m_layer.get()), hostingEntity.get(), nil));
    RESceneAddEntity(m_scene.get(), hostingEntity.get());

    CALayer *contextEntityLayer = RECALayerClientComponentGetCALayer(layerComponent.get());
    [contextEntityLayer setSeparatedState:kCALayerSeparatedStateSeparated];

    RECALayerClientComponentSetShouldSyncToRemotes(layerComponent.get(), true);

    auto clientComponent = RECALayerGetCALayerClientComponent(m_layer.get());
    auto rootEntity = REComponentGetEntity(clientComponent);
    REEntitySetName(rootEntity, "WebKit:ClientComponentRoot");
    REEntitySetName(m_model->rootEntity(), "WebKit:ModelRootEntity");
    REEntitySetParent(m_model->rootEntity(), rootEntity);

    updateBackgroundColor();
    updateTransform();
    updateOpacity();
    startAnimating();

    send(Messages::ModelProcessModelPlayer::DidFinishLoading());
}

void ModelProcessModelPlayerProxy::didFailLoading(WebCore::REModelLoader& loader, const WebCore::ResourceError& error)
{
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT(&loader == m_loader.get());

    m_loader = nullptr;

    RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy failed to load model id=%" PRIu64 " error=\"%@\"", this, m_id.toUInt64(), error.nsError().localizedDescription);

    // FIXME: Do something sensible in the failure case.
}

// MARK: - WebCore::ModelPlayer

void ModelProcessModelPlayerProxy::load(WebCore::Model& model, WebCore::LayoutSize layoutSize)
{
    dispatch_assert_queue(dispatch_get_main_queue());

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::load size=%zu id=%" PRIu64, this, model.data()->size(), m_id.toUInt64());
    sizeDidChange(layoutSize);

    WKREEngine::shared().runWithSharedScene([this, protectedThis = Ref { *this }, model = Ref { model }] (RESceneRef scene) {
        m_scene = scene;
        m_loader = WebCore::loadREModel(model.get(), *this);
    });
}

void ModelProcessModelPlayerProxy::sizeDidChange(WebCore::LayoutSize layoutSize)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::sizeDidChange w=%lf h=%lf id=%" PRIu64, this, layoutSize.width().toDouble(), layoutSize.width().toDouble(), m_id.toUInt64());
    [m_layer setFrame:CGRectMake(0, 0, layoutSize.width().toDouble(), layoutSize.width().toDouble())];
}

PlatformLayer* ModelProcessModelPlayerProxy::layer()
{
    return nullptr;
}

std::optional<WebCore::LayerHostingContextIdentifier> ModelProcessModelPlayerProxy::layerHostingContextIdentifier()
{
    return WebCore::LayerHostingContextIdentifier(m_layerHostingContext->contextID());
}

void ModelProcessModelPlayerProxy::setBackgroundColor(WebCore::Color color)
{
    m_backgroundColor = color.opaqueColor();
    updateBackgroundColor();
}

void ModelProcessModelPlayerProxy::enterFullscreen()
{
}

bool ModelProcessModelPlayerProxy::supportsMouseInteraction()
{
    return false;
}

bool ModelProcessModelPlayerProxy::supportsDragging()
{
    return false;
}

void ModelProcessModelPlayerProxy::setInteractionEnabled(bool isInteractionEnabled)
{
}

void ModelProcessModelPlayerProxy::handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayerProxy::handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayerProxy::handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime)
{
}

void ModelProcessModelPlayerProxy::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setCamera(WebCore::HTMLModelElementCamera camera, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setAnimationIsPlaying(bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setIsLoopingAnimation(bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setAnimationCurrentTime(Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::isMuted(CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setIsMuted(bool isMuted, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

Vector<RetainPtr<id>> ModelProcessModelPlayerProxy::accessibilityChildren()
{
    return { };
}

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
