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

#import "Document.h"
#import "FloatPoint3D.h"
#import "GPU.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerContentsDisplayDelegate.h"
#import "HTMLModelElement.h"
#import "ModelInlineConverters.h"
#import "ModelPlayerGraphicsLayerConfiguration.h"
#import "Navigator.h"
#import "Page.h"
#import "PlatformCALayer.h"
#import "PlatformCALayerDelegatedContents.h"
#import <WebCore/Mesh.h>
#import <WebGPU/ModelTypes.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(COCOA)
#import <Metal/Metal.h>
#endif

namespace WebCore {

class ModelDisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<ModelDisplayBufferDisplayDelegate> create(WebModelPlayer& modelPlayer, bool isOpaque = false, float contentsScale = 1)
    {
        return adoptRef(*new ModelDisplayBufferDisplayDelegate(modelPlayer, isOpaque, contentsScale));
    }
    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
        layer.setContentsScale(m_contentsScale);
        layer.setContentsFormat(m_contentsFormat);
    }
    void display(PlatformCALayer& layer) final
    {
        if (layer.isOpaque() != m_isOpaque)
            layer.setOpaque(m_isOpaque);
        if (m_displayBuffer) {
            layer.setContentsFormat(m_contentsFormat);
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { }, std::nullopt });
        } else
            layer.clearContents();

        layer.setNeedsDisplay();

        if (RefPtr player = m_modelPlayer.get())
            player->update();

    }
    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
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
    void setContentsFormat(ContentsFormat contentsFormat)
    {
        m_contentsFormat = contentsFormat;
    }
    void setOpaque(bool opaque)
    {
        m_isOpaque = opaque;
    }
private:
    ModelDisplayBufferDisplayDelegate(WebModelPlayer& modelPlayer, bool isOpaque, float contentsScale)
        : m_modelPlayer(modelPlayer)
        , m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    ThreadSafeWeakPtr<WebModelPlayer> m_modelPlayer;
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    bool m_isOpaque;
    ContentsFormat m_contentsFormat { ContentsFormat::RGBA8 };
};

Ref<WebModelPlayer> WebModelPlayer::create(Page& page, ModelPlayerClient& client)
{
    return adoptRef(*new WebModelPlayer(page, client));
}

WebModelPlayer::WebModelPlayer(Page& page, ModelPlayerClient& client)
: m_client { client }
, m_id { ModelPlayerIdentifier::generate() }
, m_page(page)
{
}

WebModelPlayer::~WebModelPlayer()
{
}

void WebModelPlayer::ensureOnMainThreadWithProtectedThis(Function<void(Ref<WebModelPlayer>)>&& task)
{
    ensureOnMainThread([protectedThis = Ref { *this }, task = WTF::move(task)]() mutable {
        task(protectedThis);
    });
}

double WebModelPlayer::duration() const
{
    return [m_modelLoader duration];
}

static Vector<uint8_t> loadData(RetainPtr<CFStringRef> filename)
{
    RetainPtr<NSBundle> myBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebCore"];
    RetainPtr<NSURL> nsFileURL = [myBundle URLForResource:(__bridge NSString *)filename.get() withExtension:@""];
    RetainPtr<NSData> data = [NSData dataWithContentsOfURL:nsFileURL.get() options:0 error:nil];
    return makeVector(data.get());
}

static MTLPixelFormat computePixelFormat(size_t bytesPerComponent, size_t channelCount)
{
    switch (bytesPerComponent) {
    default:
    case 1:
        switch (channelCount) {
        case 1:
            return MTLPixelFormatR8Unorm;
        case 2:
            return MTLPixelFormatRG8Unorm;
        case 3:
            return MTLPixelFormatRGB8Unorm;
        case 4:
        default:
            return MTLPixelFormatRGBA8Unorm;
        }
    case 2:
        switch (channelCount) {
        case 1:
            return MTLPixelFormatR16Float;
        case 2:
            return MTLPixelFormatRG16Float;
        case 3:
            return MTLPixelFormatRGB16Float;
        case 4:
        default:
            return MTLPixelFormatRGBA16Float;
        }
    case 4:
        switch (channelCount) {
        case 1:
            return MTLPixelFormatR32Float;
        case 2:
            return MTLPixelFormatRG32Float;
        case 3:
            return MTLPixelFormatRGB32Float;
        case 4:
        default:
            return MTLPixelFormatRGBA32Float;
        }
    }
}

