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
#import "DDModelPlayer.h"

#if ENABLE(GPU_PROCESS_MODEL)

#import "Document.h"
#import "FloatPoint3D.h"
#import "GPU.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerContentsDisplayDelegate.h"
#import "HTMLModelElement.h"
#import "ModelDDInlineConverters.h"
#import "ModelDDTypes.h"
#import "ModelPlayerGraphicsLayerConfiguration.h"
#import "Navigator.h"
#import "Page.h"
#import "PlatformCALayer.h"
#import "PlatformCALayerDelegatedContents.h"
#import <WebGPU/DDModelTypes.h>

namespace WebCore {

class ModelDisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<ModelDisplayBufferDisplayDelegate> create(DDModelPlayer& modelPlayer, bool isOpaque = true, float contentsScale = 1)
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
    ModelDisplayBufferDisplayDelegate(DDModelPlayer& modelPlayer, bool isOpaque, float contentsScale)
        : m_modelPlayer(modelPlayer)
        , m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    ThreadSafeWeakPtr<DDModelPlayer> m_modelPlayer;
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    bool m_isOpaque;
    ContentsFormat m_contentsFormat { ContentsFormat::RGBA8 };
};

Ref<DDModelPlayer> DDModelPlayer::create(Page& page, ModelPlayerClient& client)
{
    return adoptRef(*new DDModelPlayer(page, client));
}

DDModelPlayer::DDModelPlayer(Page& page, ModelPlayerClient& client)
: m_client { client }
, m_id { ModelPlayerIdentifier::generate() }
, m_page(page)
{
}

DDModelPlayer::~DDModelPlayer()
{
}

void DDModelPlayer::ensureOnMainThreadWithProtectedThis(Function<void(Ref<DDModelPlayer>)>&& task)
{
    ensureOnMainThread([protectedThis = Ref { *this }, task = WTFMove(task)]() mutable {
        task(protectedThis);
    });
}

// MARK: - ModelPlayer overrides.

void DDModelPlayer::load(Model& modelSource, LayoutSize size)
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

    m_currentModel = gpu->backing().createModelBacking(size.width().toUnsigned(), size.height().toUnsigned(), [protectedThis = Ref { *this }] (Vector<MachSendRight>&& surfaceHandles) {
        if (surfaceHandles.size())
            protectedThis->m_displayBuffers = WTFMove(surfaceHandles);
    });

    m_modelLoader = adoptNS([[DDBridgeModelLoader alloc] init]);
    RetainPtr nsURL = modelSource.url().createNSURL();
    Ref protectedThis = Ref { *this };
    [m_modelLoader setCallbacksWithModelUpdatedCallback:^(DDBridgeUpdateMesh *updateRequest) {
        ensureOnMainThreadWithProtectedThis([updateRequest] (Ref<DDModelPlayer> protectedThis) {
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
            }
        });
    } textureUpdatedCallback:^(DDBridgeUpdateTexture *updateTexture) {
        ensureOnMainThreadWithProtectedThis([updateTexture] (Ref<DDModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateTexture(toCpp(updateTexture));

            [protectedThis->m_modelLoader requestCompleted:updateTexture];
        });
    } materialUpdatedCallback:^(DDBridgeUpdateMaterial *updateMaterial) {
        ensureOnMainThreadWithProtectedThis([updateMaterial] (Ref<DDModelPlayer> protectedThis) {
            if (protectedThis->m_currentModel)
                protectedThis->m_currentModel->updateMaterial(toCpp(updateMaterial));

            [protectedThis->m_modelLoader requestCompleted:updateMaterial];
        });
    }];
    [m_modelLoader loadModelFrom:nsURL.get()];
}

void DDModelPlayer::notifyEntityTransformUpdated()
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

void DDModelPlayer::sizeDidChange(LayoutSize layoutSize)
{
    m_currentScale = static_cast<float>(layoutSize.minDimension());
}

void DDModelPlayer::enterFullscreen()
{
}

void DDModelPlayer::handleMouseDown(const LayoutPoint& startingPoint, MonotonicTime)
{
    m_currentPoint = startingPoint;
    m_yawAcceleration = 0.f;
    m_pitchAcceleration = 0.f;
}

void DDModelPlayer::handleMouseMove(const LayoutPoint& currentPoint, MonotonicTime)
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

bool DDModelPlayer::supportsMouseInteraction()
{
    return true;
}

