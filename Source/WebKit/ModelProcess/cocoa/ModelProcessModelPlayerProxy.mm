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
#if HAVE(CORE_RE)
#import "WKStageMode.h"
#import "WKUSDStageConverter.h"
#import <RealitySystemSupport/RealitySystemSupport.h>
#import <SurfBoardServices/SurfBoardServices.h>
#endif
#import <WebCore/Color.h>
#import <WebCore/LayerHostingContextIdentifier.h>
#import <WebCore/Model.h>
#import <WebCore/ModelPlayerGraphicsLayerConfiguration.h>
#import <WebCore/ResourceError.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#if HAVE(CORE_RE)
#import <WebKitAdditions/REModel.h>
#import <WebKitAdditions/REModelLoader.h>
#import <WebKitAdditions/REPtr.h>
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#import <WebKitAdditions/WKREEngine.h>
#endif
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/Deque.h>
#import <wtf/MathExtras.h>
#import <wtf/NakedPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/ThreadSafeWeakPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/TextStream.h>

#import "WebKitSwiftSoftLink.h"

#if HAVE(CORE_RE)
@interface WKModelProcessModelPlayerProxyObjCAdapter : NSObject<WKRKEntityDelegate, WKStageModeInteractionAware>
#else
@interface WKModelProcessModelPlayerProxyObjCAdapter : NSObject<WKRKEntityDelegate>
#endif
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

- (void)entityAnimationPlaybackStateDidUpdate:(WKRKEntity *)entity
{
    _modelProcessModelPlayerProxy->animationPlaybackStateDidUpdate(entity);
}

#if HAVE(CORE_RE)
- (void)stageModeInteractionDidUpdateModel
{
    _modelProcessModelPlayerProxy->stageModeInteractionDidUpdateModel();
}
#endif

@end

namespace WebKit {

static const Seconds unloadModelDelay { 4_s };
static constexpr auto usdzMIMEType = "model/vnd.usdz+zip"_s;

#if HAVE(CORE_RE)
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
    void scheduleModelConversion(Ref<WebCore::SharedBuffer>&&, ThreadSafeWeakPtr<ModelProcessModelPlayerProxy>&&, Function<void(ModelProcessModelPlayerProxy&, RetainPtr<NSData>&&)>&&);

private:
    void loadNextModel();

    static constexpr size_t maxLimitOnParallelLoads = 3;

    Deque<Ref<RKModelLoaderUSD>> m_pendingLoads;
    size_t m_inProgressLoadsCount { 0 };

    const Ref<WorkQueue> m_conversionQueue { WorkQueue::create("com.apple.WebKit.ModelProcess.USDConversion"_s, WorkQueue::QOS::UserInitiated) };
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

void RKUSDModelLoadScheduler::scheduleModelConversion(Ref<WebCore::SharedBuffer>&& modelData, ThreadSafeWeakPtr<ModelProcessModelPlayerProxy>&& player, Function<void(ModelProcessModelPlayerProxy&, RetainPtr<NSData>&&)>&& completion)
{
    m_conversionQueue->dispatch([modelData = WTF::move(modelData), player = WTF::move(player), completion = WTF::move(completion)] () mutable {
        // Skip the copy + conversion if the player is already gone (e.g. its <model> element was unloaded).
        if (!player.get())
            return;

        RetainPtr usdzData = [WKUSDStageConverter convert:modelData->createNSData()];

        dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([player = WTF::move(player), completion = WTF::move(completion), usdzData = WTF::move(usdzData)] () mutable {
            if (RefPtr protectedPlayer = player.get())
                completion(*protectedPlayer, WTF::move(usdzData));
        }).get());
    });
}

void RKUSDModelLoadScheduler::loadNextModel()
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

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
#endif // HAVE(CORE_RE)

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelProcessModelPlayerProxy);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(ModelProcessModelPlayerProxy::TrackedModel);

uint64_t ModelProcessModelPlayerProxy::gObjectCountForTesting = 0;

Ref<ModelProcessModelPlayerProxy> ModelProcessModelPlayerProxy::create(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection, const std::optional<String>& attributionTaskID, std::optional<int> debugEntityMemoryLimit, std::optional<int> debugImmersiveEntityMemoryLimit)
{
    return adoptRef(*new ModelProcessModelPlayerProxy(manager, identifier, WTF::move(connection), attributionTaskID, debugEntityMemoryLimit, debugImmersiveEntityMemoryLimit));
}

ModelProcessModelPlayerProxy::ModelProcessModelPlayerProxy(ModelProcessModelPlayerManagerProxy& manager, WebCore::ModelPlayerIdentifier identifier, Ref<IPC::Connection>&& connection, const std::optional<String>& attributionTaskID, std::optional<int> debugEntityMemoryLimit, std::optional<int> debugImmersiveEntityMemoryLimit)
    : m_id(identifier)
    , m_webProcessConnection(WTF::move(connection))
    , m_manager(manager)
    , m_attributionTaskID(attributionTaskID)
    , m_debugEntityMemoryLimit(debugEntityMemoryLimit)
    , m_debugImmersiveEntityMemoryLimit(debugImmersiveEntityMemoryLimit)
    , m_unloadModelTimer(RunLoop::mainSingleton(), "ModelProcessModelPlayerProxy::UnloadModelTimer"_s, this, &ModelProcessModelPlayerProxy::unloadModelTimerFired)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy initialized id=%" PRIu64, this, identifier.toUInt64());
    m_objCAdapter = adoptNS([[WKModelProcessModelPlayerProxyObjCAdapter alloc] initWithModelProcessModelPlayerProxy:*this]);
    ++gObjectCountForTesting;
}

