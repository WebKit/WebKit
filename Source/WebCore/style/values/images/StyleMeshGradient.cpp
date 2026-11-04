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

#include "config.h"
#include "StyleMeshGradient.h"

#include "MeshGradient.h"
#include "StyleAppleColorFilter.h"
#include "StyleColorResolver.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

Ref<WebCore::MeshGradient> createPlatformMeshGradient(const MeshGradient& gradient, FloatSize size, const ComputedStyle& style)
{
    auto zoom = style.usedZoomForLength();
    bool hasColorFilter = !style.appleColorFilter().isNone();
    ColorResolver colorResolver { style };

    auto resolveColor = [&](const Color& color) {
        return hasColorFilter ? colorResolver.colorResolvingCurrentColorApplyingColorFilter(color) : colorResolver.colorResolvingCurrentColor(color);
    };

    unsigned rows = gradient.rows.size();
    unsigned columns = gradient.rows[0].size();

    // 1. Resolve the position and color of each point.
    Vector<WebCore::MeshGradient::Point> points;
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            auto& point = gradient.rows[row][column];

            auto position = FloatPoint { size.width() * column / (columns - 1), size.height() * row / (rows - 1) };
            if (point.location) {
                position = WTF::switchOn(*point.location,
                    [&](const MeshGradientAbsolutePosition& absolutePosition) {
                        return evaluate<FloatPoint>(absolutePosition.position, size, zoom);
                    },
                    [&](const MeshGradientRelativePosition& relativePosition) {
                        return evaluate<FloatPoint>(relativePosition.offset, size, zoom);
                    }
                );
            }

            points.append({
                .position = position,
                .color = resolveColor(point.color),
                .controlPoints = { },
            });
        }
    }

    auto resolveControlPoints = [&](const MeshGradientPoint& point, const WebCore::MeshGradient::Point& platformPoint) -> std::optional<RectEdges<FloatPoint>> {
        if (!point.location)
            return std::nullopt;

        return WTF::switchOn(*point.location,
            [&](const MeshGradientAbsolutePosition& absolutePosition) -> std::optional<RectEdges<FloatPoint>> {
                if (!absolutePosition.controlPoints)
                    return std::nullopt;
                return RectEdges<FloatPoint>::map(*absolutePosition.controlPoints, [&](auto& controlPoint) {
                    return evaluate<FloatPoint>(controlPoint, size, zoom);
                });
            },
            [&](const MeshGradientRelativePosition& relativePosition) -> std::optional<RectEdges<FloatPoint>> {
                if (!relativePosition.controlPoints)
                    return std::nullopt;
                return RectEdges<FloatPoint>::map(*relativePosition.controlPoints, [&](auto& controlPoint) {
                    auto offset = evaluate<FloatPoint>(controlPoint.offset, size, zoom);
                    if (controlPoint.anchor)
                        return offset;
                    return platformPoint.position + offset;
                });
            }
        );
    };

    // 2. Resolve the control points, which need the positions of the points. Without control points, each side is a straight
    // line, with its control point a third of the way to the neighboring point. The sides on the edge of the mesh have no
    // neighbor, and use the point's own position.
    // FIXME: The explainer does not define the control points of a point that gives none.
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            auto& platformPoint = points[row * columns + column];

            if (auto controlPoints = resolveControlPoints(gradient.rows[row][column], platformPoint)) {
                platformPoint.controlPoints = WTF::move(*controlPoints);
                continue;
            }

            auto towardNeighbor = [&](unsigned neighborRow, unsigned neighborColumn) {
                return platformPoint.position + (points[neighborRow * columns + neighborColumn].position - platformPoint.position) / 3;
            };

            platformPoint.controlPoints = {
                row ? towardNeighbor(row - 1, column) : platformPoint.position,
                column < columns - 1 ? towardNeighbor(row, column + 1) : platformPoint.position,
                row < rows - 1 ? towardNeighbor(row + 1, column) : platformPoint.position,
                column ? towardNeighbor(row, column - 1) : platformPoint.position,
            };
        }
    }

    auto backgroundColor = gradient.backgroundColor ? resolveColor(*gradient.backgroundColor) : WebCore::Color::transparentBlack;

    return WebCore::MeshGradient::create(gradient.colorInterpolationMethod.method.value, rows, columns, WTF::move(points), backgroundColor);
}

bool containsCurrentColor(const MeshGradient& gradient)
{
    if (gradient.backgroundColor && containsCurrentColor(*gradient.backgroundColor))
        return true;

    return std::ranges::any_of(gradient.rows, [](auto& row) {
        return std::ranges::any_of(row, [](auto& point) {
            return containsCurrentColor(point.color);
        });
    });
}

} // namespace Style
} // namespace WebCore
