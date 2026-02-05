/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#import "WKRKEntity.h"
#import "WKStageMode.h"
#import <RealitySystemSupport/RealitySystemSupport.h>
#import <SurfBoardServices/SurfBoardServices.h>
#import <WebCore/Color.h>
#import <WebCore/LayerHostingContextIdentifier.h>
#import <WebCore/Model.h>
#import <WebCore/ModelPlayerGraphicsLayerConfiguration.h>
#import <WebCore/ResourceError.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebKitAdditions/REModel.h>
#import <WebKitAdditions/REModelLoader.h>
#import <WebKitAdditions/REPtr.h>
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#import <WebKitAdditions/WKREEngine.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/MathExtras.h>
#import <wtf/NakedPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/TextStream.h>

#import "WebKitSwiftSoftLink.h"

@interface WKModelProcessModelPlayerProxyObjCAdapter : NSObject<WKRKEntityDelegate, WKStageModeInteractionAware>
- (instancetype)initWithModelProcessModelPlayerProxy:(std::reference_wrapper<WebKit::ModelProcessModelPlayerProxy>)modelProcessModelPlayerProxy;
@end

@implementation WKModelProcessModelPlayerProxyObjCAdapter {
    WeakPtr<WebKit::ModelProcessModelPlayerProxy> _modelProcessModelPlayerProxy;
}

- (instancetype)initWithModelProcessModelPlayerProxy:(std::reference_wrapper<WebKit::ModelProcessModelPlayerProxy>)modelProcessModelPlayerProxy
{
    if (!(self = [super init]))
        return nil;

    _modelProcessModelPlayerProxy = modelProcessModelPlayerProxy.get();
    return self;
}

- (void)entityAnimationPlaybackStateDidUpdate:(id)entity
{
    _modelProcessModelPlayerProxy->animationPlaybackStateDidUpdate();
}

- (void)stageModeInteractionDidUpdateModel
{
    _modelProcessModelPlayerProxy->stageModeInteractionDidUpdateModel();
}

@end

namespace WebKit {

static const Seconds unloadModelDelay { 4_s };

class RKModelUSD final : public WebCore::REModel {
public:
    static Ref<RKModelUSD> create(Ref<Model> model, RetainPtr<WKRKEntity> entity)
    {
        return adoptRef(*new RKModelUSD(WTF::move(model), WTF::move(entity)));
    }

    virtual ~RKModelUSD() = default;

private:
    RKModelUSD(Ref<Model> model, RetainPtr<WKRKEntity> entity)
        : m_model { WTF::move(model) }
        , m_entity { WTF::move(entity) }
    {
    }

    // REModel overrides.
    const Model& modelSource() const final
    {
        return m_model;
    }

    REEntityRef rootEntity() const final
    {
        return nullptr;
    }

    RetainPtr<WKRKEntity> rootRKEntity() const final
    {
        return m_entity;
    }

    Ref<Model> m_model;
    RetainPtr<WKRKEntity> m_entity;
};

class RKModelLoaderUSD final : public WebCore::REModelLoader, public CanMakeWeakPtr<RKModelLoaderUSD> {
public:
    static Ref<RKModelLoaderUSD> create(Model& model, const std::optional<String>& attributionTaskID, std::optional<int> entityMemoryLimit, REModelLoaderClient& client)
    {
        return adoptRef(*new RKModelLoaderUSD(model, attributionTaskID, entityMemoryLimit, client));
    }

    virtual ~RKModelLoaderUSD() = default;

    void load(CompletionHandler<void()>&&);

    bool isCanceled() const { return m_canceled; }

private:
    RKModelLoaderUSD(Model& model, const std::optional<String>& attributionTaskID, std::optional<int> entityMemoryLimit, REModelLoaderClient& client)
        : m_canceled { false }
        , m_model { model }
        , m_attributionTaskID { attributionTaskID }
        , m_entityMemoryLimit(entityMemoryLimit)
        , m_client { client }
    {
    }

    // REModelLoader overrides.
    void cancel() final
    {
        m_canceled = true;
    }

    void didFinish(RetainPtr<WKRKEntity> entity)
    {
        if (m_canceled)
            return;

        if (auto strongClient = m_client.get())
            strongClient->didFinishLoading(*this, RKModelUSD::create(WTF::move(m_model), entity));
    }

    void didFail(ResourceError error)
    {
        if (m_canceled)
            return;

        if (auto strongClient = m_client.get())
            strongClient->didFailLoading(*this, WTF::move(error));
    }

    bool m_canceled { false };