ModelProcessModelPlayerProxy::~ModelProcessModelPlayerProxy()
{
    cancelAllLoaders();

#if HAVE(CORE_RE)
    if (m_containerEntity.get())
        REEntityRemoveFromSceneOrParent(m_containerEntity.get());

    if (m_hostingEntity.get())
        REEntityRemoveFromSceneOrParent(m_hostingEntity.get());

    [m_stageModeInteractionDriver removeInteractionContainerFromSceneOrParent];

#if HAVE(RESYNC_MEMORY_PRESSURE)
    if (auto* syncManager = REServiceLocatorGetNetworkSyncManager(REEngineGetServiceLocator(REEngineGetShared())))
        RENetworkSyncManagerRelieveMemoryPressure(syncManager, 0);
#endif
#endif // HAVE(CORE_RE)

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

void ModelProcessModelPlayerProxy::loadModel(WebCore::NodeIdentifier nodeID, Ref<WebCore::Model>&& model, WebCore::LayoutSize layoutSize, bool isForImmersive)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    if (auto* tracked = trackedModel(nodeID))
        tracked->animationStateToRestore = std::nullopt;

#if HAVE(CORE_RE)
    if (model->mimeType() != usdzMIMEType) {
        Ref<WebCore::SharedBuffer> modelData = model->data();
        RKUSDModelLoadScheduler::singleton().scheduleModelConversion(WTF::move(modelData), ThreadSafeWeakPtr { *this }, [model = WTF::move(model), layoutSize, isForImmersive, nodeID](ModelProcessModelPlayerProxy& proxy, RetainPtr<NSData>&& usdzData) mutable {
            dispatch_assert_queue(mainDispatchQueueSingleton());

            if (usdzData) {
                auto convertedBuffer = WebCore::SharedBuffer::create(usdzData.get());

                // Send converted data back to WebContent for drag-and-drop
                proxy.send(Messages::ModelProcessModelPlayer::DidConvertModelData(convertedBuffer.copyRef(), usdzMIMEType));

                auto convertedModel = WebCore::Model::create(WTF::move(convertedBuffer), usdzMIMEType, model->url());
                proxy.load(nodeID, convertedModel, layoutSize, isForImmersive);
                return;
            }

            RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy::loadModel(): Model conversion failed, continuing with original data", &proxy);
            proxy.load(nodeID, model, layoutSize, isForImmersive);
        });
        return;
    }
#endif

    // FIXME: Change the IPC message to land on load() directly
    load(nodeID, model, layoutSize, isForImmersive);
}

void ModelProcessModelPlayerProxy::reloadModel(WebCore::NodeIdentifier nodeID, Ref<WebCore::Model>&& model, WebCore::LayoutSize layoutSize, std::optional<WebCore::TransformationMatrix> entityTransformToRestore, std::optional<WebCore::ModelPlayerAnimationState> animationStateToRestore)
{
    m_entityTransformToRestore = WTF::move(entityTransformToRestore);

    auto& tracked = ensureTrackedModel(nodeID);
    if (animationStateToRestore) {
        tracked.autoplay = animationStateToRestore->autoplay();
        tracked.loop = animationStateToRestore->loop();
        if (auto playbackRate = animationStateToRestore->effectivePlaybackRate())
            tracked.playbackRate = *playbackRate;
    }
    tracked.animationStateToRestore = WTF::move(animationStateToRestore);

    bool isForImmersive = false;
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    isForImmersive = m_immersivePresentation;
#endif
    load(nodeID, model, layoutSize, isForImmersive);
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

    cancelAllLoaders();

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::unloadModelTimerFired(): inform manager to unload model id=%" PRIu64, this, m_id.toUInt64());
    strongManager->unloadModelPlayer(m_id);
}

// MARK: - RE stuff

constexpr float defaultScaleFactor = 0.36f;

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

#if ENABLE(SPATIAL_PORTAL)
static RESRT computeUnfittedSRT(simd_float3 originalBoundingBoxExtents, simd_float3 originalBoundingBoxCenter, simd_quatf currentModelRotation)
{
    constexpr float cssUnitScale = 1.0f / defaultScaleFactor;

    RESRT srt;
    srt.scale = simd_make_float3(cssUnitScale, cssUnitScale, cssUnitScale);
    srt.rotation = currentModelRotation;

    simd_float3 boundingBoxCenter = cssUnitScale * originalBoundingBoxCenter;
    float boundingBoxDepth = cssUnitScale * originalBoundingBoxExtents.z;

    srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z - boundingBoxDepth / 2.0f);

    return srt;
}
#endif

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

RESRT ModelProcessModelPlayerProxy::modelStandardizedTransformSRT(RESRT originalSRT)
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (m_immersivePresentation)
        return originalSRT;
#endif

    originalSRT.scale *= defaultScaleFactor;
    originalSRT.translation *= defaultScaleFactor;

    return originalSRT;
}

RESRT ModelProcessModelPlayerProxy::modelLocalizedTransformSRT(RESRT originalSRT)
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (m_immersivePresentation)
        return originalSRT;
#endif

    originalSRT.scale /= defaultScaleFactor;
    originalSRT.translation /= defaultScaleFactor;

    return originalSRT;
}

void ModelProcessModelPlayerProxy::computeTransform(bool setDefaultRotation)
{
    if (m_trackedModels.isEmpty() || !m_layer)
        return;

    // TODO: Once we have spatial positioninig the union won't be centered on the origin anymore.
    simd_float3 minBound = simd_make_float3(0, 0, 0);
    simd_float3 maxBound = simd_make_float3(0, 0, 0);
    bool hasBounds = false;

    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        if (!tracked->entity)
            continue;

        simd_float3 halfExtents = tracked->originalBoundingBoxExtents / 2;
        simd_float3 entityMin = tracked->originalBoundingBoxCenter - halfExtents;
        simd_float3 entityMax = tracked->originalBoundingBoxCenter + halfExtents;

        minBound = hasBounds ? simd_min(minBound, entityMin) : entityMin;
        maxBound = hasBounds ? simd_max(maxBound, entityMax) : entityMax;
        hasBounds = true;
    }

    if (!hasBounds)
        return;

    simd_float3 boundingBoxExtents = maxBound - minBound;
    simd_float3 boundingBoxCenter = (maxBound + minBound) / 2;

    simd_quatf currentModelRotation = setDefaultRotation ? simd_quaternion(0, simd_make_float3(1, 0, 0)) : m_transformSRT.rotation;

