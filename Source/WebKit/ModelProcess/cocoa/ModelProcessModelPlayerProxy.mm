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
#import "RealityKitBridging.h"
#import "WKModelProcessModelLayer.h"
#import "WKStageMode.h"
#import <RealitySystemSupport/RealitySystemSupport.h>
#import <SurfBoardServices/SurfBoardServices.h>
#import <WebCore/Color.h>
#import <WebCore/LayerHostingContextIdentifier.h>
#import <WebCore/Model.h>
#import <WebCore/ResourceError.h>
#import <WebCore/TransformationMatrix.h>
#import <WebKitAdditions/REModel.h>
#import <WebKitAdditions/REModelLoader.h>
#import <WebKitAdditions/REPtr.h>
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#import <WebKitAdditions/WKREEngine.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/MathExtras.h>
#import <wtf/NakedPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/TextStream.h>

#import "WebKitSwiftSoftLink.h"

@interface WKModelProcessModelPlayerProxyObjCAdapter : NSObject<WKSRKEntityDelegate, WKStageModeInteractionAware>
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

class RKModelUSD final : public WebCore::REModel {
public:
    static Ref<RKModelUSD> create(Ref<Model> model, RetainPtr<WKSRKEntity> entity)
    {
        return adoptRef(*new RKModelUSD(WTFMove(model), WTFMove(entity)));
    }

    virtual ~RKModelUSD() = default;

private:
    RKModelUSD(Ref<Model> model, RetainPtr<WKSRKEntity> entity)
        : m_model { WTFMove(model) }
        , m_entity { WTFMove(entity) }
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

    RetainPtr<WKSRKEntity> rootRKEntity() const final
    {
        return m_entity;
    }

    Ref<Model> m_model;
    RetainPtr<WKSRKEntity> m_entity;
};

class RKModelLoaderUSD final : public WebCore::REModelLoader, public CanMakeWeakPtr<RKModelLoaderUSD> {
public:
    static Ref<RKModelLoaderUSD> create(Model& model, REModelLoaderClient& client)
    {
        return adoptRef(*new RKModelLoaderUSD(model, client));
    }

    virtual ~RKModelLoaderUSD() = default;

    void load();

    bool isCanceled() const { return m_canceled; }

private:
    RKModelLoaderUSD(Model& model, REModelLoaderClient& client)
        : m_canceled { false }
        , m_model { model }
        , m_client { client }
    {
    }

    // REModelLoader overrides.
    void cancel() final
    {
        m_canceled = true;
    }

    void didFinish(RetainPtr<WKSRKEntity> entity)
    {
        if (m_canceled)
            return;

        if (auto strongClient = m_client.get())
            strongClient->didFinishLoading(*this, RKModelUSD::create(WTFMove(m_model), entity));
    }

    void didFail(ResourceError error)
    {
        if (m_canceled)
            return;

        if (auto strongClient = m_client.get())
            strongClient->didFailLoading(*this, WTFMove(error));
    }

    bool m_canceled { false };

    Ref<Model> m_model;
    WeakPtr<REModelLoaderClient> m_client;
};

void RKModelLoaderUSD::load()
{
    [getWKSRKEntityClass() loadFromData:m_model->data()->createNSData().get() completionHandler:makeBlockPtr([weakThis = WeakPtr { *this }] (WKSRKEntity *entity) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didFinish(entity);
    }).get()];
}

static Ref<REModelLoader> loadREModelUsingRKUSDLoader(Model& model, REModelLoaderClient& client)
{
    auto loader = RKModelLoaderUSD::create(model, client);

    dispatch_async(dispatch_get_main_queue(), [loader] () mutable {
        loader->load();
    });

    return loader;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelProcessModelPlayerProxy);

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
    m_objCAdapter = adoptNS([[WKModelProcessModelPlayerProxyObjCAdapter alloc] initWithModelProcessModelPlayerProxy:*this]);
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
}