    Ref<Model> m_model;
    std::optional<String> m_attributionTaskID;
    std::optional<int> m_entityMemoryLimit;
    WeakPtr<REModelLoaderClient> m_client;
};

static ResourceError toResourceError(String payload, Model& model)
{
    return ResourceError { [NSError errorWithDomain:@"RKModelLoaderUSD" code:-1 userInfo:@{
        NSLocalizedDescriptionKey: payload.createNSString().get(),
        NSURLErrorFailingURLErrorKey: model.url().createNSURL().get()
    }] };
}

void RKModelLoaderUSD::load(CompletionHandler<void()>&& completionHandler)
{
    RetainPtr<NSString> attributionID;
    if (m_attributionTaskID.has_value())
        attributionID = m_attributionTaskID.value().createNSString();
    [getWKRKEntityClassSingleton() loadFromData:m_model->data()->createNSData().get() withAttributionTaskID:attributionID.get() entityMemoryLimit:(m_entityMemoryLimit ? *m_entityMemoryLimit : 0) completionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)] (WKRKEntity *entity) mutable {
        completionHandler();

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!entity) {
            protectedThis->didFail(toResourceError("Failed to load the entity"_s, protectedThis->m_model));
            return;
        }

        protectedThis->didFinish(entity);
    }).get()];
}

class RKUSDModelLoadScheduler {
public:
    static RKUSDModelLoadScheduler& singleton();
    RKUSDModelLoadScheduler() = default;

    Ref<REModelLoader> scheduleModelLoad(Model&, const std::optional<String>& attributionTaskID, std::optional<int> entityMemoryLimit, REModelLoaderClient&);

private:
    void loadNextModel();

    Deque<Ref<RKModelLoaderUSD>> m_pendingLoads;
    size_t m_inProgressLoadsCount { 0 };
};

RKUSDModelLoadScheduler& RKUSDModelLoadScheduler::singleton()
{
    static auto scheduler = NeverDestroyed<RKUSDModelLoadScheduler>();
    return scheduler;
}

Ref<REModelLoader> RKUSDModelLoadScheduler::scheduleModelLoad(Model& model, const std::optional<String>& attributionTaskID, std::optional<int> entityMemoryLimit, REModelLoaderClient& client)
{
    auto loader = RKModelLoaderUSD::create(model, attributionTaskID, entityMemoryLimit, client);

    dispatch_async(mainDispatchQueueSingleton(), [this, loader] () mutable {
        m_pendingLoads.append(loader);
        loadNextModel();
    });

    return loader;
}

void RKUSDModelLoadScheduler::loadNextModel()
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    static const size_t maxLimitOnParallelLoads = 3;
    if (m_inProgressLoadsCount >= maxLimitOnParallelLoads)
        return;

    if (m_pendingLoads.isEmpty())
        return;

    auto nextLoad = m_pendingLoads.takeFirst();
    if (nextLoad->isCanceled()) {
        loadNextModel();
        return;
    }

    m_inProgressLoadsCount++;
    nextLoad->load([this] {
        dispatch_assert_queue(mainDispatchQueueSingleton());
        ASSERT(m_inProgressLoadsCount > 0);
        m_inProgressLoadsCount--;
        loadNextModel();
    });
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelProcessModelPlayerProxy);

uint64_t ModelProcessModelPlayerProxy::gObjectCountForTesting = 0;

Ref<ModelProcessModelPlayerProxy> ModelProcessModelPlayerProxy::create(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection, const std::optional<String>& attributionTaskID, std::optional<int> debugEntityMemoryLimit)
{
    return adoptRef(*new ModelProcessModelPlayerProxy(manager, identifier, WTF::move(connection), attributionTaskID, debugEntityMemoryLimit));
}

ModelProcessModelPlayerProxy::ModelProcessModelPlayerProxy(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection, const std::optional<String>& attributionTaskID, std::optional<int> debugEntityMemoryLimit)
    : m_id(identifier)
    , m_webProcessConnection(WTF::move(connection))
    , m_manager(manager)
    , m_attributionTaskID(attributionTaskID)
    , m_debugEntityMemoryLimit(debugEntityMemoryLimit)
    , m_unloadModelTimer(RunLoop::mainSingleton(), "ModelProcessModelPlayerProxy::UnloadModelTimer"_s, this, &ModelProcessModelPlayerProxy::unloadModelTimerFired)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy initialized id=%" PRIu64, this, identifier.toUInt64());
    m_objCAdapter = adoptNS([[WKModelProcessModelPlayerProxyObjCAdapter alloc] initWithModelProcessModelPlayerProxy:*this]);
    ++gObjectCountForTesting;
}