#if ENABLE(SPATIAL_PORTAL)
    bool skipAutoFit = m_portalTransform == WebCore::PortalTransformKind::None;
    if (skipAutoFit) {
        m_transformSRT = computeUnfittedSRT(boundingBoxExtents, boundingBoxCenter, currentModelRotation);
        notifyModelPlayerOfTransformChange();
        return;
    }
#endif

    // Each model's bounding sphere has to be reached from the merged centre, not from its own, so
    // an off-centre sibling widens the radius by its distance rather than being swallowed by it.
    float boundingRadius = 0;
    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        if (!tracked->entity)
            continue;

        float entityRadius = [tracked->entity boundingRadius] * tracked->originalEntityScale.x;
        boundingRadius = std::max(boundingRadius, simd_length(tracked->originalBoundingBoxCenter - boundingBoxCenter) + entityRadius);
    }

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    RESRT newSRT = computeSRT(m_layer.get(), boundingBoxExtents, boundingBoxCenter, boundingRadius, m_hasPortal, effectivePointsPerMeter(m_layer.get()), effectiveStageModeOperation(), currentModelRotation, m_immersivePresentation);
#else
    RESRT newSRT = computeSRT(m_layer.get(), boundingBoxExtents, boundingBoxCenter, boundingRadius, m_hasPortal, effectivePointsPerMeter(m_layer.get()), effectiveStageModeOperation(), currentModelRotation, false);
#endif
    m_transformSRT = newSRT;

    notifyModelPlayerOfTransformChange();
}

void ModelProcessModelPlayerProxy::notifyModelPlayerOfTransformChange()
{
    RESRT newSRT = modelStandardizedTransformSRT(m_transformSRT);
    simd_float4x4 matrix = RESRTMatrix(newSRT);
    WebCore::TransformationMatrix transform = WebCore::TransformationMatrix(matrix);

#if ENABLE(SPATIAL_PORTAL)
    send(Messages::ModelProcessModelPlayer::DidUpdatePortalTransform(transform));
#endif

    if (!m_nodeID)
        return;

    send(Messages::ModelProcessModelPlayer::DidUpdateEntityTransform(*m_nodeID, transform));
}

void ModelProcessModelPlayerProxy::updateTransform()
{
    if (!m_layer)
        return;

#if HAVE(CORE_RE)
    if (!m_containerEntityWrapper)
        return;

    // The container owns the global transforms (stagemode, portal-transform) and each model sits beneath with only its original scale applied.
    [m_containerEntityWrapper setTransform:WKEntityTransform({ m_transformSRT.scale, m_transformSRT.rotation, m_transformSRT.translation })];
    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        if (tracked->entity)
            [tracked->entity setTransform:WKEntityTransform({ tracked->originalEntityScale, simd_quaternion(0, simd_make_float3(1, 0, 0)), simd_make_float3(0, 0, 0) })];
    }
#else
    RetainPtr reportingEntity = m_modelRKEntity;
    if (!reportingEntity)
        return;

    [reportingEntity setTransform:WKEntityTransform({ m_transformSRT.scale * reportingModelScale(), m_transformSRT.rotation, m_transformSRT.translation })];
#endif
}

void ModelProcessModelPlayerProxy::updateTransformAfterLayout()
{
    if (m_transformNeedsUpdateAfterNextLayout) {
        m_transformNeedsUpdateAfterNextLayout = false;
        if (effectiveStageModeOperation() == WebCore::StageModeOperation::None) {
            if (!m_entityTransformSetByScript)
                computeTransform(false);
            updateTransform();
        } else
            updateForCurrentStageMode();
        return;
    }

    updateTransform();
}

void ModelProcessModelPlayerProxy::updateOpacity()
{
    if (!m_layer)
        return;

    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values())
        [tracked->entity setOpacity:[m_layer opacity]];
}

void ModelProcessModelPlayerProxy::animationPlaybackStateDidUpdate(WKRKEntity *entity)
{
    auto it = findTrackedModelForEntity(entity);
    if (it == m_trackedModels.end())
        return;

    auto nodeID = it->key;
    bool isPaused = [entity paused];
    float playbackRate = [entity playbackRate];
    NSTimeInterval duration = [entity duration];
    NSTimeInterval currentTime = [entity currentTime];
    RELEASE_LOG_DEBUG(ModelElement, "%p - ModelProcessModelPlayerProxy: did update animation playback state for nodeID=%" PRIu64 ": paused: %d, playbackRate: %f, duration: %f, currentTime: %f", this, nodeID.toUInt64(), isPaused, playbackRate, duration, currentTime);
    send(Messages::ModelProcessModelPlayer::DidUpdateAnimationPlaybackState(nodeID, isPaused, playbackRate, Seconds(duration), Seconds(currentTime), MonotonicTime::now()));
}

#if HAVE(CORE_RE)
// MARK: - WebCore::RELoaderClient

static RECALayerService *webDefaultLayerService(void)
{
    return REServiceLocatorGetCALayerService(REEngineGetServiceLocator(REEngineGetShared()));
}
#endif

