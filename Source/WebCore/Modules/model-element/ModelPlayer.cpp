/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ModelPlayer.h"

#include "Color.h"
#include "FloatPoint3D.h"
#include "ModelPlayerAnimationState.h"
#include "ModelPlayerTransformState.h"
#include "TransformationMatrix.h"
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)
#include <WebCore/StageModeOperations.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelPlayer);

ModelPlayer::~ModelPlayer() = default;

bool ModelPlayer::isPlaceholder() const
{
    return false;
}

std::optional<ModelPlayerAnimationState> ModelPlayer::currentAnimationState() const
{
    return std::nullopt;
}

std::optional<std::unique_ptr<ModelPlayerTransformState>> ModelPlayer::currentTransformState() const
{
    return std::nullopt;
}

void ModelPlayer::reload(Model&, LayoutSize, ModelPlayerAnimationState&, std::unique_ptr<ModelPlayerTransformState>&&)
{
}

void ModelPlayer::visibilityStateDidChange()
{
}

#if ENABLE(MODEL_ELEMENT_BOUNDING_BOX)

std::optional<FloatPoint3D> ModelPlayer::boundingBoxCenter() const
{
    return std::nullopt;
}

std::optional<FloatPoint3D> ModelPlayer::boundingBoxExtents() const
{
    return std::nullopt;
}

#endif

#if ENABLE(MODEL_ELEMENT_ENTITY_TRANSFORM)

std::optional<TransformationMatrix> ModelPlayer::entityTransform() const
{
    return std::nullopt;
}

void ModelPlayer::setEntityTransform(TransformationMatrix)
{
}

bool ModelPlayer::supportsTransform(TransformationMatrix)
{
    return false;
}

#endif

bool ModelPlayer::supportsMouseInteraction()
{
    return false;
}

bool ModelPlayer::supportsDragging()
{
    return true;
}

void ModelPlayer::setInteractionEnabled(bool)
{
}

String ModelPlayer::inlinePreviewUUIDForTesting() const
{
    return emptyString();
}

#if ENABLE(MODEL_ELEMENT_ANIMATIONS_CONTROL)

void ModelPlayer::setAutoplay(bool)
{
}

void ModelPlayer::setLoop(bool)
{
}

void ModelPlayer::setPlaybackRate(double, CompletionHandler<void(double effectivePlaybackRate)>&& completionHandler)
{
    completionHandler(1.0);
}

double ModelPlayer::duration() const
{
    return 0;
}

bool ModelPlayer::paused() const
{
    return true;
}

void ModelPlayer::setPaused(bool, CompletionHandler<void(bool succeeded)>&& completionHandler)
{
    completionHandler(false);
}

Seconds ModelPlayer::currentTime() const
{
    return 0_s;
}

void ModelPlayer::setCurrentTime(Seconds, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

#endif

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)

void ModelPlayer::setEnvironmentMap(Ref<SharedBuffer>&&)
{
}

#endif

#if ENABLE(MODEL_ELEMENT_PORTAL)

void ModelPlayer::setHasPortal(bool)
{
}

#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE)

void ModelPlayer::setStageMode(StageModeOperation)
{
}

#endif

#if ENABLE(MODEL_ELEMENT_STAGE_MODE_INTERACTION)

void ModelPlayer::beginStageModeTransform(const TransformationMatrix&)
{
}

void ModelPlayer::updateStageModeTransform(const TransformationMatrix&)
{
}

void ModelPlayer::endStageModeInteraction()
{
}

void ModelPlayer::animateModelToFitPortal(CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

void ModelPlayer::resetModelTransformAfterDrag()
{
}

#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)

void ModelPlayer::ensureImmersivePresentation(CompletionHandler<void(std::optional<LayerHostingContextIdentifier>)>&& completion)
{
    ASSERT_NOT_REACHED("ModelPlayer cannot provide a layer context identifier");
    completion(std::nullopt);
}

void ModelPlayer::exitImmersivePresentation(CompletionHandler<void()>&& completion)
{
    ASSERT_NOT_REACHED("ModelPlayer cannot exit an immersive presentation");
    completion();
}

#endif

} // namespace WebCore