ModelProcessModelPlayerProxy::~ModelProcessModelPlayerProxy()
{
    if (m_loader)
        m_loader->cancel();

    if (m_containerEntity.get())
        REEntityRemoveFromSceneOrParent(m_containerEntity.get());

    if (m_hostingEntity.get())
        REEntityRemoveFromSceneOrParent(m_hostingEntity.get());

    [m_stageModeInteractionDriver removeInteractionContainerFromSceneOrParent];

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy deallocated id=%" PRIu64, this, m_id.toUInt64());

    ASSERT(gObjectCountForTesting > 0);
    --gObjectCountForTesting;
}

std::optional<SharedPreferencesForWebProcess> ModelProcessModelPlayerProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr strongManager = m_manager.get())
        return strongManager->sharedPreferencesForWebProcess();

    return std::nullopt;
}

void ModelProcessModelPlayerProxy::invalidate()
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy invalidated id=%" PRIu64, this, m_id.toUInt64());
    [m_layer setPlayer:nullptr];
}

template<typename T>
ALWAYS_INLINE void ModelProcessModelPlayerProxy::send(T&& message)
{
    m_webProcessConnection->send(std::forward<T>(message), m_id);
}

// MARK: - Messages

void ModelProcessModelPlayerProxy::createLayer()
{
    dispatch_assert_queue(mainDispatchQueueSingleton());
    ASSERT(!m_layer);

    m_layer = adoptNS([[WKModelProcessModelLayer alloc] init]);
    [m_layer setName:@"WKModelProcessModelLayer"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.transform"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.collider"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.mesh"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.material"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.texture"];

    [m_layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_layer setPlayer:WeakPtr { this }];

    LayerHostingContextOptions contextOptions;
    m_layerHostingContext = LayerHostingContext::create(contextOptions);
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

void ModelProcessModelPlayerProxy::reloadModel(Ref<WebCore::Model>&& model, WebCore::LayoutSize layoutSize, std::optional<WebCore::TransformationMatrix> entityTransformToRestore, std::optional<WebCore::ModelPlayerAnimationState> animationStateToRestore)
{
    m_entityTransformToRestore = WTF::move(entityTransformToRestore);
    m_animationStateToRestore = WTF::move(animationStateToRestore);
    if (m_animationStateToRestore) {
        m_autoplay = m_animationStateToRestore->autoplay();
        m_loop = m_animationStateToRestore->loop();
        if (auto playbackRate = m_animationStateToRestore->effectivePlaybackRate())
            m_playbackRate = *playbackRate;
    }

    load(model, layoutSize);
}

void ModelProcessModelPlayerProxy::modelVisibilityDidChange(bool isVisible)
{
    m_unloadModelTimer.stop();

    m_isVisible = isVisible;

    if (m_isVisible)
        m_unloadModelTimer.stop();
    else
        m_unloadModelTimer.startOneShot(m_unloadDelayDisabledForTesting ? 0_s : unloadModelDelay);
}

void ModelProcessModelPlayerProxy::unloadModelTimerFired()
{
    if (m_isVisible)
        return;

    RefPtr strongManager = m_manager.get();
    if (!strongManager)
        return;

    if (m_loader) {
        m_loader->cancel();
        m_loader = nullptr;
    }

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::unloadModelTimerFired(): inform manager to unload model id=%" PRIu64, this, m_id.toUInt64());
    strongManager->unloadModelPlayer(m_id);
}

// MARK: - RE stuff

static inline simd_float2 makeMeterSizeFromPointSize(CGSize pointSize, CGFloat pointsPerMeter)
{
    return simd_make_float2(pointSize.width / pointsPerMeter, pointSize.height / pointsPerMeter);
}

static void computeScaledExtentsAndCenter(simd_float2 boundsOfLayerInMeters, simd_float3& boundingBoxExtents, simd_float3& boundingBoxCenter)
{
    if (fmin(boundingBoxExtents.x, boundingBoxExtents.y) - FLT_EPSILON > 0) {
        auto boundsScaleRatios = simd_make_float2(
            boundsOfLayerInMeters.x / boundingBoxExtents.x,
            boundsOfLayerInMeters.y / boundingBoxExtents.y
        );
        boundingBoxCenter = simd_reduce_min(boundsScaleRatios) * boundingBoxCenter;
        boundingBoxExtents = simd_reduce_min(boundsScaleRatios) * boundingBoxExtents;
    }
}

static RESRT computeSRT(CALayer *layer, simd_float3 originalBoundingBoxExtents, simd_float3 originalBoundingBoxCenter, float boundingRadius, bool isPortal, CGFloat pointsPerMeter, WebCore::StageModeOperation operation, simd_quatf currentModelRotation, bool useRealWorldTransform)
{
    auto boundsOfLayerInMeters = makeMeterSizeFromPointSize(layer.bounds.size, pointsPerMeter);
    simd_float3 boundingBoxExtents = originalBoundingBoxExtents;
    simd_float3 boundingBoxCenter = originalBoundingBoxCenter;
    computeScaledExtentsAndCenter(boundsOfLayerInMeters, boundingBoxExtents, boundingBoxCenter);

    RESRT srt;
    if (useRealWorldTransform) {
        srt.scale = simd_make_float3(1.0f, 1.0f, 1.0f);
        srt.rotation = currentModelRotation;
        srt.translation = simd_make_float3(0.0f, 0.0, 0.0f);
    } else if (operation == WebCore::StageModeOperation::None) {
        simd_float3 scale = simd_make_float3(boundingBoxExtents.x / originalBoundingBoxExtents.x, boundingBoxExtents.y / originalBoundingBoxExtents.y, boundingBoxExtents.z / originalBoundingBoxExtents.z);
        if (std::isnan(scale.x) || std::isnan(scale.y) || std::isnan(scale.z))
            scale = simd_make_float3(0.0f, 0.0f, 0.0f);

        srt.scale = scale;
        float minScale = simd_reduce_min(srt.scale);

        srt.scale = simd_make_float3(minScale, minScale, minScale);
        srt.rotation = currentModelRotation;

        if (isPortal)
            srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z - boundingBoxExtents.z / 2.0f);
        else
            srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z + boundingBoxExtents.z / 2.0f);
    } else {
        float boundingSphereDiameter = boundingRadius * 2.0f;
        float layerBoundingEdge = simd_reduce_min(boundsOfLayerInMeters);
        float minScale = layerBoundingEdge / boundingSphereDiameter;
        if (std::isnan(minScale))
            minScale = 0.0f;

        srt.scale = simd_make_float3(minScale, minScale, minScale);
        srt.rotation = currentModelRotation;
        boundingBoxCenter = srt.scale * originalBoundingBoxCenter;

        if (isPortal)
            srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z - boundingSphereDiameter * minScale / 2.0f);
        else
            srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z + boundingSphereDiameter * minScale / 2.0f);
    }

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

