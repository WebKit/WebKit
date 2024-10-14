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
#include "TransformationMatrix.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelPlayer);

ModelPlayer::~ModelPlayer() = default;

void ModelPlayer::setBackgroundColor(Color)
{
}

void ModelPlayer::setEntityTransform(TransformationMatrix)
{
}

bool ModelPlayer::supportsMouseInteraction()
{
    return false;
}

bool ModelPlayer::supportsDragging()
{
    return true;
}

bool ModelPlayer::supportsTransform(TransformationMatrix)
{
    return false;
}

void ModelPlayer::setInteractionEnabled(bool)
{
}

String ModelPlayer::inlinePreviewUUIDForTesting() const
{
    return emptyString();
}

#if ENABLE(MODEL_PROCESS)
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

void ModelPlayer::setEnvironmentMap(Ref<SharedBuffer>&&)
{
}
#endif // ENABLE(MODEL_PROCESS)

}