static std::optional<WebModel::ImageAsset> loadIBL(Ref<WebCore::SharedBuffer>&& data)
{
    RetainPtr imageAssetData = data->createNSData();
    RetainPtr imageSource = adoptCF(CGImageSourceCreateWithData((CFDataRef)imageAssetData.get(), nullptr));
    if (!imageSource) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RetainPtr platformImage = adoptCF(CGImageSourceCreateImageAtIndex(imageSource.get(), 0, nullptr));
    if (!platformImage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RetainPtr pixelDataCfData = adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(platformImage.get())));
    auto byteSpan = span(pixelDataCfData.get());

    auto width = CGImageGetWidth(platformImage.get());
    auto height = CGImageGetHeight(platformImage.get());
    auto bytesPerPixel = static_cast<size_t>(byteSpan.size() / (width * height));
    auto bytesPerComponent = CGImageGetBitsPerComponent(platformImage.get()) / 8;

    MTLPixelFormat pixelFormat = computePixelFormat(bytesPerComponent, bytesPerPixel / bytesPerComponent);

    return WebModel::ImageAsset {
        .data = Vector<uint8_t> { byteSpan },
        .width = static_cast<long>(width),
        .height = static_cast<long>(height),
        .depth = 1,
        .bytesPerPixel = static_cast<long>(bytesPerPixel),
        .textureType = MTLTextureType2D,
        .pixelFormat = pixelFormat,
        .mipmapLevelCount = 1,
        .arrayLength = 1,
        .textureUsage = MTLTextureUsageShaderRead,
        .swizzle = WebModel::ImageAssetSwizzle {
            .red = MTLTextureSwizzleRed,
            .green = MTLTextureSwizzleGreen,
            .blue = MTLTextureSwizzleBlue,
            .alpha = MTLTextureSwizzleAlpha
        }
    };
}

// MARK: - ModelPlayer overrides.

void WebModelPlayer::load(Model& modelSource, LayoutSize size)
{
    RefPtr corePage = m_page.get();
    m_modelLoader = nil;
    m_didFinishLoading = false;
    RefPtr document = corePage->localTopDocument();
    if (!document)
        return;

    RefPtr window = document->window();
    if (!window)
        return;

    RefPtr gpu = window->protectedNavigator()->gpu();
    if (!gpu)
        return;

    size.scale(document->deviceScaleFactor());
    WebModel::ImageAsset diffuseTexture {
        .data = loadData(adoptCF(static_cast<CFStringRef>(@"modelDefaultDiffuseData"))),
        .width = 64,
        .height = 64,
        .depth = 1,
        .bytesPerPixel = 2,
        .textureType = MTLTextureTypeCube,
        .pixelFormat = MTLPixelFormatR16Float,
        .mipmapLevelCount = 0,
        .arrayLength = 6,
        .textureUsage = MTLTextureUsageShaderRead,
        .swizzle = { }
    };
    WebModel::ImageAsset specularTexture {
        .data = loadData(adoptCF(static_cast<CFStringRef>(@"modelDefaultSpecularData"))),
        .width = 256,
        .height = 256,
        .depth = 1,
        .bytesPerPixel = 2,
        .textureType = MTLTextureTypeCube,
        .pixelFormat = MTLPixelFormatR16Float,
        .mipmapLevelCount = 0,
        .arrayLength = 6,
        .textureUsage = MTLTextureUsageShaderRead,
        .swizzle = { }
    };

    m_currentModel = gpu->backing().createModelBacking(size.width().toUnsigned(), size.height().toUnsigned(), diffuseTexture, specularTexture, [protectedThis = Ref { *this }] (Vector<MachSendRight>&& surfaceHandles) {
        if (surfaceHandles.size())
            protectedThis->m_displayBuffers = WTF::move(surfaceHandles);
    });

    m_modelLoader = adoptNS([[WebBridgeModelLoader alloc] init]);
    Ref protectedThis = Ref { *this };
    [m_modelLoader setCallbacksWithModelUpdatedCallback:^(WebBridgeUpdateMesh *updateRequest) {
        ensureOnMainThreadWithProtectedThis([updateRequest] (Ref<WebModelPlayer> protectedThis) {
            RefPtr model = protectedThis->m_currentModel;
            if (model) {
                model->update(toCpp(updateRequest));
                protectedThis->setStageMode(protectedThis->m_stageMode);
            }

            [protectedThis->m_modelLoader requestCompleted:updateRequest];

            if (RefPtr client = protectedThis->m_client.get(); client && !protectedThis->m_didFinishLoading) {
                protectedThis->m_didFinishLoading = true;
                client->didFinishLoading(protectedThis.get());
                auto [simdCenter, simdExtents] = model->getCenterAndExtents();
                client->didUpdateBoundingBox(protectedThis.get(), FloatPoint3D(simdCenter.x, simdCenter.y, simdCenter.z), FloatPoint3D(simdExtents.x, simdExtents.y, simdExtents.z));
                protectedThis->notifyEntityTransformUpdated();

                auto environmentMap = protectedThis->m_environmentMap;
                if (model && environmentMap) {
                    if (auto environmentMapImage = loadIBL(WTF::move(*environmentMap))) {
                        model->setEnvironmentMap(*environmentMapImage);
                        protectedThis->m_environmentMap = std::nullopt;
                    }
                }
            }
        });
    } textureUpdatedCallback:^(WebBridgeUpdateTexture *updateTexture) {
        ensureOnMainThreadWithProtectedThis([updateTexture] (Ref<WebModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateTexture(toCpp(updateTexture));

            [protectedThis->m_modelLoader requestCompleted:updateTexture];
        });
    } materialUpdatedCallback:^(WebBridgeUpdateMaterial *updateMaterial) {
        ensureOnMainThreadWithProtectedThis([updateMaterial] (Ref<WebModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateMaterial(toCpp(updateMaterial));

            [protectedThis->m_modelLoader requestCompleted:updateMaterial];
        });
    }];

    m_retainedData = modelSource.data()->createNSData();
    [m_modelLoader loadModel:m_retainedData.get()];
}