static RESRT modelStandardizedTransformSRT(RESRT originalSRT)
{
    constexpr float defaultScaleFactor = 0.36f;

    originalSRT.scale *= defaultScaleFactor;
    originalSRT.translation *= defaultScaleFactor;

    return originalSRT;
}

static RESRT modelLocalizedTransformSRT(RESRT originalSRT)
{
    constexpr float defaultScaleFactor = 0.36f;

    originalSRT.scale /= defaultScaleFactor;
    originalSRT.translation /= defaultScaleFactor;

    return originalSRT;
}

void ModelProcessModelPlayerProxy::computeTransform(bool setDefaultRotation)
{
    if (!m_modelRKEntity || !m_layer)
        return;

    // FIXME: Use the value of the 'object-fit' property here to compute an appropriate SRT.
    float boundingRadius = [m_modelRKEntity boundingRadius];
    simd_quatf currentModelRotation = setDefaultRotation ? simd_quaternion(0, simd_make_float3(1, 0, 0)) : m_transformSRT.rotation;
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    RESRT newSRT = computeSRT(m_layer.get(), m_originalBoundingBoxExtents, m_originalBoundingBoxCenter, boundingRadius, m_hasPortal, effectivePointsPerMeter(m_layer.get()), m_stageModeOperation, currentModelRotation, m_immersivePresentation);
#else
    RESRT newSRT = computeSRT(m_layer.get(), m_originalBoundingBoxExtents, m_originalBoundingBoxCenter, boundingRadius, m_hasPortal, effectivePointsPerMeter(m_layer.get()), m_stageModeOperation, currentModelRotation, false);
#endif
    m_transformSRT = newSRT;

    notifyModelPlayerOfEntityTransformChange();
}

void ModelProcessModelPlayerProxy::notifyModelPlayerOfEntityTransformChange()
{
    RESRT newSRT = modelStandardizedTransformSRT(m_transformSRT);
    simd_float4x4 matrix = RESRTMatrix(newSRT);
    WebCore::TransformationMatrix transform = WebCore::TransformationMatrix(matrix);
    send(Messages::ModelProcessModelPlayer::DidUpdateEntityTransform(transform));
}

void ModelProcessModelPlayerProxy::updateTransform()
{
    if (!m_modelRKEntity || !m_layer)
        return;

    [m_modelRKEntity setTransform:WKEntityTransform({ m_transformSRT.scale, m_transformSRT.rotation, m_transformSRT.translation })];
}

