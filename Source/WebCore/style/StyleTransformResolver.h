/*
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

#include "StyleComputedStyle.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class FloatPoint;
class FloatPoint3D;
class FloatRect;
class TransformationMatrix;
struct TransformOperationData;

namespace Style {

enum class TransformResolverOption : uint8_t {
    TransformOrigin = 1 << 0,
    Translate       = 1 << 1,
    Rotate          = 1 << 2,
    Scale           = 1 << 3,
    Offset          = 1 << 4
};

class TransformResolver {
public:
    using Option = TransformResolverOption;

    static constexpr OptionSet allTransformOperations { Option::TransformOrigin, Option::Translate, Option::Rotate, Option::Scale, Option::Offset };
    static constexpr OptionSet individualTransformOperations { Option::Translate, Option::Rotate, Option::Scale, Option::Offset };

    explicit TransformResolver(TransformationMatrix&, const RenderStyle&);
    explicit TransformResolver(TransformationMatrix&, const ComputedStyle&);

    static bool affectedByTransformOrigin(const RenderStyle&);
    static bool affectedByTransformOrigin(const ComputedStyle&);
    bool affectedByTransformOrigin() const;

    static FloatPoint3D computeTransformOrigin(const RenderStyle&, const FloatRect& boundingBox);
    static FloatPoint3D computeTransformOrigin(const ComputedStyle&, const FloatRect& boundingBox);
    FloatPoint3D computeTransformOrigin(const FloatRect& boundingBox) const;

    static FloatPoint computePerspectiveOrigin(const RenderStyle&, const FloatRect& boundingBox);
    static FloatPoint computePerspectiveOrigin(const ComputedStyle&, const FloatRect& boundingBox);
    FloatPoint computePerspectiveOrigin(const FloatRect& boundingBox) const;

    void applyPerspective(const FloatPoint& originTranslate);

    void applyTransformOrigin(const FloatPoint3D& originTranslate);
    void unapplyTransformOrigin(const FloatPoint3D& originTranslate);

    void applyCSSTransform(const TransformOperationData&, OptionSet<Option> = allTransformOperations);

    // `applyTransform`/`computedTransform` perform the following operations in order:
    //    1. applyTransformOrigin()
    //    2. applyCSSTransform()
    //    3. unapplyTransformOrigin()

    void applyTransform(const TransformOperationData&, OptionSet<Option> = allTransformOperations);

    static void applyTransform(TransformationMatrix&, const ComputedStyle&, const TransformOperationData&, OptionSet<Option> = allTransformOperations);
    static void applyTransform(TransformationMatrix&, const RenderStyle&, const TransformOperationData&, OptionSet<Option> = allTransformOperations);

    static TransformationMatrix computeTransform(const ComputedStyle&, const TransformOperationData&, OptionSet<Option> = allTransformOperations);
    static TransformationMatrix computeTransform(const RenderStyle&, const TransformOperationData&, OptionSet<Option> = allTransformOperations);

private:
    void applyMotionPathTransform(const TransformOperationData&, ZoomFactor);

    TransformationMatrix& m_transform;
    const CheckedRef<const ComputedStyle> m_style;
};

} // namespace Style
} // namespace WebCore
