/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)

#include "PathImpl.h"
#include "PlatformPath.h"
#include "WindRule.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPath.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Function.h>

namespace WebCore {

class GraphicsContext;
class PathStream;

class PathSkia final : public PathImpl {
public:
    static Ref<PathSkia> create();
    static Ref<PathSkia> create(const PathSegment&);
    static Ref<PathSkia> create(const PathStream&);
    static Ref<PathSkia> create(SkPath&&, RefPtr<PathStream>&&);

    PlatformPathPtr platformPath() const;

    void addPath(const PathSkia&, const AffineTransform&);

    bool definitelyEqual(const PathImpl&) const final;
    Ref<PathImpl> copy() const final;
    void add(PathMoveTo) final;
    void add(PathLineTo) final;
    void add(PathQuadCurveTo) final;
    void add(PathBezierCurveTo) final;
    void add(PathArcTo) final;
    void add(PathArc) final;
    void add(PathClosedArc) final;
    void add(PathEllipse) final;
    void add(PathEllipseInRect) final;
    void add(PathRect) final;
    void add(PathRoundedRect) final;
    void add(PathCloseSubpath) final;

    bool applyElements(const PathElementApplier&) const final;

    bool transform(const AffineTransform&) final;

    bool contains(const FloatPoint&, WindRule) const;
    bool strokeContains(const FloatPoint&, const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

    FloatRect strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

private:
    PathSkia() = default;
    explicit PathSkia(const SkPath&);

    bool isEmpty() const final;

    FloatPoint currentPoint() const final;

    FloatRect fastBoundingRect() const final;
    FloatRect boundingRect() const final;

    void addEllipse(const FloatPoint&, float radiusX, float radiusY, float startAngle, float endAngle, RotationDirection);

    SkPath m_platformPath;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PathSkia)
    static bool isType(const WebCore::PathImpl& pathImpl) { return !pathImpl.isPathStream(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(SKIA)
