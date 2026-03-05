/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/HTMLModelElementCamera.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/LayoutPoint.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/ModelPlayerAccessibilityChildren.h>
#include <WebCore/ModelPlayerIdentifier.h>
#include <optional>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Platform.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadSafeWeakPtr.h>

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
#include <WebCore/StageModeOperations.h>
#endif

namespace WebCore {

class FloatPoint3D;
class GraphicsLayer;
class Model;
class ModelPlayerAnimationState;
class ModelPlayerTransformState;
class SharedBuffer;
class TransformationMatrix;

struct ModelPlayerGraphicsLayerConfiguration;

class WEBCORE_EXPORT ModelPlayer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ModelPlayer> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ModelPlayer, WEBCORE_EXPORT);
public:
    virtual ~ModelPlayer();

    virtual ModelPlayerIdentifier identifier() const = 0;
    virtual bool NODELETE isPlaceholder() const;

    // Loading.
    virtual void load(Model&, LayoutSize) = 0;
    virtual void NODELETE reload(Model&, LayoutSize, ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&);

    // Graphics.
    virtual void configureGraphicsLayer(GraphicsLayer&, ModelPlayerGraphicsLayerConfiguration&&) = 0;

    // State changes.
    virtual void NODELETE visibilityStateDidChange();
    virtual void sizeDidChange(LayoutSize) = 0;

    // State accessors.
    virtual std::optional<ModelPlayerAnimationState> currentAnimationState() const;
    virtual std::optional<std::unique_ptr<ModelPlayerTransformState>> currentTransformState() const;

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)
    virtual std::optional<FloatPoint3D> boundingBoxCenter() const;
    virtual std::optional<FloatPoint3D> boundingBoxExtents() const;
#endif

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)
    virtual std::optional<TransformationMatrix> entityTransform() const;
    virtual void setEntityTransform(TransformationMatrix);
    virtual bool supportsTransform(TransformationMatrix);
#endif

    // Fullscreen.
    virtual void enterFullscreen() = 0;

    // Interaction.
    virtual bool NODELETE supportsMouseInteraction();
    virtual bool NODELETE supportsDragging();
    virtual void NODELETE setInteractionEnabled(bool);
    virtual void handleMouseDown(const LayoutPoint&, MonotonicTime) = 0;
    virtual void handleMouseMove(const LayoutPoint&, MonotonicTime) = 0;
    virtual void handleMouseUp(const LayoutPoint&, MonotonicTime) = 0;
#if ENABLE(MODEL_ELEMENT_STAGE_MODE_INTERACTION)
    virtual void beginStageModeTransform(const TransformationMatrix&);
    virtual void updateStageModeTransform(const TransformationMatrix&);
    virtual void endStageModeInteraction();
    virtual void animateModelToFitPortal(CompletionHandler<void(bool)>&&);
    virtual void resetModelTransformAfterDrag();
#endif

    virtual void getCamera(CompletionHandler<void(std::optional<HTMLModelElementCamera>&&)>&&) = 0;
    virtual void setCamera(HTMLModelElementCamera, CompletionHandler<void(bool success)>&&) = 0;
    virtual void isPlayingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) = 0;
    virtual void setAnimationIsPlaying(bool, CompletionHandler<void(bool success)>&&) = 0;
    virtual void isLoopingAnimation(CompletionHandler<void(std::optional<bool>&&)>&&) = 0;
    virtual void setIsLoopingAnimation(bool, CompletionHandler<void(bool success)>&&) = 0;
    virtual void animationDuration(CompletionHandler<void(std::optional<Seconds>&&)>&&) = 0;
    virtual void animationCurrentTime(CompletionHandler<void(std::optional<Seconds>&&)>&&) = 0;
    virtual void setAnimationCurrentTime(Seconds, CompletionHandler<void(bool success)>&&) = 0;

    virtual void hasAudio(CompletionHandler<void(std::optional<bool>&&)>&&) = 0;
    virtual void isMuted(CompletionHandler<void(std::optional<bool>&&)>&&) = 0;
    virtual void setIsMuted(bool, CompletionHandler<void(bool success)>&&) = 0;

    virtual String inlinePreviewUUIDForTesting() const;

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
    virtual ModelPlayerAccessibilityChildren accessibilityChildren() = 0;
#endif

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)
    virtual void setAutoplay(bool);
    virtual void setLoop(bool);
    virtual void setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&&);
    virtual double duration() const;
    virtual bool paused() const;
    virtual void setPaused(bool, CompletionHandler<void(bool succeeded)>&&);
    virtual Seconds currentTime() const;
    virtual void setCurrentTime(Seconds, CompletionHandler<void()>&&);
#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
    virtual void setEnvironmentMap(Ref<SharedBuffer>&& data);
#endif

#if ENABLE(MODEL_ELEMENT_PORTAL)
    virtual void setHasPortal(bool);
#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
    virtual void setStageMode(StageModeOperation);
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    virtual void ensureImmersivePresentation(CompletionHandler<void(std::optional<LayerHostingContextIdentifier>)>&&);
    virtual void exitImmersivePresentation(CompletionHandler<void()>&&);
#endif
};

} // namespace WebCore
