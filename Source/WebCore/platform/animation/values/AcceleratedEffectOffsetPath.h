/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#if ENABLE(THREADED_ANIMATIONS)

#include <WebCore/AcceleratedEffectBasicShapePath.h>
#include <WebCore/AcceleratedEffectBoxPath.h>
#include <WebCore/AcceleratedEffectRayPath.h>
#include <WebCore/AcceleratedEffectReferencePath.h>
#include <optional>
#include <wtf/Variant.h>

namespace WebCore {

struct BlendingContext;
struct TransformOperationData;

// <'offset-path'> = none | [ [ <ray()> | <url> | <basic-shape> ] || <coord-box> ]
// https://drafts.csswg.org/motion-1/#propdef-offset-path
struct AcceleratedEffectOffsetPath {
    struct None { constexpr bool operator==(const None&) const = default; };
    using RayPath = AcceleratedEffectRayPath;
    using ReferencePath = AcceleratedEffectReferencePath;
    using BasicShapePath = AcceleratedEffectBasicShapePath;
    using BoxPath = AcceleratedEffectBoxPath;

    Variant<None, RayPath, ReferencePath, BasicShapePath, BoxPath> value { None { } };

    bool isNone() const { return WTF::holdsAlternative<None>(value); }
    bool isRayPath() const { return WTF::holdsAlternative<RayPath>(value); }
    bool isReferencePath() const { return WTF::holdsAlternative<ReferencePath>(value); }
    bool isBasicShapePath() const { return WTF::holdsAlternative<BasicShapePath>(value); }
    bool isBoxPath() const { return WTF::holdsAlternative<BoxPath>(value); }

    bool operator==(const AcceleratedEffectOffsetPath&) const = default;
};

// MARK: - Path Evaluation

std::optional<WebCore::Path> tryPath(const AcceleratedEffectOffsetPath&, const TransformOperationData&);

// MARK: - Blending

bool canBlend(const AcceleratedEffectOffsetPath&, const AcceleratedEffectOffsetPath&);
AcceleratedEffectOffsetPath blend(const AcceleratedEffectOffsetPath&, const AcceleratedEffectOffsetPath&, const BlendingContext&);

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