void ModelProcessModelPlayerProxy::didFinishLoading(WebCore::REModelLoader& loader, Ref<WebCore::REModel> model)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    auto it = findTrackedModelForLoader(loader);
    if (it == m_trackedModels.end())
        return;

    auto nodeID = it->key;
    it->value->loader = nullptr;

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy finished loading model nodeID=%" PRIu64 " id=%" PRIu64, this, nodeID.toUInt64(), m_id.toUInt64());

    RetainPtr<WKRKEntity> loadedEntity;
#if HAVE(CORE_RE)
    bool canLoadWithRealityKit = [getWKRKEntityClassSingleton() isLoadFromDataAvailable];
    if (canLoadWithRealityKit)
        loadedEntity = model->rootRKEntity();
    else if (model->rootEntity())
        loadedEntity = adoptNS([allocWKRKEntityInstance() initWithCoreEntity:model->rootEntity()]);
#else
    loadedEntity = model->rootRKEntity();
#endif

    it->value->entity = loadedEntity;
    // Every entity gets a delegate, since playback is reported per node.
    [loadedEntity setDelegate:m_objCAdapter.get()];

    // The entity's bounding box is relative to its own coordinate space, so the original scale
    // still has to be applied.
    WKEntityTransform originalTransform = [loadedEntity transform];
    it->value->originalEntityScale = originalTransform.scale;
    it->value->originalBoundingBoxExtents = [loadedEntity boundingBoxExtents] * it->value->originalEntityScale;
    it->value->originalBoundingBoxCenter = [loadedEntity boundingBoxCenter] * it->value->originalEntityScale;

    simd_float3 boundingBoxCenter = it->value->originalBoundingBoxCenter;
    simd_float3 boundingBoxExtents = it->value->originalBoundingBoxExtents;

#if HAVE(CORE_RE)
    ensurePortalEntityHierarchy();

    [loadedEntity setName:@"WebKit:ModelRootEntity"];
    parentToContainer(loadedEntity.get());

    if (!canLoadWithRealityKit)
        RENetworkMarkEntityMetadataDirty(model->rootEntity());
#endif // HAVE(CORE_RE)

    // The first model loaded receives the portal's environment map.
    bool isReportingModel = !m_nodeID || *m_nodeID == nodeID;
    if (isReportingModel) {
        m_nodeID = nodeID;
        m_modelRKEntity = loadedEntity;
    }

#if HAVE(CORE_RE)
    [m_stageModeInteractionDriver setContainerTransformInPortal];
#endif // HAVE(CORE_RE)

    if (m_entityTransformToRestore) {
        setEntityTransform(nodeID, *m_entityTransformToRestore);
        notifyModelPlayerOfTransformChange();
        m_entityTransformToRestore = std::nullopt;
    } else {
        computeTransform(true);
        updateTransform();
    }

#if HAVE(CORE_RE)
    applyStageModeOperationToDriver();
#endif // HAVE(CORE_RE)

    updateOpacity();
    setUpLoadedEntity(nodeID, loadedEntity.get());

    if (isReportingModel) {
        applyEnvironmentMapDataAndRelease([weakThis = WeakPtr { *this }] () mutable {
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->triggerModelLoadedCallbacks(true);
#endif
        });
    } else
        [loadedEntity applyDefaultIBL];

    send(Messages::ModelProcessModelPlayer::DidFinishLoading(nodeID, WebCore::FloatPoint3D(boundingBoxCenter.x, boundingBoxCenter.y, boundingBoxCenter.z), WebCore::FloatPoint3D(boundingBoxExtents.x, boundingBoxExtents.y, boundingBoxExtents.z)));
}

void ModelProcessModelPlayerProxy::didFailLoading(WebCore::REModelLoader& loader, const WebCore::ResourceError& error)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    auto it = findTrackedModelForLoader(loader);

    RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy failed to load model id=%" PRIu64 " error=\"%@\"", this, m_id.toUInt64(), error.nsError().localizedDescription);

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    triggerModelLoadedCallbacks(false);
#endif

    if (it == m_trackedModels.end())
        return;

    auto nodeID = it->key;
    m_trackedModels.remove(it);
    clearReportingModelIfNeeded(nodeID);

    send(Messages::ModelProcessModelPlayer::DidFailLoading(nodeID));
}

// MARK: - WebCore::ModelPlayer

static const int defaultEntityMemoryLimit = 100; // MB
static const int defaultImmersiveEntityMemoryLimit = 500; // MB

int ModelProcessModelPlayerProxy::entityMemoryLimit(bool isForImmersive) const
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    if (isForImmersive)
        return m_debugImmersiveEntityMemoryLimit.value_or(defaultImmersiveEntityMemoryLimit);
#else
    UNUSED_PARAM(isForImmersive);
#endif
    return m_debugEntityMemoryLimit.value_or(defaultEntityMemoryLimit);
}

class SimpleModelLoader final : public WebCore::REModelLoader {
public:
    static Ref<SimpleModelLoader> create() { return adoptRef(*new SimpleModelLoader); }
    bool isCanceled() const { return m_canceled; }
private:
    void cancel() final { m_canceled = true; }
    bool m_canceled { false };
};

#if !HAVE(CORE_RE)
class SimpleREModel final : public WebCore::REModel {
public:
    static Ref<SimpleREModel> create(RetainPtr<WKRKEntity>&& entity) { return adoptRef(*new SimpleREModel(WTF::move(entity))); }
    RetainPtr<WKRKEntity> rootRKEntity() const final { return m_entity; }
private:
    explicit SimpleREModel(RetainPtr<WKRKEntity>&& entity)
        : m_entity(WTF::move(entity)) { }
    RetainPtr<WKRKEntity> m_entity;
};
#endif

