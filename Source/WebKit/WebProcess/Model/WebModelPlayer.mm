/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebModelPlayer.h"

#if ENABLE(GPU_PROCESS_MODEL)

#import "Logging.h"
#import "Mesh.h"
#import "ModelInlineConverters.h"
#import "ModelProcessModelPlayerTransformState.h"
#import "ModelTypes.h"
#import "RemoteGPUProxy.h"
#import "RemoteMeshProxy.h"
#import "WKStageModeOrbitSimulator.h"
#import <WebCore/Chrome.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentEventLoop.h>
#import <WebCore/FloatPoint3D.h>
#import <WebCore/GPU.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#import <WebCore/HTMLModelElement.h>
#import <WebCore/ImageBuffer.h>
#import <WebCore/ModelPlayerAnimationState.h>
#import <WebCore/ModelPlayerGraphicsLayerConfiguration.h>
#import <WebCore/ModelPlayerTransformState.h>
#import <WebCore/Navigator.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformCALayer.h>
#import <WebCore/PlatformCALayerDelegatedContents.h>
#import <WebCore/PlatformDynamicRangeLimit.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScreenProperties.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#import <Metal/Metal.h>
#endif

#if ENABLE(SPATIAL_PORTAL)
#import <WebCore/CSSUnits.h>
#endif

#import "WebKitSwiftSoftLink.h"

namespace WebKit {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(WebModelPlayer::HostedModel);

WebModelPlayer::HostedModel::~HostedModel() = default;

class ModelDisplayBufferDisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<ModelDisplayBufferDisplayDelegate> create(WebModelPlayer& modelPlayer, float contentsScale = 1)
    {
        return adoptRef(*new ModelDisplayBufferDisplayDelegate(modelPlayer, contentsScale));
    }
    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(WebCore::PlatformCALayer& layer) final
    {
        layer.setOpaque(false);
        layer.setContentsScale(m_contentsScale);
        layer.setContentsFormat(m_contentsFormat);
    }
    void display(WebCore::PlatformCALayer& layer) final
    {
        if (m_displayBuffer) {
            layer.setContentsFormat(m_contentsFormat);
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { } });
        } else
            layer.clearContents();

        if (RefPtr player = m_modelPlayer)
            player->scheduleUpdateIfNeeded();
    }
    WebCore::GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return WebCore::GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
    }
    void setDisplayBuffer(const WTF::MachSendRight& displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer = { };
            return;
        }

        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;

        m_displayBuffer = MachSendRight { displayBuffer };
    }
    void setContentsFormat(WebCore::ContentsFormat contentsFormat)
    {
        m_contentsFormat = contentsFormat;
    }
    void setModelPlayer(WebModelPlayer& modelPlayer)
    {
        m_modelPlayer = modelPlayer;
    }
private:
    ModelDisplayBufferDisplayDelegate(WebModelPlayer& modelPlayer, float contentsScale)
        : m_modelPlayer(modelPlayer)
        , m_contentsScale(contentsScale)
    {
    }
    ThreadSafeWeakPtr<WebModelPlayer> m_modelPlayer;
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    WebCore::ContentsFormat m_contentsFormat { WebCore::ContentsFormat::RGBA16F };
#else
    WebCore::ContentsFormat m_contentsFormat { WebCore::ContentsFormat::RGBA8 };
#endif
};

Ref<WebModelPlayer> WebModelPlayer::create(WebCore::Page& page, WebCore::ModelPlayerClient& client)
{
    return adoptRef(*new WebModelPlayer(page, client));
}

WebModelPlayer::WebModelPlayer(WebCore::Page& page, WebCore::ModelPlayerClient& client)
: m_client { client }
, m_id { WebCore::ModelPlayerIdentifier::generate() }
, m_page(page)
{
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    updateScreenHeadroomFromPage();

    if (RefPtr document = page.localTopDocument()) {
        lazyInitialize(m_screenPropertiesChangedObserver, ScreenPropertiesChangedObserver::create([weakThis = ThreadSafeWeakPtr { *this }](WebCore::PlatformDisplayID displayID) {
            RefPtr protectedThis { weakThis };
            if (!protectedThis)
                return;
            auto platformScreen = WebCore::PlatformScreen::singleton();
            if (auto* data = platformScreen->screenData(displayID))
                protectedThis->updateScreenHeadroom(data->currentEDRHeadroom, data->suppressEDR);
        }));

        document->addScreenPropertiesChangedObserver(*m_screenPropertiesChangedObserver);
    }
#endif
}

WebModelPlayer::~WebModelPlayer() = default;

void WebModelPlayer::ensureOnMainThreadWithProtectedThis(Function<void(Ref<WebModelPlayer>)>&& task)
{
    ensureOnMainThread([protectedThis = protect(*this), task = WTF::move(task)]() mutable {
        task(protectedThis);
    });
}

WebModelPlayer::HostedModel& WebModelPlayer::ensureHostedModel(WebCore::NodeIdentifier nodeID)
{
    return m_hostedModels.ensure(nodeID, [] {
        return makeUniqueRef<HostedModel>();
    }).iterator->value.get();
}

WebModelPlayer::HostedModel* WebModelPlayer::hostedModel(WebCore::NodeIdentifier nodeID)
{
    auto it = m_hostedModels.find(nodeID);
    return it == m_hostedModels.end() ? nullptr : it->value.ptr();
}

const WebModelPlayer::HostedModel* WebModelPlayer::hostedModel(WebCore::NodeIdentifier nodeID) const
{
    auto it = m_hostedModels.find(nodeID);
    return it == m_hostedModels.end() ? nullptr : it->value.ptr();
}

double WebModelPlayer::duration(WebCore::NodeIdentifier nodeID) const
{
    CheckedPtr hosted = hostedModel(nodeID);
    if (!hosted)
        return 0;
    if (hosted->animationStateToRestore)
        return hosted->animationStateToRestore->duration().seconds();
    return [hosted->loader duration];
}

static std::optional<WebCore::SharedMemoryHandle> loadData(RetainPtr<CFStringRef> filename)
{
    RetainPtr<NSBundle> myBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
    RetainPtr<NSURL> nsFileURL = [myBundle URLForResource:(__bridge NSString *)filename.get() withExtension:@""];
    RetainPtr<NSData> data = [NSData dataWithContentsOfURL:nsFileURL.get() options:0 error:nil];
    if (!data || ![data length])
        return std::nullopt;
    return WebCore::SharedMemoryHandle::createCopy(WTF::span(data.get()), WebCore::SharedMemoryProtection::ReadOnly);
}

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
static WebCore::ContentsFormat contentsFormatForDynamicRange(bool isStandard)
{
    return isStandard ? WebCore::ContentsFormat::RGBA8 : WebCore::ContentsFormat::RGBA16F;
}
#endif

// MARK: - ModelPlayer overrides.

