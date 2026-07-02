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
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScreenProperties.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/threads/BinarySemaphore.h>

#import "WebKitSwiftSoftLink.h"

#if PLATFORM(COCOA)
#import <Metal/Metal.h>
#endif

namespace WebKit {

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
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { }, std::nullopt });
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
        m_screenPropertiesChangedObserver = ScreenPropertiesChangedObserver::create([weakThis = ThreadSafeWeakPtr { *this }](WebCore::PlatformDisplayID displayID) {
            RefPtr protectedThis { weakThis };
            if (!protectedThis)
                return;
            auto platformScreen = WebCore::PlatformScreen::singleton();
            if (auto* data = platformScreen->screenData(displayID))
                protectedThis->updateScreenHeadroom(data->currentEDRHeadroom, data->suppressEDR);
        });

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

double WebModelPlayer::duration() const
{
    if (m_cachedAnimationState)
        return m_cachedAnimationState->duration().seconds();
    return [m_modelLoader duration];
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

void WebModelPlayer::load(WebCore::Model& modelSource, WebCore::LayoutSize size, bool)
{
    RefPtr corePage { m_page };
    if (!corePage)
        return;
    m_modelLoader = nil;
    m_didFinishLoading = false;
    m_renderTextureIndex = 0;
    m_displayTextureIndex = 0;
    m_hasRenderedFrame = false;
    m_isUpdateLoopRunning = false;
    RefPtr document = corePage->localTopDocument();
    if (!document)
        return;

    RefPtr window = document->window();
    if (!window)
        return;

    RefPtr gpu = protect(window->navigator())->gpu();
    if (!gpu)
        return;

    auto cssSize = size;
    size.scale(document->deviceScaleFactor());
    m_currentPixelSize = WebCore::IntSize(size.width().toUnsigned(), size.height().toUnsigned());

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    m_cachedModelSource = &modelSource;
    m_lastLayoutSize = cssSize;
#endif

    WEBMODEL_WEB_MODEL_PLAYER_DECLARE_DIFFUSE_AND_SPECULAR_TEXTURES

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    bool standardDynamicRange = m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard();
    m_usingStandardDynamicRange = standardDynamicRange;
#else
    bool standardDynamicRange = false;
#endif

    m_lastSentContentsHeadroom = -1.f;
    m_currentModel = static_cast<RemoteGPUProxy&>(gpu->backing()).createModelBacking(m_currentPixelSize.width(), m_currentPixelSize.height(), WTF::move(diffuseTexture), WTF::move(specularTexture), standardDynamicRange, [protectedThis = protect(*this)] (Vector<MachSendRight>&& surfaceHandles) {
        if (surfaceHandles.size()) {
            protectedThis->m_displayBuffers = WTF::move(surfaceHandles);
            protectedThis->m_renderTextureIndex = 0;
            protectedThis->m_displayTextureIndex = 0;
            protectedThis->m_hasRenderedFrame = false;
            protectedThis->updateScreenHeadroomFromPage();
        }
    });
    if (!m_currentModel)
        return;
    m_currentModel->setViewportSize(cssSize.width().toFloat(), cssSize.height().toFloat());

    m_modelLoader = adoptNS([allocWKBridgeModelLoaderInstance() initWithGPUFamily:MTLGPUFamilyApple7]);
    Ref protectedThis { *this };
    [m_modelLoader setCallbacksWithModelUpdatedCallback:^(NSArray<WKBridgeUpdateMesh *> *updateRequest) {
        ensureOnMainThreadWithProtectedThis([updateRequest] (Ref<WebModelPlayer> protectedThis) {
            RefPtr model = protectedThis->m_currentModel;
            if (model) {
                model->update(makeVector(updateRequest, [](WKBridgeUpdateMesh *update) {
                    return std::optional { convert(update) };
                }));
                protectedThis->setStageMode(protectedThis->m_stageMode);
            }

            [protectedThis->m_modelLoader requestCompleted:updateRequest];

            if (!model)
                return;

            if (RefPtr client = protectedThis->m_client; client && !protectedThis->m_didFinishLoading) {
                protectedThis->m_didFinishLoading = true;
                [protectedThis->m_modelLoader setLoop:protectedThis->m_isLooping];
                protectedThis->m_cachedAnimationState = protectedThis->currentAnimationState();

                client->didFinishLoading(protectedThis.get());
                auto [center, extents] = protectedThis->boundingBoxCenterAndExtents();
                client->didUpdateBoundingBox(protectedThis.get(), center, extents);
                protectedThis->notifyEntityTransformUpdated();

                if (auto environmentMap = protectedThis->m_environmentMap)
                    protectedThis->setEnvironmentMap(WTF::move(*environmentMap));
            }
            protectedThis->startUpdateLoopIfNeeded();
        });
    } textureUpdatedCallback:^(NSArray<WKBridgeUpdateTexture *> *updateTexture) {
        ensureOnMainThreadWithProtectedThis([updateTexture] (Ref<WebModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateTexture(makeVector(updateTexture, [](WKBridgeUpdateTexture *update) {
                    return std::optional { convert(update) };
                }));

            [protectedThis->m_modelLoader requestCompleted:updateTexture];
            protectedThis->startUpdateLoopIfNeeded();
        });
    } materialUpdatedCallback:^(NSArray<WKBridgeUpdateMaterial *> *updateMaterial) {
        ensureOnMainThreadWithProtectedThis([updateMaterial] (Ref<WebModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateMaterial(makeVector(updateMaterial, [](WKBridgeUpdateMaterial *update) {
                    return std::optional { convert(update) };
                }));

            [protectedThis->m_modelLoader requestCompleted:updateMaterial];
            protectedThis->startUpdateLoopIfNeeded();
        });
    } processRemovalsCallback:^(WKBridgeRemovals *removals) {
        ensureOnMainThreadWithProtectedThis([removals] (Ref<WebModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel) {
                protectedThis->m_currentModel->processRemovals(convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.meshRemovals), convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.materialRemovals), convert<WKBridgeTypedResourceId, WebModel::TypedResourceId>(removals.textureRemovals), [] (bool) {
                });
            }

            [protectedThis->m_modelLoader requestCompleted:removals];
            protectedThis->startUpdateLoopIfNeeded();
        });
    }];

    m_retainedData = modelSource.data()->createNSData();
    if ([m_modelLoader loadModel:m_retainedData.get() mimeType:modelSource.mimeType().createNSString().get()])
        startUpdateLoopIfNeeded();
    else if (RefPtr client = m_client)
        client->didFailLoading(protectedThis.get(), { });
}