void ModelProcessModelPlayerProxy::updateTransformAfterLayout()
{
    if (m_transformNeedsUpdateAfterNextLayout) {
        updateForCurrentStageMode();
        m_transformNeedsUpdateAfterNextLayout = false;
        return;
    }

    updateTransform();
}

void ModelProcessModelPlayerProxy::updateOpacity()
{
    if (!m_modelRKEntity || !m_layer)
        return;

    [m_modelRKEntity setOpacity:[m_layer opacity]];
}

void ModelProcessModelPlayerProxy::startAnimating()
{
    if (!m_modelRKEntity || !m_layer)
        return;

    [m_modelRKEntity setUpAnimationWithAutoPlay:m_autoplay];
    [m_modelRKEntity setLoop:m_loop];
    [m_modelRKEntity setPlaybackRate:m_playbackRate];
}

void ModelProcessModelPlayerProxy::animationPlaybackStateDidUpdate()
{
    bool isPaused = paused();
    float playbackRate = [m_modelRKEntity playbackRate];
    NSTimeInterval duration = this->duration();
    NSTimeInterval currentTime = this->currentTime().seconds();
    RELEASE_LOG_DEBUG(ModelElement, "%p - ModelProcessModelPlayerProxy: did update animation playback state: paused: %d, playbackRate: %f, duration: %f, currentTime: %f", this, isPaused, playbackRate, duration, currentTime);
    send(Messages::ModelProcessModelPlayer::DidUpdateAnimationPlaybackState(isPaused, playbackRate, Seconds(duration), Seconds(currentTime), MonotonicTime::now()));
}
// MARK: - WebCore::RELoaderClient

static RECALayerService *webDefaultLayerService(void)
{
    return REServiceLocatorGetCALayerService(REEngineGetServiceLocator(REEngineGetShared()));
}

void ModelProcessModelPlayerProxy::didFinishLoading(WebCore::REModelLoader& loader, Ref<WebCore::REModel> model)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());
    ASSERT(&loader == m_loader.get());

    bool canLoadWithRealityKit = [getWKRKEntityClassSingleton() isLoadFromDataAvailable];

    m_loader = nullptr;
    if (canLoadWithRealityKit)
        m_modelRKEntity = model->rootRKEntity();
    else if (model->rootEntity())
        m_modelRKEntity = adoptNS([allocWKRKEntityInstance() initWithCoreEntity:model->rootEntity()]);
    [m_modelRKEntity setDelegate:m_objCAdapter.get()];

    m_originalBoundingBoxExtents = [m_modelRKEntity boundingBoxExtents];
    m_originalBoundingBoxCenter = [m_modelRKEntity boundingBoxCenter];

    m_hostingEntity = adoptRE(REEntityCreate());
    REEntitySetName(m_hostingEntity.get(), "WebKit:EntityWithRootComponent");

    REPtr<REComponentRef> layerComponent = adoptRE(RECALayerServiceCreateRootComponent(webDefaultLayerService(), CALayerGetContext(m_layer.get()), m_hostingEntity.get(), nil));
    RESceneAddEntity(m_scene.get(), m_hostingEntity.get());

    CALayer *contextEntityLayer = RECALayerClientComponentGetCALayer(layerComponent.get());
    [contextEntityLayer setSeparatedState:kCALayerSeparatedStateTracked];

    RECALayerClientComponentSetShouldSyncToRemotes(layerComponent.get(), true);

    auto clientComponent = RECALayerGetCALayerClientComponent(m_layer.get());
    auto clientComponentEntity = REComponentGetEntity(clientComponent);
    REEntitySetName(clientComponentEntity, "WebKit:ClientComponentEntity");
    if (canLoadWithRealityKit)
        [model->rootRKEntity() setName:@"WebKit:ModelRootEntity"];
    else
        REEntitySetName(model->rootEntity(), "WebKit:ModelRootEntity");

    if (canLoadWithRealityKit)
        [model->rootRKEntity() setParentCoreEntity:clientComponentEntity preservingWorldTransform:NO];
    else {
        REEntitySetParent(model->rootEntity(), clientComponentEntity);
    }

    m_stageModeInteractionDriver = adoptNS([allocWKStageModeInteractionDriverInstance() initWithModel:m_modelRKEntity.get() container:clientComponentEntity delegate:m_objCAdapter.get()]);

    REEntitySubtreeAddNetworkComponentRecursive([m_stageModeInteractionDriver interactionContainerRef]);
    RENetworkMarkEntityMetadataDirty([m_stageModeInteractionDriver interactionContainerRef]);

    applyStageModeOperationToDriver();

    if (!canLoadWithRealityKit)
        RENetworkMarkEntityMetadataDirty(model->rootEntity());

    if (m_entityTransformToRestore) {
        setEntityTransform(*m_entityTransformToRestore);
        notifyModelPlayerOfEntityTransformChange();
        m_entityTransformToRestore = std::nullopt;
    } else {
        computeTransform(true);
        updateTransform();
    }
    [m_stageModeInteractionDriver setContainerTransformInPortal];

    updateOpacity();
    startAnimating();
    if (m_animationStateToRestore) {
        [m_modelRKEntity setPaused:m_animationStateToRestore->paused()];
        [m_modelRKEntity setCurrentTime:m_animationStateToRestore->currentTime().seconds()];
        m_animationStateToRestore = std::nullopt;
    }

    applyEnvironmentMapDataAndRelease([weakThis = WeakPtr { *this }] () mutable {
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (RefPtr protectedThis = weakThis.get())
        protectedThis->triggerModelLoadedCallbacks(true);
#endif
    });

    send(Messages::ModelProcessModelPlayer::DidFinishLoading(WebCore::FloatPoint3D(m_originalBoundingBoxCenter.x, m_originalBoundingBoxCenter.y, m_originalBoundingBoxCenter.z), WebCore::FloatPoint3D(m_originalBoundingBoxExtents.x, m_originalBoundingBoxExtents.y, m_originalBoundingBoxExtents.z)));
}

