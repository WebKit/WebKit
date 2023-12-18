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

#include "FloatRoundedRect.h"
#include "RenderLayerModelObject.h"

namespace WebCore {

class BoxPathOperation;
class RenderElement;
class PathOperation;
class RayPathOperation;
class ShapePathOperation;

struct TransformOperationData;

struct MotionPathData {
    FloatRoundedRect containingBlockBoundingRect;
    FloatPoint offsetFromContainingBlock;
    FloatPoint usedStartingPosition;
};

class MotionPath {
public:
    static std::optional<MotionPathData> motionPathDataForRenderer(const RenderElement&);
    static bool needsUpdateAfterContainingBlockLayout(const PathOperation&);
    static void applyMotionPathTransform(const RenderStyle&, const TransformOperationData&, TransformationMatrix&);
    WEBCORE_EXPORT static std::optional<Path> computePathForBox(const BoxPathOperation&, const TransformOperationData&);
    static std::optional<Path> computePathForRay(const RayPathOperation&, const TransformOperationData&);
    static double lengthForRayPath(const RayPathOperation&, const MotionPathData&);
    static double lengthForRayContainPath(const FloatRect& elementRect, double computedPathLength);
    WEBCORE_EXPORT static std::optional<Path> computePathForShape(const ShapePathOperation&, const TransformOperationData&);
};

} // namespace WebCore