void WebModelPlayer::notifyEntityTransformUpdated()
{
    RefPtr model = m_currentModel;
    RefPtr client = m_client.get();
    if (!model || !client || !model->entityTransform())
        return;

    auto scaledTransform = *model->entityTransform();
    auto scale = m_currentScale;
    scaledTransform.column0 *= scale;
    scaledTransform.column1 *= scale;
    scaledTransform.column2 *= scale;
    client->didUpdateEntityTransform(*this, TransformationMatrix(static_cast<simd_float4x4>(scaledTransform)));
}

void WebModelPlayer::sizeDidChange(LayoutSize layoutSize)
{
    m_currentScale = static_cast<float>(layoutSize.minDimension());
}

void WebModelPlayer::enterFullscreen()
{
}

void WebModelPlayer::handleMouseDown(const LayoutPoint& startingPoint, MonotonicTime)
{
    m_currentPoint = startingPoint;
    m_yawAcceleration = 0.f;
    m_pitchAcceleration = 0.f;
}

void WebModelPlayer::handleMouseMove(const LayoutPoint& currentPoint, MonotonicTime)
{
    if (!m_currentPoint)
        return;

    float deltaX = static_cast<float>(m_currentPoint->x() - currentPoint.x());
    float deltaY = static_cast<float>(currentPoint.y() - m_currentPoint->y());
    m_currentPoint = currentPoint;
    if (RefPtr model = m_currentModel) {
        if (m_yawAcceleration * deltaX < 0.f)
            m_yawAcceleration = 0.f;
        if (m_pitchAcceleration * deltaY < 0.f)
            m_pitchAcceleration = 0.f;

        m_yawAcceleration += 0.1f * deltaX;
        m_pitchAcceleration += 0.1f * deltaY;
    }
}

bool WebModelPlayer::supportsMouseInteraction()
{
    return true;
}

void WebModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
    m_currentPoint = std::nullopt;
}

void WebModelPlayer::getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&)
{
}

void WebModelPlayer::setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
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

void WebModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void WebModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void WebModelPlayer::setIsMuted(bool, CompletionHandler<void(bool success)>&&)
{
}

void WebModelPlayer::updateScene()
{
}

ModelPlayerAccessibilityChildren WebModelPlayer::accessibilityChildren()
{
    return { };
}

WebCore::ModelPlayerIdentifier WebModelPlayer::identifier() const
{
    return m_id;
}