void DDModelPlayer::handleMouseUp(const LayoutPoint&, MonotonicTime)
{
    m_currentPoint = std::nullopt;
}

void DDModelPlayer::getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&)
{
}

void DDModelPlayer::setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DDModelPlayer::animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&)
{
}

void DDModelPlayer::setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::isMuted(CompletionHandler<void(std::optional<bool>&&)>&&)
{
}

void DDModelPlayer::setIsMuted(bool, CompletionHandler<void(bool success)>&&)
{
}

void DDModelPlayer::updateScene()
{
}

ModelPlayerAccessibilityChildren DDModelPlayer::accessibilityChildren()
{
    return { };
}

WebCore::ModelPlayerIdentifier DDModelPlayer::identifier() const
{
    return m_id;
}

void DDModelPlayer::configureGraphicsLayer(GraphicsLayer& graphicsLayer, ModelPlayerGraphicsLayerConfiguration&&)
{
    graphicsLayer.setContentsDisplayDelegate(contentsDisplayDelegate(), GraphicsLayer::ContentsLayerPurpose::Canvas);
}

const MachSendRight* DDModelPlayer::displayBuffer() const
{
    if (m_currentTexture >= m_displayBuffers.size())
        return nullptr;

    return &m_displayBuffers[m_currentTexture];
}

GraphicsLayerContentsDisplayDelegate* DDModelPlayer::contentsDisplayDelegate()
{
    if (!m_contentsDisplayDelegate) {
        RefPtr modelDisplayDelegate = ModelDisplayBufferDisplayDelegate::create(*this);
        m_contentsDisplayDelegate = modelDisplayDelegate;
        modelDisplayDelegate->setDisplayBuffer(*displayBuffer());
    }

    return m_contentsDisplayDelegate.get();
}

void DDModelPlayer::simulate(float elapsedTime)
{
    RefPtr model = m_currentModel;
    if (!model)
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

void DDModelPlayer::update()
{
    constexpr float elapsedTime = 1.f / 60.f;
    simulate(elapsedTime);

    [m_modelLoader update:elapsedTime];
    if (RefPtr currentModel = m_currentModel)
        currentModel->render();

    if (++m_currentTexture >= m_displayBuffers.size())
        m_currentTexture = 0;
    if (auto* machSendRight = displayBuffer(); machSendRight && contentsDisplayDelegate())
        RefPtr { m_contentsDisplayDelegate }->setDisplayBuffer(*machSendRight);

    if (RefPtr client = m_client.get())
        client->didUpdate(*this);
}

bool DDModelPlayer::supportsTransform(TransformationMatrix transformationMatrix)
{
    if (m_stageMode != StageModeOperation::None)
        return false;

    if (RefPtr currentModel = m_currentModel)
        return currentModel->supportsTransform(transformationMatrix);

    return false;
}

void DDModelPlayer::play(bool playing)
{
    if (RefPtr model = m_currentModel) {
        model->play(playing);
        m_pauseState = playing ? PauseState::Playing : PauseState::Paused;
    }
}

void DDModelPlayer::setAutoplay(bool autoplay)
{
    if (m_pauseState == PauseState::Paused)
        return;

    play(autoplay);
    m_pauseState = autoplay ? PauseState::Playing : PauseState::Paused;
}

void DDModelPlayer::setPaused(bool paused, CompletionHandler<void(bool succeeded)>&& completion)
{
    play(!paused);
    completion(!!m_currentModel);
}

bool DDModelPlayer::paused() const
{
    return m_pauseState != PauseState::Playing;
}

std::optional<TransformationMatrix> DDModelPlayer::entityTransform() const
{
    if (RefPtr model = m_currentModel) {
        if (auto transform = model->entityTransform())
            return static_cast<simd_float4x4>(*transform);
    }

    return std::nullopt;
}

void DDModelPlayer::setStageMode(StageModeOperation stageMode)
{
    m_stageMode = stageMode;
    if (m_stageMode == StageModeOperation::None)
        return;

    if (RefPtr model = m_currentModel) {
        model->setStageMode(m_stageMode);
        notifyEntityTransformUpdated();
    }
}

void DDModelPlayer::setEntityTransform(TransformationMatrix matrix)
{
    if (RefPtr model = m_currentModel) {
        model->setEntityTransform(static_cast<simd_float4x4>(matrix));
        notifyEntityTransformUpdated();
    }
}

} // namespace WebCore

#endif