void ModelProcessModelPlayerProxy::load(WebCore::NodeIdentifier nodeID, WebCore::Model& model, WebCore::LayoutSize layoutSize, bool isForImmersive)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    auto& tracked = ensureTrackedModel(nodeID);
    if (RefPtr previousLoader = std::exchange(tracked.loader, nullptr))
        previousLoader->cancel();

    RetainPtr previousEntity = std::exchange(tracked.entity, nullptr);
    if (previousEntity) {
        clearReportingModelIfNeeded(nodeID);
        [previousEntity setDelegate:nil];
        [previousEntity removeFromParentEntity];
    }

    // The reporting model is designated before its load completes because the immersive paths need
    // a node id while a load is still in flight; the entity itself is adopted in didFinishLoading.
    if (!m_nodeID)
        m_nodeID = nodeID;

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    m_currentModel = &model;
#endif

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::load nodeID=%" PRIu64 " size=%zu isForImmersive=%d id=%" PRIu64, this, nodeID.toUInt64(), model.data()->size(), isForImmersive, m_id.toUInt64());
    sizeDidChange(layoutSize);

#if HAVE(CORE_RE) || ENABLE(MODEL_ELEMENT_IMMERSIVE)
    auto memoryLimit = entityMemoryLimit(isForImmersive);
#endif
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    m_loadedEntityMemoryLimit = memoryLimit;
#endif

#if HAVE(CORE_RE)
    WKREEngine::singleton().runWithSharedScene([this, protectedThis = protect(*this), model = protect(model), memoryLimit, nodeID] (RESceneRef scene) {
        dispatch_assert_queue(mainDispatchQueueSingleton());
        m_scene = scene;
        RefPtr<WebCore::REModelLoader> loader;
        if ([getWKRKEntityClassSingleton() isLoadFromDataAvailable])
            loader = RKUSDModelLoadScheduler::singleton().scheduleModelLoad(model.get(), m_attributionTaskID, memoryLimit, *this);
        else
            loader = WebCore::loadREModel(model.get(), *this);

        auto it = m_trackedModels.find(nodeID);
        if (it == m_trackedModels.end()) {
            loader->cancel();
            return;
        }
        it->value->loader = WTF::move(loader);
    });
#else
    auto loader = SimpleModelLoader::create();
    tracked.loader = loader.ptr();

    RetainPtr<NSData> modelData = model.data()->createNSData();
    [getWKRKEntityClassSingleton() loadFromData:modelData.get() withAttributionTaskID:nil entityMemoryLimit:0 completionHandler:makeBlockPtr([weakThis = WeakPtr { *this }, loader = WTF::move(loader)] (WKRKEntity *entity) mutable {
        if (loader->isCanceled())
            return;

        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!entity) {
            protectedThis->didFailLoading(loader.get(), WebCore::ResourceError { WebCore::errorDomainWebKitInternal, 0, { }, "Failed to load model data"_s });
            return;
        }

        protectedThis->didFinishLoading(loader.get(), SimpleREModel::create(entity));
    }).get()];
#endif
}

void ModelProcessModelPlayerProxy::unloadModel(WebCore::NodeIdentifier nodeID)
{
    dispatch_assert_queue(mainDispatchQueueSingleton());

    auto it = m_trackedModels.find(nodeID);
    if (it == m_trackedModels.end())
        return;

    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::unloadModel nodeID=%" PRIu64 " id=%" PRIu64, this, nodeID.toUInt64(), m_id.toUInt64());

    RefPtr loader = it->value->loader;
    RetainPtr entity = it->value->entity;
    m_trackedModels.remove(it);

    if (loader)
        loader->cancel();

    if (entity) {
        [entity setDelegate:nil];
        [entity removeFromParentEntity];
    }

    clearReportingModelIfNeeded(nodeID);

    // The portal-wide fit covered the removed model, so it has to be recomputed without it.
    computeTransform(true);
    updateTransform();
}

void ModelProcessModelPlayerProxy::clearReportingModelIfNeeded(WebCore::NodeIdentifier nodeID)
{
    if (!m_nodeID || *m_nodeID != nodeID)
        return;

    [m_modelRKEntity setDelegate:nil];

    m_nodeID = std::nullopt;
    m_modelRKEntity = nil;
}

simd_float3 ModelProcessModelPlayerProxy::reportingModelScale() const
{
    if (!m_nodeID)
        return simd_make_float3(1, 1, 1);

    auto it = m_trackedModels.find(*m_nodeID);
    return it == m_trackedModels.end() ? simd_make_float3(1, 1, 1) : it->value->originalEntityScale;
}

ModelProcessModelPlayerProxy::TrackedModelMap::iterator ModelProcessModelPlayerProxy::findTrackedModelForLoader(const WebCore::REModelLoader& loader)
{
    return std::ranges::find_if(m_trackedModels, [&](auto& entry) {
        return entry.value->loader.get() == &loader;
    });
}

ModelProcessModelPlayerProxy::TrackedModelMap::iterator ModelProcessModelPlayerProxy::findTrackedModelForEntity(WKRKEntity *entity)
{
    if (!entity)
        return m_trackedModels.end();

    return std::ranges::find_if(m_trackedModels, [&](auto& entry) {
        return entry.value->entity.get() == entity;
    });
}

RetainPtr<WKRKEntity> ModelProcessModelPlayerProxy::entityForNode(WebCore::NodeIdentifier nodeID) const
{
    if (auto* tracked = trackedModel(nodeID))
        return tracked->entity;

    return nil;
}

ModelProcessModelPlayerProxy::TrackedModel& ModelProcessModelPlayerProxy::ensureTrackedModel(WebCore::NodeIdentifier nodeID)
{
    return m_trackedModels.ensure(nodeID, [] {
        return makeUniqueRef<TrackedModel>();
    }).iterator->value.get();
}