bool WebModelPlayer::ensureModelBacking(WebCore::LayoutSize size)
{
    if (m_currentModel)
        return true;

    RefPtr corePage { m_page };
    if (!corePage)
        return false;
    m_renderTextureIndex = 0;
    m_displayTextureIndex = 0;
    m_hasRenderedFrame = false;
    m_isUpdateLoopRunning = false;
    RefPtr document = corePage->localTopDocument();
    if (!document)
        return false;

    RefPtr window = document->window();
    if (!window)
        return false;

    RefPtr gpu = protect(window->navigator())->gpu();
    if (!gpu)
        return false;

    auto cssSize = size;
    size.scale(document->deviceScaleFactor());
    m_currentPixelSize = WebCore::IntSize(size.width().toUnsigned(), size.height().toUnsigned());

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    m_lastLayoutSize = cssSize;
#endif

    WEBMODEL_WEB_MODEL_PLAYER_DECLARE_DIFFUSE_AND_SPECULAR_TEXTURES

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    bool standardDynamicRange = m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard();
    m_usingStandardDynamicRange = standardDynamicRange;
#else
    bool standardDynamicRange = false;
#endif

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    m_lastSentContentsHeadroom = -1.f;
#endif
    m_currentModel = static_cast<RemoteGPUProxy&>(gpu->backing()).createModelBacking(m_currentPixelSize.width(), m_currentPixelSize.height(), WTF::move(diffuseTexture), WTF::move(specularTexture), standardDynamicRange, [protectedThis = protect(*this)] (Vector<MachSendRight>&& surfaceHandles) {
        if (surfaceHandles.size()) {
            protectedThis->m_displayBuffers = WTF::move(surfaceHandles);
            protectedThis->m_renderTextureIndex = 0;
            protectedThis->m_displayTextureIndex = 0;
            protectedThis->m_hasRenderedFrame = false;
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
            protectedThis->updateScreenHeadroomFromPage();
#endif
        }
    });
    if (!m_currentModel)
        return false;
    m_currentModel->setViewportSize(cssSize.width().toFloat(), cssSize.height().toFloat());

#if ENABLE(SPATIAL_PORTAL)
    if (m_portalTransform)
        protect(m_currentModel)->setPortalTransform(*m_portalTransform);
#endif

    m_environmentMapPushedToCurrentModel = false;
    pushEnvironmentMapToModel();

    return true;
}

static void trackResourceIDs(HashMap<String, WebModel::TypedResourceId>& tracked, const Vector<WebModel::TypedResourceId>& identifiers)
{
    for (auto& identifier : identifiers)
        tracked.add(identifier.value, identifier);
}

static void forgetResourceIDs(HashMap<String, WebModel::TypedResourceId>& tracked, const Vector<WebModel::TypedResourceId>& removals)
{
    for (auto& identifier : removals)
        tracked.remove(identifier.value);
}

void WebModelPlayer::setLoaderCallbacks(WebCore::NodeIdentifier nodeID, WKBridgeModelLoader *loader)
{
    auto dispatch = [weakThis = ThreadSafeWeakPtr { *this }, weakLoader = WeakObjCPtr<WKBridgeModelLoader> { loader }, nodeID](auto* request, auto&& task) {
        ensureOnMainThread([weakThis, weakLoader, nodeID, request = RetainPtr { request }, task = WTF::move(task)] {
            RefPtr protectedThis { weakThis };
            RetainPtr loader = weakLoader.get();
            if (!protectedThis || !loader)
                return;

            if (CheckedPtr hosted = protectedThis->hostedModel(nodeID); hosted && hosted->loader == loader)
                task(*protectedThis, *hosted, request.get());

            [loader requestCompleted:request.get()];
            protectedThis->startUpdateLoopIfNeeded();
        });
    };

    [loader setCallbacksWithModelUpdatedCallback:^(NSArray<WKBridgeUpdateMesh *> *updateRequest) {
        dispatch(updateRequest, [nodeID](WebModelPlayer& player, HostedModel& hosted, NSArray<WKBridgeUpdateMesh *> *updateRequest) {
            RefPtr model = player.m_currentModel;
            if (!model)
                return;

            auto descriptors = makeVector(updateRequest, [](WKBridgeUpdateMesh *update) {
                return std::optional { convert(update) };
            });
            trackResourceIDs(hosted.meshIDs, descriptors.map([](auto& descriptor) {
                return descriptor.identifier;
            }));
            model->updateModel(nodeID, WTF::move(descriptors));

            if (RefPtr client = player.m_client; client && !hosted.didFinishLoading) {
                hosted.didFinishLoading = true;
                [hosted.loader setLoop:hosted.isLooping];
                if (auto animationStateToRestore = std::exchange(hosted.animationStateToRestore, std::nullopt)) {
                    player.play(nodeID, hosted, !animationStateToRestore->paused());
                    [hosted.loader setCurrentTime:std::clamp(animationStateToRestore->currentTime().seconds(), 0.0, [hosted.loader duration])];
#if ENABLE(SPATIAL_PORTAL)
                    player.pushAnchoredPortalChildTransforms(nodeID);
#endif
                } else if (hosted.autoplay && hosted.pauseState == HostedModel::PauseState::None)
                    player.play(nodeID, hosted, true);
                hosted.pendingClientFinishLoadingNotification = true;
            }

            player.setStageMode(player.m_stageMode);
        });
    } textureUpdatedCallback:^(NSArray<WKBridgeUpdateTexture *> *updateTexture) {
        dispatch(updateTexture, [](WebModelPlayer& player, HostedModel& hosted, NSArray<WKBridgeUpdateTexture *> *updateTexture) {
            RefPtr model = player.m_currentModel;
            if (!model)
                return;

            auto descriptors = makeVector(updateTexture, [](WKBridgeUpdateTexture *update) {
                return std::optional { convert(update) };
            });
            trackResourceIDs(hosted.textureIDs, descriptors.map([](auto& descriptor) {
                return descriptor.identifier;
            }));
            model->updateTexture(WTF::move(descriptors));
        });
    } materialUpdatedCallback:^(NSArray<WKBridgeUpdateMaterial *> *updateMaterial) {
        dispatch(updateMaterial, [](WebModelPlayer& player, HostedModel& hosted, NSArray<WKBridgeUpdateMaterial *> *updateMaterial) {
            RefPtr model = player.m_currentModel;
            if (!model)
                return;

            auto descriptors = makeVector(updateMaterial, [](WKBridgeUpdateMaterial *update) {
                return std::optional { convert(update) };
            });
            trackResourceIDs(hosted.materialIDs, descriptors.map([](auto& descriptor) {
                return descriptor.identifier;
            }));
            model->updateMaterial(WTF::move(descriptors));
        });
    } processRemovalsCallback:^(WKBridgeRemovals *removals) {
        dispatch(removals, [](WebModelPlayer& player, HostedModel& hosted, WKBridgeRemovals *removals) {
            auto meshRemovals = convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.meshRemovals);
            auto materialRemovals = convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.materialRemovals);
            auto textureRemovals = convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.textureRemovals);

            forgetResourceIDs(hosted.meshIDs, meshRemovals);
            forgetResourceIDs(hosted.materialIDs, materialRemovals);
            forgetResourceIDs(hosted.textureIDs, textureRemovals);

            if (RefPtr model = player.m_currentModel) {
                model->processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), [](bool) {
                });
            }
        });
    }];
}

