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

#pragma once

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include <WebCore/AcceleratedEffectOffsetAnchor.h>
#include <WebCore/AcceleratedEffectOffsetDistance.h>
#include <WebCore/AcceleratedEffectOffsetPosition.h>
#include <WebCore/AcceleratedEffectOffsetRotate.h>
#include <WebCore/AcceleratedEffectOpacity.h>
#include <WebCore/AcceleratedEffectTransformBox.h>
#include <WebCore/AcceleratedEffectTransformOrigin.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/PathOperation.h>
#include <WebCore/RotateTransformOperation.h>
#include <WebCore/ScaleTransformOperation.h>
#include <WebCore/TransformOperations.h>
#include <WebCore/TransformationMatrix.h>
#include <WebCore/TranslateTransformOperation.h>

namespace WebCore {

class IntRect;
class Path;
class RenderLayerModelObject;
class RenderStyle;

struct AcceleratedEffectValues {
    AcceleratedEffectOpacity opacity { };
    // FIXME: It is a layering violation to use `TransformOperationData` here, as it is defined in the rendering directory.
    std::optional<TransformOperationData> transformOperationData;
    AcceleratedEffectTransformOrigin transformOrigin { };
    AcceleratedEffectTransformBox transformBox { AcceleratedEffectTransformBox::ContentBox };
    TransformOperations transform { };
    RefPtr<TransformOperation> translate;
    RefPtr<TransformOperation> scale;
    RefPtr<TransformOperation> rotate;
    // FIXME: It is a layering violation to use `PathOperation` here, as it is defined in the rendering directory.
    RefPtr<PathOperation> offsetPath;
    AcceleratedEffectOffsetDistance offsetDistance { };
    // FIXME: This `offsetPosition` is not used.
    AcceleratedEffectOffsetPosition offsetPosition { };
    AcceleratedEffectOffsetAnchor offsetAnchor { };
    AcceleratedEffectOffsetRotate offsetRotate { };
    FilterOperations filter { };
    FilterOperations backdropFilter { };

    AcceleratedEffectValues() = default;
    // FIXME: It is a layering violation to use `RenderStyle` and `RenderLayerModelObject` here, as they are defined in the rendering directory.
    AcceleratedEffectValues(const RenderStyle&, const IntRect&, const RenderLayerModelObject* = nullptr);
    AcceleratedEffectValues(AcceleratedEffectOpacity opacity, std::optional<TransformOperationData>&& transformOperationData, AcceleratedEffectTransformOrigin transformOrigin, AcceleratedEffectTransformBox transformBox, TransformOperations&& transform, RefPtr<TransformOperation>&& translate, RefPtr<TransformOperation>&& scale, RefPtr<TransformOperation>&& rotate, RefPtr<PathOperation>&& offsetPath, AcceleratedEffectOffsetDistance offsetDistance, AcceleratedEffectOffsetPosition offsetPosition, AcceleratedEffectOffsetAnchor offsetAnchor, AcceleratedEffectOffsetRotate offsetRotate, FilterOperations&& filter, FilterOperations&& backdropFilter)
        : opacity(opacity)
        , transformOperationData(WTFMove(transformOperationData))
        , transformOrigin(transformOrigin)
        , transformBox(transformBox)
        , transform(WTFMove(transform))
        , translate(WTFMove(translate))
        , scale(WTFMove(scale))
        , rotate(WTFMove(rotate))
        , offsetPath(WTFMove(offsetPath))
        , offsetDistance(offsetDistance)
        , offsetPosition(offsetPosition)
        , offsetAnchor(offsetAnchor)
        , offsetRotate(offsetRotate)
        , filter(WTFMove(filter))
        , backdropFilter(WTFMove(backdropFilter))
    {
    }

    WEBCORE_EXPORT AcceleratedEffectValues clone() const;
    WEBCORE_EXPORT TransformationMatrix computedTransformationMatrix(const FloatRect&) const;
};

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