std::optional<SharedPreferencesForWebProcess> ModelProcessModelPlayerProxy::sharedPreferencesForWebProcess() const
{
    if (RefPtr strongManager = m_manager.get())
        return strongManager->sharedPreferencesForWebProcess();

    return std::nullopt;
}

bool ModelProcessModelPlayerProxy::transformSupported(const simd_float4x4& transform)
{
    RESRT srt = REMakeSRTFromMatrix(transform);

    // Scale must be uniform across all 3 axis
    if (simd_reduce_max(srt.scale) - simd_reduce_min(srt.scale) > FLT_EPSILON) {
        RELEASE_LOG_ERROR(ModelElement, "Rejecting non-uniform scaling %.05f %.05f %.05f", srt.scale[0], srt.scale[1], srt.scale[2]);
        return false;
    }

    // Matrix must be a SRT (scale/rotation/translation) matrix - no shear.
    // RESRT itself is already clean of shear, so we just need to see if the input is the same as the cleaned RESRT
    simd_float4x4 noShearMatrix = RESRTMatrix(srt);
    if (!simd_almost_equal_elements(transform, noShearMatrix, FLT_EPSILON)) {
        RELEASE_LOG_ERROR(ModelElement, "Rejecting shear matrix");
        return false;
    }

    return true;
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
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT(!m_layer);

    m_layer = adoptNS([[WKModelProcessModelLayer alloc] init]);
    [m_layer setName:@"WKModelProcessModelLayer"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.transform"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.collider"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.mesh"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.material"];
    [m_layer setValue:@YES forKeyPath:@"separatedOptions.updates.texture"];

    [m_layer setPlayer:WeakPtr { this }];

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

// MARK: - RE stuff

static inline simd_float2 makeMeterSizeFromPointSize(CGSize pointSize, CGFloat pointsPerMeter)
{
    return simd_make_float2(pointSize.width / pointsPerMeter, pointSize.height / pointsPerMeter);
}

static void computeScaledExtentsAndCenter(simd_float2 boundsOfLayerInMeters, simd_float3& boundingBoxExtents, simd_float3& boundingBoxCenter)
{
    if (simd_reduce_min(boundingBoxExtents) - FLT_EPSILON > 0) {
        auto boundsScaleRatios = simd_make_float2(
            boundsOfLayerInMeters.x / boundingBoxExtents.x,
            boundsOfLayerInMeters.y / boundingBoxExtents.y
        );
        boundingBoxCenter = simd_reduce_min(boundsScaleRatios) * boundingBoxCenter;
        boundingBoxExtents = simd_reduce_min(boundsScaleRatios) * boundingBoxExtents;
    }
}

static RESRT computeSRT(CALayer *layer, simd_float3 originalBoundingBoxExtents, simd_float3 originalBoundingBoxCenter, float pitch, float yaw, bool isPortal, CGFloat pointsPerMeter)
{
    auto boundsOfLayerInMeters = makeMeterSizeFromPointSize(layer.bounds.size, pointsPerMeter);
    simd_float3 boundingBoxExtents = originalBoundingBoxExtents;
    simd_float3 boundingBoxCenter = originalBoundingBoxCenter;
    computeScaledExtentsAndCenter(boundsOfLayerInMeters, boundingBoxExtents, boundingBoxCenter);

    RESRT srt;
    srt.scale = simd_make_float3(boundingBoxExtents.x / originalBoundingBoxExtents.x, boundingBoxExtents.y / originalBoundingBoxExtents.y, boundingBoxExtents.z / originalBoundingBoxExtents.z);
    float minScale = simd_reduce_min(srt.scale);
    srt.scale = simd_make_float3(minScale, minScale, minScale); // FIXME: assume object-fit:contain for now

    // Must be normalized, but these obviously are.
    simd_float3 xAxis = simd_make_float3(1, 0, 0);
    simd_float3 yAxis = simd_make_float3(0, 1, 0);

    // FIXME: These should rotate around the center point of the model.
    simd_quatf pitchQuat = simd_quaternion(deg2rad(pitch), xAxis);
    simd_quatf yawQuat = simd_quaternion(deg2rad(yaw), yAxis);
    srt.rotation = simd_mul(pitchQuat, yawQuat);

    if (isPortal)
        srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z - boundingBoxExtents.z / 2.0f);
    else
        srt.translation = simd_make_float3(-boundingBoxCenter.x, -boundingBoxCenter.y, -boundingBoxCenter.z + boundingBoxExtents.z / 2.0f);

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

void ModelProcessModelPlayerProxy::computeTransform()
{
    if (!m_model || !m_layer)
        return;

    // FIXME: Use the value of the 'object-fit' property here to compute an appropriate SRT.
    RESRT newSRT = computeSRT(m_layer.get(), m_originalBoundingBoxExtents, m_originalBoundingBoxCenter, m_pitch, m_yaw, m_hasPortal, effectivePointsPerMeter(m_layer.get()));
    m_transformSRT = newSRT;

    simd_float4x4 matrix = RESRTMatrix(m_transformSRT);
    WebCore::TransformationMatrix transform = WebCore::TransformationMatrix(matrix);
    send(Messages::ModelProcessModelPlayer::DidUpdateEntityTransform(transform));
}

void ModelProcessModelPlayerProxy::updateTransform()
{
    if (!m_model || !m_layer)
        return;

    [m_modelRKEntity setTransform:WKEntityTransform({ m_transformSRT.scale, m_transformSRT.rotation, m_transformSRT.translation })];
}

void ModelProcessModelPlayerProxy::updateOpacity()
{
    if (!m_model || !m_layer)
        return;

    [m_modelRKEntity setOpacity:[m_layer opacity]];
}

void ModelProcessModelPlayerProxy::startAnimating()
{
    if (!m_model || !m_layer)
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
    dispatch_assert_queue(dispatch_get_main_queue());
    ASSERT(&loader == m_loader.get());

    bool canLoadWithRealityKit = [getWKSRKEntityClass() isLoadFromDataAvailable];

    m_loader = nullptr;
    m_model = WTFMove(model);
    if (canLoadWithRealityKit)
        m_modelRKEntity = m_model->rootRKEntity();
    else if (m_model->rootEntity())
        m_modelRKEntity = adoptNS([allocWKSRKEntityInstance() initWithCoreEntity:m_model->rootEntity()]);
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
        [m_model->rootRKEntity() setName:@"WebKit:ModelRootEntity"];
    else
        REEntitySetName(m_model->rootEntity(), "WebKit:ModelRootEntity");

    if (canLoadWithRealityKit)
        [m_model->rootRKEntity() setParentCoreEntity:clientComponentEntity preservingWorldTransform:NO];
    else {
        REEntitySetParent(m_model->rootEntity(), clientComponentEntity);
    }

    m_stageModeInteractionDriver = adoptNS([allocWKStageModeInteractionDriverInstance() initWithModel:m_modelRKEntity.get() container:clientComponentEntity delegate:m_objCAdapter.get()]);

    REEntitySubtreeAddNetworkComponentRecursive([m_stageModeInteractionDriver interactionContainerRef]);
    RENetworkMarkEntityMetadataDirty([m_stageModeInteractionDriver interactionContainerRef]);

    applyStageModeOperationToDriver();

    if (!canLoadWithRealityKit)
        RENetworkMarkEntityMetadataDirty(m_model->rootEntity());

    computeTransform();
    updateTransform();
    [m_stageModeInteractionDriver setContainerTransformInPortal];

    updateOpacity();
    startAnimating();

    applyEnvironmentMapDataAndRelease();

    send(Messages::ModelProcessModelPlayer::DidFinishLoading(WebCore::FloatPoint3D(m_originalBoundingBoxCenter.x, m_originalBoundingBoxCenter.y, m_originalBoundingBoxCenter.z), WebCore::FloatPoint3D(m_originalBoundingBoxExtents.x, m_originalBoundingBoxExtents.y, m_originalBoundingBoxExtents.z)));
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
        if ([getWKSRKEntityClass() isLoadFromDataAvailable])
            m_loader = loadREModelUsingRKUSDLoader(model.get(), *this);
        else
            m_loader = WebCore::loadREModel(model.get(), *this);
    });
}