void WebModelPlayer::load(WebCore::NodeIdentifier nodeID, WebCore::Model& modelSource, WebCore::LayoutSize size, bool)
{
    if (CheckedPtr hosted = hostedModel(nodeID))
        hosted->animationStateToRestore = std::nullopt;

    loadHostedModel(nodeID, modelSource, size);
}

void WebModelPlayer::loadHostedModel(WebCore::NodeIdentifier nodeID, WebCore::Model& modelSource, WebCore::LayoutSize size)
{
    if (!ensureModelBacking(size))
        return;

    auto& hosted = ensureHostedModel(nodeID);
    removeHostedModelResources(nodeID, hosted);
    hosted.didFinishLoading = false;
    hosted.pendingClientFinishLoadingNotification = false;
    hosted.pauseState = HostedModel::PauseState::None;

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    hosted.cachedModelSource = &modelSource;
#endif

    hosted.loader = adoptNS([allocWKBridgeModelLoaderInstance() initWithGPUFamily:MTLGPUFamilyApple7]);
    setLoaderCallbacks(nodeID, hosted.loader.get());

#if ENABLE(SPATIAL_PORTAL)
    if (hosted.childTransform || hosted.anchorNode)
        pushPortalChildTransform(nodeID);
#endif

    hosted.retainedData = modelSource.data()->createNSData();
    Ref protectedThis { *this };
    if ([hosted.loader loadModel:hosted.retainedData.get() mimeType:modelSource.mimeType().createNSString().get()]) {
#if ENABLE(SPATIAL_PORTAL)
        for (auto& dependent : m_hostedModels.values()) {
            if (dependent->anchorNode == nodeID)
                dependent->anchorCorrection = std::nullopt;
        }
        pushAnchoredPortalChildTransforms(nodeID);
#endif
        startUpdateLoopIfNeeded();
    } else if (RefPtr client = m_client)
        client->didFailLoading(protectedThis.get(), nodeID, { });
}

void WebModelPlayer::removeHostedModelResources(WebCore::NodeIdentifier nodeID, HostedModel& hosted)
{
    auto meshIDs = copyToVector(std::exchange(hosted.meshIDs, { }).values());
    auto materialIDs = copyToVector(std::exchange(hosted.materialIDs, { }).values());
    auto textureIDs = copyToVector(std::exchange(hosted.textureIDs, { }).values());

    if (RefPtr model = m_currentModel) {
        if (!meshIDs.isEmpty() || !materialIDs.isEmpty() || !textureIDs.isEmpty()) {
            model->processRemovals(WTF::move(meshIDs), WTF::move(materialIDs), WTF::move(textureIDs), [](bool) {
            });
        }
        model->removeModel(nodeID);
    }

    hosted.loader = nil;
    hosted.retainedData = nil;
}

void WebModelPlayer::unload(WebCore::NodeIdentifier nodeID)
{
    auto hosted = m_hostedModels.take(nodeID);
    if (!hosted)
        return;

    removeHostedModelResources(nodeID, *hosted);

    if (m_hostedModels.isEmpty()) {
        releaseModelResources();
        return;
    }

#if ENABLE(SPATIAL_PORTAL)
    pushAnchoredPortalChildTransforms(nodeID);
#endif

    notifyEntityTransformUpdated();
    startUpdateLoopIfNeeded();
}

void WebModelPlayer::notifyEntityTransformUpdated()
{
#if ENABLE(SPATIAL_PORTAL)
    if (isSpatialPortal()) {
        m_needsEntityTransformNotification = false;
        notifyPortalTransformUpdated();
        return;
    }
#endif

    RefPtr model = m_currentModel;
    RefPtr client { m_client };
    if (!model || !client || !model->entityTransform())
        return;

    m_needsEntityTransformNotification = false;
    auto transform = WebCore::TransformationMatrix(static_cast<simd_float4x4>(*model->entityTransform()));
    for (auto nodeID : copyToVector(m_hostedModels.keys()))
        client->didUpdateEntityTransform(*this, nodeID, transform);
}

void WebModelPlayer::sizeDidChange(WebCore::LayoutSize size)
{
    RefPtr currentModel = m_currentModel;
    if (!currentModel)
        return;

    RefPtr corePage { m_page };
    if (!corePage)
        return;
    RefPtr document = corePage->localTopDocument();
    if (!document)
        return;

    auto cssSize = size;
    size.scale(document->deviceScaleFactor());
    auto newPixelSize = WebCore::IntSize(size.width().toUnsigned(), size.height().toUnsigned());
    if (newPixelSize == m_currentPixelSize)
        return;

    m_currentPixelSize = newPixelSize;
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    m_lastLayoutSize = cssSize;
#endif

    currentModel->sizeDidChange(newPixelSize.width(), newPixelSize.height(), [protectedThis = protect(*this)](Vector<MachSendRight>&& newBuffers) {
        if (newBuffers.isEmpty())
            return;

        protectedThis->m_displayBuffers = WTF::move(newBuffers);
        protectedThis->m_renderTextureIndex = 0;
        protectedThis->m_displayTextureIndex = 0;
        protectedThis->m_hasRenderedFrame = false;
        if (protectedThis->m_contentsDisplayDelegate)
            protect(protectedThis->m_contentsDisplayDelegate)->setDisplayBuffer(*protectedThis->displayBuffer());
        protectedThis->startUpdateLoopIfNeeded();
    });

    if (RefPtr model = m_currentModel)
        model->setViewportSize(cssSize.width().toFloat(), cssSize.height().toFloat());
    notifyEntityTransformUpdated();
}

void WebModelPlayer::enterFullscreen()
{
}

void WebModelPlayer::handleMouseDown(const WebCore::LayoutPoint& startingPoint, MonotonicTime)
{
    m_initialPoint = startingPoint;
    if (!m_orbitSimulator) {
        lazyInitialize(m_orbitSimulator, adoptNS([[WKStageModeOrbitSimulator alloc] init]));
        // Seed from the current pose so a gesture after reload doesn't snap to default.
        if (RefPtr model = m_currentModel) {
            if (auto transform = model->entityTransform()) {
                auto matrix = static_cast<simd_float4x4>(*transform);
                simd_float3 c0 = simd_normalize(simd_make_float3(matrix.columns[0]));
                simd_float3 c1 = simd_normalize(simd_make_float3(matrix.columns[1]));
                simd_float3 c2 = simd_normalize(simd_make_float3(matrix.columns[2]));
                [m_orbitSimulator setCurrentYaw:std::atan2(-c2.x, c0.x) pitch:std::atan2(-c1.z, c1.y)];
            }
        }
    }
    [m_orbitSimulator gestureDidBegin];
    startUpdateLoopIfNeeded();
}