void ModelProcessModelPlayerProxy::didFailLoading(WebCore::REModelLoader& loader, const WebCore::ResourceError& error)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());
    ASSERT(&loader == m_loader.get());

    m_loader = nullptr;

    RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy failed to load model id=%" PRIu64 " error=\"%@\"", this, m_id.toUInt64(), error.nsError().localizedDescription);

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    triggerModelLoadedCallbacks(false);
#endif

    send(Messages::ModelProcessModelPlayer::DidFailLoading());
}

// MARK: - WebCore::ModelPlayer

static int defaultEntityMemoryLimit = 100; // MB

void ModelProcessModelPlayerProxy::load(WebCore::Model& model, WebCore::LayoutSize layoutSize)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::load size=%zu id=%" PRIu64, this, model.data()->size(), m_id.toUInt64());
    sizeDidChange(layoutSize);

    WKREEngine::singleton().runWithSharedScene([this, protectedThis = Ref { *this }, model = Ref { model }] (RESceneRef scene) {
        m_scene = scene;
        if ([getWKRKEntityClassSingleton() isLoadFromDataAvailable])
            m_loader = RKUSDModelLoadScheduler::singleton().scheduleModelLoad(model.get(), m_attributionTaskID, m_debugEntityMemoryLimit ? *m_debugEntityMemoryLimit : defaultEntityMemoryLimit, *this);
        else
            m_loader = WebCore::loadREModel(model.get(), *this);
    });
}

void ModelProcessModelPlayerProxy::sizeDidChange(WebCore::LayoutSize layoutSize)
{
    RELEASE_LOG_INFO(ModelElement, "%p - ModelProcessModelPlayerProxy::sizeDidChange w=%lf h=%lf id=%" PRIu64, this, layoutSize.width().toDouble(), layoutSize.height().toDouble(), m_id.toUInt64());

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    m_layoutSize = layoutSize;

    if (m_immersivePresentation)
        return;
#endif

    auto width = layoutSize.width().toDouble();
    auto height = layoutSize.height().toDouble();
    if (!m_transformNeedsUpdateAfterNextLayout && m_stageModeOperation != WebCore::StageModeOperation::None && m_modelRKEntity && m_layer)
        m_transformNeedsUpdateAfterNextLayout = width != CGRectGetWidth([m_layer frame]) || height != CGRectGetHeight([m_layer frame]);
    [m_layer setFrame:CGRectMake(0, 0, width, height)];
}

void ModelProcessModelPlayerProxy::configureGraphicsLayer(WebCore::GraphicsLayer&, WebCore::ModelPlayerGraphicsLayerConfiguration&&)
{
}

std::optional<WebCore::LayerHostingContextIdentifier> ModelProcessModelPlayerProxy::layerHostingContextIdentifier()
{
    return WebCore::LayerHostingContextIdentifier(m_layerHostingContext->contextID());
}