void WebModelPlayer::configureGraphicsLayer(GraphicsLayer& graphicsLayer, ModelPlayerGraphicsLayerConfiguration&&)
{
    graphicsLayer.setContentsDisplayDelegate(contentsDisplayDelegate(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

const MachSendRight* WebModelPlayer::displayBuffer() const
{
    if (m_currentTexture >= m_displayBuffers.size())
        return nullptr;

    return &m_displayBuffers[m_currentTexture];
}

GraphicsLayerContentsDisplayDelegate* WebModelPlayer::contentsDisplayDelegate()
{
    if (!m_contentsDisplayDelegate) {
        RefPtr modelDisplayDelegate = ModelDisplayBufferDisplayDelegate::create(*this);
        m_contentsDisplayDelegate = modelDisplayDelegate;
        modelDisplayDelegate->setDisplayBuffer(*displayBuffer());
    }

    return m_contentsDisplayDelegate.get();
}

void WebModelPlayer::simulate(float elapsedTime)
{
    RefPtr model = m_currentModel;
    if (!model || !m_didFinishLoading)
        return;

    m_yawAcceleration *= 0.95f;
    m_pitchAcceleration *= 0.95f;

    m_yawAcceleration = std::clamp(m_yawAcceleration, -5.f, 5.f);
    m_pitchAcceleration = std::clamp(m_pitchAcceleration, -5.f, 5.f);
    if (fabs(m_yawAcceleration) < 0.01f)
        m_yawAcceleration = 0.f;
    if (fabs(m_pitchAcceleration) < 0.01f)
        m_pitchAcceleration = 0.f;

    m_yaw += m_yawAcceleration * elapsedTime;
    m_pitch += m_pitchAcceleration * elapsedTime;
    m_pitch *= (1.f - elapsedTime);

    model->setRotation(m_yaw, m_pitch);
}

void WebModelPlayer::setPlaybackRate(double newRate, CompletionHandler<void(double effectivePlaybackRate)>&& completion)
{
    m_playbackRate = newRate;
    completion(newRate);
}

void WebModelPlayer::update()
{
    constexpr float elapsedTime = 1.f / 60.f;
    simulate(elapsedTime);

    auto timeDelta = paused() ? 0.f : (m_playbackRate * elapsedTime);
    if (!m_isLooping && [m_modelLoader currentTime] > [m_modelLoader duration])
        timeDelta = 0.f;

    [m_modelLoader update:timeDelta];
    if (m_didFinishLoading) {
        if (RefPtr currentModel = m_currentModel)
            currentModel->render();

        if (++m_currentTexture >= m_displayBuffers.size())
            m_currentTexture = 0;
        if (auto* machSendRight = displayBuffer(); machSendRight && contentsDisplayDelegate())
            RefPtr { m_contentsDisplayDelegate }->setDisplayBuffer(*machSendRight);
    }

    if (RefPtr client = m_client.get())
        client->didUpdate(*this);
}

bool WebModelPlayer::supportsTransform(TransformationMatrix transformationMatrix)
{
    if (m_stageMode != StageModeOperation::None)
        return false;

    if (RefPtr currentModel = m_currentModel)
        return currentModel->supportsTransform(transformationMatrix);

    return false;
}

void WebModelPlayer::play(bool playing)
{
    if (RefPtr model = m_currentModel) {
        model->play(playing);
        m_pauseState = playing ? PauseState::Playing : PauseState::Paused;
    }
}

void WebModelPlayer::setAutoplay(bool autoplay)
{
    if (m_pauseState == PauseState::Paused)
        return;

    play(autoplay);
    m_pauseState = autoplay ? PauseState::Playing : PauseState::Paused;
}

void WebModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completion)
{
    play(!paused);
    completion(!!m_currentModel);
}

bool WebModelPlayer::paused() const
{
    return m_pauseState != PauseState::Playing;
}

std::optional<TransformationMatrix> WebModelPlayer::entityTransform() const
{
#if PLATFORM(COCOA)
    if (RefPtr model = m_currentModel) {
        if (auto transform = model->entityTransform())
            return static_cast<simd_float4x4>(*transform);
    }
#endif
    return std::nullopt;
}

void WebModelPlayer::setStageMode(StageModeOperation stageMode)
{
    m_stageMode = stageMode;
    if (m_stageMode == StageModeOperation::None)
        return;

    if (RefPtr model = m_currentModel) {
        model->setStageMode(m_stageMode);
        notifyEntityTransformUpdated();
    }
}

void WebModelPlayer::setEntityTransform(TransformationMatrix matrix)
{
    if (RefPtr model = m_currentModel) {
        model->setEntityTransform(static_cast<simd_float4x4>(matrix));
        notifyEntityTransformUpdated();
    }
}

void WebModelPlayer::setEnvironmentMap(Ref<WebCore::SharedBuffer>&& data)
{
    bool success = false;
    if (RefPtr currentModel = m_currentModel; currentModel && m_didFinishLoading) {
        if (auto environmentMap = loadIBL(WTF::move(data))) {
            currentModel->setEnvironmentMap(*environmentMap);
            m_environmentMap = std::nullopt;
        }
        success = true;
    } else
        m_environmentMap = WTF::move(data);

    if (RefPtr client = m_client.get())
        client->didFinishEnvironmentMapLoading(*this, success);
}

} // namespace WebCore

#endif