void WebModelPlayer::notifyEntityTransformUpdated()
{
    RefPtr model = m_currentModel;
    RefPtr client { m_client };
    if (!model || !client || !model->entityTransform())
        return;

    m_needsEntityTransformNotification = false;
    client->didUpdateEntityTransform(*this, WebCore::TransformationMatrix(static_cast<simd_float4x4>(*model->entityTransform())));
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
        m_orbitSimulator = adoptNS([[WKStageModeOrbitSimulator alloc] init]);
        // Seed from the current pose so a gesture after reload doesn't snap to default.
        if (auto transform = entityTransform()) {
            auto matrix = static_cast<simd_float4x4>(*transform);
            simd_float3 c0 = simd_normalize(simd_make_float3(matrix.columns[0]));
            simd_float3 c1 = simd_normalize(simd_make_float3(matrix.columns[1]));
            simd_float3 c2 = simd_normalize(simd_make_float3(matrix.columns[2]));
            [m_orbitSimulator setCurrentYaw:std::atan2(-c2.x, c0.x) pitch:std::atan2(-c1.z, c1.y)];
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

void WebModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void WebModelPlayer::setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&)
{
}

void WebModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&& completion)
{
    completion(m_isLooping);
}

void WebModelPlayer::setIsLoopingAnimation(bool shouldLoop, CompletionHandler<void(bool success)>&& completion)
{
    m_isLooping = shouldLoop;
    completion(shouldLoop);
}

void WebModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&& completion)
{
    completion(Seconds(duration()));
}

void WebModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void WebModelPlayer::setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&)
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

const MachSendRight* WebModelPlayer::displayBuffer() const
{
    if (m_displayTextureIndex >= m_displayBuffers.size())
        return nullptr;

    return &m_displayBuffers[m_displayTextureIndex];
}

RefPtr<WebCore::ImageBuffer> WebModelPlayer::snapshotCurrentFrame(const WebCore::FloatSize& deviceSize, const WebCore::DestinationColorSpace& colorSpace)
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