void ModelProcessModelPlayerProxy::sizeDidChange(WebCore::LayoutSize layoutSize)
{
    RELEASE_LOG(ModelElement, "%p - ModelProcessModelPlayerProxy::sizeDidChange w=%lf h=%lf id=%" PRIu64, this, layoutSize.width().toDouble(), layoutSize.height().toDouble(), m_id.toUInt64());
    [m_layer setFrame:CGRectMake(0, 0, layoutSize.width().toDouble(), layoutSize.height().toDouble())];
}

PlatformLayer* ModelProcessModelPlayerProxy::layer()
{
    return nullptr;
}

std::optional<WebCore::LayerHostingContextIdentifier> ModelProcessModelPlayerProxy::layerHostingContextIdentifier()
{
    return WebCore::LayerHostingContextIdentifier(m_layerHostingContext->contextID());
}

void ModelProcessModelPlayerProxy::setEntityTransform(WebCore::TransformationMatrix transform)
{
    m_transformSRT = REMakeSRTFromMatrix(transform);
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

Vector<RetainPtr<id>> ModelProcessModelPlayerProxy::accessibilityChildren()
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
    m_transientEnvironmentMapData = WTFMove(data);
    if (m_modelRKEntity)
        applyEnvironmentMapDataAndRelease();
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

void ModelProcessModelPlayerProxy::stageModeInteractionDidUpdateModel()
{
    if (stageModeInteractionInProgress() && m_modelRKEntity) {
        WKEntityTransform entityTransform = [m_modelRKEntity transform];
        m_transformSRT = RESRT {
            .scale = entityTransform.scale,
            .rotation = entityTransform.rotation,
            .translation = entityTransform.translation
        };

        simd_float4x4 matrix = RESRTMatrix(m_transformSRT);
        WebCore::TransformationMatrix transform = WebCore::TransformationMatrix(matrix);
        send(Messages::ModelProcessModelPlayer::DidUpdateEntityTransform(transform));
    }
}

bool ModelProcessModelPlayerProxy::stageModeInteractionInProgress() const
{
    return [m_stageModeInteractionDriver stageModeInteractionInProgress];
}

void ModelProcessModelPlayerProxy::applyEnvironmentMapDataAndRelease()
{
    if (m_transientEnvironmentMapData) {
        if (m_transientEnvironmentMapData->size() > 0) {
            [m_modelRKEntity applyIBLData:m_transientEnvironmentMapData->createNSData().get() withCompletion:^(BOOL succeeded) {
                if (!succeeded)
                    [m_modelRKEntity applyDefaultIBL];
                send(Messages::ModelProcessModelPlayer::DidFinishEnvironmentMapLoading(succeeded));
            }];
        } else {
            [m_modelRKEntity applyDefaultIBL];
            send(Messages::ModelProcessModelPlayer::DidFinishEnvironmentMapLoading(true));
        }
        m_transientEnvironmentMapData = nullptr;
    } else {
        [m_modelRKEntity applyDefaultIBL];
    }
}

void ModelProcessModelPlayerProxy::setHasPortal(bool hasPortal)
{
    if (m_hasPortal == hasPortal)
        return;

    m_hasPortal = hasPortal;

    computeTransform();
    updateTransform();
}

void ModelProcessModelPlayerProxy::setStageMode(WebCore::StageModeOperation stagemodeOp)
{
    if (m_stageModeOperation == stagemodeOp)
        return;

    m_stageModeOperation = stagemodeOp;
    applyStageModeOperationToDriver();
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

} // namespace WebKit

#endif // ENABLE(MODEL_PROCESS)