void WebModelPlayer::handleMouseMove(const WebCore::LayoutPoint& currentPoint, MonotonicTime)
{
    if (!m_initialPoint)
        return;

    static constexpr float kDefaultDragToRotationMultiplier = 0.005;
    static float kDragToRotationMultiplier = [] {
        auto factor = [[NSUserDefaults standardUserDefaults] floatForKey:@"WebKitModelDragToRotationMultiplier"];
        return kDefaultDragToRotationMultiplier * (factor > 0 ? factor : 1.0f);
    }();

    float totalDeltaX = static_cast<float>(m_initialPoint->x() - currentPoint.x()) * kDragToRotationMultiplier;
    float totalDeltaY = static_cast<float>(currentPoint.y() - m_initialPoint->y()) * kDragToRotationMultiplier;

    RetainPtr orbitSimulator = m_orbitSimulator;
    if (!orbitSimulator)
        return;

    [orbitSimulator gestureDidUpdateWithDeltaX:totalDeltaX deltaY:totalDeltaY];
    startUpdateLoopIfNeeded();
}

bool WebModelPlayer::supportsMouseInteraction()
{
    return true;
}

void WebModelPlayer::handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime)
{
    m_initialPoint = std::nullopt;
    if (RetainPtr orbitSimulator = m_orbitSimulator) {
        [orbitSimulator gestureDidEnd];
        startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::getCamera(CompletionHandler<void(std::optional<WebCore::HTMLModelElementCamera>&&)>&&)
{
}

void WebModelPlayer::setCamera(WebCore::HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
{
}

void WebModelPlayer::isPlayingAnimation(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void WebModelPlayer::setAnimationIsPlaying(WebCore::NodeIdentifier, bool, CompletionHandler<void(bool success)>&&)
{
}

void WebModelPlayer::isLoopingAnimation(WebCore::NodeIdentifier nodeID, CompletionHandler<void(std::optional<bool>&&)>&& completion)
{
    CheckedPtr hosted = hostedModel(nodeID);
    completion(hosted ? std::optional { hosted->isLooping } : std::nullopt);
}

void WebModelPlayer::setIsLoopingAnimation(WebCore::NodeIdentifier nodeID, bool shouldLoop, CompletionHandler<void(bool success)>&& completion)
{
    setLoop(nodeID, shouldLoop);
    completion(shouldLoop);
}

void WebModelPlayer::animationDuration(WebCore::NodeIdentifier nodeID, CompletionHandler<void(std::optional<Seconds>&&)>&& completion)
{
    completion(Seconds(duration(nodeID)));
}

void WebModelPlayer::animationCurrentTime(WebCore::NodeIdentifier, CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void WebModelPlayer::setAnimationCurrentTime(WebCore::NodeIdentifier, Seconds, CompletionHandler<void(bool success)>&&)
{
}

void WebModelPlayer::updateScene()
{
}

WebCore::ModelPlayerAccessibilityChildren WebModelPlayer::accessibilityChildren()
{
    return { };
}

WebCore::ModelPlayerIdentifier WebModelPlayer::identifier() const
{
    return m_id;
}

bool WebModelPlayer::isPlaceholder() const
{
    return !m_currentModel;
}

void WebModelPlayer::configureGraphicsLayer(WebCore::GraphicsLayer& graphicsLayer, WebCore::ModelPlayerGraphicsLayerConfiguration&& configuration)
{
    m_graphicsLayer = graphicsLayer;
    graphicsLayer.setContentsDisplayDelegate(contentsDisplayDelegate(), WebCore::GraphicsLayer::ContentsLayerPurpose::Canvas);
}

void WebModelPlayer::adoptContentsDisplayDelegateFrom(WebCore::ModelPlayer& previousPlayer)
{
    auto* webPrevious = dynamicDowncast<WebModelPlayer>(previousPlayer);
    if (!webPrevious)
        return;

    RefPtr delegate = std::exchange(webPrevious->m_contentsDisplayDelegate, nullptr);
    if (!delegate)
        return;

    delegate->setModelPlayer(*this);
    m_contentsDisplayDelegate = WTF::move(delegate);
}

const MachSendRight* WebModelPlayer::displayBuffer() const
{
    if (m_displayTextureIndex >= m_displayBuffers.size())
        return nullptr;

    return &m_displayBuffers[m_displayTextureIndex];
}

RefPtr<WebCore::ImageBuffer> WebModelPlayer::snapshotCurrentFrame(const WebCore::FloatSize& deviceSize, const WebCore::ColorSpace& colorSpace)
{
    RefPtr currentModel { m_currentModel };
    if (!currentModel || !m_hasRenderedFrame || m_displayTextureIndex >= m_displayBuffers.size())
        return nullptr;

    RefPtr corePage { m_page };
    if (!corePage)
        return nullptr;

    RefPtr imageBuffer { WebCore::ImageBuffer::create(deviceSize, WebCore::RenderingMode::Accelerated, WebCore::RenderingPurpose::Snapshot, 1, colorSpace, WebCore::PixelFormat::BGRA8, &corePage->chrome()) };
    if (!imageBuffer)
        return nullptr;

    imageBuffer->flushDrawingContext();
    currentModel->paintCurrentFrameToImageBuffer(imageBuffer->renderingResourceIdentifier(), m_displayTextureIndex);
    return imageBuffer;
}

WebCore::GraphicsLayerContentsDisplayDelegate* WebModelPlayer::contentsDisplayDelegate()
{
    if (auto buffer = displayBuffer(); !m_contentsDisplayDelegate && buffer) {
        RefPtr modelDisplayDelegate = ModelDisplayBufferDisplayDelegate::create(*this);
        m_contentsDisplayDelegate = modelDisplayDelegate;
        modelDisplayDelegate->setDisplayBuffer(*buffer);
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
        if (m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard())
            modelDisplayDelegate->setContentsFormat(WebCore::ContentsFormat::RGBA8);
#endif
    }

    return m_contentsDisplayDelegate.get();
}

bool WebModelPlayer::simulate(float elapsedTime)
{
    RefPtr model = m_currentModel;
    if (!model)
        return false;

    RetainPtr orbitSimulator = m_orbitSimulator;
    if (!orbitSimulator)
        return false;

    bool isGestureActive = m_initialPoint.has_value();
    bool inertiaActive = [orbitSimulator stepWithElapsedTime:elapsedTime];
    if (isGestureActive || inertiaActive) {
        float yaw = [orbitSimulator currentYaw];
        float pitch = [orbitSimulator currentPitch];
        model->setRotation(yaw, pitch);
        m_needsEntityTransformNotification = true;
        return true;
    }
    return false;
}

void WebModelPlayer::setPlaybackRate(WebCore::NodeIdentifier nodeID, double newRate, CompletionHandler<void(double effectivePlaybackRate)>&& completion)
{
    auto& hosted = ensureHostedModel(nodeID);
    hosted.playbackRate = newRate;
    if (auto& animationStateToRestore = hosted.animationStateToRestore; animationStateToRestore && animationStateToRestore->effectivePlaybackRate() != newRate) {
        animationStateToRestore->setCurrentTime(animationStateToRestore->currentTime(), MonotonicTime::now());
        animationStateToRestore->setPlaybackRate(newRate);
    }
    startUpdateLoopIfNeeded();
    completion(newRate);
}

void WebModelPlayer::startUpdateLoopIfNeeded()
{
    if (m_isUpdateLoopRunning)
        return;
    m_isUpdateLoopRunning = true;
    scheduleUpdateIfNeeded();
}

void WebModelPlayer::scheduleUpdateIfNeeded()
{
    if (!m_isUpdateLoopRunning || m_isUpdateScheduled)
        return;

    RefPtr corePage { m_page };
    if (!corePage)
        return;

    RefPtr document = corePage->localTopDocument();
    if (!document)
        return;

    m_isUpdateScheduled = true;
    protect(document->eventLoop())->queueTask(WebCore::TaskSource::ModelElement, [protectedThis = protect(*this)] {
        protectedThis->m_isUpdateScheduled = false;
        protectedThis->update();
    });
}

void WebModelPlayer::update()
{
    if (m_hostedModels.isEmpty())
        return;

    if (!m_isUpdateLoopRunning || m_isUpdating)
        return;

    m_isUpdating = true;

    auto now = MonotonicTime::now();
    float elapsed = m_lastUpdateTime ? static_cast<float>((now - m_lastUpdateTime).seconds()) : (1.f / 60.f);
    float elapsedTime = std::clamp(elapsed, 1.f / 120.f, 1.f / 15.f);
    m_lastUpdateTime = now;

    bool stageModeActive = simulate(elapsedTime);
    bool everyChildIsAtRest = true;
    bool didReachEnd = false;

    for (auto nodeID : copyToVector(m_hostedModels.keys())) {
        CheckedPtr hosted = hostedModel(nodeID);
        if (!hosted || !hosted->loader)
            continue;

        bool wasPlaying = hosted->isPlaying();
        RetainPtr loader = hosted->loader;
        float timeDelta = wasPlaying ? static_cast<float>(hosted->playbackRate) * elapsedTime : 0.f;
        hosted = nullptr;

        [loader update:timeDelta];

        hosted = hostedModel(nodeID);
        if (!hosted)
            continue;

#if ENABLE(SPATIAL_PORTAL)
        if (wasPlaying)
            pushAnchoredPortalChildTransforms(nodeID);
#endif

        if (wasPlaying && hosted->didFinishLoading && !hosted->isLooping) {
            double currentTime = [loader currentTime];
            bool reachedEnd = hosted->playbackRate < 0 ? currentTime <= 0 : currentTime >= [loader duration];
            if (reachedEnd) {
                hosted->pauseState = HostedModel::PauseState::Paused;
                didReachEnd = true;
            }
        }

        if (hosted->isPlaying())
            everyChildIsAtRest = false;
    }

    if (didReachEnd)
        updateMeshPlayState();

    if (!render())
        m_isUpdating = false;

    if (everyChildIsAtRest && !stageModeActive)
        m_isUpdateLoopRunning = false;

    if (m_needsEntityTransformNotification)
        notifyEntityTransformUpdated();
}

bool WebModelPlayer::hasAnyLoadedChild() const
{
    return std::ranges::any_of(m_hostedModels.values(), [](auto& hosted) {
        return hosted->didFinishLoading;
    });
}

bool WebModelPlayer::render()
{
    if (!hasAnyLoadedChild())
        return false;

    RefPtr currentModel = m_currentModel;
    if (!currentModel)
        return false;

    uint32_t textureIndex = m_renderTextureIndex;
    if (++m_renderTextureIndex >= m_displayBuffers.size())
        m_renderTextureIndex = 0;

    currentModel->render(textureIndex, [protectedThis = protect(*this), textureIndex] (bool result) mutable {
        protectedThis->ensureOnMainThreadWithProtectedThis([result, textureIndex] (Ref<WebModelPlayer> protectedThis) {
            protectedThis->m_isUpdating = false;
            if (!result)
                return;

            protectedThis->m_displayTextureIndex = textureIndex;
            protectedThis->m_hasRenderedFrame = true;
            if (auto* machSendRight = protectedThis->displayBuffer(); machSendRight && protectedThis->contentsDisplayDelegate()) {
                Ref delegate = *protectedThis->m_contentsDisplayDelegate;
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
                // Apply the contents format together with the new display buffer so the
                // layer's format always matches the IOSurface being shown. This is what
                // completes the deferred format swap when the model is reloaded due to
                // a dynamic-range-limit change.
                delegate->setContentsFormat(contentsFormatForDynamicRange(protectedThis->m_usingStandardDynamicRange));
#endif
                delegate->setDisplayBuffer(*machSendRight);
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
                protectedThis->updateScreenHeadroomFromPage();
#endif
            }

            protectedThis->notifyClientDidFinishLoading();
            protectedThis->scheduleDisplayUpdate();
        });
    });

    return true;
}

void WebModelPlayer::scheduleDisplayUpdate()
{
    if (RefPtr graphicsLayer = m_graphicsLayer)
        graphicsLayer->setContentsNeedsDisplay();
}

void WebModelPlayer::notifyClientDidFinishLoading()
{
    RefPtr client { m_client };
    if (!client)
        return;

    Ref protectedThis { *this };
    bool notifiedAnyone = false;

    for (auto nodeID : copyToVector(m_hostedModels.keys())) {
        {
            CheckedPtr hosted = hostedModel(nodeID);
            if (!hosted || !hosted->pendingClientFinishLoadingNotification)
                continue;

            hosted->pendingClientFinishLoadingNotification = false;
        }
        notifiedAnyone = true;

        client->didFinishLoading(protectedThis, nodeID);

        if (m_currentModel) {
            auto [center, extents] = boundingBoxCenterAndExtents(nodeID);
            client->didUpdateBoundingBox(protectedThis, nodeID, center, extents);
        }
    }

    if (!notifiedAnyone)
        return;

    notifyEntityTransformUpdated();

    if (!m_environmentMapPushedToCurrentModel)
        pushEnvironmentMapToModel();
}

bool WebModelPlayer::supportsTransform(WebCore::TransformationMatrix transformationMatrix)
{
    if (m_stageMode != WebCore::StageModeOperation::None)
        return false;

    return RemoteMeshProxy::supportsTransform(transformationMatrix);
}

void WebModelPlayer::play(WebCore::NodeIdentifier nodeID, HostedModel& hosted, bool playing)
{
    if (!m_currentModel)
        return;

    if (playing && !hosted.isLooping && currentTime(nodeID) >= Seconds(duration(nodeID)))
        [hosted.loader setCurrentTime:0];

    hosted.pauseState = playing ? HostedModel::PauseState::Playing : HostedModel::PauseState::Paused;
    updateMeshPlayState();

    if (playing)
        startUpdateLoopIfNeeded();
}

void WebModelPlayer::updateMeshPlayState()
{
    RefPtr model = m_currentModel;
    if (!model)
        return;

    model->play(std::ranges::any_of(m_hostedModels.values(), [](auto& hosted) {
        return hosted->isPlaying();
    }));
}

void WebModelPlayer::setLoop(WebCore::NodeIdentifier nodeID, bool loop)
{
    auto& hosted = ensureHostedModel(nodeID);
    if (hosted.isLooping == loop)
        return;

    hosted.isLooping = loop;
    if (hosted.animationStateToRestore)
        hosted.animationStateToRestore->setLoop(loop);
    [hosted.loader setLoop:loop];
    startUpdateLoopIfNeeded();
}

void WebModelPlayer::setAutoplay(WebCore::NodeIdentifier nodeID, bool autoplay)
{
    auto& hosted = ensureHostedModel(nodeID);
    hosted.autoplay = autoplay;
    if (hosted.animationStateToRestore)
        hosted.animationStateToRestore->setAutoplay(autoplay);
}

void WebModelPlayer::setPaused(WebCore::NodeIdentifier nodeID, bool paused, CompletionHandler<void(bool succeeded)>&& completion)
{
    CheckedPtr hosted = hostedModel(nodeID);
    if (!hosted)
        return completion(false);

    if (auto& animationStateToRestore = hosted->animationStateToRestore) {
        if (animationStateToRestore->paused() != paused) {
            animationStateToRestore->setCurrentTime(animationStateToRestore->currentTime(), MonotonicTime::now());
            animationStateToRestore->setPaused(paused);
        }
    } else
        play(nodeID, *hosted, !paused);

    completion(m_currentModel && (paused || duration(nodeID) > 0));
}

bool WebModelPlayer::paused(WebCore::NodeIdentifier nodeID) const
{
    CheckedPtr hosted = hostedModel(nodeID);
    if (!hosted)
        return true;
    if (hosted->animationStateToRestore)
        return hosted->animationStateToRestore->paused();
    return !hosted->isPlaying();
}

Seconds WebModelPlayer::currentTime(WebCore::NodeIdentifier nodeID) const
{
    CheckedPtr hosted = hostedModel(nodeID);
    if (!hosted)
        return 0_s;
    if (hosted->animationStateToRestore)
        return hosted->animationStateToRestore->currentTime();
    if (!hosted->loader)
        return 0_s;
    return Seconds([hosted->loader currentTime]);
}

void WebModelPlayer::setCurrentTime(WebCore::NodeIdentifier nodeID, Seconds currentTime, CompletionHandler<void()>&& completion)
{
    double clamped = std::clamp(currentTime.seconds(), 0.0, duration(nodeID));
    if (CheckedPtr hosted = hostedModel(nodeID)) {
        if (hosted->animationStateToRestore)
            hosted->animationStateToRestore->setCurrentTime(Seconds(clamped), MonotonicTime::now());
        else {
            [hosted->loader setCurrentTime:clamped];
#if ENABLE(SPATIAL_PORTAL)
            pushAnchoredPortalChildTransforms(nodeID);
#endif
        }
    }
    startUpdateLoopIfNeeded();
    completion();
}

std::optional<WebCore::TransformationMatrix> WebModelPlayer::entityTransform(WebCore::NodeIdentifier nodeID) const
{
#if ENABLE(SPATIAL_PORTAL)
    if (isSpatialPortal()) {
        CheckedPtr hosted = hostedModel(nodeID);
        return hosted ? hosted->childTransform : std::nullopt;
    }
#else
    UNUSED_PARAM(nodeID);
#endif

#if PLATFORM(COCOA)
    if (RefPtr model = m_currentModel) {
        if (auto transform = model->entityTransform())
            return static_cast<simd_float4x4>(*transform);
    }
#endif
    return std::nullopt;
}

void WebModelPlayer::setStageMode(WebCore::StageModeOperation stageMode)
{
    m_stageMode = stageMode;
    if (RefPtr model = m_currentModel) {
        model->setStageMode(effectiveStageMode());
        notifyEntityTransformUpdated();
        startUpdateLoopIfNeeded();
    }
}

WebCore::StageModeOperation WebModelPlayer::effectiveStageMode() const
{
#if ENABLE(SPATIAL_PORTAL)
    if (m_portalAction == WebCore::PortalActionKind::Orbit)
        return WebCore::StageModeOperation::Orbit;
#endif
    return m_stageMode;
}

#if ENABLE(SPATIAL_PORTAL)
void WebModelPlayer::setPortalTransform(const WebCore::UsedPortalTransform& portalTransform)
{
    m_portalTransform = portalTransform;

    if (RefPtr model = m_currentModel) {
        model->setPortalTransform(portalTransform);
        notifyPortalTransformUpdated();
        startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::setPortalAction(WebCore::PortalActionKind kind)
{
    if (m_portalAction == kind)
        return;

    m_portalAction = kind;

    if (m_portalAction == WebCore::PortalActionKind::None) {
        [m_orbitSimulator reset];
        m_initialPoint = std::nullopt;
    }

    if (RefPtr model = m_currentModel) {
        model->setStageMode(effectiveStageMode());
        notifyPortalTransformUpdated();
        startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::notifyPortalTransformUpdated()
{
    RefPtr model = m_currentModel;
    RefPtr client = m_client;
    if (!model || !client)
        return;

    if (auto transform = model->resolvedPortalTransform())
        client->didUpdatePortalTransform(*this, WebCore::TransformationMatrix(static_cast<simd_float4x4>(*transform)));
}

void WebModelPlayer::setAnchor(WebCore::NodeIdentifier nodeID, std::optional<WebCore::NodeIdentifier> anchorNode, const String& placement)
{
    auto& hosted = ensureHostedModel(nodeID);
    if (hosted.anchorNode == anchorNode && hosted.anchorPlacementName == placement)
        return;

    hosted.anchorNode = anchorNode;
    hosted.anchorPlacementName = placement;
    hosted.anchorCorrection = std::nullopt;

    if (!m_currentModel)
        return;

    pushPortalChildTransform(nodeID);
    pushAnchoredPortalChildTransforms(nodeID);
    startUpdateLoopIfNeeded();
}

WebCore::TransformationMatrix WebModelPlayer::HostedModel::anchorPlacementTransform(const HostedModel& anchor)
{
    RetainPtr anchorLoader = anchor.loader;
    if (!anchorLoader || anchorPlacementName.isEmpty())
        return { };

    static constexpr float cssPixelsPerMeter = WebCore::CSS::pixelsPerCm * 100;
    simd_float4x4 placementMatrix = [anchorLoader anchorPlacementNamed:anchorPlacementName.createNSString().get()];
    placementMatrix.columns[3] = simd_make_float4(simd_make_float3(placementMatrix.columns[3]) * cssPixelsPerMeter, 1);

    WebCore::TransformationMatrix placement { placementMatrix };
    WebCore::TransformationMatrix::Decomposed4Type decomposition;
    if (!placement.decompose4(decomposition))
        return { };

    if (!anchorCorrection) {
        auto& rotation = decomposition.quaternion;
        anchorCorrection = WebCore::TransformationMatrix::fromQuaternion({ -rotation.x, -rotation.y, -rotation.z, rotation.w });
    }

    return placement * (*anchorCorrection);
}

static WebCore::TransformationMatrix childTransformInAnchorFrame(WebCore::TransformationMatrix childTransform, const WebCore::TransformationMatrix& anchorFrame)
{
    WebCore::TransformationMatrix::Decomposed4Type decomposition;
    if (!anchorFrame.decompose4(decomposition))
        return childTransform;

    double scale = std::max({ std::abs(decomposition.scaleX), std::abs(decomposition.scaleY), std::abs(decomposition.scaleZ) });
    if (scale <= std::numeric_limits<float>::epsilon())
        return childTransform;

    childTransform.setM41(childTransform.m41() / scale);
    childTransform.setM42(childTransform.m42() / scale);
    childTransform.setM43(childTransform.m43() / scale);
    return childTransform;
}

void WebModelPlayer::pushPortalChildTransform(WebCore::NodeIdentifier nodeID)
{
    RefPtr model = m_currentModel;
    CheckedPtr hosted = hostedModel(nodeID);
    if (!model || !hosted || !isSpatialPortal())
        return;

    auto resolvedAnchor = [&](const HostedModel& model) -> CheckedPtr<HostedModel> {
        CheckedPtr anchor = model.anchorNode ? hostedModel(*model.anchorNode) : nullptr;
        return anchor && anchor->loader ? anchor : nullptr;
    };

    Vector<CheckedPtr<HostedModel>> chain { hosted };
    for (CheckedPtr anchor = resolvedAnchor(*hosted); anchor; anchor = resolvedAnchor(*anchor)) {
        if (chain.size() >= m_hostedModels.size()) {
            RELEASE_LOG_ERROR(ModelElement, "%p - WebModelPlayer::pushPortalChildTransform: refusing to anchor nodeID=%" PRIu64 " because its anchor chain leads back to it", this, nodeID.toUInt64());
            chain = { hosted };
            break;
        }
        chain.append(anchor);
    }

    WebCore::TransformationMatrix transform;
    for (size_t index = chain.size(); index--;) {
        CheckedPtr current = chain[index];
        if (index + 1 < chain.size())
            transform = transform * current->anchorPlacementTransform(*chain[index + 1]);
        transform = transform * childTransformInAnchorFrame(current->childTransform.value_or(WebCore::TransformationMatrix { }), transform);
    }
    model->setPortalChildTransform(nodeID, static_cast<simd_float4x4>(transform));
}

bool WebModelPlayer::isAnchoredTo(const HostedModel& hosted, WebCore::NodeIdentifier ancestorNode) const
{
    auto anchorNode = hosted.anchorNode;
    for (size_t step = 0; anchorNode && step < m_hostedModels.size(); ++step) {
        if (*anchorNode == ancestorNode)
            return true;
        CheckedPtr anchor = hostedModel(*anchorNode);
        anchorNode = anchor ? anchor->anchorNode : std::nullopt;
    }
    return false;
}

void WebModelPlayer::pushAnchoredPortalChildTransforms(WebCore::NodeIdentifier anchorNode)
{
    for (auto& [nodeID, hosted] : m_hostedModels) {
        if (isAnchoredTo(hosted.get(), anchorNode))
            pushPortalChildTransform(nodeID);
    }
}
#endif

void WebModelPlayer::setEntityTransform(WebCore::NodeIdentifier nodeID, WebCore::TransformationMatrix transform)
{
#if ENABLE(SPATIAL_PORTAL)
    if (isSpatialPortal()) {
        ensureHostedModel(nodeID).childTransform = transform;

        if (m_currentModel) {
            pushPortalChildTransform(nodeID);
            pushAnchoredPortalChildTransforms(nodeID);
            startUpdateLoopIfNeeded();
        }
        return;
    }
#else
    UNUSED_PARAM(nodeID);
#endif

    if (RefPtr model = m_currentModel) {
        model->setEntityTransform(static_cast<simd_float4x4>(transform));
        notifyEntityTransformUpdated();
        startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data, const URL& sourceURL)
{
    m_environmentMapKind = WebCore::EnvironmentMapKind::Custom;
    m_environmentMapURL = sourceURL;
    m_environmentMap = WTF::move(data);
    m_environmentMapPushedToCurrentModel = false;
    pushEnvironmentMapToModel();
}

#if ENABLE(SPATIAL_PORTAL)
void WebModelPlayer::disableEnvironmentMap()
{
    if (m_environmentMapKind == WebCore::EnvironmentMapKind::None)
        return;

    m_environmentMapKind = WebCore::EnvironmentMapKind::None;
    m_environmentMapURL = { };
    m_environmentMap = std::nullopt;
    m_environmentMapPushedToCurrentModel = false;
    pushEnvironmentMapToModel();
}

void WebModelPlayer::enableSystemEnvironmentMap()
{
    if (m_environmentMapKind == WebCore::EnvironmentMapKind::Default)
        return;

    m_environmentMapKind = WebCore::EnvironmentMapKind::Default;
    m_environmentMapURL = { };
    m_environmentMap = std::nullopt;
    m_environmentMapPushedToCurrentModel = false;
    pushEnvironmentMapToModel();
}
#endif

String WebModelPlayer::environmentMapForTesting() const
{
    switch (m_environmentMapKind) {
    case WebCore::EnvironmentMapKind::None:
        return "none"_s;
    case WebCore::EnvironmentMapKind::Default:
        return "auto"_s;
    case WebCore::EnvironmentMapKind::Custom:
        return m_environmentMapURL.string();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void WebModelPlayer::pushEnvironmentMapToModel()
{
    RefPtr currentModel = m_currentModel;
    if (!currentModel)
        return;

#if ENABLE(SPATIAL_PORTAL)
    switch (m_environmentMapKind) {
    case WebCore::EnvironmentMapKind::None:
        currentModel->removeEnvironmentMap();
        m_environmentMapPushedToCurrentModel = true;
        startUpdateLoopIfNeeded();
        return;

    case WebCore::EnvironmentMapKind::Default:
        currentModel->setDefaultEnvironmentMap();
        m_environmentMapPushedToCurrentModel = true;
        startUpdateLoopIfNeeded();
        return;

    case WebCore::EnvironmentMapKind::Custom:
        break;
    }
#else
    if (m_environmentMapKind != WebCore::EnvironmentMapKind::Custom)
        return;
#endif

    auto data = m_environmentMap;
    if (!data)
        return;

    RetainPtr<WKBridgeModelLoader> loader;
    for (auto& hosted : m_hostedModels.values()) {
        if (hosted->didFinishLoading && hosted->loader) {
            loader = hosted->loader;
            break;
        }
    }

    if (!loader)
        return;

    bool success = false;
    if (auto environmentMap = [loader loadEnvironmentMap:(*data)->createNSData().get()]) {
        currentModel->setEnvironmentMap(convert(environmentMap));
        success = true;
    }
    m_environmentMapPushedToCurrentModel = true;
    startUpdateLoopIfNeeded();

    if (RefPtr client = m_client)
        client->didFinishEnvironmentMapLoading(*this, success);
}

static bool disableReloading()
{
    static bool disableReloading = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitDisableModelPlayerReloading"];
    return disableReloading;
}

void WebModelPlayer::releaseModelResources()
{
    m_currentModel = nullptr;
    for (auto& hosted : m_hostedModels.values()) {
        hosted->loader = nil;
        hosted->retainedData = nil;
        hosted->didFinishLoading = false;
        hosted->pendingClientFinishLoadingNotification = false;
        hosted->meshIDs.clear();
        hosted->materialIDs.clear();
        hosted->textureIDs.clear();
    }
    m_displayBuffers.clear();
    m_environmentMapPushedToCurrentModel = false;
    m_isUpdateLoopRunning = false;
    m_isUpdateScheduled = false;
    m_isUpdating = false;
    m_displayTextureIndex = 0;
    m_hasRenderedFrame = false;
}

void WebModelPlayer::visibilityStateDidChange()
{
    RefPtr client { m_client };
    if (!client || disableReloading() || client->isVisible())
        return;

    RefPtr page = m_page;
    if (!page)
        return;

    Ref protectedThis { *this };
    client->didUnload(protectedThis);
}

void WebModelPlayer::reload(WebCore::NodeIdentifier nodeID, WebCore::Model& modelSource, WebCore::LayoutSize size, WebCore::ModelPlayerAnimationState& animationState, std::unique_ptr<WebCore::ModelPlayerTransformState>&& transformState)
{
    if (disableReloading())
        return;

    auto& hosted = ensureHostedModel(nodeID);
    hosted.autoplay = animationState.autoplay();
    hosted.isLooping = animationState.loop();
    if (auto playbackRate = animationState.effectivePlaybackRate())
        hosted.playbackRate = *playbackRate;
    hosted.animationStateToRestore = animationState;

    loadHostedModel(nodeID, modelSource, size);

    if (transformState) {
        setStageMode(transformState->stageMode());
        if (auto entityTransform = transformState->entityTransform())
            setEntityTransform(nodeID, *entityTransform);
    }
}

std::optional<WebCore::ModelPlayerAnimationState> WebModelPlayer::currentAnimationState(WebCore::NodeIdentifier nodeID) const
{
    CheckedPtr hosted = hostedModel(nodeID);
    if (!hosted)
        return std::nullopt;

    if (hosted->animationStateToRestore)
        return hosted->animationStateToRestore;

    if (!m_currentModel)
        return std::nullopt;

    bool paused = !hosted->isPlaying();
    Seconds animationDuration { duration(nodeID) };
    std::optional<double> effectivePlaybackRate = hosted->playbackRate;
    std::optional<Seconds> lastCachedCurrentTime = currentTime(nodeID);
    std::optional<MonotonicTime> lastCachedClockTimestamp = MonotonicTime::now();

    return WebCore::ModelPlayerAnimationState(hosted->autoplay, hosted->isLooping, paused, animationDuration, effectivePlaybackRate, lastCachedCurrentTime, lastCachedClockTimestamp);
}

std::pair<WebCore::FloatPoint3D, WebCore::FloatPoint3D> WebModelPlayer::boundingBoxCenterAndExtents(WebCore::NodeIdentifier nodeID) const
{
    RefPtr model = m_currentModel;
    if (!model)
        return { };

    auto bounds = model->centerAndExtents(nodeID);
    if (!bounds)
        return { };

    auto [simdCenter, simdExtents] = *bounds;
    return { WebCore::FloatPoint3D(simdCenter.x, simdCenter.y, simdCenter.z), WebCore::FloatPoint3D(simdExtents.x, simdExtents.y, simdExtents.z) };
}

std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> WebModelPlayer::currentTransformState(WebCore::NodeIdentifier nodeID) const
{
    if (!hostedModel(nodeID) || !m_currentModel)
        return std::nullopt;

    std::optional<WebCore::TransformationMatrix> transform = entityTransform(nodeID);

    auto [center, extents] = boundingBoxCenterAndExtents(nodeID);

    return ModelProcessModelPlayerTransformState::create(transform, center, extents, false, m_stageMode);
}

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
void WebModelPlayer::updateContentsHeadroom()
{
    constexpr auto visionProHeadroom = 2.f;
    auto contentsHeadroom = std::min(visionProHeadroom, WebCore::headroomForDynamicRangeLimit(m_dynamicRangeLimit, m_currentEDRHeadroom, m_suppressEDR));
    if (fabs(contentsHeadroom - m_lastSentContentsHeadroom) < 0.01f)
        return;
    if (RefPtr model = m_currentModel; model && hasAnyLoadedChild()) {
        m_lastSentContentsHeadroom = contentsHeadroom;
        model->updateContentsHeadroom(contentsHeadroom);
    }
}

void WebModelPlayer::updateScreenHeadroomFromPage()
{
    RefPtr page { m_page };
    if (!page)
        return;

    auto platformScreen = WebCore::PlatformScreen::singleton();
    if (auto data = platformScreen->screenData(page->displayID()))
        updateScreenHeadroom(data->currentEDRHeadroom, data->suppressEDR);
}

void WebModelPlayer::updateScreenHeadroom(float currentEDRHeadroom, bool suppressEDR)
{
    m_currentEDRHeadroom = currentEDRHeadroom;
    m_suppressEDR = suppressEDR;
    updateContentsHeadroom();
}

void WebModelPlayer::setDynamicRangeLimit(WebCore::PlatformDynamicRangeLimit dynamicRangeLimit, float currentEDRHeadroom, bool suppressEDR)
{
    bool limitChanged = m_dynamicRangeLimit != dynamicRangeLimit;
    bool headroomChanged = m_suppressEDR != suppressEDR || m_currentEDRHeadroom != currentEDRHeadroom;

    if (!limitChanged && !headroomChanged)
        return;

    m_dynamicRangeLimit = dynamicRangeLimit;
    m_currentEDRHeadroom = currentEDRHeadroom;
    m_suppressEDR = suppressEDR;

    dynamicRangeLimitDidChange();
    updateContentsHeadroom();
    startUpdateLoopIfNeeded();
}

std::optional<double> WebModelPlayer::getEffectiveDynamicRangeLimitValue() const
{
    return WebCore::effectiveDynamicRangeLimit(m_dynamicRangeLimit, m_suppressEDR).value();
}

void WebModelPlayer::dynamicRangeLimitDidChange()
{
    bool newIsStandard = m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard();
    if (newIsStandard == m_usingStandardDynamicRange)
        return;

    struct PendingReload {
        WebCore::NodeIdentifier nodeID;
        RefPtr<WebCore::Model> modelSource;
        WebCore::ModelPlayerAnimationState animationState;
        std::unique_ptr<WebCore::ModelPlayerTransformState> transformState;
    };

    Vector<PendingReload> pendingReloads;
    for (auto nodeID : copyToVector(m_hostedModels.keys())) {
        CheckedPtr hosted = hostedModel(nodeID);
        if (!hosted || !hosted->cachedModelSource)
            continue;

        auto animationState = currentAnimationState(nodeID);
        if (!animationState)
            continue;

        std::unique_ptr<WebCore::ModelPlayerTransformState> transformState;
        if (auto transformStateOpt = currentTransformState(nodeID))
            transformState = WTF::move(*transformStateOpt);

        pendingReloads.append(PendingReload { nodeID, hosted->cachedModelSource, *animationState, WTF::move(transformState) });
    }

    if (pendingReloads.isEmpty())
        return;

    releaseModelResources();

    for (auto& pending : pendingReloads)
        reload(pending.nodeID, *pending.modelSource, m_lastLayoutSize, pending.animationState, WTF::move(pending.transformState));
}
#endif

}

#endif
