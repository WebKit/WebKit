/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AcceleratedEffectValues.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

namespace WebCore {

void AcceleratedEffectValues::copyTransformOperations(const TransformOperations& src)
{
    auto& transformOperations = transform.operations();
    auto& srcTransformOperations = src.operations();
    transformOperations.reserveCapacity(srcTransformOperations.size());
    for (auto& srcTransformOperation : srcTransformOperations)
        transformOperations.uncheckedAppend(srcTransformOperation.copyRef());
}

AcceleratedEffectValues::AcceleratedEffectValues(const AcceleratedEffectValues& src)
{
    opacity = src.opacity;

    copyTransformOperations(src.transform);
    translate = src.translate.copyRef();
    scale = src.scale.copyRef();
    rotate = src.rotate.copyRef();
    transformOrigin = src.transformOrigin;

    offsetPath = src.offsetPath.copyRef();
    offsetDistance = src.offsetDistance;
    offsetPosition = src.offsetPosition;
    offsetAnchor = src.offsetAnchor;
    offsetRotate = src.offsetRotate;

    for (auto& srcFilterOperation : src.filter.operations())
        filter.operations().append(srcFilterOperation.copyRef());

#if ENABLE(FILTERS_LEVEL_2)
    for (auto& srcBackdropFilterOperation : src.backdropFilter.operations())
        backdropFilter.operations().append(srcBackdropFilterOperation.copyRef());
#endif
}

AcceleratedEffectValues::AcceleratedEffectValues(const RenderStyle& style)
{
    opacity = style.opacity();

    copyTransformOperations(style.transform());
    translate = style.translate();
    scale = style.scale();
    rotate = style.rotate();
    transformOrigin = style.transformOriginXY();

    offsetPath = style.offsetPath();
    offsetPosition = style.offsetPosition();
    offsetAnchor = style.offsetAnchor();
    offsetRotate = style.offsetRotate();
    offsetDistance = style.offsetDistance();

    for (auto srcFilterOperation : style.filter().operations())
        filter.operations().append(srcFilterOperation.copyRef());

#if ENABLE(FILTERS_LEVEL_2)
    for (auto srcBackdropFilterOperation : style.backdropFilter().operations())
        backdropFilter.operations().append(srcBackdropFilterOperation.copyRef());
#endif
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