ModelProcessModelPlayerProxy::TrackedModel* ModelProcessModelPlayerProxy::trackedModel(WebCore::NodeIdentifier nodeID)
{
    auto it = m_trackedModels.find(nodeID);
    if (it == m_trackedModels.end())
        return nullptr;

    return it->value.ptr();
}

const ModelProcessModelPlayerProxy::TrackedModel* ModelProcessModelPlayerProxy::trackedModel(WebCore::NodeIdentifier nodeID) const
{
    auto it = m_trackedModels.find(nodeID);
    if (it == m_trackedModels.end())
        return nullptr;

    return it->value.ptr();
}

void ModelProcessModelPlayerProxy::cancelAllLoaders()
{
    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        if (RefPtr loader = std::exchange(tracked->loader, nullptr))
            loader->cancel();
    }
}

void ModelProcessModelPlayerProxy::setUpLoadedEntity(WebCore::NodeIdentifier nodeID, WKRKEntity *entity)
{
    auto* tracked = trackedModel(nodeID);
    if (!tracked || !entity || !m_layer)
        return;

    [entity setUpAnimationWithAutoPlay:tracked->autoplay];
    [entity setLoop:tracked->loop];
    [entity setPlaybackRate:tracked->playbackRate];

    if (auto animationStateToRestore = std::exchange(tracked->animationStateToRestore, std::nullopt)) {
        [entity setPaused:animationStateToRestore->paused()];
        [entity setCurrentTime:animationStateToRestore->currentTime().seconds()];
    }
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
    if (!m_transformNeedsUpdateAfterNextLayout && !m_trackedModels.isEmpty() && m_layer)
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

void ModelProcessModelPlayerProxy::setEntityTransform(WebCore::NodeIdentifier, WebCore::TransformationMatrix transform)
{
    RESRT newSRT = REMakeSRTFromMatrix(transform);
    m_transformSRT = modelLocalizedTransformSRT(newSRT);
    m_entityTransformSetByScript = true;
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

void ModelProcessModelPlayerProxy::isPlayingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setAnimationIsPlaying(WebCore::NodeIdentifier, bool isPlaying, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::isLoopingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setIsLoopingAnimation(WebCore::NodeIdentifier, bool isLooping, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

void ModelProcessModelPlayerProxy::animationDuration(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::animationCurrentTime(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&& completionHandler)
{
    completionHandler(std::nullopt);
}

void ModelProcessModelPlayerProxy::setAnimationCurrentTime(WebCore::NodeIdentifier, Seconds currentTime, CompletionHandler<void(bool success)>&& completionHandler)
{
    completionHandler(false);
}

WebCore::ModelPlayerAccessibilityChildren ModelProcessModelPlayerProxy::accessibilityChildren()
{
    return { };
}

void ModelProcessModelPlayerProxy::setAutoplay(WebCore::NodeIdentifier nodeID, bool autoplay)
{
    ensureTrackedModel(nodeID).autoplay = autoplay;
}

void ModelProcessModelPlayerProxy::setLoop(WebCore::NodeIdentifier nodeID, bool loop)
{
    auto& tracked = ensureTrackedModel(nodeID);
    tracked.loop = loop;

    if (RetainPtr entity = tracked.entity)
        [entity setLoop:loop];
}

void ModelProcessModelPlayerProxy::setPlaybackRate(WebCore::NodeIdentifier nodeID, double playbackRate, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    auto& tracked = ensureTrackedModel(nodeID);
    tracked.playbackRate = playbackRate;

    RetainPtr entity = tracked.entity;
    if (!entity)
        return completionHandler(playbackRate);

    [entity setPlaybackRate:playbackRate];
    completionHandler([entity playbackRate]);
}

double ModelProcessModelPlayerProxy::duration(WebCore::NodeIdentifier nodeID) const
{
    RetainPtr entity = entityForNode(nodeID);
    return entity ? [entity duration] : 0;
}

bool ModelProcessModelPlayerProxy::paused(WebCore::NodeIdentifier nodeID) const
{
    RetainPtr entity = entityForNode(nodeID);
    return entity ? [entity paused] : true;
}

void ModelProcessModelPlayerProxy::setPaused(WebCore::NodeIdentifier nodeID, bool paused, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    RetainPtr entity = entityForNode(nodeID);
    if (!entity)
        return completionHandler(false);

    [entity setPaused:paused];
    completionHandler(paused == [entity paused]);
}

Seconds ModelProcessModelPlayerProxy::currentTime(WebCore::NodeIdentifier nodeID) const
{
    RetainPtr entity = entityForNode(nodeID);
    return entity ? Seconds([entity currentTime]) : 0_s;
}

void ModelProcessModelPlayerProxy::setCurrentTime(WebCore::NodeIdentifier nodeID, Seconds currentTime, CompletionHandler<void()>&& completionHandler)
{
    if (RetainPtr entity = entityForNode(nodeID))
        [entity setCurrentTime:currentTime.seconds()];

    completionHandler();
}

void ModelProcessModelPlayerProxy::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    m_persistedEnvironmentMapData = data.copyRef();
#endif
    m_transientEnvironmentMapData = WTF::move(data);
    if (m_modelRKEntity)
        applyEnvironmentMapDataAndRelease([] { });
}

void ModelProcessModelPlayerProxy::beginStageModeTransform(const WebCore::TransformationMatrix& transform)
{
#if HAVE(CORE_RE)
    simd_float4x4 transformMatrix = simd_float4x4(transform);
    [m_stageModeInteractionDriver interactionDidBegin:transformMatrix];
#endif
}

void ModelProcessModelPlayerProxy::updateStageModeTransform(const WebCore::TransformationMatrix& transform)
{
#if HAVE(CORE_RE)
    simd_float4x4 transformMatrix = simd_float4x4(transform);
    [m_stageModeInteractionDriver interactionDidUpdate:transformMatrix];
#endif
}

void ModelProcessModelPlayerProxy::endStageModeInteraction()
{
#if HAVE(CORE_RE)
    [m_stageModeInteractionDriver interactionDidEnd];
#endif
}

void ModelProcessModelPlayerProxy::resetModelTransformAfterDrag()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=291289
}

void ModelProcessModelPlayerProxy::stageModeInteractionDidUpdateModel()
{
#if HAVE(CORE_RE)
    if (stageModeInteractionInProgress())
        updateTransformSRT();
#endif
}

bool ModelProcessModelPlayerProxy::stageModeInteractionInProgress() const
{
#if HAVE(CORE_RE)
    return [m_stageModeInteractionDriver stageModeInteractionInProgress];
#else
    return false;
#endif
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

#if ENABLE(SPATIAL_PORTAL)

void ModelProcessModelPlayerProxy::setPortalTransform(WebCore::PortalTransformKind kind)
{
    if (m_portalTransform == kind)
        return;

    m_portalTransform = kind;

    computeTransform(effectiveStageModeOperation() == WebCore::StageModeOperation::None);
    updateTransform();
}

void ModelProcessModelPlayerProxy::setPortalAction(WebCore::PortalActionKind kind)
{
    if (m_portalAction == kind)
        return;

    m_portalAction = kind;

    if (m_portalAction == WebCore::PortalActionKind::None) {
        computeTransform(true);
        updateTransform();
    }

    updateForCurrentStageMode();
}

#endif

WebCore::StageModeOperation ModelProcessModelPlayerProxy::effectiveStageModeOperation() const
{
#if ENABLE(SPATIAL_PORTAL)
    if (m_portalAction == WebCore::PortalActionKind::Orbit)
        return WebCore::StageModeOperation::Orbit;
#endif

    return m_stageModeOperation;
}

void ModelProcessModelPlayerProxy::updateForCurrentStageMode()
{
    if (effectiveStageModeOperation() != WebCore::StageModeOperation::None) {
        computeTransform(false);
#if HAVE(CORE_RE)
        [m_containerEntityWrapper recenterEntityAtTransform:WKEntityTransform({ m_transformSRT.scale, m_transformSRT.rotation, m_transformSRT.translation })];
#else
        [m_modelRKEntity recenterEntityAtTransform:WKEntityTransform({ m_transformSRT.scale * reportingModelScale(), m_transformSRT.rotation, m_transformSRT.translation })];
#endif
        updateTransformSRT();
    }

#if HAVE(CORE_RE)
    applyStageModeOperationToDriver();
#endif
}

void ModelProcessModelPlayerProxy::setStageMode(WebCore::StageModeOperation stagemodeOp)
{
    if (m_stageModeOperation == stagemodeOp)
        return;

    m_stageModeOperation = stagemodeOp;

    if (m_stageModeOperation != WebCore::StageModeOperation::None)
        m_entityTransformSetByScript = false;

    updateForCurrentStageMode();
}

void ModelProcessModelPlayerProxy::updateTransformSRT()
{
#if HAVE(CORE_RE)
    if (!m_containerEntityWrapper)
        return;

    WKEntityTransform containerTransform = [m_containerEntityWrapper transform];
    m_transformSRT = RESRT {
        .scale = containerTransform.scale,
        .rotation = containerTransform.rotation,
        .translation = containerTransform.translation
    };
#else
    if (!m_modelRKEntity)
        return;

    WKEntityTransform entityTransform = [m_modelRKEntity transform];
    m_transformSRT = RESRT {
        .scale = entityTransform.scale / reportingModelScale(),
        .rotation = entityTransform.rotation,
        .translation = entityTransform.translation
    };
#endif

    notifyModelPlayerOfTransformChange();
}

#if HAVE(CORE_RE)
void ModelProcessModelPlayerProxy::applyStageModeOperationToDriver()
{
    switch (effectiveStageModeOperation()) {
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
#endif // HAVE(CORE_RE)

#if HAVE(CORE_RE)
void ModelProcessModelPlayerProxy::ensurePortalEntityHierarchy()
{
    if (m_hostingEntity && m_containerEntity)
        return;

    if (!m_hostingEntity) {
        m_hostingEntity = adoptRE(REEntityCreate());
        REEntitySetName(m_hostingEntity.get(), "WebKit:EntityWithRootComponent");

        REPtr<REComponentRef> layerComponent = adoptRE(RECALayerServiceCreateRootComponent(webDefaultLayerService(), CALayerGetContext(m_layer.get()), m_hostingEntity.get(), nil));
        RESceneAddEntity(m_scene.get(), m_hostingEntity.get());

        CALayer *contextEntityLayer = RECALayerClientComponentGetCALayer(layerComponent.get());
        [contextEntityLayer setSeparatedState:kCALayerSeparatedStateTracked];

        RECALayerClientComponentSetShouldSyncToRemotes(layerComponent.get(), true);
    }

    auto clientComponent = RECALayerGetCALayerClientComponent(m_layer.get());
    auto clientComponentEntity = REComponentGetEntity(clientComponent);
    REEntitySetName(clientComponentEntity, "WebKit:ClientComponentEntity");

    if (!m_containerEntity) {
        m_containerEntity = adoptRE(REEntityCreate());
        REEntitySetName(m_containerEntity.get(), "WebKit:PortalContainer");
        REEntitySetParent(m_containerEntity.get(), clientComponentEntity);

        m_containerEntityWrapper = adoptNS([allocWKRKEntityInstance() initWithCoreEntity:m_containerEntity.get()]);
        [m_containerEntityWrapper setTransform:WKEntityTransform({
            .scale = simd_make_float3(1, 1, 1),
            .rotation = simd_quaternion(0, simd_make_float3(1, 0, 0)),
            .translation = simd_make_float3(0, 0, 0)
        })];

        REEntitySubtreeAddNetworkComponentRecursive(m_containerEntity.get());
        RENetworkMarkEntityMetadataDirty(m_containerEntity.get());

        // StageMode operates on the whole portal.
        m_stageModeInteractionDriver = adoptNS([allocWKStageModeInteractionDriverInstance()
            initWithModel:m_containerEntityWrapper.get()
            container:clientComponentEntity
            delegate:m_objCAdapter.get()]);

        REEntitySubtreeAddNetworkComponentRecursive([m_stageModeInteractionDriver interactionContainerRef]);
        RENetworkMarkEntityMetadataDirty([m_stageModeInteractionDriver interactionContainerRef]);
    }
}

void ModelProcessModelPlayerProxy::parentToContainer(WKRKEntity *childEntity)
{
    [childEntity setParentCoreEntity:m_containerEntity.get() preservingWorldTransform:NO];
    [childEntity setReferenceEntity:m_containerEntityWrapper.get()];
}
#endif // HAVE(CORE_RE)

void ModelProcessModelPlayerProxy::applyDefaultIBL()
{
    [m_modelRKEntity applyDefaultIBL];
}

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

void ModelProcessModelPlayerProxy::teardownEntity()
{
    cancelAllLoaders();

    // Keeps each map entry, because captureStateForReload() has just recorded playback state there
    // and the caller reloads immediately afterwards. Releasing the entity is what frees the memory.
    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        tracked->loader = nullptr;
        [tracked->entity setDelegate:nil];
        tracked->entity = nullptr;
    }
#if HAVE(CORE_RE)
    if (m_containerEntity.get())
        REEntityRemoveFromSceneOrParent(m_containerEntity.get());
    m_containerEntity = nullptr;
    m_containerEntityWrapper = nil;

    if (m_hostingEntity.get())
        REEntityRemoveFromSceneOrParent(m_hostingEntity.get());
    m_hostingEntity = nullptr;

    [m_stageModeInteractionDriver removeInteractionContainerFromSceneOrParent];
    m_stageModeInteractionDriver = nil;
#endif
    m_modelRKEntity = nil;
    m_loadedEntityMemoryLimit = std::nullopt;
}

void ModelProcessModelPlayerProxy::captureStateForReload()
{
    for (UniqueRef<TrackedModel>& tracked : m_trackedModels.values()) {
        if (!tracked->entity)
            continue;

        tracked->animationStateToRestore = WebCore::ModelPlayerAnimationState { tracked->autoplay, tracked->loop, [tracked->entity paused], Seconds([tracked->entity duration]),
            std::optional<double> { tracked->playbackRate }, std::optional<Seconds> { Seconds([tracked->entity currentTime]) }, std::optional<MonotonicTime> { MonotonicTime::now() } };
    }

    // Re-stage the env map for the post-reload entity. Entity transform is intentionally NOT captured:
    // the immersive/non-immersive coordinate spaces differ (see modelStandardizedTransformSRT), so a
    // captured matrix would be in the wrong space after the boundary. didFinishLoading recomputes
    // transform via computeTransform(true) for the new presentation mode.
    if (m_persistedEnvironmentMapData)
        m_transientEnvironmentMapData = m_persistedEnvironmentMapData;
}

void ModelProcessModelPlayerProxy::ensureImmersivePresentation(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&& completion)
{
    // FIXME: Add immersive presentation for spatial portal
    if (m_trackedModels.size() > 1) {
        RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessModelPlayerProxy::ensureImmersivePresentation: unsupported for %u hosted models id=%" PRIu64, this, m_trackedModels.size(), m_id.toUInt64());
        return completion(std::nullopt);
    }

    int targetLimit = entityMemoryLimit(true);
    bool atTargetLimit = m_loadedEntityMemoryLimit && *m_loadedEntityMemoryLimit == targetLimit;

    if (m_modelRKEntity && !atTargetLimit)
        captureStateForReload();

    setImmersivePresentation(true);

    if (m_nodeID && m_currentModel && !m_trackedModels.isEmpty() && !atTargetLimit) {
        RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::ensureImmersivePresentation: reloading at %dMB id=%" PRIu64, this, targetLimit, m_id.toUInt64());
        teardownEntity();
        load(*m_nodeID, *m_currentModel, m_layoutSize, true);
    }

    ensureModelLoaded([weakThis = WeakPtr { *this }, completion = WTF::move(completion)] (bool loaded) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completion(std::nullopt);

        if (loaded)
            completion(protectedThis->layerHostingContextIdentifier().value());
        else {
            protectedThis->setImmersivePresentation(false);
            completion(std::nullopt);
        }
    });
}

void ModelProcessModelPlayerProxy::exitImmersivePresentation(CompletionHandler<void()>&& completion)
{
    int targetLimit = entityMemoryLimit(false);
    bool atTargetLimit = m_loadedEntityMemoryLimit && *m_loadedEntityMemoryLimit == targetLimit;

    if (m_modelRKEntity && !atTargetLimit)
        captureStateForReload();

    setImmersivePresentation(false);

    if (m_nodeID && m_currentModel && !m_trackedModels.isEmpty() && !atTargetLimit) {
        RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::exitImmersivePresentation: reloading at %dMB id=%" PRIu64, this, targetLimit, m_id.toUInt64());
        teardownEntity();
        load(*m_nodeID, *m_currentModel, m_layoutSize, false);
        ensureModelLoaded([completion = WTF::move(completion)] (bool) mutable {
            completion();
        });
        return;
    }

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
    m_entityTransformToRestore = std::nullopt;
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
