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

static RectEdges<FloatPoint> completeControlPoints(std::span<const FloatPoint> controlPoints, FloatPoint meshPoint)
{
    // "Control points are defined in the standard "top, right, bottom, left" order
    //  (similar to properties like 'margin'). If omitted, a control point takes its
    //  value from the specified point on the opposite side, rotated 180 degrees around
    //  the mesh point. (The second point, if omitted, takes its value from the first,
    //  rotated 90 degrees.)"
    // https://github.com/w3c/csswg-drafts/pull/14655

    auto rotated90Degrees = [&](FloatPoint controlPoint) {
        auto offset = controlPoint - meshPoint;
        return meshPoint + FloatSize { -offset.height(), offset.width() };
    };

    auto rotated180Degrees = [&](FloatPoint controlPoint) {
        return meshPoint + (meshPoint - controlPoint);
    };

    auto top = controlPoints[0];
    auto right = controlPoints.size() > 1 ? controlPoints[1] : rotated90Degrees(top);
    auto bottom = controlPoints.size() > 2 ? controlPoints[2] : rotated180Degrees(top);
    auto left = controlPoints.size() > 3 ? controlPoints[3] : rotated180Degrees(right);

    return { top, right, bottom, left };
}

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

    auto defaultPosition = [&](unsigned row, unsigned column) {
        return FloatPoint { size.width() * column / (columns - 1), size.height() * row / (rows - 1) };
    };

    // 1. Resolve the position and color of each point.
    Vector<WebCore::MeshGradient::Point> points;
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            auto& point = gradient.rows[row][column];

            auto position = defaultPosition(row, column);
            if (point.location) {
                position = WTF::switchOn(*point.location,
                    [&](const MeshGradientAbsolutePosition& absolutePosition) {
                        return evaluate<FloatPoint>(absolutePosition.position, size, zoom);
                    },
                    [&](const MeshGradientRelativePosition& relativePosition) {
                        return position + evaluate<FloatPoint>(relativePosition.offset, size, zoom);
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

    auto resolveControlPoints = [&](const MeshGradientPoint& point, FloatPoint position, FloatPoint relativeTo) -> std::optional<RectEdges<FloatPoint>> {
        if (!point.location)
            return std::nullopt;

        return WTF::switchOn(*point.location,
            [&](const MeshGradientAbsolutePosition& absolutePosition) -> std::optional<RectEdges<FloatPoint>> {
                if (!absolutePosition.controlPoints)
                    return std::nullopt;
                auto controlPoints = WTF::map<4>(*absolutePosition.controlPoints, [&](auto& controlPoint) {
                    return evaluate<FloatPoint>(controlPoint, size, zoom);
                });
                return completeControlPoints(controlPoints.span(), position);
            },
            [&](const MeshGradientRelativePosition& relativePosition) -> std::optional<RectEdges<FloatPoint>> {
                if (!relativePosition.controlPoints)
                    return std::nullopt;
                auto controlPoints = WTF::map<4>(*relativePosition.controlPoints, [&](auto& controlPoint) {
                    auto offset = evaluate<FloatPoint>(controlPoint.offset, size, zoom);
                    if (controlPoint.anchor)
                        return offset;
                    return relativeTo + offset;
                });
                return completeControlPoints(controlPoints.span(), position);
            }
        );
    };

    // 2. Resolve the control points, which need the positions of the points. Without control points, each side is a straight
    // line, with its control point a third of the way to the neighboring point. The sides on the edge of the mesh have no
    // neighbor, and use the point's own position.
    // FIXME: The draft does not define the control points of a point that gives none.
    for (unsigned row = 0; row < rows; ++row) {
        for (unsigned column = 0; column < columns; ++column) {
            auto& platformPoint = points[row * columns + column];

            if (auto controlPoints = resolveControlPoints(gradient.rows[row][column], platformPoint.position, defaultPosition(row, column))) {
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