void WebModelPlayer::setPlaybackRate(double newRate, CompletionHandler<void(double effectivePlaybackRate)>&& completion)
{
    m_playbackRate = newRate;
    if (m_cachedAnimationState) {
        updateClockTimeOnAnimationState();
        m_cachedAnimationState->setPlaybackRate(newRate);
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
    document->eventLoop().queueTask(WebCore::TaskSource::ModelElement, [protectedThis = protect(*this)] {
        protectedThis->m_isUpdateScheduled = false;
        protectedThis->update();
    });
}

void WebModelPlayer::update()
{
    if (!m_isUpdateLoopRunning || m_isUpdating)
        return;

    m_isUpdating = true;

    auto now = MonotonicTime::now();
    float elapsed = m_lastUpdateTime ? static_cast<float>((now - m_lastUpdateTime).seconds()) : (1.f / 60.f);
    float elapsedTime = std::clamp(elapsed, 1.f / 120.f, 1.f / 15.f);
    m_lastUpdateTime = now;

    bool stageModeActive = simulate(elapsedTime);
    bool isAtRest = paused() && !stageModeActive;

    auto timeDelta = paused() ? 0.f : (m_playbackRate * elapsedTime);

    [m_modelLoader update:timeDelta];
    double currentTime = [m_modelLoader currentTime];
    bool reachedEnd = m_playbackRate < 0 ? currentTime <= 0 : currentTime >= [m_modelLoader duration];
    if (!m_isLooping && !paused() && reachedEnd) {
        m_pauseState = PauseState::Paused;
        if (m_cachedAnimationState) {
            updateClockTimeOnAnimationState();
            m_cachedAnimationState->setPaused(true);
        }
    }

    if (!render())
        m_isUpdating = false;

    if (isAtRest)
        m_isUpdateLoopRunning = false;

    if (m_needsEntityTransformNotification)
        notifyEntityTransformUpdated();
}

bool WebModelPlayer::render()
{
    if (!m_didFinishLoading)
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
                protectedThis->updateScreenHeadroomFromPage();
            }

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

bool WebModelPlayer::supportsTransform(WebCore::TransformationMatrix transformationMatrix)
{
    if (m_stageMode != WebCore::StageModeOperation::None)
        return false;

    return RemoteMeshProxy::supportsTransform(transformationMatrix);
}

void WebModelPlayer::play(bool playing)
{
    if (RefPtr model = m_currentModel) {
        if (playing && !m_isLooping && currentTime() >= Seconds(duration())) {
            [m_modelLoader setCurrentTime:0];
            if (m_cachedAnimationState)
                m_cachedAnimationState->setCurrentTime(Seconds(0), MonotonicTime::now());
        }
        model->play(playing);
        m_pauseState = playing ? PauseState::Playing : PauseState::Paused;
        if (m_cachedAnimationState) {
            updateClockTimeOnAnimationState();
            m_cachedAnimationState->setPaused(!playing);
        }
        if (playing)
            startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::setLoop(bool loop)
{
    if (m_isLooping == loop)
        return;

    m_isLooping = loop;
    if (m_cachedAnimationState)
        m_cachedAnimationState->setLoop(loop);
    [m_modelLoader setLoop:loop];
    startUpdateLoopIfNeeded();
}

void WebModelPlayer::setAutoplay(bool autoplay)
{
    if (m_pauseState == PauseState::Paused)
        return;

    play(autoplay);
    m_pauseState = autoplay ? PauseState::Playing : PauseState::Paused;
    if (m_cachedAnimationState) {
        updateClockTimeOnAnimationState();
        m_cachedAnimationState->setAutoplay(autoplay);
        m_cachedAnimationState->setPaused(!autoplay);
    }
}

void WebModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completion)
{
    play(!paused);
    completion(m_currentModel && (paused || duration() > 0));
}

bool WebModelPlayer::paused() const
{
    return m_pauseState != PauseState::Playing;
}

Seconds WebModelPlayer::currentTime() const
{
    if (m_modelLoader)
        return Seconds([m_modelLoader currentTime]);
    if (m_cachedAnimationState)
        return m_cachedAnimationState->currentTime();
    return 0_s;
}

void WebModelPlayer::updateClockTimeOnAnimationState()
{
    if (m_cachedAnimationState)
        m_cachedAnimationState->setCurrentTime(m_cachedAnimationState->currentTime(), MonotonicTime::now());
}

void WebModelPlayer::setCurrentTime(Seconds currentTime, CompletionHandler<void()>&& completion)
{
    double clamped = std::clamp(currentTime.seconds(), 0.0, duration());
    if (m_cachedAnimationState)
        m_cachedAnimationState->setCurrentTime(Seconds(clamped), MonotonicTime::now());
    [m_modelLoader setCurrentTime:clamped];
    startUpdateLoopIfNeeded();
    completion();
}

std::optional<WebCore::TransformationMatrix> WebModelPlayer::entityTransform() const
{
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
        model->setStageMode(m_stageMode);
        notifyEntityTransformUpdated();
        startUpdateLoopIfNeeded();
    }
}

void WebModelPlayer::setEntityTransform(WebCore::TransformationMatrix matrix)
{
    if (RefPtr model = m_currentModel) {
        model->setEntityTransform(static_cast<simd_float4x4>(matrix));
        notifyEntityTransformUpdated();
        startUpdateLoopIfNeeded();
        return;
    }

    if (m_cachedTransformState)
        (*m_cachedTransformState)->setEntityTransform(matrix);
}

void WebModelPlayer::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
    RefPtr currentModel = m_currentModel;
    if (!currentModel || !m_didFinishLoading || !m_modelLoader) {
        m_environmentMap = WTF::move(data);
        return;
    }

    bool success = false;
    if (auto environmentMap = [m_modelLoader loadEnvironmentMap:data->createNSData().get()]) {
        currentModel->setEnvironmentMap(convert(environmentMap));
        m_environmentMap = std::nullopt;
        success = true;
    }
    startUpdateLoopIfNeeded();

    if (RefPtr client = m_client)
        client->didFinishEnvironmentMapLoading(*this, success);
}