void ModelProcessModelPlayerProxy::setEntityTransform(WebCore::TransformationMatrix transform)
{
    RESRT newSRT = REMakeSRTFromMatrix(transform);
    m_transformSRT = modelLocalizedTransformSRT(newSRT);
    updateTransform();
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

WebCore::ModelPlayerAccessibilityChildren ModelProcessModelPlayerProxy::accessibilityChildren()
{
    return { };
}

void ModelProcessModelPlayerProxy::setAutoplay(bool autoplay)
{
    m_autoplay = autoplay;
}

void ModelProcessModelPlayerProxy::setLoop(bool loop)
{
    m_loop = loop;
    [m_modelRKEntity setLoop:m_loop];
}

void ModelProcessModelPlayerProxy::setPlaybackRate(double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    m_playbackRate = playbackRate;
    [m_modelRKEntity setPlaybackRate:m_playbackRate];
    completionHandler(m_modelRKEntity ? [m_modelRKEntity playbackRate] : 1.0);
}

double ModelProcessModelPlayerProxy::duration() const
{
    return [m_modelRKEntity duration];
}

bool ModelProcessModelPlayerProxy::paused() const
{
    return [m_modelRKEntity paused];
}

void ModelProcessModelPlayerProxy::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    [m_modelRKEntity setPaused:paused];
    completionHandler(paused == [m_modelRKEntity paused]);
}

Seconds ModelProcessModelPlayerProxy::currentTime() const
{
    return Seconds([m_modelRKEntity currentTime]);
}

void ModelProcessModelPlayerProxy::setCurrentTime(Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    [m_modelRKEntity setCurrentTime:currentTime.seconds()];
    completionHandler();
}

void ModelProcessModelPlayerProxy::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
    m_transientEnvironmentMapData = WTF::move(data);
    if (m_modelRKEntity)
        applyEnvironmentMapDataAndRelease([] { });
}

void ModelProcessModelPlayerProxy::beginStageModeTransform(const WebCore::TransformationMatrix& transform)
{
    simd_float4x4 transformMatrix = simd_float4x4(transform);
    [m_stageModeInteractionDriver interactionDidBegin:transformMatrix];
}

void ModelProcessModelPlayerProxy::updateStageModeTransform(const WebCore::TransformationMatrix& transform)
{
    simd_float4x4 transformMatrix = simd_float4x4(transform);
    [m_stageModeInteractionDriver interactionDidUpdate:transformMatrix];
}

void ModelProcessModelPlayerProxy::endStageModeInteraction()
{
    [m_stageModeInteractionDriver interactionDidEnd];
}

void ModelProcessModelPlayerProxy::resetModelTransformAfterDrag()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=291289
}

void ModelProcessModelPlayerProxy::stageModeInteractionDidUpdateModel()
{
    if (stageModeInteractionInProgress() && m_modelRKEntity)
        updateTransformSRT();
}

bool ModelProcessModelPlayerProxy::stageModeInteractionInProgress() const
{
    return [m_stageModeInteractionDriver stageModeInteractionInProgress];
}

void ModelProcessModelPlayerProxy::animateModelToFitPortal(CompletionHandler<void(bool)>&& completionHandler)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=291289
    completionHandler(true);
}

#if HAVE(MODEL_MEMORY_ATTRIBUTION)
static void setIBLAssetOwnership(const String& attributionTaskID, REAssetRef iblAsset)
{
    auto attributionIDString = attributionTaskID.utf8();

    if (REPtr<REAssetRef> skyboxTexture = REIBLAssetGetSkyboxTexture(iblAsset)) {
        RELEASE_LOG_DEBUG(ModelElement, "Attributing skyboxTexture to task ID: %s", attributionIDString.data());
        REAssetSetMemoryAttributionTarget(skyboxTexture.get(), attributionIDString.data());
    }
    if (REPtr<REAssetRef> diffuseTexture = REIBLAssetGetDiffuseTexture(iblAsset)) {
        RELEASE_LOG_DEBUG(ModelElement, "Attributing diffuseTexture to task ID: %s", attributionIDString.data());
        REAssetSetMemoryAttributionTarget(diffuseTexture.get(), attributionIDString.data());
    }
    if (REPtr<REAssetRef> specularTexture = REIBLAssetGetSpecularTexture(iblAsset)) {
        RELEASE_LOG_DEBUG(ModelElement, "Attributing specularTexture to task ID: %s", attributionIDString.data());
        REAssetSetMemoryAttributionTarget(specularTexture.get(), attributionIDString.data());
    }
}
#endif