static bool disableReloading()
{
    static bool disableReloading = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitDisableModelPlayerReloading"];
    return disableReloading;
}

void WebModelPlayer::visibilityStateDidChange()
{
    // When the model becomes invisible, release memory-intensive resources.
    // When it becomes visible again, HTMLModelElement will trigger a reload through startLoadModelTimer().
    RefPtr client { m_client };
    if (!client || disableReloading())
        return;

    if (!client->isVisible()) {
        m_cachedAnimationState = currentAnimationState();
        m_cachedTransformState = currentTransformState();

        // Model is no longer visible - release resources to save memory
        m_currentModel = nullptr;
        m_retainedData = nil;
        m_didFinishLoading = false;
        m_modelLoader = nil;
        m_displayBuffers.clear();
        m_environmentMap = std::nullopt;
        m_isUpdateLoopRunning = false;
        m_isUpdateScheduled = false;
        m_isUpdating = false;
        m_displayTextureIndex = 0;
        m_hasRenderedFrame = false;
#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
        m_cachedModelSource = nullptr;
#endif
    }
}

void WebModelPlayer::reload(WebCore::Model& modelSource, WebCore::LayoutSize size, WebCore::ModelPlayerAnimationState& animationState, std::unique_ptr<WebCore::ModelPlayerTransformState>&& transformState)
{
    if (disableReloading())
        return;

    load(modelSource, size, false);
    m_cachedAnimationState = animationState;
    if (transformState) {
        setStageMode(transformState->stageMode());
        if (auto entityTransform = transformState->entityTransform())
            setEntityTransform(*entityTransform);
    }

    setAutoplay(animationState.autoplay());
    setLoop(animationState.loop());
    setPaused(animationState.paused(), [] (bool) { });
    if (auto playbackRate = animationState.effectivePlaybackRate())
        setPlaybackRate(*playbackRate, [] (double) { });
    setCurrentTime(animationState.currentTime(), [] { });
}

std::optional<WebCore::ModelPlayerAnimationState> WebModelPlayer::currentAnimationState() const
{
    if (!m_currentModel)
        return m_cachedAnimationState;

    bool paused = m_pauseState != PauseState::Playing;
    bool autoplay = !paused;
    Seconds animationDuration { duration() };
    std::optional<double> effectivePlaybackRate = m_playbackRate;
    std::optional<Seconds> lastCachedCurrentTime = currentTime();
    std::optional<MonotonicTime> lastCachedClockTimestamp = MonotonicTime::now();

    return WebCore::ModelPlayerAnimationState(autoplay, m_isLooping, paused, animationDuration, effectivePlaybackRate, lastCachedCurrentTime, lastCachedClockTimestamp);
}

std::pair<WebCore::FloatPoint3D, WebCore::FloatPoint3D> WebModelPlayer::boundingBoxCenterAndExtents() const
{
    auto [simdCenter, simdExtents] = m_currentModel->getCenterAndExtents();
    if ([m_modelLoader treatZAsUpAxis])
        return { WebCore::FloatPoint3D(simdCenter.x, -simdCenter.z, simdCenter.y), WebCore::FloatPoint3D(simdExtents.x, simdExtents.z, simdExtents.y) };
    return { WebCore::FloatPoint3D(simdCenter.x, simdCenter.y, simdCenter.z), WebCore::FloatPoint3D(simdExtents.x, simdExtents.y, simdExtents.z) };
}

std::optional<std::unique_ptr<WebCore::ModelPlayerTransformState>> WebModelPlayer::currentTransformState() const
{
    if (!m_currentModel) {
        if (m_cachedTransformState)
            return (*m_cachedTransformState)->clone();
        return std::nullopt;
    }

    std::optional<WebCore::TransformationMatrix> transform = entityTransform();

    auto [center, extents] = boundingBoxCenterAndExtents();

    return ModelProcessModelPlayerTransformState::create(transform, center, extents, false, m_stageMode);
}

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
static float interpolateHeadroom(float headroomForLow, float headroomForHigh, float limit, float limitLow, float limitHigh)
{
    if (headroomForHigh <= headroomForLow || limitHigh <= limitLow)
        return headroomForHigh;
    return std::lerp(headroomForLow, headroomForHigh, (limit - limitLow) / (limitHigh - limitLow));
}

float WebModelPlayer::computeContentsHeadroom()
{
    if (m_currentEDRHeadroom <= 1.f)
        return m_currentEDRHeadroom;

    if (m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::noLimit())
        return m_currentEDRHeadroom;

    constexpr auto forcedStandardHeadroom = 1.0000001f;

    if (m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard())
        return forcedStandardHeadroom;

    auto limitValue = m_dynamicRangeLimit.value();

    if (m_suppressEDR) {
        if (limitValue >= WebCore::PlatformDynamicRangeLimit::constrained().value())
            return m_currentEDRHeadroom;
        return interpolateHeadroom(forcedStandardHeadroom, m_currentEDRHeadroom, limitValue, WebCore::PlatformDynamicRangeLimit::standard().value(), WebCore::PlatformDynamicRangeLimit::constrained().value());
    }

    constexpr auto maxConstrainedHeadroom = 1.6f;
    auto suppressedHeadroom = std::min(maxConstrainedHeadroom, m_currentEDRHeadroom);
    if (limitValue <= WebCore::PlatformDynamicRangeLimit::constrained().value())
        return interpolateHeadroom(forcedStandardHeadroom, suppressedHeadroom, limitValue, WebCore::PlatformDynamicRangeLimit::standard().value(), WebCore::PlatformDynamicRangeLimit::constrained().value());
    return interpolateHeadroom(suppressedHeadroom, m_currentEDRHeadroom, limitValue, WebCore::PlatformDynamicRangeLimit::constrained().value(), WebCore::PlatformDynamicRangeLimit::noLimit().value());
}

void WebModelPlayer::updateContentsHeadroom()
{
    constexpr auto visionProHeadroom = 2.f;
    auto contentsHeadroom = std::min(visionProHeadroom, computeContentsHeadroom());
    if (fabs(contentsHeadroom - m_lastSentContentsHeadroom) < 0.01f)
        return;
    if (RefPtr model = m_currentModel; model && m_didFinishLoading) {
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
    auto limitValue = m_dynamicRangeLimit.value();
    auto suppressValue = m_suppressEDR ? WebCore::PlatformDynamicRangeLimit::constrained().value() : WebCore::PlatformDynamicRangeLimit::noLimit().value();
    return std::min(limitValue, suppressValue);
}

void WebModelPlayer::dynamicRangeLimitDidChange()
{
    if (!m_cachedModelSource)
        return;

    bool newIsStandard = m_dynamicRangeLimit == WebCore::PlatformDynamicRangeLimit::standard();
    if (newIsStandard == m_usingStandardDynamicRange)
        return;

    auto animationState = currentAnimationState();
    if (!animationState)
        return;
    auto transformStateOpt = currentTransformState();
    std::unique_ptr<WebCore::ModelPlayerTransformState> transformState;
    if (transformStateOpt)
        transformState = WTF::move(*transformStateOpt);

    reload(*m_cachedModelSource, m_lastLayoutSize, *animationState, WTF::move(transformState));
}
#endif

}

#endif