void ModelProcessModelPlayerProxy::applyEnvironmentMapDataAndRelease(CompletionHandler<void()>&& completion)
{
    if (m_transientEnvironmentMapData) {
        if (m_transientEnvironmentMapData->size() > 0) {
            [m_modelRKEntity applyIBLData:m_transientEnvironmentMapData->createNSData().get() attributionHandler:makeBlockPtr([weakThis = WeakPtr { *this }] (REAssetRef coreEnvironmentResourceAsset) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || !protectedThis->m_attributionTaskID || !coreEnvironmentResourceAsset)
                    return;

#if HAVE(MODEL_MEMORY_ATTRIBUTION)
                setIBLAssetOwnership(*(protectedThis->m_attributionTaskID), coreEnvironmentResourceAsset);
#endif
            }).get() withCompletion:makeBlockPtr([weakThis = WeakPtr { *this }, completion = WTF::move(completion)] (BOOL succeeded) mutable {
                completion();
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (!succeeded)
                    protectedThis->applyDefaultIBL();

                protectedThis->send(Messages::ModelProcessModelPlayer::DidFinishEnvironmentMapLoading(succeeded));
            }).get()];
        } else {
            applyDefaultIBL();
            completion();
            send(Messages::ModelProcessModelPlayer::DidFinishEnvironmentMapLoading(true));
        }
        m_transientEnvironmentMapData = nullptr;
    } else {
        applyDefaultIBL();
        completion();
    }
}

void ModelProcessModelPlayerProxy::setHasPortal(bool hasPortal)
{
    if (m_hasPortal == hasPortal)
        return;

    m_hasPortal = hasPortal;

    computeTransform(true);
    updateTransform();
}

void ModelProcessModelPlayerProxy::updateForCurrentStageMode()
{
    if (m_stageModeOperation != WebCore::StageModeOperation::None) {
        computeTransform(false);
        [m_modelRKEntity recenterEntityAtTransform:WKEntityTransform({ m_transformSRT.scale, m_transformSRT.rotation, m_transformSRT.translation })];
        updateTransformSRT();
    }

    applyStageModeOperationToDriver();
}

void ModelProcessModelPlayerProxy::setStageMode(WebCore::StageModeOperation stagemodeOp)
{
    if (m_stageModeOperation == stagemodeOp)
        return;

    m_stageModeOperation = stagemodeOp;

    updateForCurrentStageMode();
}

void ModelProcessModelPlayerProxy::updateTransformSRT()
{
    WKEntityTransform entityTransform = [m_modelRKEntity transform];
    m_transformSRT = RESRT {
        .scale = entityTransform.scale,
        .rotation = entityTransform.rotation,
        .translation = entityTransform.translation
    };

    notifyModelPlayerOfEntityTransformChange();
}

void ModelProcessModelPlayerProxy::applyStageModeOperationToDriver()
{
    switch (m_stageModeOperation) {
    case WebCore::StageModeOperation::Orbit: {
        [m_stageModeInteractionDriver operationDidUpdate:WKStageModeOperationOrbit];
        break;
    }

    case WebCore::StageModeOperation::None: {
        [m_stageModeInteractionDriver operationDidUpdate:WKStageModeOperationNone];
        break;
    }
    }
}

void ModelProcessModelPlayerProxy::applyDefaultIBL()
{
    [m_modelRKEntity applyDefaultIBL];
}

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

void ModelProcessModelPlayerProxy::ensureImmersivePresentation(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&& completion)
{
    setImmersivePresentation(true);
    ensureModelLoaded([weakThis = WeakPtr { *this }, completion = WTF::move(completion)] (bool loaded) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completion(std::nullopt);

        RetainPtr entity = protectedThis->m_modelRKEntity;
        if (loaded && entity) {
            [entity ensureSceneUnderstanding];
            completion(protectedThis->layerHostingContextIdentifier().value());
        } else {
            protectedThis->setImmersivePresentation(false);
            completion(std::nullopt);
        }
    });
}

void ModelProcessModelPlayerProxy::exitImmersivePresentation(CompletionHandler<void()>&& completion)
{
    setImmersivePresentation(false);
    completion();
}

void ModelProcessModelPlayerProxy::setImmersivePresentation(bool immersivePresentation)
{
    if (immersivePresentation)
        [m_layer setFrame:CGRectMake(0, 0, 0, 0)];
    else {
        auto width = m_layoutSize.width().toDouble();
        auto height = m_layoutSize.height().toDouble();
        [m_layer setFrame:CGRectMake(0, 0, width, height)];
    }

    m_immersivePresentation = immersivePresentation;
    computeTransform(false);
    updateTransform();
}

void ModelProcessModelPlayerProxy::ensureModelLoaded(CompletionHandler<void(bool)>&& completion)
{
    if (m_modelRKEntity) {
        completion(true);
        return;
    }

    m_modelLoadedCallbacks.append(WTF::move(completion));
}

void ModelProcessModelPlayerProxy::triggerModelLoadedCallbacks(bool result)
{
    for (auto& callback : std::exchange(m_modelLoadedCallbacks, { }))
        callback(result);
}

#endif

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
